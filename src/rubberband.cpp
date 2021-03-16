#include "rubberband.h"

#include <QApplication>
#include <QPainter>
#include <QStyleOptionRubberBand>

RubberBand::RubberBand(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
}

RubberBand::~RubberBand()
{
}

void RubberBand::paint(QPainter *painter)
{
    if (!qApp) {
        return;
    }

    QPalette palette;
    palette.setColor(QPalette::Highlight, m_color);

    QStyleOptionRubberBand opt;
    opt.state = QStyle::State_None;
    opt.direction = qApp->layoutDirection();
    opt.styleObject = this;
    opt.palette = palette;
    opt.shape = QRubberBand::Rectangle;
    opt.opaque = false;
    opt.rect = contentsBoundingRect().toRect();
    qApp->style()->drawControl(QStyle::CE_RubberBand, &opt, painter);
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

void RubberBand::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_UNUSED(oldGeometry);

    m_geometry = newGeometry;

    update();

    QQuickItem::geometryChanged(newGeometry, oldGeometry);
}
