// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktopview.h"
#include "helper/folderpreferences.h"
#include "qmltypes.h"

#include <QQmlEngine>
#include <QQmlExtensionPlugin>

/**
 * "org.cutefish.filemanager.desktop" -- the file manager's desktop, packaged so
 * that any QML host (currently cutefish-shell) can place it in a window without
 * linking against file operation libraries or knowing anything about files.
 */
class CutefishDesktopPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    void registerTypes(const char *uri) override
    {
        Q_ASSERT(QLatin1String(uri) == QLatin1String("org.cutefish.filemanager.desktop"));

        CutefishFM::initResources();
        CutefishFM::registerQmlTypes();
        FolderPreferences::useDesktop();

        qmlRegisterType<DesktopView>(uri, 1, 0, "DesktopView");
    }

    void initializeEngine(QQmlEngine *engine, const char *uri) override
    {
        Q_UNUSED(uri)

        // Qt 6 only runs this for the first engine that imports the module, so
        // DesktopView registers the providers itself as well. Doing it here too
        // keeps them available before any desktop item is instantiated.
        CutefishFM::registerImageProviders(engine);
    }
};

#include "desktopplugin.moc"
