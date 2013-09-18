/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qqmlobjectcreator_p.h"

#include <private/qqmlengine_p.h>
#include <private/qqmlabstractbinding_p.h>
#include <private/qqmlvmemetaobject_p.h>
#include <private/qv4function_p.h>
#include <private/qv4functionobject_p.h>
#include <private/qqmlcontextwrapper_p.h>
#include <private/qqmlbinding_p.h>
#include <private/qqmlstringconverters_p.h>
#include <private/qqmlboundsignal_p.h>
#include <private/qqmltrace_p.h>
#include <private/qqmlcomponentattached_p.h>
#include <QQmlComponent>
#include <private/qqmlcomponent_p.h>

QT_USE_NAMESPACE

namespace {
struct ActiveOCRestorer
{
    ActiveOCRestorer(QmlObjectCreator *creator, QQmlEnginePrivate *ep)
    : ep(ep), oldCreator(ep->activeObjectCreator) { ep->activeObjectCreator = creator; }
    ~ActiveOCRestorer() { ep->activeObjectCreator = oldCreator; }

    QQmlEnginePrivate *ep;
    QmlObjectCreator *oldCreator;
};
}

QQmlCompilePass::QQmlCompilePass(const QUrl &url, const QV4::CompiledData::QmlUnit *unit)
    : url(url)
    , qmlUnit(unit)
{
}

void QQmlCompilePass::recordError(const QV4::CompiledData::Location &location, const QString &description)
{
    QQmlError error;
    error.setUrl(url);
    error.setLine(location.line);
    error.setColumn(location.column);
    error.setDescription(description);
    errors << error;
}

#define COMPILE_EXCEPTION(token, desc) \
    { \
        recordError((token)->location, desc); \
        return false; \
    }

static QAtomicInt classIndexCounter(0);

QQmlPropertyCacheCreator::QQmlPropertyCacheCreator(QQmlEnginePrivate *enginePrivate, const QV4::CompiledData::QmlUnit *unit, const QUrl &url, const QQmlImports *imports,
                                                   QHash<int, QQmlCompiledData::TypeReference> *resolvedTypes)
    : QQmlCompilePass(url, unit)
    , enginePrivate(enginePrivate)
    , imports(imports)
    , resolvedTypes(resolvedTypes)
{
}

bool QQmlPropertyCacheCreator::create(const QV4::CompiledData::Object *obj, QQmlPropertyCache **resultCache, QByteArray *vmeMetaObjectData)
{
    Q_ASSERT(!stringAt(obj->inheritedTypeNameIndex).isEmpty());

    QQmlCompiledData::TypeReference typeRef = resolvedTypes->value(obj->inheritedTypeNameIndex);
    QQmlPropertyCache *baseTypeCache = typeRef.createPropertyCache(QQmlEnginePrivate::get(enginePrivate));
    Q_ASSERT(baseTypeCache);
    if (obj->nProperties == 0 && obj->nSignals == 0 && obj->nFunctions == 0) {
        *resultCache = baseTypeCache;
        vmeMetaObjectData->clear();
        return true;
    }

    QQmlPropertyCache *cache = baseTypeCache->copyAndReserve(QQmlEnginePrivate::get(enginePrivate), obj->nProperties, obj->nFunctions, obj->nSignals);
    *resultCache = cache;

    vmeMetaObjectData->clear();

    struct TypeData {
        QV4::CompiledData::Property::Type dtype;
        int metaType;
    } builtinTypes[] = {
        { QV4::CompiledData::Property::Var, QMetaType::QVariant },
        { QV4::CompiledData::Property::Variant, QMetaType::QVariant },
        { QV4::CompiledData::Property::Int, QMetaType::Int },
        { QV4::CompiledData::Property::Bool, QMetaType::Bool },
        { QV4::CompiledData::Property::Real, QMetaType::Double },
        { QV4::CompiledData::Property::String, QMetaType::QString },
        { QV4::CompiledData::Property::Url, QMetaType::QUrl },
        { QV4::CompiledData::Property::Color, QMetaType::QColor },
        { QV4::CompiledData::Property::Font, QMetaType::QFont },
        { QV4::CompiledData::Property::Time, QMetaType::QTime },
        { QV4::CompiledData::Property::Date, QMetaType::QDate },
        { QV4::CompiledData::Property::DateTime, QMetaType::QDateTime },
        { QV4::CompiledData::Property::Rect, QMetaType::QRectF },
        { QV4::CompiledData::Property::Point, QMetaType::QPointF },
        { QV4::CompiledData::Property::Size, QMetaType::QSizeF },
        { QV4::CompiledData::Property::Vector2D, QMetaType::QVector2D },
        { QV4::CompiledData::Property::Vector3D, QMetaType::QVector3D },
        { QV4::CompiledData::Property::Vector4D, QMetaType::QVector4D },
        { QV4::CompiledData::Property::Matrix4x4, QMetaType::QMatrix4x4 },
        { QV4::CompiledData::Property::Quaternion, QMetaType::QQuaternion }
    };
    static const uint builtinTypeCount = sizeof(builtinTypes) / sizeof(TypeData);

    QByteArray newClassName;

    if (false /* ### compileState->root == obj && !compileState->nested*/) {
#if 0 // ###
        QString path = output->url.path();
        int lastSlash = path.lastIndexOf(QLatin1Char('/'));
        if (lastSlash > -1) {
            QString nameBase = path.mid(lastSlash + 1, path.length()-lastSlash-5);
            if (!nameBase.isEmpty() && nameBase.at(0).isUpper())
                newClassName = nameBase.toUtf8() + "_QMLTYPE_" +
                               QByteArray::number(classIndexCounter.fetchAndAddRelaxed(1));
        }
#endif
    }
    if (newClassName.isEmpty()) {
        newClassName = QQmlMetaObject(baseTypeCache).className();
        newClassName.append("_QML_");
        newClassName.append(QByteArray::number(classIndexCounter.fetchAndAddRelaxed(1)));
    }

    cache->_dynamicClassName = newClassName;

    int aliasCount = 0;
    int varPropCount = 0;

    const QV4::CompiledData::Property *p = obj->propertyTable();
    for (quint32 i = 0; i < obj->nProperties; ++i, ++p) {

        if (p->type == QV4::CompiledData::Property::Alias)
            aliasCount++;
        else if (p->type == QV4::CompiledData::Property::Var)
            varPropCount++;

#if 0 // ### Do this elsewhere
        // No point doing this for both the alias and non alias cases
        QQmlPropertyData *d = property(obj, p->name);
        if (d && d->isFinal())
            COMPILE_EXCEPTION(p, tr("Cannot override FINAL property"));
#endif
    }

    typedef QQmlVMEMetaData VMD;

    QByteArray &dynamicData = *vmeMetaObjectData = QByteArray(sizeof(QQmlVMEMetaData)
                                                              + obj->nProperties * sizeof(VMD::PropertyData)
                                                              + obj->nFunctions * sizeof(VMD::MethodData)
                                                              + aliasCount * sizeof(VMD::AliasData), 0);

    int effectivePropertyIndex = cache->propertyIndexCacheStart;
    int effectiveMethodIndex = cache->methodIndexCacheStart;

    // For property change signal override detection.
    // We prepopulate a set of signal names which already exist in the object,
    // and throw an error if there is a signal/method defined as an override.
    QSet<QString> seenSignals;
    seenSignals << QStringLiteral("destroyed") << QStringLiteral("parentChanged") << QStringLiteral("objectNameChanged");
    QQmlPropertyCache *parentCache = cache;
    while ((parentCache = parentCache->parent())) {
        if (int pSigCount = parentCache->signalCount()) {
            int pSigOffset = parentCache->signalOffset();
            for (int i = pSigOffset; i < pSigCount; ++i) {
                QQmlPropertyData *currPSig = parentCache->signal(i);
                // XXX TODO: find a better way to get signal name from the property data :-/
                for (QQmlPropertyCache::StringCache::ConstIterator iter = parentCache->stringCache.begin();
                        iter != parentCache->stringCache.end(); ++iter) {
                    if (currPSig == (*iter).second) {
                        seenSignals.insert(iter.key());
                        break;
                    }
                }
            }
        }
    }

    // First set up notify signals for properties - first normal, then var, then alias
    enum { NSS_Normal = 0, NSS_Var = 1, NSS_Alias = 2 };
    for (int ii = NSS_Normal; ii <= NSS_Alias; ++ii) { // 0 == normal, 1 == var, 2 == alias

        if (ii == NSS_Var && varPropCount == 0) continue;
        else if (ii == NSS_Alias && aliasCount == 0) continue;

        const QV4::CompiledData::Property *p = obj->propertyTable();
        for (quint32 i = 0; i < obj->nProperties; ++i, ++p) {
            if ((ii == NSS_Normal && (p->type == QV4::CompiledData::Property::Alias ||
                                      p->type == QV4::CompiledData::Property::Var)) ||
                ((ii == NSS_Var) && (p->type != QV4::CompiledData::Property::Var)) ||
                ((ii == NSS_Alias) && (p->type != QV4::CompiledData::Property::Alias)))
                continue;

            quint32 flags = QQmlPropertyData::IsSignal | QQmlPropertyData::IsFunction |
                            QQmlPropertyData::IsVMESignal;

            QString changedSigName = stringAt(p->nameIndex) + QLatin1String("Changed");
            seenSignals.insert(changedSigName);

            cache->appendSignal(changedSigName, flags, effectiveMethodIndex++);
        }
    }

    // Dynamic signals
    for (uint i = 0; i < obj->nSignals; ++i) {
        const QV4::CompiledData::Signal *s = obj->signalAt(i);
        const int paramCount = s->nParameters;

        QList<QByteArray> names;
        QVarLengthArray<int, 10> paramTypes(paramCount?(paramCount + 1):0);

        if (paramCount) {
            paramTypes[0] = paramCount;

            for (int i = 0; i < paramCount; ++i) {
                const QV4::CompiledData::Parameter *param = s->parameterAt(i);
                names.append(stringAt(param->nameIndex).toUtf8());
                if (param->type < builtinTypeCount) {
                    // built-in type
                    paramTypes[i + 1] = builtinTypes[param->type].metaType;
                } else {
                    // lazily resolved type
                    Q_ASSERT(param->type == QV4::CompiledData::Property::Custom);
                    const QString customTypeName = stringAt(param->customTypeNameIndex);
                    QQmlType *qmltype = 0;
                    if (!imports->resolveType(customTypeName, &qmltype, 0, 0, 0))
                        COMPILE_EXCEPTION(s, tr("Invalid signal parameter type: %1").arg(customTypeName));

                    if (qmltype->isComposite()) {
                        QQmlTypeData *tdata = enginePrivate->typeLoader.getType(qmltype->sourceUrl());
                        Q_ASSERT(tdata);
                        Q_ASSERT(tdata->isComplete());

                        QQmlCompiledData *data = tdata->compiledData();

                        paramTypes[i + 1] = data->metaTypeId;

                        tdata->release();
                    } else {
                        paramTypes[i + 1] = qmltype->typeId();
                    }
                }
            }
        }

        ((QQmlVMEMetaData *)dynamicData.data())->signalCount++;

        quint32 flags = QQmlPropertyData::IsSignal | QQmlPropertyData::IsFunction |
                        QQmlPropertyData::IsVMESignal;
        if (paramCount)
            flags |= QQmlPropertyData::HasArguments;

        QString signalName = stringAt(s->nameIndex);
        if (seenSignals.contains(signalName))
            COMPILE_EXCEPTION(s, tr("Duplicate signal name: invalid override of property change signal or superclass signal"));
        seenSignals.insert(signalName);

        cache->appendSignal(signalName, flags, effectiveMethodIndex++,
                            paramCount?paramTypes.constData():0, names);
    }


    // Dynamic slots
    const quint32 *functionIndex = obj->functionOffsetTable();
    for (quint32 i = 0; i < obj->nFunctions; ++i, ++functionIndex) {
        const QV4::CompiledData::Function *s = qmlUnit->header.functionAt(*functionIndex);
        int paramCount = s->nFormals;

        quint32 flags = QQmlPropertyData::IsFunction | QQmlPropertyData::IsVMEFunction;

        if (paramCount)
            flags |= QQmlPropertyData::HasArguments;

        QString slotName = stringAt(s->nameIndex);
        if (seenSignals.contains(slotName))
            COMPILE_EXCEPTION(s, tr("Duplicate method name: invalid override of property change signal or superclass signal"));
        // Note: we don't append slotName to the seenSignals list, since we don't
        // protect against overriding change signals or methods with properties.

        const quint32 *formalsIndices = s->formalsTable();
        QList<QByteArray> parameterNames;
        parameterNames.reserve(paramCount);
        for (int i = 0; i < paramCount; ++i)
            parameterNames << stringAt(formalsIndices[i]).toUtf8();

        cache->appendMethod(slotName, flags, effectiveMethodIndex++, parameterNames);
    }


    // Dynamic properties (except var and aliases)
    int effectiveSignalIndex = cache->signalHandlerIndexCacheStart;
    /* const QV4::CompiledData::Property* */ p = obj->propertyTable();
    for (quint32 i = 0; i < obj->nProperties; ++i, ++p) {

        if (p->type == QV4::CompiledData::Property::Alias ||
            p->type == QV4::CompiledData::Property::Var)
            continue;

        int propertyType = 0;
        int vmePropertyType = 0;
        quint32 propertyFlags = 0;

        if (p->type < builtinTypeCount) {
            propertyType = builtinTypes[p->type].metaType;
            vmePropertyType = propertyType;

            if (p->type == QV4::CompiledData::Property::Variant)
                propertyFlags |= QQmlPropertyData::IsQVariant;
        } else {
            Q_ASSERT(p->type == QV4::CompiledData::Property::CustomList ||
                     p->type == QV4::CompiledData::Property::Custom);

            QQmlType *qmltype = 0;
            if (!imports->resolveType(stringAt(p->customTypeNameIndex), &qmltype, 0, 0, 0)) {
                COMPILE_EXCEPTION(p, tr("Invalid property type"));
            }

            Q_ASSERT(qmltype);
            if (qmltype->isComposite()) {
                QQmlTypeData *tdata = enginePrivate->typeLoader.getType(qmltype->sourceUrl());
                Q_ASSERT(tdata);
                Q_ASSERT(tdata->isComplete());

                QQmlCompiledData *data = tdata->compiledData();

                if (p->type == QV4::CompiledData::Property::Custom) {
                    propertyType = data->metaTypeId;
                    vmePropertyType = QMetaType::QObjectStar;
                } else {
                    propertyType = data->listMetaTypeId;
                    vmePropertyType = qMetaTypeId<QQmlListProperty<QObject> >();
                }

                tdata->release();
            } else {
                if (p->type == QV4::CompiledData::Property::Custom) {
                    propertyType = qmltype->typeId();
                    vmePropertyType = QMetaType::QObjectStar;
                } else {
                    propertyType = qmltype->qListTypeId();
                    vmePropertyType = qMetaTypeId<QQmlListProperty<QObject> >();
                }
            }

            if (p->type == QV4::CompiledData::Property::Custom)
                propertyFlags |= QQmlPropertyData::IsQObjectDerived;
            else
                propertyFlags |= QQmlPropertyData::IsQList;
        }

        if ((!p->flags & QV4::CompiledData::Property::IsReadOnly) && p->type != QV4::CompiledData::Property::CustomList)
            propertyFlags |= QQmlPropertyData::IsWritable;


        QString propertyName = stringAt(p->nameIndex);
        if (i == obj->indexOfDefaultProperty) cache->_defaultPropertyName = propertyName;
        cache->appendProperty(propertyName, propertyFlags, effectivePropertyIndex++,
                              propertyType, effectiveSignalIndex);

        effectiveSignalIndex++;

        VMD *vmd = (QQmlVMEMetaData *)dynamicData.data();
        (vmd->propertyData() + vmd->propertyCount)->propertyType = vmePropertyType;
        vmd->propertyCount++;
    }

    // Now do var properties
    /* const QV4::CompiledData::Property* */ p = obj->propertyTable();
    for (quint32 i = 0; i < obj->nProperties; ++i, ++p) {

        if (p->type != QV4::CompiledData::Property::Var)
            continue;

        quint32 propertyFlags = QQmlPropertyData::IsVarProperty;
        if (!p->flags & QV4::CompiledData::Property::IsReadOnly)
            propertyFlags |= QQmlPropertyData::IsWritable;

        VMD *vmd = (QQmlVMEMetaData *)dynamicData.data();
        (vmd->propertyData() + vmd->propertyCount)->propertyType = QMetaType::QVariant;
        vmd->propertyCount++;
        ((QQmlVMEMetaData *)dynamicData.data())->varPropertyCount++;

        QString propertyName = stringAt(p->nameIndex);
        if (i == obj->indexOfDefaultProperty) cache->_defaultPropertyName = propertyName;
        cache->appendProperty(propertyName, propertyFlags, effectivePropertyIndex++,
                              QMetaType::QVariant, effectiveSignalIndex);

        effectiveSignalIndex++;
    }

    // Alias property count.  Actual data is setup in buildDynamicMetaAliases
    ((QQmlVMEMetaData *)dynamicData.data())->aliasCount = aliasCount;

    // Dynamic slot data - comes after the property data
    /*const quint32* */functionIndex = obj->functionOffsetTable();
    for (quint32 i = 0; i < obj->nFunctions; ++i, ++functionIndex) {
        const QV4::CompiledData::Function *s = qmlUnit->header.functionAt(*functionIndex);

        VMD::MethodData methodData = { int(s->nFormals),
                                       /* body offset*/0,
                                       /* body length*/0,
                                       /* s->location.start.line */0 }; // ###

        VMD *vmd = (QQmlVMEMetaData *)dynamicData.data();
        VMD::MethodData &md = *(vmd->methodData() + vmd->methodCount);
        vmd->methodCount++;
        md = methodData;
    }

    return true;
}

static void removeBindingOnProperty(QObject *o, int index)
{
    int coreIndex = index & 0x0000FFFF;
    int valueTypeIndex = (index & 0xFFFF0000 ? index >> 16 : -1);

    QQmlAbstractBinding *binding = QQmlPropertyPrivate::setBinding(o, coreIndex, valueTypeIndex, 0);
    if (binding) binding->destroy();
}

QmlObjectCreator::QmlObjectCreator(QQmlContextData *parentContext, QQmlCompiledData *compiledData)
    : QQmlCompilePass(compiledData->url, compiledData->qmlUnit)
    , componentAttached(0)
    , engine(parentContext->engine)
    , jsUnit(compiledData->compilationUnit)
    , parentContext(parentContext)
    , context(0)
    , resolvedTypes(compiledData->resolvedTypes)
    , propertyCaches(compiledData->propertyCaches)
    , vmeMetaObjectData(compiledData->datas)
    , compiledData(compiledData)
    , _qobject(0)
    , _compiledObject(0)
    , _ddata(0)
    , _propertyCache(0)
    , _vmeMetaObject(0)
    , _qmlContext(0)
{
}

QObject *QmlObjectCreator::create(int subComponentIndex, QObject *parent)
{
    int objectToCreate;

    if (subComponentIndex == -1) {
        objectIndexToId = compiledData->objectIndexToIdForRoot;
        objectToCreate = qmlUnit->indexOfRootObject;
    } else {
        objectIndexToId = compiledData->objectIndexToIdPerComponent[subComponentIndex];
        const QV4::CompiledData::Object *compObj = qmlUnit->objectAt(subComponentIndex);
        objectToCreate = compObj->bindingTable()->value.objectIndex;
    }

    context = new QQmlContextData;
    context->isInternal = true;
    context->url = compiledData->url;
    context->urlString = compiledData->name;
    context->imports = compiledData->importCache;
    context->imports->addref();
    context->setParent(parentContext);

    QVector<QQmlContextData::ObjectIdMapping> mapping(objectIndexToId.count());
    for (QHash<int, int>::ConstIterator it = objectIndexToId.constBegin(), end = objectIndexToId.constEnd();
         it != end; ++it) {
        const QV4::CompiledData::Object *obj = qmlUnit->objectAt(it.key());

        QQmlContextData::ObjectIdMapping m;
        m.id = it.value();
        m.name = stringAt(obj->idIndex);
        mapping[m.id] = m;
    }
    context->setIdPropertyData(mapping);

    QObject *instance = createInstance(objectToCreate, parent);

    QQmlData *ddata = QQmlData::get(instance);
    Q_ASSERT(ddata);
    ddata->compiledData = compiledData;
    ddata->compiledData->addref();

    return instance;
}

void QmlObjectCreator::setPropertyValue(QQmlPropertyData *property, const QV4::CompiledData::Binding *binding)
{
    QQmlPropertyPrivate::WriteFlags propertyWriteFlags = QQmlPropertyPrivate::BypassInterceptor |
                                                               QQmlPropertyPrivate::RemoveBindingOnAliasWrite;
    int propertyWriteStatus = -1;
    void *argv[] = { 0, 0, &propertyWriteStatus, &propertyWriteFlags };

    // ### enums

    switch (property->propType) {
    case QMetaType::QVariant: {
        if (binding->type == QV4::CompiledData::Binding::Type_Number) {
            double n = binding->valueAsNumber();
            if (double(int(n)) == n) {
                if (property->isVarProperty()) {
                    _vmeMetaObject->setVMEProperty(property->coreIndex, QV4::Value::fromInt32(int(n)));
                } else {
                    int i = int(n);
                    QVariant value(i);
                    argv[0] = &value;
                    QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
                }
            } else {
                if (property->isVarProperty()) {
                    _vmeMetaObject->setVMEProperty(property->coreIndex, QV4::Value::fromDouble(n));
                } else {
                    QVariant value(n);
                    argv[0] = &value;
                    QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
                }
            }
        } else if (binding->type == QV4::CompiledData::Binding::Type_Boolean) {
            if (property->isVarProperty()) {
                _vmeMetaObject->setVMEProperty(property->coreIndex, QV4::Value::fromBoolean(binding->valueAsBoolean()));
            } else {
                QVariant value(binding->valueAsBoolean());
                argv[0] = &value;
                QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
            }
        } else {
            QString stringValue = binding->valueAsString(&qmlUnit->header);
            if (property->isVarProperty()) {
                _vmeMetaObject->setVMEProperty(property->coreIndex, QV4::Value::fromString(QV8Engine::getV4(engine), stringValue));
            } else {
                QVariant value = QQmlStringConverters::variantFromString(stringValue);
                argv[0] = &value;
                QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
            }
        }
    }
    break;
    case QVariant::String: {
        if (binding->type == QV4::CompiledData::Binding::Type_String) {
            QString value = binding->valueAsString(&qmlUnit->header);
            argv[0] = &value;
            QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
        } else {
            recordError(binding->location, tr("Invalid property assignment: string expected"));
        }
    }
    break;
    case QVariant::StringList: {
        if (binding->type == QV4::CompiledData::Binding::Type_String) {
            QStringList value(binding->valueAsString(&qmlUnit->header));
            argv[0] = &value;
            QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
        } else {
            recordError(binding->location, tr("Invalid property assignment: string or string list expected"));
        }
    }
    break;
    case QVariant::ByteArray: {
        if (binding->type == QV4::CompiledData::Binding::Type_String) {
            QByteArray value(binding->valueAsString(&qmlUnit->header).toUtf8());
            argv[0] = &value;
            QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
        } else {
            recordError(binding->location, tr("Invalid property assignment: byte array expected"));
        }
    }
    break;
    case QVariant::Url: {
        if (binding->type == QV4::CompiledData::Binding::Type_String) {
            QString string = binding->valueAsString(&qmlUnit->header);
            // Encoded dir-separators defeat QUrl processing - decode them first
            string.replace(QLatin1String("%2f"), QLatin1String("/"), Qt::CaseInsensitive);
            QUrl value = string.isEmpty() ? QUrl() : this->url.resolved(QUrl(string));
            // Apply URL interceptor
            if (engine->urlInterceptor())
                value = engine->urlInterceptor()->intercept(value, QQmlAbstractUrlInterceptor::UrlString);
            argv[0] = &value;
            QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
        } else {
            recordError(binding->location, tr("Invalid property assignment: url expected"));
        }
    }
    break;
    case QVariant::UInt: {
        if (binding->type == QV4::CompiledData::Binding::Type_Number) {
            double d = binding->valueAsNumber();
            if (double(uint(d)) == d) {
                uint value = uint(d);
                argv[0] = &value;
                QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
                break;
            }
        }
        recordError(binding->location, tr("Invalid property assignment: unsigned int expected"));
    }
    break;
    case QVariant::Int: {
        if (binding->type == QV4::CompiledData::Binding::Type_Number) {
            double d = binding->valueAsNumber();
            if (double(int(d)) == d) {
                int value = int(d);
                argv[0] = &value;
                QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
                break;
            }
        }
        recordError(binding->location, tr("Invalid property assignment: int expected"));
    }
    break;
    case QMetaType::Float: {
        if (binding->type == QV4::CompiledData::Binding::Type_Number) {
            float value = float(binding->valueAsNumber());
            argv[0] = &value;
            QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
        } else {
            recordError(binding->location, tr("Invalid property assignment: number expected"));
        }
    }
    break;
    case QVariant::Double: {
        if (binding->type == QV4::CompiledData::Binding::Type_Number) {
            double value = binding->valueAsNumber();
            argv[0] = &value;
            QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
        } else {
            recordError(binding->location, tr("Invalid property assignment: number expected"));
        }
    }
    break;
    case QVariant::Color: {
        bool ok = false;
        uint colorValue = QQmlStringConverters::rgbaFromString(binding->valueAsString(&qmlUnit->header), &ok);

        if (ok) {
            struct { void *data[4]; } buffer;
            if (QQml_valueTypeProvider()->storeValueType(property->propType, &colorValue, &buffer, sizeof(buffer))) {
                argv[0] = reinterpret_cast<void *>(&buffer);
                QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
            }
        } else {
            recordError(binding->location, tr("Invalid property assignment: color expected"));
        }
    }
    break;
#ifndef QT_NO_DATESTRING
    case QVariant::Date: {
        bool ok = false;
        QDate value = QQmlStringConverters::dateFromString(binding->valueAsString(&qmlUnit->header), &ok);
        if (ok) {
            argv[0] = &value;
            QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
        } else {
            recordError(binding->location, tr("Invalid property assignment: date expected"));
        }
    }
    break;
    case QVariant::Time: {
        bool ok = false;
        QTime value = QQmlStringConverters::timeFromString(binding->valueAsString(&qmlUnit->header), &ok);
        if (ok) {
            argv[0] = &value;
            QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
        } else {
            recordError(binding->location, tr("Invalid property assignment: time expected"));
        }
    }
    break;
    case QVariant::DateTime: {
        bool ok = false;
        QDateTime value = QQmlStringConverters::dateTimeFromString(binding->valueAsString(&qmlUnit->header), &ok);
        if (ok) {
            argv[0] = &value;
            QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
        } else {
            recordError(binding->location, tr("Invalid property assignment: datetime expected"));
        }
    }
    break;
#endif // QT_NO_DATESTRING
    case QVariant::Point: {
        bool ok = false;
        QPoint value = QQmlStringConverters::pointFFromString(binding->valueAsString(&qmlUnit->header), &ok).toPoint();
        if (ok) {
            argv[0] = &value;
            QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
        } else {
            recordError(binding->location, tr("Invalid property assignment: point expected"));
        }
    }
    break;
    case QVariant::PointF: {
        bool ok = false;
        QPointF value = QQmlStringConverters::pointFFromString(binding->valueAsString(&qmlUnit->header), &ok);
        if (ok) {
            argv[0] = &value;
            QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
        } else {
            recordError(binding->location, tr("Invalid property assignment: point expected"));
        }
    }
    break;
    case QVariant::Size: {
        bool ok = false;
        QSize value = QQmlStringConverters::sizeFFromString(binding->valueAsString(&qmlUnit->header), &ok).toSize();
        if (ok) {
            argv[0] = &value;
            QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
        } else {
            recordError(binding->location, tr("Invalid property assignment: size expected"));
        }
    }
    break;
    case QVariant::SizeF: {
        bool ok = false;
        QSizeF value = QQmlStringConverters::sizeFFromString(binding->valueAsString(&qmlUnit->header), &ok);
        if (ok) {
            argv[0] = &value;
            QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
        } else {
            recordError(binding->location, tr("Invalid property assignment: size expected"));
        }
    }
    break;
    case QVariant::Rect: {
        bool ok = false;
        QRect value = QQmlStringConverters::rectFFromString(binding->valueAsString(&qmlUnit->header), &ok).toRect();
        if (ok) {
            argv[0] = &value;
            QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
        } else {
            recordError(binding->location, tr("Invalid property assignment: point expected"));
        }
    }
    break;
    case QVariant::RectF: {
        bool ok = false;
        QRectF value = QQmlStringConverters::rectFFromString(binding->valueAsString(&qmlUnit->header), &ok);
        if (ok) {
            argv[0] = &value;
            QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
        } else {
            recordError(binding->location, tr("Invalid property assignment: point expected"));
        }
    }
    break;
    case QVariant::Bool: {
        if (binding->type == QV4::CompiledData::Binding::Type_Boolean) {
            bool value = binding->valueAsBoolean();
            argv[0] = &value;
            QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
        } else {
            recordError(binding->location, tr("Invalid property assignment: boolean expected"));
        }
    }
    break;
    case QVariant::Vector3D: {
        struct {
            float xp;
            float yp;
            float zy;
        } vec;
        if (QQmlStringConverters::createFromString(QMetaType::QVector3D, binding->valueAsString(&qmlUnit->header), &vec, sizeof(vec))) {
            argv[0] = reinterpret_cast<void *>(&vec);
            QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
        } else {
            recordError(binding->location, tr("Invalid property assignment: 3D vector expected"));
        }
    }
    break;
    case QVariant::Vector4D: {
        struct {
            float xp;
            float yp;
            float zy;
            float wp;
        } vec;
        if (QQmlStringConverters::createFromString(QMetaType::QVector4D, binding->valueAsString(&qmlUnit->header), &vec, sizeof(vec))) {
            argv[0] = reinterpret_cast<void *>(&vec);
            QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
        } else {
            recordError(binding->location, tr("Invalid property assignment: 4D vector expected"));
        }
    }
    break;
    case QVariant::RegExp:
        recordError(binding->location, tr("Invalid property assignment: regular expression expected; use /pattern/ syntax"));
        break;
    default: {
        // generate single literal value assignment to a list property if required
        if (property->propType == qMetaTypeId<QList<qreal> >()) {
            if (binding->type == QV4::CompiledData::Binding::Type_Number) {
                QList<qreal> value;
                value.append(binding->valueAsNumber());
                argv[0] = reinterpret_cast<void *>(&value);
                QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
            } else {
                recordError(binding->location, tr("Invalid property assignment: real or array of reals expected"));
            }
            break;
        } else if (property->propType == qMetaTypeId<QList<int> >()) {
            if (binding->type == QV4::CompiledData::Binding::Type_Number) {
                double n = binding->valueAsNumber();
                if (double(int(n)) == n) {
                    QList<int> value;
                    value.append(int(n));
                    argv[0] = reinterpret_cast<void *>(&value);
                    QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
                    break;
                } else {
                    recordError(binding->location, tr("Invalid property assignment: int or array of ints expected"));
                }
            }
            break;
        } else if (property->propType == qMetaTypeId<QList<bool> >()) {
            if (binding->type == QV4::CompiledData::Binding::Type_Boolean) {
                QList<bool> value;
                value.append(binding->valueAsBoolean());
                argv[0] = reinterpret_cast<void *>(&value);
                QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
            } else {
                recordError(binding->location, tr("Invalid property assignment: bool or array of bools expected"));
            }
            break;
        } else if (property->propType == qMetaTypeId<QList<QUrl> >()) {
            if (binding->type == QV4::CompiledData::Binding::Type_String) {
                QString urlString = binding->valueAsString(&qmlUnit->header);
                QUrl u = urlString.isEmpty() ? QUrl() : this->url.resolved(QUrl(urlString));
                QList<QUrl> value;
                value.append(u);
                argv[0] = reinterpret_cast<void *>(&value);
                QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
            } else {
                recordError(binding->location, tr("Invalid property assignment: url or array of urls expected"));
            }
            break;
        } else if (property->propType == qMetaTypeId<QList<QString> >()) {
            if (binding->type == QV4::CompiledData::Binding::Type_String) {
                QList<QString> value;
                value.append(binding->valueAsString(&qmlUnit->header));
                argv[0] = reinterpret_cast<void *>(&value);
                QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
            } else {
                recordError(binding->location, tr("Invalid property assignment: string or array of strings expected"));
            }
            break;
        } else if (property->propType == qMetaTypeId<QJSValue>()) {
            QJSValue value;
            if (binding->type == QV4::CompiledData::Binding::Type_Boolean) {
                value = QJSValue(binding->valueAsBoolean());
            } else if (binding->type == QV4::CompiledData::Binding::Type_Number) {
                double n = binding->valueAsNumber();
                if (double(int(n)) == n) {
                    value = QJSValue(int(n));
                } else
                    value = QJSValue(n);
            } else {
                value = QJSValue(binding->valueAsString(&qmlUnit->header));
            }
            argv[0] = reinterpret_cast<void *>(&value);
            QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
            break;
        }

        // otherwise, try a custom type assignment
        QString stringValue = binding->valueAsString(&qmlUnit->header);
        QQmlMetaType::StringConverter converter = QQmlMetaType::customStringConverter(property->propType);
        if (converter) {
            QVariant value = (*converter)(stringValue);

            QMetaProperty metaProperty = _qobject->metaObject()->property(property->coreIndex);
            if (value.isNull() || ((int)metaProperty.type() != property->propType && metaProperty.userType() != property->propType)) {
                recordError(binding->location, tr("Cannot assign value %1 to property %2").arg(stringValue).arg(QString::fromUtf8(metaProperty.name())));
                break;
            }

            argv[0] = value.data();
            QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
        } else {
            recordError(binding->location, tr("Invalid property assignment: unsupported type \"%1\"").arg(QString::fromLatin1(QMetaType::typeName(property->propType))));
        }
    }
    break;
    }
}

void QmlObjectCreator::setupBindings()
{
    QQmlListProperty<void> savedList;
    qSwap(_currentList, savedList);

    QQmlPropertyData *property = 0;

    const QV4::CompiledData::Binding *binding = _compiledObject->bindingTable();
    for (quint32 i = 0; i < _compiledObject->nBindings; ++i, ++binding) {

        if (!property || (i > 0 && (binding - 1)->propertyNameIndex != binding->propertyNameIndex)) {
            QString name = stringAt(binding->propertyNameIndex);
            if (!name.isEmpty())
                property = _propertyCache->property(name, _qobject, context);
            else
                property = 0;

            if (property && property->isQList()) {
                void *argv[1] = { (void*)&_currentList };
                QMetaObject::metacall(_qobject, QMetaObject::ReadProperty, property->coreIndex, argv);
            } else if (_currentList.object)
                _currentList = QQmlListProperty<void>();

        }

        if (!setPropertyValue(property, i, binding))
            return;
    }

    qSwap(_currentList, savedList);
}

bool QmlObjectCreator::setPropertyValue(QQmlPropertyData *property, int bindingIndex, const QV4::CompiledData::Binding *binding)
{
    if (binding->type == QV4::CompiledData::Binding::Type_AttachedProperty) {
        const QV4::CompiledData::Object *obj = qmlUnit->objectAt(binding->value.objectIndex);
        Q_ASSERT(stringAt(obj->inheritedTypeNameIndex).isEmpty());
        QQmlType *attachedType = resolvedTypes.value(binding->propertyNameIndex).type;
        const int id = attachedType->attachedPropertiesId();
        QObject *qmlObject = qmlAttachedPropertiesObjectById(id, _qobject);
        QQmlRefPointer<QQmlPropertyCache> cache = QQmlEnginePrivate::get(engine)->cache(qmlObject);
        if (!populateInstance(binding->value.objectIndex, qmlObject, cache))
            return false;
        return true;
    }

    QObject *createdSubObject = 0;
    if (binding->type == QV4::CompiledData::Binding::Type_Object) {
        createdSubObject = createInstance(binding->value.objectIndex, _qobject);
        if (!createdSubObject)
            return false;
    }

    // Child item:
    // ...
    //    Item {
    //        ...
    //    }
    if (!property)
        return true;

    if (binding->type == QV4::CompiledData::Binding::Type_GroupProperty) {
        const QV4::CompiledData::Object *obj = qmlUnit->objectAt(binding->value.objectIndex);
        if (stringAt(obj->inheritedTypeNameIndex).isEmpty()) {
            QQmlValueType *valueType = QQmlValueTypeFactory::valueType(property->propType);

            valueType->read(_qobject, property->coreIndex);

            QQmlRefPointer<QQmlPropertyCache> cache = QQmlEnginePrivate::get(engine)->cache(valueType);
            if (!populateInstance(binding->value.objectIndex, valueType, cache))
                return false;

            valueType->write(_qobject, property->coreIndex, QQmlPropertyPrivate::BypassInterceptor);
            return true;
        }
    }

    if (_ddata->hasBindingBit(property->coreIndex))
        removeBindingOnProperty(_qobject, property->coreIndex);

    if (binding->type == QV4::CompiledData::Binding::Type_Script) {
        QV4::Function *runtimeFunction = jsUnit->runtimeFunctions[binding->value.compiledScriptIndex];
        QV4::Value function = QV4::Value::fromObject(QV4::FunctionObject::creatScriptFunction(_qmlContext, runtimeFunction));

        if (binding->flags & QV4::CompiledData::Binding::IsSignalHandlerExpression) {
            int signalIndex = _propertyCache->methodIndexToSignalIndex(property->coreIndex);
            QQmlBoundSignal *bs = new QQmlBoundSignal(_qobject, signalIndex, _qobject, engine);
            QQmlBoundSignalExpression *expr = new QQmlBoundSignalExpression(_qobject, signalIndex,
                                                                            context, _qobject, function);

            bs->takeExpression(expr);
        } else {
            QQmlBinding *qmlBinding = new QQmlBinding(function, _qobject, context,
                                                      QString(), 0, 0); // ###
            qmlBinding->setTarget(_qobject, *property, context);
            qmlBinding->addToObject();

            _createdBindings[bindingIndex] = qmlBinding;
            qmlBinding->m_mePtr = &_createdBindings[bindingIndex];
        }
        return true;
    }

    if (binding->type == QV4::CompiledData::Binding::Type_Object) {
        QQmlPropertyPrivate::WriteFlags propertyWriteFlags = QQmlPropertyPrivate::BypassInterceptor |
                                                                   QQmlPropertyPrivate::RemoveBindingOnAliasWrite;
        int propertyWriteStatus = -1;
        void *argv[] = { 0, 0, &propertyWriteStatus, &propertyWriteFlags };

        if (const char *iid = QQmlMetaType::interfaceIId(property->propType)) {
            void *ptr = createdSubObject->qt_metacast(iid);
            if (ptr) {
                argv[0] = &ptr;
                QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
            } else {
                recordError(binding->location, tr("Cannot assign object to interface property"));
                return false;
            }
        } else if (property->propType == QMetaType::QVariant) {
            if (property->isVarProperty()) {
                QV4::ExecutionEngine *v4 = QV8Engine::getV4(engine);
                QV4::Scope scope(v4);
                QV4::ScopedValue wrappedObject(scope, QV4::QObjectWrapper::wrap(QV8Engine::getV4(engine), createdSubObject));
                _vmeMetaObject->setVMEProperty(property->coreIndex, wrappedObject);
            } else {
                QVariant value = QVariant::fromValue(createdSubObject);
                argv[0] = &value;
                QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
            }
        } else if (property->isQList()) {
            Q_ASSERT(_currentList.object);

            void *itemToAdd = createdSubObject;

            const char *iid = 0;
            int listItemType = QQmlEnginePrivate::get(engine)->listType(property->propType);
            if (listItemType != -1)
                iid = QQmlMetaType::interfaceIId(listItemType);
            if (iid)
                itemToAdd = createdSubObject->qt_metacast(iid);

            if (_currentList.append)
                _currentList.append(&_currentList, itemToAdd);
        } else {
            QQmlEnginePrivate *enginePrivate = QQmlEnginePrivate::get(engine);

            // We want to raw metaObject here as the raw metaobject is the
            // actual property type before we applied any extensions that might
            // effect the properties on the type, but don't effect assignability
            QQmlPropertyCache *propertyMetaObject = enginePrivate->rawPropertyCacheForType(property->propType);

            // Will be true if the assgned type inherits propertyMetaObject
            bool isAssignable = false;
            // Determine isAssignable value
            if (propertyMetaObject) {
                QQmlPropertyCache *c = enginePrivate->cache(createdSubObject);
                while (c && !isAssignable) {
                    isAssignable |= c == propertyMetaObject;
                    c = c->parent();
                }
            }

            if (isAssignable) {
                argv[0] = &createdSubObject;
                QMetaObject::metacall(_qobject, QMetaObject::WriteProperty, property->coreIndex, argv);
            } else {
                recordError(binding->location, tr("Cannot assign object to property"));
                return false;
            }
        }
        return true;
    }

    if (property->isQList()) {
        recordError(binding->location, tr("Cannot assign primitives to lists"));
        return false;
    }

    setPropertyValue(property, binding);
    return true;
}

void QmlObjectCreator::setupFunctions()
{
    QQmlVMEMetaObject *vme = QQmlVMEMetaObject::get(_qobject);

    const quint32 *functionIdx = _compiledObject->functionOffsetTable();
    for (quint32 i = 0; i < _compiledObject->nFunctions; ++i, ++functionIdx) {
        QV4::Function *runtimeFunction = jsUnit->runtimeFunctions[*functionIdx];
        const QString name = runtimeFunction->name->toQString();

        QQmlPropertyData *property = _propertyCache->property(name, _qobject, context);
        if (!property->isVMEFunction())
            continue;

        QV4::FunctionObject *function = QV4::FunctionObject::creatScriptFunction(_qmlContext, runtimeFunction);
        vme->setVmeMethod(property->coreIndex, QV4::Value::fromObject(function));
    }
}

QObject *QmlObjectCreator::createInstance(int index, QObject *parent)
{
    ActiveOCRestorer ocRestorer(this, QQmlEnginePrivate::get(engine));

    bool isComponent = false;
    QObject *instance = 0;

    if (compiledData->isComponent(index)) {
        isComponent = true;
        QQmlComponent *component = new QQmlComponent(engine, compiledData, index, parent);
        QQmlComponentPrivate::get(component)->creationContext = context;
        instance = component;
    } else {
        const QV4::CompiledData::Object *obj = qmlUnit->objectAt(index);

        QQmlCompiledData::TypeReference typeRef = resolvedTypes.value(obj->inheritedTypeNameIndex);
        QQmlType *type = typeRef.type;
        if (type) {
            instance = type->create();
        } else {
            Q_ASSERT(typeRef.component);
            QmlObjectCreator subCreator(context, typeRef.component);
            instance = subCreator.create();
        }
        // ### use no-event variant
        if (parent)
            instance->setParent(parent);
    }

    QQmlData *ddata = QQmlData::get(instance, /*create*/true);
    if (index == qmlUnit->indexOfRootObject) {
        if (ddata->context) {
            Q_ASSERT(ddata->context != context);
            Q_ASSERT(ddata->outerContext);
            Q_ASSERT(ddata->outerContext != context);
            QQmlContextData *c = ddata->context;
            while (c->linkedContext) c = c->linkedContext;
            c->linkedContext = context;
        } else
            context->addObject(instance);
        ddata->ownContext = true;
    } else if (!ddata->context)
        context->addObject(instance);

    ddata->outerContext = context;

    QHash<int, int>::ConstIterator idEntry = objectIndexToId.find(index);
    if (idEntry != objectIndexToId.constEnd())
        context->setIdProperty(idEntry.value(), instance);

    if (!isComponent) {
        QQmlRefPointer<QQmlPropertyCache> cache = propertyCaches.value(index);
        Q_ASSERT(!cache.isNull());

        if (!populateInstance(index, instance, cache))
            return 0;
    }

    return instance;
}

void QmlObjectCreator::finalize()
{
    {
    QQmlTrace trace("VME Binding Enable");
    trace.event("begin binding eval");

    Q_ASSERT(allCreatedBindings.isEmpty() || allCreatedBindings.isDetached());

    for (QLinkedList<QVector<QQmlAbstractBinding*> >::Iterator it = allCreatedBindings.begin(), end = allCreatedBindings.end();
         it != end; ++it) {
        const QVector<QQmlAbstractBinding *> &bindings = *it;
        for (int i = 0; i < bindings.count(); ++i) {
            QQmlAbstractBinding *b = bindings.at(i);
            if (!b)
                continue;
            b->m_mePtr = 0;
            QQmlData *data = QQmlData::get(b->object());
            Q_ASSERT(data);
            data->clearPendingBindingBit(b->propertyIndex());
            b->setEnabled(true, QQmlPropertyPrivate::BypassInterceptor |
                          QQmlPropertyPrivate::DontRemoveBinding);
        }
    }
    }

    {
    QQmlTrace trace("VME Component.onCompleted Callbacks");
    while (componentAttached) {
        QQmlComponentAttached *a = componentAttached;
        a->rem();
        QQmlData *d = QQmlData::get(a->parent());
        Q_ASSERT(d);
        Q_ASSERT(d->context);
        a->add(&d->context->componentAttached);
        // ### designer if (componentCompleteEnabled())
            emit a->completed();

#if 0 // ###
        if (watcher.hasRecursed() || interrupt.shouldInterrupt())
            return 0;
#endif
    }
    }
}

bool QmlObjectCreator::populateInstance(int index, QObject *instance, QQmlRefPointer<QQmlPropertyCache> cache)
{
    const QV4::CompiledData::Object *obj = qmlUnit->objectAt(index);

    QQmlData *declarativeData = QQmlData::get(instance, /*create*/true);

    qSwap(_propertyCache, cache);
    qSwap(_qobject, instance);
    qSwap(_compiledObject, obj);
    qSwap(_ddata, declarativeData);

    QQmlVMEMetaObject *vmeMetaObject = 0;
    const QByteArray data = vmeMetaObjectData.value(index);
    if (!data.isEmpty()) {
        // install on _object
        vmeMetaObject = new QQmlVMEMetaObject(_qobject, _propertyCache, reinterpret_cast<const QQmlVMEMetaData*>(data.constData()));
        if (_ddata->propertyCache)
            _ddata->propertyCache->release();
        _ddata->propertyCache = _propertyCache;
        _ddata->propertyCache->addref();
    } else {
        vmeMetaObject = QQmlVMEMetaObject::get(_qobject);
    }

    _ddata->lineNumber = _compiledObject->location.line;
    _ddata->columnNumber = _compiledObject->location.column;

    qSwap(_vmeMetaObject, vmeMetaObject);

    QVector<QQmlAbstractBinding*> createdBindings(_compiledObject->nBindings, 0);
    qSwap(_createdBindings, createdBindings);

    QV4::ExecutionEngine *v4 = QV8Engine::getV4(engine);
    QV4::Scope valueScope(v4);
    QV4::ScopedValue scopeObject(valueScope, QV4::QmlContextWrapper::qmlScope(QV8Engine::get(engine), context, _qobject));
    QV4::QmlBindingWrapper *qmlBindingWrapper = new (v4->memoryManager) QV4::QmlBindingWrapper(v4->rootContext, scopeObject->asObject());
    QV4::ScopedValue qmlScopeFunction(valueScope, QV4::Value::fromObject(qmlBindingWrapper));
    QV4::ExecutionContext *qmlContext = qmlBindingWrapper->context();

    qSwap(_qmlContext, qmlContext);

    setupBindings();
    setupFunctions();

    qSwap(_qmlContext, qmlContext);

    qSwap(_createdBindings, createdBindings);
    qSwap(_vmeMetaObject, vmeMetaObject);
    qSwap(_propertyCache, cache);
    qSwap(_ddata, declarativeData);
    qSwap(_compiledObject, obj);
    qSwap(_qobject, instance);

    allCreatedBindings.append(_createdBindings);

    return errors.isEmpty();
}


QQmlAnonymousComponentResolver::QQmlAnonymousComponentResolver(const QUrl &url, const QV4::CompiledData::QmlUnit *qmlUnit,
                                                               const QHash<int, QQmlCompiledData::TypeReference> &resolvedTypes,
                                                               const QList<QQmlPropertyCache *> &propertyCaches)
    : QQmlCompilePass(url, qmlUnit)
    , _componentIndex(-1)
    , resolvedTypes(resolvedTypes)
    , propertyCaches(propertyCaches)
{
}

bool QQmlAnonymousComponentResolver::resolve()
{
    Q_ASSERT(componentRoots.isEmpty());

    // Find objects that are Components. This is missing an extra pass
    // that finds implicitly defined components, i.e.
    //    someProperty: Item { ... }
    // when someProperty _is_ a QQmlComponent. In that case the Item {}
    // should be implicitly surrounded by Component {}

    for (int i = 0; i < qmlUnit->nObjects; ++i) {
        const QV4::CompiledData::Object *obj = qmlUnit->objectAt(i);
        if (stringAt(obj->inheritedTypeNameIndex).isEmpty())
            continue;

        QQmlRefPointer<QQmlPropertyCache> cache = propertyCaches.value(i);
        if (!cache || cache->metaObject() != &QQmlComponent::staticMetaObject)
            continue;

        componentRoots.append(i);
        // Sanity checks: There can be only an (optional) id property and
        // a default property, that defines the component tree.
    }

    std::sort(componentRoots.begin(), componentRoots.end());

    // For each component's tree, remember to which component the children
    // belong to
    for (int i = 0; i < componentRoots.count(); ++i) {
        const QV4::CompiledData::Object *component = qmlUnit->objectAt(componentRoots.at(i));

        if (component->nFunctions > 0)
            COMPILE_EXCEPTION(component, tr("Component objects cannot declare new functions."));
        if (component->nProperties > 0)
            COMPILE_EXCEPTION(component, tr("Component objects cannot declare new properties."));
        if (component->nSignals > 0)
            COMPILE_EXCEPTION(component, tr("Component objects cannot declare new signals."));

        if (component->nBindings == 0)
            COMPILE_EXCEPTION(component, tr("Cannot create empty component specification"));

        const QV4::CompiledData::Binding *rootBinding = component->bindingTable();
        if (component->nBindings > 1 || rootBinding->type != QV4::CompiledData::Binding::Type_Object)
            COMPILE_EXCEPTION(rootBinding, tr("Component elements may not contain properties other than id"));

        _componentIndex = i;
        _ids.clear();
        if (!recordComponentSubTree(rootBinding->value.objectIndex))
            break;
    }

    return errors.isEmpty();
}

bool QQmlAnonymousComponentResolver::recordComponentSubTree(int objectIndex)
{
    const QV4::CompiledData::Object *obj = qmlUnit->objectAt(objectIndex);

    // Only include creatable types. Everything else is synthetic, such as group property
    // objects.
    if (!stringAt(obj->inheritedTypeNameIndex).isEmpty())
        objectIndexToComponentIndex.insert(objectIndex, _componentIndex);

    QString id = stringAt(obj->idIndex);
    if (!id.isEmpty()) {
        if (_ids.contains(obj->idIndex)) {
            recordError(obj->locationOfIdProperty, tr("id is not unique"));
            return false;
        }
        _ids.insert(obj->idIndex);
    }

    const QV4::CompiledData::Binding *binding = obj->bindingTable();
    for (int i = 0; i < obj->nBindings; ++i, ++binding) {
        if (binding->type != QV4::CompiledData::Binding::Type_Object
            && binding->type != QV4::CompiledData::Binding::Type_AttachedProperty
            && binding->type != QV4::CompiledData::Binding::Type_GroupProperty)
            continue;

        // Stop at Component boundary
        if (std::binary_search(componentRoots.constBegin(), componentRoots.constEnd(), binding->value.objectIndex))
            continue;

        if (!recordComponentSubTree(binding->value.objectIndex))
            return false;
    }

    return true;
}
