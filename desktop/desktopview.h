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

#ifndef DESKTOPVIEW_H
#define DESKTOPVIEW_H

#include <QQuickView>
#include <QScreen>

class DesktopView : public QQuickView
{
    Q_OBJECT
    Q_PROPERTY(QRect screenRect READ screenRect NOTIFY screenRectChanged)
    Q_PROPERTY(QRect screenAvailableRect READ screenAvailableRect NOTIFY screenAvailableGeometryChanged)

public:
    explicit DesktopView(QScreen *screen = nullptr, QQuickView *parent = nullptr);

    QRect screenRect();
    QRect screenAvailableRect();

signals:
    void screenRectChanged();
    void screenAvailableGeometryChanged();

private slots:
    void onPrimaryScreenChanged(QScreen *screen);
    void onGeometryChanged();
    void onAvailableGeometryChanged(const QRect &geometry);

private:
    QScreen *m_screen;
    QRect m_screenRect;
    QRect m_screenAvailableRect;
};

#endif // DESKTOPVIEW_H
