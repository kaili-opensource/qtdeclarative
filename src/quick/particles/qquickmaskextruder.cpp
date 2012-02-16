/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the Declarative module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickmaskextruder_p.h"
#include <QtQml/qqml.h>
#include <QtQml/qqmlinfo.h>
#include <QImage>
#include <QDebug>
QT_BEGIN_NAMESPACE
/*!
    \qmlclass MaskShape QQuickMaskExtruder
    \inqmlmodule QtQuick.Particles 2
    \inherits Shape
    \brief The MaskShape element allows you to represent an image as a shape to affectors and emitters.

*/
/*!
    \qmlproperty url QtQuick.Particles2::MaskShape::source

    The image to use as the mask. Areas with non-zero opacity
    will be considered inside the shape.
*/


QQuickMaskExtruder::QQuickMaskExtruder(QObject *parent) :
    QQuickParticleExtruder(parent)
  , m_lastWidth(-1)
  , m_lastHeight(-1)
{
}

void QQuickMaskExtruder::setSource(QUrl arg)
{
    if (m_source != arg) {
        m_source = arg;

        m_lastHeight = -1;//Trigger reset
        m_lastWidth = -1;
        emit sourceChanged(arg);
        startMaskLoading();
    }
}

void QQuickMaskExtruder::startMaskLoading()
{
    m_pix.clear(this);
    if (m_source.isEmpty())
        return;
    m_pix.load(qmlEngine(this), m_source);
    if (m_pix.isLoading())
        m_pix.connectFinished(this, SLOT(finishMaskLoading()));
    else
        finishMaskLoading();
}

void QQuickMaskExtruder::finishMaskLoading()
{
    if (m_pix.isError())
        qmlInfo(this) << m_pix.error();
}

QPointF QQuickMaskExtruder::extrude(const QRectF &r)
{
    ensureInitialized(r);
    if (!m_mask.count() || m_img.isNull())
        return r.topLeft();
    const QPointF p = m_mask[rand() % m_mask.count()];
    //### Should random sub-pixel positioning be added?
    return p + r.topLeft();
}

bool QQuickMaskExtruder::contains(const QRectF &bounds, const QPointF &point)
{
    ensureInitialized(bounds);//###Current usage patterns WILL lead to different bounds/r calls. Separate list?
    if (m_img.isNull())
        return false;
    QPoint p = point.toPoint() - bounds.topLeft().toPoint();
    return m_img.rect().contains(p) && (bool)m_img.pixelIndex(p);
}

void QQuickMaskExtruder::ensureInitialized(const QRectF &r)
{
    if (m_lastWidth == r.width() && m_lastHeight == r.height())
        return;//Same as before
    if (!m_pix.isReady())
        return;
    m_lastWidth = r.width();
    m_lastHeight = r.height();

    m_mask.clear();

    m_img = m_pix.image().createAlphaMask();
    m_pix.clear();
    m_img = m_img.convertToFormat(QImage::Format_Mono);//Else LSB, but I think that's easier
    m_img = m_img.scaled(r.size().toSize());//TODO: Do they need aspect ratio stuff? Or tiling?
    for (int i=0; i<r.width(); i++){
        for (int j=0; j<r.height(); j++){
            if (m_img.pixelIndex(i,j))//Direct bit manipulation is presumably more efficient
                m_mask << QPointF(i,j);
        }
    }
}
QT_END_NAMESPACE
