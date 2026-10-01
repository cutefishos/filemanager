// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qmltypes.h"

#include "model/placesmodel.h"
#include "model/foldermodel.h"
#include "model/pathbarmodel.h"
#include "model/pathcompletionmodel.h"
#include "model/positioner.h"
#include "widgets/rubberband.h"
#include "widgets/itemviewadapter.h"
#include "helper/datehelper.h"
#include "helper/fm.h"
#include "helper/shortcut.h"
#include "helper/previewcontroller.h"
#include "helper/previewclient.h"
#include "helper/filelauncher.h"
#include "mimetype/mimeappmanager.h"
#include "desktopiconprovider.h"
#include "thumbnailprovider.h"

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
    qmlRegisterAnonymousType<FolderContentsModel>(uri, 1);
    qmlRegisterType<FolderModel>(uri, 1, 0, "FolderModel");
    qmlRegisterType<PathBarModel>(uri, 1, 0, "PathBarModel");
    qmlRegisterType<PathCompletionModel>(uri, 1, 0, "PathCompletionModel");
    qmlRegisterType<Positioner>(uri, 1, 0, "Positioner");
    qmlRegisterType<RubberBand>(uri, 1, 0, "RubberBand");
    qmlRegisterType<ItemViewAdapter>(uri, 1, 0, "ItemViewAdapter");
    qmlRegisterType<Fm>(uri, 1, 0, "Fm");
    qmlRegisterType<PreviewClient>(uri, 1, 0, "PreviewClient");
    qmlRegisterType<PreviewController>(uri, 1, 0, "PreviewController");
    qmlRegisterType<ShortCut>(uri, 1, 0, "ShortCut");

    // Process-wide objects shared by every engine; the engines must not own them.
    qmlRegisterSingletonType<MimeAppManager>(uri, 1, 0, "MimeAppManager",
                                             [](QQmlEngine *, QJSEngine *) -> QObject * {
        QQmlEngine::setObjectOwnership(MimeAppManager::self(), QQmlEngine::CppOwnership);
        return MimeAppManager::self();
    });
    qmlRegisterSingletonType<FileLauncher>(uri, 1, 0, "FileLauncher",
                                           [](QQmlEngine *, QJSEngine *) -> QObject * {
        QQmlEngine::setObjectOwnership(FileLauncher::self(), QQmlEngine::CppOwnership);
        return FileLauncher::self();
    });

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

    if (!engine->imageProvider(QStringLiteral("fileicon")))
        engine->addImageProvider(QStringLiteral("fileicon"), new DesktopIconProvider);

    if (!engine->imageProvider(QStringLiteral("icontheme")))
        engine->addImageProvider(QStringLiteral("icontheme"), new DesktopIconProvider);
}

}
