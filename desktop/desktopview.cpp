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

#include "desktopview.h"
#include "thumbnailer/thumbnailprovider.h"

#include <QQmlEngine>
#include <QQmlContext>

#include <QDebug>
#include <QApplication>
#include <QScreen>

#include <KWindowSystem>

DesktopView::DesktopView(QQuickView *parent)
    : QQuickView(parent)
{
    m_screenRect = qApp->primaryScreen()->geometry();
    m_screenAvailableRect = qApp->primaryScreen()->availableVirtualGeometry();

    KWindowSystem::setType(winId(), NET::Desktop);
    KWindowSystem::setState(winId(), NET::KeepBelow);

    engine()->rootContext()->setContextProperty("desktopView", this);
    engine()->addImageProvider("thumbnailer", new ThumbnailProvider());

    setTitle(tr("Desktop"));
    setScreen(qApp->primaryScreen());
    setResizeMode(QQuickView::SizeRootObjectToView);
    setSource(QStringLiteral("qrc:/qml/Desktop/Main.qml"));

    onGeometryChanged();

    connect(qApp->primaryScreen(), &QScreen::virtualGeometryChanged, this, &DesktopView::onGeometryChanged);
    connect(qApp->primaryScreen(), &QScreen::geometryChanged, this, &DesktopView::onGeometryChanged);
    connect(qApp->primaryScreen(), &QScreen::availableGeometryChanged, this, &DesktopView::onAvailableGeometryChanged);
    connect(qApp->primaryScreen(), &QScreen::virtualGeometryChanged, this, &DesktopView::onAvailableGeometryChanged);
}

QRect DesktopView::screenRect()
{
    return m_screenRect;
}

QRect DesktopView::screenAvailableRect()
{
    return m_screenAvailableRect;
}

void DesktopView::onGeometryChanged()
{
    m_screenRect = qApp->primaryScreen()->geometry();
    setGeometry(m_screenRect);
    emit screenRectChanged();
}

void DesktopView::onAvailableGeometryChanged(const QRect &geometry)
{
    Q_UNUSED(geometry);

    m_screenAvailableRect = qApp->primaryScreen()->availableVirtualGeometry();
    emit screenAvailableGeometryChanged();
}
