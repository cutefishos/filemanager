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
#include <QPointer>
#include <QQuickWindow>
#include <QPixmapCache>
#include <QTimer>

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

    QQuickWindow *w = quickWindow();

    if (w)
        w->installEventFilter(this);
    else
        deleteLater();
}

void Window::show()
{
    if (QQuickWindow *w = quickWindow()) {
        QPointer<QQuickWindow> window(w);

        // Let QML finish its initial layout pass before the native window is
        // exposed. Showing it in the same call stack can expose the initial
        // size for one frame and then resize it, which appears as a flash.
        QTimer::singleShot(0, w, [window] {
            if (!window)
                return;

            window->show();
            window->raise();
            window->requestActivate();
        });
    }
}

QQuickWindow *Window::quickWindow() const
{
    const QList<QObject *> objects = rootObjects();
    for (QObject *object : objects) {
        if (QQuickWindow *w = qobject_cast<QQuickWindow *>(object))
            return w;
    }

    return nullptr;
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
