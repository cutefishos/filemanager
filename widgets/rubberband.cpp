#include "rubberband.h"

#include <QQuickWindow>
#include <QSGRectangleNode>

RubberBand::RubberBand(QQuickItem *parent)
    : QQuickItem(parent)
{
    // Scene graph nodes only move vertices; a painted item re-rasterises and
    // uploads the whole band on every pointer move, which lags behind the cursor.
    setFlag(ItemHasContents);
}

RubberBand::~RubberBand()
{
}

QSGNode *RubberBand::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data)
{
    Q_UNUSED(data);

    const QRectF rect = boundingRect();

    if (rect.isEmpty()) {
        delete oldNode;
        return nullptr;
    }

    QSGNode *root = oldNode;

    if (!root) {
        root = new QSGNode;
        for (int i = 0; i < 5; ++i) {
            QSGRectangleNode *node = window()->createRectangleNode();
            root->appendChildNode(node);
        }
    }

    QColor fillColor = m_color;
    fillColor.setAlphaF(0.12);

    QColor borderColor = m_color;
    borderColor.setAlphaF(0.7);

    // A whole number of device pixels, or the edge alternates between one and
    // two pixels as the band moves at a fractional scale.
    const qreal dpr = window()->effectiveDevicePixelRatio();
    const qreal hairline = qMax<qreal>(1.0, qRound(dpr)) / dpr;
    const qreal border = qMin(hairline, qMin(rect.width(), rect.height()) / 2);
    const QRectF rects[5] = {
            rect.adjusted(border, border, -border, -border),
            QRectF(rect.left(), rect.top(), rect.width(), border),
            QRectF(rect.left(), rect.bottom() - border, rect.width(), border),
            QRectF(rect.left(), rect.top() + border, border, rect.height() - 2 * border),
            QRectF(rect.right() - border, rect.top() + border, border, rect.height() - 2 * border),
    };

    QSGNode *child = root->firstChild();
    for (int i = 0; i < 5; ++i, child = child->nextSibling()) {
        auto *node = static_cast<QSGRectangleNode *>(child);
        node->setRect(rects[i]);
        node->setColor(i == 0 ? fillColor : borderColor);
    }

    return root;
}

QColor RubberBand::color() const
{
    return m_color;
}

void RubberBand::setColor(QColor color)
{
    if (m_color != color) {
        m_color = color;
        update();
        emit colorChanged();
    }
}

void RubberBand::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    update();

    QQuickItem::geometryChange(newGeometry, oldGeometry);
}
