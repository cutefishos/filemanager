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

#include <QApplication>
#include <QCommandLineParser>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <QTranslator>
#include <QLocale>

#include "model/placesmodel.h"
#include "model/foldermodel.h"
#include "model/pathbarmodel.h"
#include "model/positioner.h"
#include "widgets/rubberband.h"
#include "widgets/itemviewadapter.h"
#include "desktop/desktopsettings.h"
#include "desktop/desktopview.h"
#include "helper/thumbnailer.h"
#include "helper/datehelper.h"
#include "helper/fm.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QApplication app(argc, argv);
    app.setOrganizationName("cutefishos");

    // Translations
    QLocale locale;
    QString qmFilePath = QString("%1/%2.qm").arg("/usr/share/cutefish-filemanager/translations/").arg(locale.name());
    if (QFile::exists(qmFilePath)) {
        QTranslator *translator = new QTranslator(app.instance());
        if (translator->load(qmFilePath)) {
            app.installTranslator(translator);
        } else {
            translator->deleteLater();
        }
    }

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
    qmlRegisterAnonymousType<QAction>(uri, 1);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("File Manager"));
    parser.addHelpOption();

    parser.addPositionalArgument("files", "Files", "[FILE1, FILE2,...]");

    QCommandLineOption desktopOption(QStringList() << "d" << "desktop" << "Desktop Mode");
    parser.addOption(desktopOption);

    QCommandLineOption emptyTrashOption(QStringList() << "e" << "empty-trash" << "Empty Trash");
    parser.addOption(emptyTrashOption);

    parser.process(app);

    if (parser.isSet(desktopOption)) {
        app.setApplicationName("cutefish-desktop");
        DesktopView view;
        view.show();
        return app.exec();
    } else if (parser.isSet(emptyTrashOption)) {
        // Empty Dialog
        QQmlApplicationEngine engine;
        const QUrl url(QStringLiteral("qrc:/qml/Dialogs/EmptyTrashDialog.qml"));
        engine.load(url);
        return app.exec();
    }

    QQmlApplicationEngine engine;
    const QUrl url(QStringLiteral("qrc:/qml/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    // Handle urls
    if (!parser.positionalArguments().isEmpty()) {
        QStringList arguments = parser.positionalArguments();
        QUrl url(arguments.first());
        if (!url.isValid())
            url = QUrl::fromLocalFile(arguments.first());

        if (url.isValid())
            engine.rootContext()->setContextProperty("arg", arguments.first());
        else
            engine.rootContext()->setContextProperty("arg", "");
    } else {
        engine.rootContext()->setContextProperty("arg", "");
    }

    engine.load(url);
    engine.addImageProvider("thumbnailer", new Thumbnailer());

    return app.exec();
}
