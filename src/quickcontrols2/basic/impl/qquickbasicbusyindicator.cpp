/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls 2 module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickbasicbusyindicator_p.h"

#include <QtQuick/private/qquickitem_p.h>
#include <QtQuick/private/qsgadaptationlayer_p.h>
#include <QtQuickControls2Impl/private/qquickanimatednode_p.h>

QT_BEGIN_NAMESPACE

static const int CircleCount = 10;
static const int TotalDuration = 100 * CircleCount * 2;
static const QRgb TransparentColor = 0x00000000;

static QPointF moveCircle(const QPointF &pos, qreal rotation, qreal distance)
{
    return pos - QTransform().rotate(rotation).map(QPointF(0, distance));
}

class QQuickBasicBusyIndicatorNode : public QQuickAnimatedNode
{
public:
    QQuickBasicBusyIndicatorNode(QQuickBasicBusyIndicator *item);

    void updateCurrentTime(int time) override;
    void sync(QQuickItem *item) override;

private:
    QColor m_pen;
    QColor m_fill;
};

QQuickBasicBusyIndicatorNode::QQuickBasicBusyIndicatorNode(QQuickBasicBusyIndicator *item)
    : QQuickAnimatedNode(item)
{
    setLoopCount(Infinite);
    setDuration(TotalDuration);
    setCurrentTime(item->elapsed());

    for (int i = 0; i < CircleCount; ++i) {
        QSGTransformNode *transformNode = new QSGTransformNode;
        appendChildNode(transformNode);

        QQuickItemPrivate *d = QQuickItemPrivate::get(item);
        QSGInternalRectangleNode *rectNode = d->sceneGraphContext()->createInternalRectangleNode();
        rectNode->setAntialiasing(true);
        transformNode->appendChildNode(rectNode);
    }
}

void QQuickBasicBusyIndicatorNode::updateCurrentTime(int time)
{
    const qreal percentageComplete = time / qreal(TotalDuration);
    const qreal firstPhaseProgress = percentageComplete <= 0.5 ? percentageComplete * 2 : 0;
    const qreal secondPhaseProgress = percentageComplete > 0.5 ? (percentageComplete - 0.5) * 2 : 0;

    QSGTransformNode *transformNode = static_cast<QSGTransformNode*>(firstChild());
    Q_ASSERT(transformNode->type() == QSGNode::TransformNodeType);
    for (int i = 0; i < CircleCount; ++i) {
        QSGInternalRectangleNode *rectNode = static_cast<QSGInternalRectangleNode*>(transformNode->firstChild());
        Q_ASSERT(rectNode->type() == QSGNode::GeometryNodeType);

        const bool fill = (firstPhaseProgress > qreal(i) / CircleCount) || (secondPhaseProgress > 0 && secondPhaseProgress < qreal(i) / CircleCount);
        rectNode->setColor(fill ? m_fill : QColor::fromRgba(TransparentColor));
        rectNode->setPenColor(m_pen);
        rectNode->setPenWidth(1);
        rectNode->update();

        transformNode = static_cast<QSGTransformNode*>(transformNode->nextSibling());
    }
}

void QQuickBasicBusyIndicatorNode::sync(QQuickItem *item)
{
    const qreal w = item->width();
    const qreal h = item->height();
    const qreal sz = qMin(w, h);
    const qreal dx = (w - sz) / 2;
    const qreal dy = (h - sz) / 2;
    const int circleRadius = sz / 12;

    m_pen = static_cast<QQuickBasicBusyIndicator *>(item)->pen();
    m_fill = static_cast<QQuickBasicBusyIndicator *>(item)->fill();

    QSGTransformNode *transformNode = static_cast<QSGTransformNode *>(firstChild());
    for (int i = 0; i < CircleCount; ++i) {
        Q_ASSERT(transformNode->type() == QSGNode::TransformNodeType);

        QSGInternalRectangleNode *rectNode = static_cast<QSGInternalRectangleNode *>(transformNode->firstChild());
        Q_ASSERT(rectNode->type() == QSGNode::GeometryNodeType);

        QPointF pos = QPointF(sz / 2 - circleRadius, sz / 2 - circleRadius);
        pos = moveCircle(pos, 360.0 / CircleCount * i, sz / 2 - circleRadius);

        QMatrix4x4 m;
        m.translate(dx + pos.x(), dy + pos.y());
        transformNode->setMatrix(m);

        rectNode->setRect(QRectF(QPointF(), QSizeF(circleRadius * 2, circleRadius * 2)));
        rectNode->setRadius(circleRadius);

        transformNode = static_cast<QSGTransformNode *>(transformNode->nextSibling());
    }
}

QQuickBasicBusyIndicator::QQuickBasicBusyIndicator(QQuickItem *parent) :
    QQuickItem(parent)
{
    setFlag(ItemHasContents);
}

QColor QQuickBasicBusyIndicator::pen() const
{
    return m_pen;
}

void QQuickBasicBusyIndicator::setPen(const QColor &pen)
{
    if (pen == m_pen)
        return;

    m_pen = pen;
    update();
}

QColor QQuickBasicBusyIndicator::fill() const
{
    return m_fill;
}

void QQuickBasicBusyIndicator::setFill(const QColor &fill)
{
    if (fill == m_fill)
        return;

    m_fill = fill;
    update();
}

bool QQuickBasicBusyIndicator::isRunning() const
{
    return isVisible();
}

void QQuickBasicBusyIndicator::setRunning(bool running)
{
    if (running)
        setVisible(true);
}

int QQuickBasicBusyIndicator::elapsed() const
{
    return m_elapsed;
}

void QQuickBasicBusyIndicator::itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &data)
{
    QQuickItem::itemChange(change, data);
    switch (change) {
    case ItemOpacityHasChanged:
        if (qFuzzyIsNull(data.realValue))
            setVisible(false);
        break;
    case ItemVisibleHasChanged:
        update();
        break;
    default:
        break;
    }
}

QSGNode *QQuickBasicBusyIndicator::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *)
{
    QQuickBasicBusyIndicatorNode *node = static_cast<QQuickBasicBusyIndicatorNode *>(oldNode);
    if (isRunning() && width() > 0 && height() > 0) {
        if (!node) {
            node = new QQuickBasicBusyIndicatorNode(this);
            node->start();
        }
        node->sync(this);
    } else {
        m_elapsed = node ? node->currentTime() : 0;
        delete node;
        node = nullptr;
    }
    return node;
}

QT_END_NAMESPACE

#include "moc_qquickbasicbusyindicator_p.cpp"
