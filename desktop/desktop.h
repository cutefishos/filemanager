/*
 * Copyright (C) 2021 CutefishOS Team.
 *
 * Author:     Reion Wong <reionwong@gmail.com>
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

#ifndef DESKTOP_H
#define DESKTOP_H

#include <QObject>
#include <QScreen>
#include <QDBusInterface>

#include "desktopview.h"

class Desktop : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int leftMargin READ leftMargin NOTIFY marginsChanged)
    Q_PROPERTY(int rightMargin READ rightMargin NOTIFY marginsChanged)
    Q_PROPERTY(int bottomMargin READ bottomMargin NOTIFY marginsChanged)

public:
    explicit Desktop(QObject *parent = nullptr);

    int leftMargin() const;
    int rightMargin() const;
    int bottomMargin() const;

signals:
    void marginsChanged();

private slots:
    void screenAdded(QScreen *qscreen);
    void screenRemoved(QScreen *qscreen);
    void updateMargins();

private:
    QMap<QScreen *, DesktopView *> m_list;
    QDBusInterface m_dockInterface;

    int m_leftMargin;
    int m_rightMargin;
    int m_bottomMargin;
};

#endif // DESKTOP_H
