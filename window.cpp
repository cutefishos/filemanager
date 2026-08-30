/*
 * Copyright (C) 2021 CutefishOS Team.
 *
 * Author:     Reion Wong <reion@cutefishos.com>
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

#include "window.h"
#include "qmltypes.h"
#include <QEvent>
#include <QDebug>
#include <QQuickWindow>
#include <QPixmapCache>

Window::Window(QObject *parent)
    : QQmlApplicationEngine(parent)
{
    // Qt6 only calls QQmlExtensionPlugin::initializeEngine() for the first engine
    // that imports a module, so FishUI's "icontheme" provider is missing in every
    // engine created afterwards. Register our own copy for each window.
    CutefishFM::registerImageProviders(this);
}

void Window::load(const QUrl &url)
{
    QQmlApplicationEngine::load(url);

    QQuickWindow *w = qobject_cast<QQuickWindow *>(rootObjects().first());

    if (w)
        w->installEventFilter(this);
}

bool Window::eventFilter(QObject *obj, QEvent *e)
{
    if (e->type() == QEvent::Close) {
        QPixmapCache::clear();
        clearComponentCache();
        deleteLater();
        e->accept();
    }

    return QObject::eventFilter(obj, e);
}
