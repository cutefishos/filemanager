// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chooserwindow.h"
#include "filechooser.h"
#include "waylandparent.h"
#include "windowmanager.h"

#include <QQuickWindow>
#include <QTimer>

namespace {
const QUrl chooserWindow(QStringLiteral("qrc:/chooser/FileChooser.qml"));
}

bool preloadChooserWindow(WindowManager *windows)
{
    return windows->preload(chooserWindow);
}

QQuickWindow *openChooserWindow(WindowManager *windows, FileChooser *chooser)
{
    QQuickWindow *window =
            windows->create(chooserWindow, {{QStringLiteral("chooser"), QVariant::fromValue(chooser)}});
    if (!window)
        return nullptr;

    const QString parentWindow = chooser->request().value("parent_window").toString();
    const bool modal = chooser->request().value("modal").toBool(true);
    // Wayland parenting and modality go through xdg-foreign and xdg_dialog_v1,
    // which Qt does not drive itself; see applyWaylandParent().
    QTimer::singleShot(0, window, [window, parentWindow, modal] {
        window->show();
        applyWaylandParent(window, parentWindow, modal);
        window->requestActivate();
    });
    return window;
}
