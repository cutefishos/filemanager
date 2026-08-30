/*
 * Copyright (C) 2021 CutefishOS Team.
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
#include "qmltypes.h"

#include <QQmlEngine>
#include <QQmlExtensionPlugin>

/**
 * "org.cutefish.filemanager.desktop" -- the file manager's desktop, packaged so
 * that any QML host (currently cutefish-shell) can place it in a window without
 * linking against KIO or knowing anything about files.
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
