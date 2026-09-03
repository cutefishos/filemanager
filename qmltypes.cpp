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

#include "qmltypes.h"

#include "model/placesmodel.h"
#include "model/foldermodel.h"
#include "model/pathbarmodel.h"
#include "model/positioner.h"
#include "widgets/rubberband.h"
#include "widgets/itemviewadapter.h"
#include "helper/datehelper.h"
#include "helper/fm.h"
#include "helper/shortcut.h"
#include "desktopiconprovider.h"
#include "thumbnailer/thumbnailprovider.h"

#include "draganddrop/declarativedragdropevent.h"
#include "draganddrop/declarativedroparea.h"
#include "draganddrop/declarativemimedata.h"

#include <QQmlEngine>
#include <QMimeData>
#include <QAction>

// Q_INIT_RESOURCE declares an extern symbol, so it must be expanded at global
// scope rather than inside the CutefishFM namespace.
static void initCoreResources()
{
    Q_INIT_RESOURCE(qml);
}

namespace CutefishFM {

void registerQmlTypes()
{
    static bool registered = false;
    if (registered)
        return;
    registered = true;

    const char *uri = "Cutefish.FileManager";
    const char *dragandrop_uri = "Cutefish.DragDrop";

    qmlRegisterType<PlacesModel>(uri, 1, 0, "PlacesModel");
    qmlRegisterType<FolderModel>(uri, 1, 0, "FolderModel");
    qmlRegisterType<PathBarModel>(uri, 1, 0, "PathBarModel");
    qmlRegisterType<Positioner>(uri, 1, 0, "Positioner");
    qmlRegisterType<RubberBand>(uri, 1, 0, "RubberBand");
    qmlRegisterType<ItemViewAdapter>(uri, 1, 0, "ItemViewAdapter");
    qmlRegisterType<Fm>(uri, 1, 0, "Fm");
    qmlRegisterType<ShortCut>(uri, 1, 0, "ShortCut");

    qmlRegisterAnonymousType<QAction>(uri, 1);
    qmlRegisterAnonymousType<QMimeData>(dragandrop_uri, 1);

    qmlRegisterType<DeclarativeDropArea>(dragandrop_uri, 1, 0, "DropArea");
    qmlRegisterUncreatableType<DeclarativeMimeData>(dragandrop_uri, 1, 0, "MimeData",
        QStringLiteral("MimeData cannot be created from QML."));
    qmlRegisterUncreatableType<DeclarativeDragDropEvent>(dragandrop_uri, 2, 0, "DragDropEvent",
        QStringLiteral("DragDropEvent cannot be created from QML."));
}

void initResources()
{
    // The core is a static library, so the resource initialiser is only pulled
    // in if something references it explicitly.
    initCoreResources();
}

void registerImageProviders(QQmlEngine *engine)
{
    if (!engine)
        return;

    if (!engine->imageProvider(QStringLiteral("thumbnailer")))
        engine->addImageProvider(QStringLiteral("thumbnailer"), new ThumbnailProvider);

    if (!engine->imageProvider(QStringLiteral("icontheme")))
        engine->addImageProvider(QStringLiteral("icontheme"), new DesktopIconProvider);
}

}
