/*
 * Copyright (C) 2021 CutefishOS Team.
 *
 * Author:     revenmartin <revenmartin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "rubberband.h"

#include <QPainter>
#include <QPen>

RubberBand::RubberBand(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
}

RubberBand::~RubberBand()
{
}

void RubberBand::paint(QPainter *painter)
{
    const QRectF rect = contentsBoundingRect();
    QColor fillColor = m_color;
    fillColor.setAlphaF(0.12);

    QColor borderColor = m_color;
    borderColor.setAlphaF(0.7);

    painter->fillRect(rect, fillColor);
    painter->setPen(QPen(borderColor, 1.0));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(rect.adjusted(0.5, 0.5, -0.5, -0.5));
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
