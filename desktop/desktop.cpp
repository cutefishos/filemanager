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

#include "desktop.h"
#include <QQmlContext>
#include <QQmlEngine>

#include <QGuiApplication>
#include <QDBusServiceWatcher>

Desktop::Desktop(QObject *parent)
    : QObject(parent)
    , m_dockInterface("org.cutefish.Dock",
                    "/Dock",
                    "org.cutefish.Dock", QDBusConnection::sessionBus())
    , m_leftMargin(0)
    , m_rightMargin(0)
    , m_bottomMargin(0)
{
    if (m_dockInterface.isValid()) {
        updateMargins();
        connect(&m_dockInterface, SIGNAL(primaryGeometryChanged()), this, SLOT(updateMargins()));
        connect(&m_dockInterface, SIGNAL(directionChanged()), this, SLOT(updateMargins()));
    } else {
        QDBusServiceWatcher *watcher = new QDBusServiceWatcher("org.cutefish.Dock",
                                                               QDBusConnection::sessionBus(),
                                                               QDBusServiceWatcher::WatchForUnregistration,
                                                               this);
        connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this, [=] {
            updateMargins();
            connect(&m_dockInterface, SIGNAL(primaryGeometryChanged()), this, SLOT(updateMargins()));
            connect(&m_dockInterface, SIGNAL(directionChanged()), this, SLOT(updateMargins()));
        });
    }

    for (QScreen *screen : QGuiApplication::screens()) {
        screenAdded(screen);
    }

    connect(qApp, &QGuiApplication::screenAdded, this, &Desktop::screenAdded);
    connect(qApp, &QGuiApplication::screenRemoved, this, &Desktop::screenRemoved);
}

int Desktop::leftMargin() const
{
    return m_leftMargin;
}

int Desktop::rightMargin() const
{
    return m_rightMargin;
}

int Desktop::bottomMargin() const
{
    return m_bottomMargin;
}

void Desktop::screenAdded(QScreen *screen)
{
    if (!m_list.contains(screen)) {
        DesktopView *view = new DesktopView(screen);
        view->engine()->rootContext()->setContextProperty("Desktop", this);
        view->show();
        m_list.insert(screen, view);
    }
}

void Desktop::screenRemoved(QScreen *screen)
{
    if (m_list.contains(screen)) {
        DesktopView *view = m_list.find(screen).value();
        view->setVisible(false);
        view->deleteLater();
        m_list.remove(screen);
    }
}

void Desktop::updateMargins()
{
    QRect dockGeometry = m_dockInterface.property("primaryGeometry").toRect();
    int dockDirection = m_dockInterface.property("direction").toInt();

    m_leftMargin = 0;
    m_rightMargin = 0;
    m_bottomMargin = 0;

    if (dockDirection == 0) {
        m_leftMargin = dockGeometry.width();
    } else if (dockDirection == 1) {
        m_bottomMargin = dockGeometry.height();
    } else if (dockDirection == 2) {
        m_rightMargin = dockGeometry.width();
    }

    emit marginsChanged();
}
