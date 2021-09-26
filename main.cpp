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

#include "application.h"
#include "model/placesmodel.h"
#include "model/foldermodel.h"
#include "model/pathbarmodel.h"
#include "model/positioner.h"
#include "widgets/rubberband.h"
#include "widgets/itemviewadapter.h"
#include "desktop/desktop.h"
#include "desktop/desktopsettings.h"
#include "desktop/desktopview.h"
#include "helper/datehelper.h"
#include "helper/fm.h"
#include "helper/shortcut.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);

    // Register QML Type.
    const char *uri = "Cutefish.FileManager";
    qmlRegisterType<PlacesModel>(uri, 1, 0, "PlacesModel");
    qmlRegisterType<FolderModel>(uri, 1, 0, "FolderModel");
    qmlRegisterType<PathBarModel>(uri, 1, 0, "PathBarModel");
    qmlRegisterType<Positioner>(uri, 1, 0, "Positioner");
    qmlRegisterType<RubberBand>(uri, 1, 0, "RubberBand");
    qmlRegisterType<ItemViewAdapter>(uri, 1, 0, "ItemViewAdapter");
    qmlRegisterType<DesktopSettings>(uri, 1, 0, "DesktopSettings");
    qmlRegisterType<Fm>(uri, 1, 0, "Fm");
    qmlRegisterType<ShortCut>(uri, 1, 0, "ShortCut");

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    qmlRegisterType<QAction>();
#else
    qmlRegisterAnonymousType<QAction>(uri, 1);
#endif

    Application app(argc, argv);
    return app.run();
}
