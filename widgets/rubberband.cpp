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

    const qreal border = qMin<qreal>(1.0, qMin(rect.width(), rect.height()) / 2);
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

bool RubberBand::intersects(const QRectF &rect) const
{
    return m_geometry.intersects(rect);
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
    Q_UNUSED(oldGeometry);

    m_geometry = newGeometry;

    update();

    QQuickItem::geometryChange(newGeometry, oldGeometry);
}
