/*
 * Copyright (C) 2021 CutefishOS Team.
 *
 * Author:     rekols <revenmartin@gmail.com>
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
#include <QQmlApplicationEngine>
#include <QTranslator>
#include <QLocale>
#include <QAction>
#include <QCommandLineParser>

#include "fmlist.h"
#include "fm.h"
#include "basemodel.h"
#include "baselist.h"
#include "handy.h"
#include "placeslist.h"
#include "pathlist.h"

#include "desktop/desktopsettings.h"
#include "desktop/desktopview.h"
#include "rubberband.h"

#include "lib/foldermodel.h"
#include "lib/placesmodel.h"
#include "lib/itemviewadapter.h"
#include "lib/positioner.h"

int main(int argc, char *argv[])
{
    const char *uri = "Cutefish.FileManager";

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

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("File Manager"));
    parser.addHelpOption();

    QCommandLineOption desktopOption(QStringList() << "d" << "desktop" << "Desktop Mode");
    parser.addOption(desktopOption);
    parser.process(app);

    qmlRegisterAnonymousType<QAction>(uri, 1);
    qmlRegisterType<DesktopSettings>(uri, 1, 0, "DesktopSettings");
    qmlRegisterType<RubberBand>(uri, 1, 0, "RubberBand");

    qmlRegisterType<FolderModel>(uri, 1, 0, "FolderModel");
    qmlRegisterType<ItemViewAdapter>(uri, 1, 0, "ItemViewAdapter");
    qmlRegisterType<Positioner>(uri, 1, 0, "Positioner");

    qmlRegisterType<PlacesModel>(uri, 1, 0, "PlacesModel");

    if (parser.isSet(desktopOption)) {
        DesktopView view;
        view.show();

        return app.exec();
    }

    qmlRegisterAnonymousType<BaseList>(uri, 1); // ABSTRACT BASE LIST
    qmlRegisterType<BaseModel>(uri, 1, 0, "BaseModel"); // BASE MODEL
    qmlRegisterType<PlacesList>(uri, 1, 0, "PlacesList");
    qmlRegisterType<PathList>(uri, 1, 0, "PathList");

    qmlRegisterType<FMList>(uri, 1, 0, "FMList");
    qmlRegisterSingletonType<FMStatic>(uri, 1, 0, "FM", [](QQmlEngine *engine, QJSEngine *scriptEngine) -> QObject * {
        Q_UNUSED(engine)
        Q_UNUSED(scriptEngine)
        return new FMStatic;
    });

    qmlRegisterSingletonType<Handy>(uri, 1, 0, "Handy", [](QQmlEngine *engine, QJSEngine *scriptEngine) -> QObject * {
        Q_UNUSED(engine)
        Q_UNUSED(scriptEngine)
        return new Handy;
    });

    QQmlApplicationEngine engine;
    const QUrl url(QStringLiteral("qrc:/qml/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
