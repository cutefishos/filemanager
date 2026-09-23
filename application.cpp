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
#include "helper/dialogs.h"
#include "helper/iojobui.h"
#include "operations/fileoperationsclient.h"
#include "operations/operationtask.h"
#include "windowmanager.h"

#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QMap>
#include <QPixmapCache>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QTimer>
#include <QTranslator>

namespace {
const QString serviceName = QStringLiteral("com.cutefish.FileManager");
const QString freedesktopServiceName = QStringLiteral("org.freedesktop.FileManager1");
const QUrl browserWindow(QStringLiteral("qrc:/qml/main.qml"));
}

Application::Application(int &argc, char **argv)
    : QApplication(argc, argv)
{
    Cutefish::Application::prepare();
    CutefishFM::installIoJobUi();
    setWindowIcon(QIcon::fromTheme("file-manager"));

    // Quitting is decided by quitIfIdle(): Qt's own rule would quit on the last
    // window while a file operation it presents is still running.
    setQuitOnLastWindowClosed(false);
}

Application::~Application()
{
    CutefishFM::setFolderOpener(nullptr);

    // Windows and components go before the engine they were created in.
    delete m_windows;
    delete m_engine;
}

int Application::run()
{
    if (!parseCommandLineArgs())
        return m_exitCode;

    scheduleIdleCheck();
    return QApplication::exec();
}

bool Application::registerServices()
{
    if (m_servicesRegistered)
        return true;

    auto bus = QDBusConnection::sessionBus();

    // Objects first, so a call arriving as soon as the name is ours finds them.
    if (!findChild<FileManagerAdaptor *>()) {
        new FileManagerAdaptor(this);
        new DBusInterface(this);
    }
    bus.registerObject(QStringLiteral("/FileManager"), this);

    if (!bus.registerService(serviceName)) {
        bus.unregisterObject(QStringLiteral("/FileManager"));
        return false;
    }

    if (bus.interface())
        bus.interface()->registerService(freedesktopServiceName,
                                         QDBusConnectionInterface::QueueService);
    m_servicesRegistered = true;
    return true;
}

WindowManager *Application::ensureWindows()
{
    if (m_windows)
        return m_windows;

    // Before any QML exists, so every window is translated.
    QLocale locale;
    const QString qmFilePath = QString("%1/%2.qm")
                                       .arg("/usr/share/cutefish-filemanager/translations/")
                                       .arg(locale.name());
    if (QFile::exists(qmFilePath)) {
        auto *translator = new QTranslator(this);
        if (translator->load(qmFilePath))
            installTranslator(translator);
        else
            delete translator;
    }

    QPixmapCache::setCacheLimit(2048);

    m_engine = new QQmlEngine;
    m_windows = new WindowManager(m_engine);
    m_windows->installIncubationController();
    WindowManager::setDefault(m_windows);

    CutefishFM::setFolderOpener([this](const QStringList &paths, const QStringList &selection) {
        for (const QString &path : paths)
            openWindow(path, selection);
    });

    connect(m_windows, &WindowManager::windowCountChanged, this, &Application::scheduleIdleCheck);
    connect(FileOperationsClient::instance(), &FileOperationsClient::activeTaskCountChanged, this,
            &Application::scheduleIdleCheck);
    return m_windows;
}

void Application::unregisterServices()
{
    if (!m_servicesRegistered)
        return;

    auto bus = QDBusConnection::sessionBus();
    bus.unregisterService(serviceName);
    if (bus.interface())
        bus.interface()->unregisterService(freedesktopServiceName);
    m_servicesRegistered = false;
}

WindowManager *Application::windowManager() const
{
    return m_windows;
}

void Application::openFiles(const QStringList &paths)
{
    for (const QString &path : paths)
        openWindow(path);
}

void Application::moveToTrash(const QStringList &paths)
{
    // Its progress dialog needs the window manager.
    ensureWindows();
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
    if (!CutefishFM::Dialogs::confirmEmptyTrash(ensureWindows(), nullptr))
        m_exitCode = EXIT_FAILURE;
}

void Application::showItems(const QList<QUrl> &items)
{
    // One window per folder, with every item asked for in it selected.
    QMap<QString, QStringList> folders;
    for (const QUrl &item : items) {
        const QString folder = item.isLocalFile()
                ? QFileInfo(item.toLocalFile()).absolutePath()
                : item.adjusted(QUrl::StripTrailingSlash | QUrl::RemoveFilename).toString();
        folders[folder].append(item.toString());
    }

    for (auto it = folders.cbegin(); it != folders.cend(); ++it)
        openWindow(it.key(), it.value());
}

void Application::showItemProperties(const QList<QUrl> &items)
{
    Cutefish::IO::FileItemList fileItems;
    for (const QUrl &url : items)
        fileItems.append(Cutefish::IO::FileItem(url));

    CutefishFM::Dialogs::properties(ensureWindows(), fileItems, nullptr);
}

QQuickWindow *Application::openWindow(const QString &path, const QStringList &selection)
{
    QQuickWindow *window = ensureWindows()->create(
            browserWindow, {{QStringLiteral("startPath"), path},
                            {QStringLiteral("initialSelection"), selection}});
    if (!window)
        m_exitCode = EXIT_FAILURE;
    return window;
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

    if (!registerServices()) {
        QDBusInterface iface(serviceName, QStringLiteral("/FileManager"), serviceName,
                             QDBusConnection::sessionBus());
        QDBusMessage reply;
        if (parser.isSet(emptyTrashOption))
            reply = iface.call("emptyTrash");
        else if (parser.isSet(moveToTrashOption))
            reply = iface.call("moveToTrash", parser.positionalArguments());
        else
            reply = iface.call("openFiles", formatUriList(parser.positionalArguments()));

        // The running instance may have been quitting just now: take over.
        if (reply.type() != QDBusMessage::ErrorMessage || !registerServices())
            return false;
    }

    // Each action leaves a window or a task behind, or nothing: then the
    // idle check ends the process.
    if (parser.isSet(emptyTrashOption))
        emptyTrash();
    else if (parser.isSet(moveToTrashOption))
        moveToTrash(parser.positionalArguments());
    else
        openFiles(formatUriList(parser.positionalArguments()));

    return true;
}

bool Application::isIdle() const
{
    return (!m_windows || m_windows->windowCount() == 0)
            && FileOperationsClient::instance()->activeTaskCount() == 0;
}

void Application::scheduleIdleCheck()
{
    if (m_idleCheckScheduled)
        return;

    m_idleCheckScheduled = true;
    QTimer::singleShot(0, this, [this] {
        m_idleCheckScheduled = false;
        quitIfIdle();
    });
}

void Application::quitIfIdle()
{
    if (!isIdle())
        return;

    // Give up the names first, then look again: a request that was already on
    // its way is delivered in between and keeps the process alive.
    unregisterServices();
    QTimer::singleShot(0, this, [this] {
        if (isIdle()) {
            exit(m_exitCode);
            return;
        }
        registerServices();
    });
}
