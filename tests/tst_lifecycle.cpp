#include "application.h"
#include "model/foldermodel.h"
#include "operations/fileoperationsclient.h"
#include "operations/operationtask.h"
#include "qmltypes.h"
#include "windowmanager.h"

#include <QDir>
#include <QProcess>
#include <QPointer>
#include <QDeadlineTimer>
#include <QDBusConnectionInterface>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <task.h>
#include <preferences.h>

// Sanitizer builds run several times slower; the watchdogs scale with them.
#if defined(__SANITIZE_ADDRESS__) || (defined(__has_feature) && __has_feature(address_sanitizer))
static constexpr int kTimeScale = 5;
#else
static constexpr int kTimeScale = 1;
#endif

static QList<QWindow *> browserWindows()
{
    QList<QWindow *> windows;
    for (auto *window : QGuiApplication::topLevelWindows()) {
        if (window->isVisible() && window->property("tabs").isValid())
            windows.append(window);
    }
    return windows;
}

static int activatedRequest(Application &app, const QString &folder, bool showFolders)
{
    const QUrl url = QUrl::fromLocalFile(folder + "/Screenshot #1.png");
    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::WriteOnly))
        return 6;
    file.close();

    bool selected = false;
    QTimer poll;
    poll.setInterval(50);
    QObject::connect(&poll, &QTimer::timeout, &app, [&] {
        const auto windows = browserWindows();
        if (windows.size() != 1)
            return;
        auto *page = windows.first()->property("_folderPage").value<QObject *>();
        if (!page)
            return;
        auto *model = qobject_cast<FolderModel *>(page->property("model").value<QObject *>());
        auto *view = page->property("currentView").value<QQuickItem *>();
        if (!model || !view || model->status() != FolderModel::Ready)
            return;
        if (model->resolvedUrl() != QUrl::fromLocalFile(folder))
            return;
        if (showFolders ? model->hasSelection()
                        : (model->selectedUrls() != QList<QUrl> {url}
                           || view->property("currentIndex").toInt() < 0))
            return;
        selected = true;
        poll.stop();
        windows.first()->close();
    });
    QTimer::singleShot(100, &app, [&] {
        if (!browserWindows().isEmpty()) {
            app.exit(7);
            return;
        }
        auto message = QDBusMessage::createMethodCall(
                "org.freedesktop.FileManager1", "/org/freedesktop/FileManager1",
                "org.freedesktop.FileManager1", showFolders ? "ShowFolders" : "ShowItems");
        message << QStringList {showFolders ? QUrl::fromLocalFile(folder).toString() : url.toString()}
                << QString();
        QDBusConnection::sessionBus().asyncCall(message);
        poll.start();
    });
    QTimer::singleShot(5000 * kTimeScale, &app, [&] { app.exit(4); });
    const int result = app.run();
    return selected ? result : 5;
}

// Two browser windows; the application ends once both are closed.
static int closingWindowsQuits(Application &app, const QString &folder)
{
    bool closedLastWindow = false;
    QTimer::singleShot(0, &app, [&] {
        app.openFiles({folder, folder});
        const auto windows = browserWindows();
        if (windows.size() != 2) {
            app.exit(2);
            return;
        }
        windows.first()->close();
        QTimer::singleShot(100, &app, [&] {
            const auto remaining = browserWindows();
            if (remaining.size() != 1) {
                app.exit(3);
                return;
            }
            closedLastWindow = true;
            remaining.first()->close();
        });
    });
    QTimer::singleShot(5000 * kTimeScale, &app, [&] { app.exit(4); });
    const int result = app.exec();
    return closedLastWindow ? result : 5;
}

// A running operation outlives the last window: the application waits for it.
// With the daemon on the bus, a task runs there and is presented here by a
// remote task object, which must go once its result is out.
static int remoteTaskIsReleased(Application &app, const QString &folder)
{
    QProcess daemon;
    daemon.start(QCoreApplication::applicationDirPath()
                         + QStringLiteral("/cutefish-filemanager-daemon"));
    if (!daemon.waitForStarted())
        return 6;
    QFile file(folder + "/photo.svg");
    if (!file.open(QIODevice::WriteOnly))
        return 6;
    file.write("<svg/>");
    file.close();
    QDir(folder).mkdir("target");

    int outcome = 5;
    QTimer::singleShot(0, &app, [&] {
        const auto *bus = QDBusConnection::sessionBus().interface();
        QDeadlineTimer deadline(3000 * kTimeScale);
        while (!bus->isServiceRegistered(QStringLiteral("com.cutefish.FileOperations"))
               && !deadline.hasExpired()) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        }
        QPointer<OperationTask> task = FileOperationsClient::instance()->copy(
                {QUrl::fromLocalFile(folder + "/photo.svg")},
                QUrl::fromLocalFile(folder + "/target"));
        if (!task || !QByteArray(task->metaObject()->className()).contains("Remote")) {
            outcome = 3;
            app.exit(3);
            return;
        }
        QObject::connect(task, &OperationTask::result, &app, [&, task] {
            QTimer::singleShot(100, &app, [&, task] {
                outcome = task ? 7 : (FileOperationsClient::instance()->activeTaskCount() == 0 ? 0 : 8);
                app.exit(outcome);
            });
        });
    });
    QTimer::singleShot(5000 * kTimeScale, &app, [&] { app.exit(4); });
    const int result = app.exec();
    daemon.terminate();
    daemon.waitForFinished(3000);
    return outcome == 0 ? result : outcome;
}

static int runningTaskKeepsApplication(Application &app, const QString &folder)
{
    QDir(folder).mkdir("target");
    for (const QString &path : {folder + "/photo.svg", folder + "/target/photo.svg"}) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return 6;
        file.write("<svg/>");
    }

    bool resolved = false;
    QTimer::singleShot(0, &app, [&] {
        app.openFiles({folder});
        const auto windows = browserWindows();
        if (windows.size() != 1) {
            app.exit(2);
            return;
        }

        // Stops on the existing target and waits for an answer.
        QPointer<OperationTask> task = FileOperationsClient::instance()->copy(
                {QUrl::fromLocalFile(folder + "/photo.svg")},
                QUrl::fromLocalFile(folder + "/target"));
        windows.first()->close();

        QTimer::singleShot(500 * kTimeScale, &app, [&, task] {
            if (!browserWindows().isEmpty() || !task || task->isFinished()) {
                app.exit(3);
                return;
            }
            // Its progress dialog is still there to ask about the conflict.
            if (app.windowManager()->windowCount() != 1) {
                app.exit(7);
                return;
            }
            resolved = true;
            task->resolveConflict(int(Cutefish::IO::ConflictPolicy::Skip));
        });
    });
    QTimer::singleShot(5000 * kTimeScale, &app, [&] { app.exit(4); });
    const int result = app.exec();
    return resolved ? result : 5;
}

// Declining to empty the trash leaves nothing to do.
static int rejectedEmptyTrashQuits(Application &app)
{
    bool rejected = false;
    QTimer::singleShot(0, &app, [&] {
        app.emptyTrash();
        QTimer::singleShot(200, &app, [&] {
            const auto windows = app.windowManager()->windows();
            if (windows.size() != 1 || !windows.first()->isVisible()) {
                app.exit(2);
                return;
            }
            rejected = true;
            windows.first()->close();
        });
    });
    QTimer::singleShot(5000 * kTimeScale, &app, [&] { app.exit(4); });
    const int result = app.exec();
    return rejected ? result : 5;
}

// The session's daemon holds the names with no window, opens what the bus asks
// for, and outlives its last window.
static int daemonOutlivesWindows(Application &app, const QString &folder)
{
    const auto call = [](const QString &method, const QVariantList &arguments) {
        auto message = QDBusMessage::createMethodCall("com.cutefish.FileManager", "/FileManager",
                                                      "com.cutefish.FileManager", method);
        message.setArguments(arguments);
        message.setAutoStartService(false);
        QDBusConnection::sessionBus().asyncCall(message);
    };

    bool survived = false;
    QTimer::singleShot(500, &app, [&] {
        if (!app.windowManager() || app.windowManager()->windowCount() != 0) {
            app.exit(2);
            return;
        }
        call("openFiles", {QStringList {folder}});
        QTimer::singleShot(500, &app, [&] {
            const auto windows = browserWindows();
            if (windows.size() != 1) {
                app.exit(3);
                return;
            }
            windows.first()->close();
            QTimer::singleShot(500, &app, [&] {
                if (app.windowManager()->windowCount() != 0
                    || !QDBusConnection::sessionBus().interface()->isServiceRegistered(
                            "com.cutefish.FileManager")) {
                    app.exit(6);
                    return;
                }
                // The dock's Empty Trash: its dialog belongs to the daemon too.
                call("emptyTrash", {});
                QTimer::singleShot(500, &app, [&] {
                    if (app.windowManager()->windowCount() != 1) {
                        app.exit(7);
                        return;
                    }
                    survived = true;
                    app.exit(0);
                });
            });
        });
    });
    QTimer::singleShot(5000 * kTimeScale, &app, [&] { app.exit(4); });
    const int result = app.run();
    return survived ? result : 5;
}

static QList<QWindow *> chooserWindows()
{
    QList<QWindow *> windows;
    for (auto *window : QGuiApplication::topLevelWindows()) {
        if (window->isVisible() && window->objectName() == QLatin1String("fileChooserWindow"))
            windows.append(window);
    }
    return windows;
}

// The daemon serves the portal's dialogs: one window per Open, answered with
// the helper's JSON when Close or the user ends it, and it outlives them.
static int daemonServesChooser(Application &app, const QString &folder)
{
    // Over the bus like the portal: a call to a service of this very connection
    // would be delivered in-process.
    auto portal = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "portal");
    const auto call = [&portal](const QString &method, const QVariantList &arguments) {
        auto message = QDBusMessage::createMethodCall("com.cutefish.FileChooser",
                                                      "/com/cutefish/FileChooser",
                                                      "com.cutefish.FileChooser", method);
        message.setArguments(arguments);
        message.setAutoStartService(false);
        return QDBusPendingReply<QString>(portal.asyncCall(message, 60000));
    };
    const QString request = QString::fromUtf8(
            QJsonDocument(QJsonObject {{"mode", "OpenFile"}, {"current_folder", folder}})
                    .toJson(QJsonDocument::Compact));
    const auto cancelled = [](const QDBusPendingReply<QString> &reply) {
        return reply.isFinished() && !reply.isError()
                && QJsonDocument::fromJson(reply.value().toUtf8()).object()
                == QJsonObject {{"response", 1}};
    };

    QDBusPendingReply<QString> first;
    QDBusPendingReply<QString> second;
    QDBusPendingReply<QString> duplicate;
    bool served = false;
    QTimer::singleShot(500, &app, [&] {
        first = call("Open", {"/request/a", request});
        second = call("Open", {"/request/b", request});
        duplicate = call("Open", {"/request/a", request});
        QTimer::singleShot(1000, &app, [&] {
            if (chooserWindows().size() != 2 || !duplicate.isError()
                || app.windowManager()->windowCount() != 2) {
                app.exit(2);
                return;
            }
            call("Close", {"/request/a"});
            QTimer::singleShot(500, &app, [&] {
                const auto windows = chooserWindows();
                if (!cancelled(first) || second.isFinished() || windows.size() != 1) {
                    app.exit(3);
                    return;
                }
                windows.first()->close();
                QTimer::singleShot(500, &app, [&] {
                    if (!cancelled(second) || app.windowManager()->windowCount() != 0
                        || !QDBusConnection::sessionBus().interface()->isServiceRegistered(
                                "com.cutefish.FileChooser")) {
                        app.exit(6);
                        return;
                    }
                    served = true;
                    app.exit(0);
                });
            });
        });
    });
    QTimer::singleShot(10000 * kTimeScale, &app, [&] { app.exit(4); });
    const int result = app.run();
    return served ? result : 5;
}

int main(int argc, char **argv)
{
    QTemporaryDir config;
    QTemporaryDir data;
    QTemporaryDir folder;
    qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
    qputenv("XDG_DATA_HOME", data.path().toUtf8());
    CutefishFM::registerQmlTypes();
    CutefishFM::initResources();
    const QString scenario = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("windows");
    const bool activated = scenario.startsWith("activation-");
    QByteArray activationArgument("--dbus-activated");
    QByteArray daemonArgument("--daemon");
    if (activated) {
        QSettings settings(Preferences::file("filemanager"), QSettings::IniFormat);
        settings.setValue("viewMethod", scenario == "activation-list" ? 0 : 1);
        argv[1] = activationArgument.data();
    } else if (scenario == "daemon" || scenario == "daemon-chooser") {
        argv[1] = daemonArgument.data();
    }
    Application app(argc, argv);

    // Qt's own last-window rule must not be what ends the application.
    QWindow helper;
    helper.show();

    if (activated)
        return activatedRequest(app, folder.path(), scenario == "activation-folders");
    if (scenario == "daemon")
        return daemonOutlivesWindows(app, folder.path());
    if (scenario == "daemon-chooser")
        return daemonServesChooser(app, folder.path());
    if (scenario == "task")
        return runningTaskKeepsApplication(app, folder.path());
    if (scenario == "remote-task")
        return remoteTaskIsReleased(app, folder.path());
    if (scenario == "emptytrash")
        return rejectedEmptyTrashQuits(app);
    return closingWindowsQuits(app, folder.path());
}
