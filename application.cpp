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

#include "application.h"
#include "dbusinterface.h"
#include "window.h"
#include "desktop/desktop.h"
#include "thumbnailer/thumbnailprovider.h"
#include "filemanageradaptor.h"

#include <QCommandLineParser>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <QDBusConnection>
#include <QPixmapCache>
#include <QTranslator>
#include <QFileInfo>
#include <QIcon>
#include <QDir>

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
    , m_instance(false)
{
    if (QDBusConnection::sessionBus().registerService("com.cutefish.FileManager")) {
        setOrganizationName("cutefishos");
        setWindowIcon(QIcon::fromTheme("file-manager"));

        new FileManagerAdaptor(this);
        new DBusInterface;
        QDBusConnection::sessionBus().registerObject("/FileManager", this);

        // Translations
        QLocale locale;
        QString qmFilePath = QString("%1/%2.qm").arg("/usr/share/cutefish-filemanager/translations/").arg(locale.name());
        if (QFile::exists(qmFilePath)) {
            QTranslator *translator = new QTranslator(this);
            if (translator->load(qmFilePath)) {
                installTranslator(translator);
            } else {
                translator->deleteLater();
            }
        }

        m_instance = true;
    }
}

int Application::run()
{
    if (!parseCommandLineArgs())
        return 0;

    return QApplication::exec();
}

void Application::openFiles(const QStringList &paths)
{
    for (const QString &path : paths) {
        openWindow(path);
    }
}

void Application::emptyTrash()
{
    Window *w = new Window;
    w->load(QUrl("qrc:/qml/Dialogs/EmptyTrashDialog.qml"));
}

void Application::openWindow(const QString &path)
{
    Window *w = new Window;
    w->rootContext()->setContextProperty("arg", path);
    w->addImageProvider("thumbnailer", new ThumbnailProvider());
    w->load(QUrl("qrc:/qml/main.qml"));
}

QStringList Application::formatUriList(const QStringList &list)
{
    QStringList val = list;

    if (list.isEmpty()) {
        val.append(QDir::currentPath());
    }

    return val;
}

bool Application::parseCommandLineArgs()
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("File Manager"));
    parser.addHelpOption();

    parser.addPositionalArgument("files", "Files", "[FILE1, FILE2,...]");

    QCommandLineOption desktopOption(QStringList() << "d" << "desktop" << "Desktop Mode");
    parser.addOption(desktopOption);

    QCommandLineOption emptyTrashOption(QStringList() << "e" << "empty-trash" << "Empty Trash");
    parser.addOption(emptyTrashOption);

    parser.process(arguments());

    if (m_instance) {
        QPixmapCache::setCacheLimit(2048);

        if (parser.isSet(desktopOption)) {
            Desktop desktop;
        } else {
            openFiles(formatUriList(parser.positionalArguments()));
        }
    } else {
        QDBusInterface iface("com.cutefish.FileManager",
                             "/FileManager",
                             "com.cutefish.FileManager",
                             QDBusConnection::sessionBus(), this);

        if (parser.isSet(emptyTrashOption)) {
            // Empty Dialog
            iface.call("emptyTrash");
        } else {
            iface.call("openFiles", formatUriList(parser.positionalArguments()));
        }
    }

    return m_instance;
}
