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
#include "cutefishapplication.h"
#include "dbusinterface.h"
#include "filemanageradaptor.h"
#include "helper/iojobui.h"
#include "operations/fileoperationsclient.h"
#include "operations/operationtask.h"
#include "window.h"

#include <QCommandLineParser>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <QDBusConnection>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QPixmapCache>
#include <QTimer>
#include <QTranslator>

Application::Application(int &argc, char **argv)
    : QApplication(argc, argv)
    , m_instance(false)
{
    Cutefish::Application::prepare();
    CutefishFM::installIoJobUi();
    setQuitOnLastWindowClosed(true);
    if (QDBusConnection::sessionBus().registerService("com.cutefish.FileManager")) {
        setWindowIcon(QIcon::fromTheme("file-manager"));

        new FileManagerAdaptor(this);
        new DBusInterface;
        QDBusConnection::sessionBus().registerObject("/FileManager", this);

        // Translations
        QLocale locale;
        QString qmFilePath = QString("%1/%2.qm")
                                     .arg("/usr/share/cutefish-filemanager/translations/")
                                     .arg(locale.name());
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

void Application::moveToTrash(const QStringList &paths)
{
    startTrashJob(paths);
}

OperationTask *Application::startTrashJob(const QStringList &paths)
{
    if (paths.isEmpty())
        return nullptr;

    QList<QUrl> urls;

    for (const QString &path : paths) {
        urls.append(QUrl::fromLocalFile(path));
    }

    return FileOperationsClient::instance()->trash(urls);
}

void Application::emptyTrash()
{
    Window *w = new Window;
    w->load(QUrl("qrc:/qml/Dialogs/EmptyTrashDialog.qml"));
    trackWindow(w);
}

void Application::openWindow(const QString &path)
{
    Window *w = new Window;
    w->rootContext()->setContextProperty("arg", path);
    w->load(QUrl("qrc:/qml/main.qml"));
    trackWindow(w);
}

void Application::trackWindow(Window *window)
{
    ++m_windowCount;
    const int exitCode = window->quickWindow() ? EXIT_SUCCESS : EXIT_FAILURE;
    connect(window, &QObject::destroyed, this, [this, exitCode] {
        --m_windowCount;
        // Hidden helper windows must not keep the browser's D-Bus service alive.
        QTimer::singleShot(0, this, [this, exitCode] {
            if (m_windowCount == 0)
                QCoreApplication::exit(exitCode);
        });
    });
}

QStringList Application::formatUriList(const QStringList &list)
{
    QStringList val;

    for (const QString &path : list) {
        val.append(path == "." ? QDir::currentPath() : path);
    }

    if (val.isEmpty()) {
        val.append(QDir::currentPath());
    }

    return val;
}

bool Application::parseCommandLineArgs()
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Files"));
    parser.addHelpOption();

    parser.addPositionalArgument("files", "Files", "[FILE1, FILE2,...]");

    QCommandLineOption emptyTrashOption(QStringList() << "e" << "empty-trash" << "Empty Trash");
    parser.addOption(emptyTrashOption);

    QCommandLineOption moveToTrashOption(QStringList()
                                         << "mtr" << "move-to-trash" << "Move To Trash");
    parser.addOption(moveToTrashOption);

    parser.process(arguments());

    // Pick the action first, then decide who performs it. The trash options used
    // to be handled only when another instance was already running, which was
    // always true while the desktop was a file manager process of its own. It no
    // longer is -- the desktop moved into cutefish-shell -- so these options
    // have to work in the first instance too.
    if (m_instance) {
        QPixmapCache::setCacheLimit(2048);

        if (parser.isSet(emptyTrashOption)) {
            emptyTrash();
        } else if (parser.isSet(moveToTrashOption)) {
            OperationTask *job = startTrashJob(parser.positionalArguments());

            if (!job)
                return false;

            // Deferred so the progress dialog's deleteLater runs before the
            // engines and the task go down with the application.
            connect(job, &OperationTask::result, this, [] {
                QTimer::singleShot(0, qApp, &QCoreApplication::quit);
            });
        } else {
            openFiles(formatUriList(parser.positionalArguments()));
        }
    } else {
        QDBusInterface iface("com.cutefish.FileManager", "/FileManager", "com.cutefish.FileManager",
                             QDBusConnection::sessionBus(), this);

        if (parser.isSet(emptyTrashOption)) {
            // Empty Dialog
            iface.call("emptyTrash");
        } else if (parser.isSet(moveToTrashOption)) {
            iface.call("moveToTrash", parser.positionalArguments());
        } else {
            iface.call("openFiles", formatUriList(parser.positionalArguments()));
        }
    }

    return m_instance;
}
