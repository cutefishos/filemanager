#include "application.h"
#include "model/foldermodel.h"
#include "operations/fileoperationsclient.h"
#include "operations/operationtask.h"
#include "qmltypes.h"
#include "windowmanager.h"

#include <QDir>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QFile>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <task.h>

namespace {
QList<QWindow *> browserWindows()
{
    QList<QWindow *> windows;
    for (auto *window : QGuiApplication::topLevelWindows()) {
        if (window->isVisible() && window->property("tabs").isValid())
            windows.append(window);
    }
    return windows;
}

int activatedRequest(Application &app, const QString &folder, bool showFolders)
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
    QTimer::singleShot(5000, &app, [&] { app.exit(4); });
    const int result = app.run();
    return selected ? result : 5;
}

// Two browser windows; the application ends once both are closed.
int closingWindowsQuits(Application &app, const QString &folder)
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
    QTimer::singleShot(5000, &app, [&] { app.exit(4); });
    const int result = app.exec();
    return closedLastWindow ? result : 5;
}

// A running operation outlives the last window: the application waits for it.
int runningTaskKeepsApplication(Application &app, const QString &folder)
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
        OperationTask *task = FileOperationsClient::instance()->copy(
                {QUrl::fromLocalFile(folder + "/photo.svg")},
                QUrl::fromLocalFile(folder + "/target"));
        windows.first()->close();

        QTimer::singleShot(500, &app, [&, task] {
            if (!browserWindows().isEmpty() || task->isFinished()) {
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
    QTimer::singleShot(5000, &app, [&] { app.exit(4); });
    const int result = app.exec();
    return resolved ? result : 5;
}

// Declining to empty the trash leaves nothing to do.
int rejectedEmptyTrashQuits(Application &app)
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
    QTimer::singleShot(5000, &app, [&] { app.exit(4); });
    const int result = app.exec();
    return rejected ? result : 5;
}
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
    if (activated) {
        QSettings settings("cutefish", "filemanager");
        settings.setValue("viewMethod", scenario == "activation-list" ? 0 : 1);
        argv[1] = activationArgument.data();
    }
    Application app(argc, argv);

    // Qt's own last-window rule must not be what ends the application.
    QWindow helper;
    helper.show();

    if (activated)
        return activatedRequest(app, folder.path(), scenario == "activation-folders");
    if (scenario == "task")
        return runningTaskKeepsApplication(app, folder.path());
    if (scenario == "emptytrash")
        return rejectedEmptyTrashQuits(app);
    return closingWindowsQuits(app, folder.path());
}
