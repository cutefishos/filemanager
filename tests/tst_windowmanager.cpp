#include "dialogs/fileproperties.h"
#include "helper/dialogs.h"
#include "model/foldermodel.h"
#include "operations/fileoperationsclient.h"
#include "operations/operationtask.h"
#include "qmltypes.h"
#include "windowmanager.h"

#include <QEventLoop>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <task.h>

class WindowManagerTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_config;

    static QUrl fixture(const char *name)
    {
        return QUrl::fromLocalFile(QStringLiteral(TEST_QML_DIR "/") + QLatin1String(name));
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        CutefishFM::registerQmlTypes();
        CutefishFM::initResources();
    }

    void registersPerEngine()
    {
        QQmlEngine engine;
        QVERIFY(!WindowManager::forEngine(&engine));
        {
            WindowManager manager(&engine);
            QCOMPARE(WindowManager::forEngine(&engine), &manager);
            QCOMPARE(WindowManager::ensure(&engine), &manager);

            WindowManager::setDefault(&manager);
            QCOMPARE(WindowManager::defaultManager(), &manager);
            WindowManager::setDefaultIfUnset(nullptr);
            QCOMPARE(WindowManager::defaultManager(), &manager);
        }
        QVERIFY(!WindowManager::forEngine(&engine));
        QVERIFY(!WindowManager::defaultManager());
    }

    void createsVisibleWindowSynchronously()
    {
        QQmlEngine engine;
        WindowManager manager(&engine);
        QSignalSpy count(&manager, &WindowManager::windowCountChanged);

        QQuickWindow *window = manager.create(fixture("Visible.qml"));
        QVERIFY(window);
        QVERIFY(window->isVisible());
        QCOMPARE(manager.windowCount(), 1);
        QCOMPARE(manager.windows(), QList<QQuickWindow *> {window});
        QCOMPARE(QQmlEngine::objectOwnership(window), QQmlEngine::CppOwnership);
        QCOMPARE(count.size(), 1);
        QCOMPARE(count.first().first().toInt(), 1);
    }

    void reusesComponents()
    {
        QQmlEngine engine;
        WindowManager manager(&engine);

        QVERIFY(manager.create(fixture("Plain.qml")));
        QVERIFY(manager.create(fixture("Plain.qml")));
        QVERIFY(manager.create(fixture("Visible.qml")));
        QCOMPARE(manager.findChildren<QQmlComponent *>().size(), 2);
        QCOMPARE(manager.windowCount(), 3);
    }

    void passesRequiredProperties()
    {
        QQmlEngine engine;
        WindowManager manager(&engine);

        QQuickWindow *window = manager.create(fixture("Required.qml"), {{"label", "hello"}});
        QVERIFY(window);
        QCOMPARE(window->property("label").toString(), QString("hello"));
    }

    void missingRequiredPropertyFails()
    {
        QQmlEngine engine;
        WindowManager manager(&engine);
        const int windowsBefore = QGuiApplication::allWindows().size();

        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("cannot create .*Required\\.qml"));
        QVERIFY(!manager.create(fixture("Required.qml")));
        QCOMPARE(manager.windowCount(), 0);
        QCOMPARE(QGuiApplication::allWindows().size(), windowsBefore);
    }

    void rejectsNonWindowRoots()
    {
        QQmlEngine engine;
        WindowManager manager(&engine);

        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("NotWindow\\.qml is not a window"));
        QVERIFY(!manager.create(fixture("NotWindow.qml")));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("cannot load .*Missing\\.qml"));
        QVERIFY(!manager.create(fixture("Missing.qml")));
        QCOMPARE(manager.windowCount(), 0);
    }

    void closedWindowIsDestroyedLater()
    {
        QQmlEngine engine;
        WindowManager manager(&engine);
        QPointer<QQuickWindow> window = manager.create(fixture("Visible.qml"));
        QVERIFY(window);
        QSignalSpy count(&manager, &WindowManager::windowCountChanged);

        window->close();
        // Still alive right after close(): its handlers may be on the stack.
        QVERIFY(window);
        QCOMPARE(manager.windowCount(), 1);
        QTRY_VERIFY(!window);
        QCOMPARE(manager.windowCount(), 0);
        QCOMPARE(count.size(), 1);
        QCOMPARE(count.first().first().toInt(), 0);
    }

    void closeFromQmlIsSafe()
    {
        QQmlEngine engine;
        WindowManager manager(&engine);
        QPointer<QQuickWindow> window = manager.create(fixture("Plain.qml"));
        QVERIFY(window);
        window->show();

        QVERIFY(QMetaObject::invokeMethod(window, "closeSelf"));
        QVERIFY(window);
        QTRY_VERIFY(!window);
    }

    void rejectedCloseKeepsWindow()
    {
        QQmlEngine engine;
        WindowManager manager(&engine);
        QPointer<QQuickWindow> window = manager.create(fixture("RejectClose.qml"));
        QVERIFY(window);

        window->close();
        QTest::qWait(50);
        QVERIFY(window);
        QVERIFY(window->isVisible());
        QCOMPARE(manager.windowCount(), 1);

        window->setProperty("allowClose", true);
        window->close();
        QTRY_VERIFY(!window);
    }

    void releasesWindowsNeverShown()
    {
        QQmlEngine engine;
        WindowManager manager(&engine);
        QPointer<QQuickWindow> window = manager.create(fixture("Plain.qml"));
        QVERIFY(window);

        // close() is a no-op without a platform window, so nothing happens.
        window->close();
        QTest::qWait(20);
        QVERIFY(window);

        manager.release(window);
        manager.release(window);
        QVERIFY(window);
        QTRY_VERIFY(!window);
        QCOMPARE(manager.windowCount(), 0);
    }

    void showsDeferred()
    {
        QQmlEngine engine;
        WindowManager manager(&engine);
        QPointer<QQuickWindow> window =
                manager.create(fixture("Plain.qml"), {}, nullptr, WindowManager::ShowDeferred);
        QVERIFY(window);
        QVERIFY(!window->isVisible());
        QTRY_VERIFY(window->isVisible());

        // A window released before its deferred show never appears.
        QPointer<QQuickWindow> dropped =
                manager.create(fixture("Plain.qml"), {}, nullptr, WindowManager::ShowDeferred);
        manager.release(dropped);
        QTRY_VERIFY(!dropped);
    }

    void closesWithOwner()
    {
        QQmlEngine engine;
        WindowManager manager(&engine);
        QPointer<QQuickWindow> owner = manager.create(fixture("Visible.qml"));
        QPointer<QQuickWindow> shown =
                manager.create(fixture("Plain.qml"), {}, owner, WindowManager::CloseWithOwner);
        QPointer<QQuickWindow> hidden =
                manager.create(fixture("Plain.qml"), {}, owner, WindowManager::CloseWithOwner);
        QPointer<QQuickWindow> independent = manager.create(fixture("Plain.qml"), {}, owner);
        QVERIFY(owner && shown && hidden && independent);
        QCOMPARE(shown->transientParent(), owner.data());
        QCOMPARE(independent->transientParent(), owner.data());
        shown->show();
        QSignalSpy closing(shown.data(), &QQuickWindow::closing);

        owner->close();
        QTRY_VERIFY(!owner);
        QTRY_VERIFY(!shown);
        QTRY_VERIFY(!hidden);
        QCOMPARE(closing.size(), 1);
        QVERIFY(independent);
        QCOMPARE(manager.windowCount(), 1);
    }

    void closesWithUnmanagedOwner()
    {
        QQmlEngine engine;
        WindowManager manager(&engine);
        auto *owner = new QWindow;
        QPointer<QQuickWindow> child =
                manager.create(fixture("Plain.qml"), {}, owner, WindowManager::CloseWithOwner);
        QVERIFY(child);
        QCOMPARE(child->transientParent(), owner);

        delete owner;
        QTRY_VERIFY(!child);
    }

    void waitsForNestedLoops()
    {
        QQmlEngine engine;
        WindowManager manager(&engine);
        QPointer<QQuickWindow> window;
        bool aliveAfterNestedLoop = false;

        QEventLoop outer;
        QTimer::singleShot(0, &outer, [&] {
            window = manager.create(fixture("Visible.qml"));

            // Stands in for QDrag::exec(): the window closes while it runs.
            QEventLoop nested;
            QTimer::singleShot(0, &nested, [&] { window->close(); });
            QTimer::singleShot(250, &nested, &QEventLoop::quit);
            nested.exec();

            aliveAfterNestedLoop = !window.isNull();
            outer.quit();
        });
        outer.exec();

        QVERIFY(aliveAfterNestedLoop);
        QTRY_VERIFY(!window);
    }

    void deletesPendingItemsFirst()
    {
        QQmlEngine engine;
        WindowManager manager(&engine);
        QPointer<QQuickWindow> window = manager.create(fixture("Plain.qml"));
        QVERIFY(window);
        auto *doomed = window->findChild<QQuickItem *>("doomed");
        QVERIFY(doomed);

        QStringList order;
        connect(doomed, &QObject::destroyed, this, [&] { order.append("item"); });
        connect(window, &QObject::destroyed, this, [&] { order.append("window"); });

        // The item is only posted for deletion when the window goes: without
        // flushing first it would outlive its window. Posted from inside the
        // event loop, as delegates are.
        QEventLoop loop;
        QTimer::singleShot(0, &loop, [&] {
            manager.release(window);
            doomed->deleteLater();
        });
        connect(window, &QObject::destroyed, &loop, &QEventLoop::quit);
        QTimer::singleShot(2000, &loop, &QEventLoop::quit);
        loop.exec();
        QVERIFY(!window);
        QCOMPARE(order, QStringList({"item", "window"}));
    }

    void destroyAllDoesNotClose()
    {
        QQmlEngine engine;
        WindowManager manager(&engine);
        QPointer<QQuickWindow> first = manager.create(fixture("Visible.qml"));
        QPointer<QQuickWindow> second =
                manager.create(fixture("Visible.qml"), {}, first, WindowManager::CloseWithOwner);
        QSignalSpy closingFirst(first.data(), &QQuickWindow::closing);
        QSignalSpy closingSecond(second.data(), &QQuickWindow::closing);

        manager.destroyAll();
        QVERIFY(!first);
        QVERIFY(!second);
        QCOMPARE(manager.windowCount(), 0);
        QCOMPARE(closingFirst.size(), 0);
        QCOMPARE(closingSecond.size(), 0);
        QVERIFY(manager.findChildren<QQmlComponent *>().isEmpty());

        // Still usable afterwards.
        QVERIFY(manager.create(fixture("Plain.qml")));
    }

    void managerTeardownDestroysWindows()
    {
        QPointer<QQuickWindow> window;
        {
            QQmlEngine engine;
            WindowManager manager(&engine);
            window = manager.create(fixture("Visible.qml"));
            QVERIFY(window);
        }
        QVERIFY(!window);
    }

    void drivesAsynchronousLoaders()
    {
        {
            // A plain engine has no incubation controller: the Loader is synchronous.
            QQmlEngine engine;
            WindowManager manager(&engine);
            QQuickWindow *window = manager.create(fixture("AsyncLoader.qml"));
            QVERIFY(window);
            QCOMPARE(window->property("loaderStatus").toInt(), 1); // Loader.Ready
        }
        {
            QQmlEngine engine;
            WindowManager manager(&engine);
            manager.installIncubationController();
            QVERIFY(engine.incubationController());
            QQuickWindow *window = manager.create(fixture("AsyncLoader.qml"));
            QVERIFY(window);
            QCOMPARE(window->property("loaderStatus").toInt(), 2); // Loader.Loading
            QTRY_COMPARE(window->property("loaderStatus").toInt(), 1);
        }
    }

    void browserWindowsLeaveNothingBehind()
    {
        QTemporaryDir folder;
        for (const char *name : {"a.txt", "b.txt", "c"}) {
            QFile file(folder.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }

        QQmlEngine engine;
        WindowManager manager(&engine);
        manager.installIncubationController();
        const int windowsBefore = QGuiApplication::allWindows().size();

        for (int i = 0; i < 20; ++i) {
            QPointer<QQuickWindow> window = manager.create(
                    QUrl("qrc:/qml/main.qml"), {{"startPath", folder.path()}});
            QVERIFY(window);
            // The grid, with a selected item, is where effects reference items.
            window->property("settings").value<QObject *>()->setProperty("viewMethod", 1);
            auto *page = window->property("_folderPage").value<QObject *>();
            QVERIFY(page);
            auto *model = page->property("model").value<FolderModel *>();
            QVERIFY(model);
            QTRY_COMPARE(model->count(), 3);
            QTRY_VERIFY(page->property("currentView").value<QQuickItem *>());
            model->setSelected(i % 3);

            // The last tab closes the window from inside QML.
            QVERIFY(QMetaObject::invokeMethod(window, "closeTab", Q_ARG(QVariant, 0)));
            QTRY_VERIFY(!window);
        }

        QCOMPARE(manager.windowCount(), 0);
        QCOMPARE(QGuiApplication::allWindows().size(), windowsBefore);
    }

    void releasesProgressOfQuickTasks()
    {
        QQmlEngine engine;
        WindowManager manager(&engine);
        QTemporaryDir dir;

        QPointer<OperationTask> task = FileOperationsClient::instance()->mkdir(
                QUrl::fromLocalFile(dir.filePath("created")));
        QSignalSpy finished(task.data(), &OperationTask::result);
        QPointer<QQuickWindow> window = CutefishFM::Dialogs::ioTask(&manager, task);
        QVERIFY(window);
        QTRY_COMPARE(finished.size(), 1);
        QTRY_VERIFY(!task);
        QTRY_VERIFY(!window);
        QCOMPARE(manager.windowCount(), 0);
    }

    void keepsProgressOfFailedTasks()
    {
        QQmlEngine engine;
        WindowManager manager(&engine);
        QTemporaryDir dir;

        QPointer<OperationTask> task = FileOperationsClient::instance()->mkdir(
                QUrl::fromLocalFile(dir.filePath("missing/created")));
        QPointer<QQuickWindow> window = CutefishFM::Dialogs::ioTask(&manager, task);
        QVERIFY(window);
        // The error stays on screen after the finished task is deleted.
        QTRY_VERIFY(!task);
        QVERIFY(!window->property("errorMessage").toString().isEmpty());
        QTRY_VERIFY(window->isVisible());
        QTest::qWait(50);
        QVERIFY(window);

        window->close();
        QTRY_VERIFY(!window);
    }

    void deleteConfirmationOutlivesItsModel()
    {
        QQmlEngine engine;
        WindowManager manager(&engine);
        auto *model = new FolderModel;
        QPointer<QQuickWindow> dialog = CutefishFM::Dialogs::confirmDelete(&manager, model, nullptr);
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->isVisible());

        delete model;
        QVERIFY(QMetaObject::invokeMethod(dialog, "accept"));
        QTRY_VERIFY(!dialog);
    }

    void confirmationClosesWithOwner()
    {
        QQmlEngine engine;
        WindowManager manager(&engine);
        FolderModel model;
        QPointer<QQuickWindow> owner = manager.create(fixture("Visible.qml"));
        QPointer<QQuickWindow> dialog = CutefishFM::Dialogs::confirmDelete(&manager, &model, owner);
        QVERIFY(dialog);
        QCOMPARE(dialog->transientParent(), owner.data());
        QTRY_VERIFY(dialog->isVisible());
        QSignalSpy rejected(dialog.data(), SIGNAL(rejected()));

        owner->close();
        QTRY_VERIFY(!owner);
        QTRY_VERIFY(!dialog);
        QCOMPARE(rejected.size(), 1);
    }

    void teardownDoesNotCancelConflicts()
    {
        QTemporaryDir dir;
        QVERIFY(QDir(dir.path()).mkdir("target"));
        for (const QString &path : {dir.filePath("photo.svg"), dir.filePath("target/photo.svg")}) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("<svg/>");
        }

        QPointer<OperationTask> task = FileOperationsClient::instance()->copy(
                {QUrl::fromLocalFile(dir.filePath("photo.svg"))},
                QUrl::fromLocalFile(dir.filePath("target")));
        bool canceled = true;
        QObject receiver;
        connect(task.data(), &OperationTask::result, &receiver,
                [&canceled](OperationTask *finished) { canceled = finished->wasCanceled(); });
        {
            QQmlEngine engine;
            WindowManager manager(&engine);
            QQuickWindow *window = CutefishFM::Dialogs::ioTask(&manager, task);
            QVERIFY(window);
            QTRY_VERIFY(window->property("conflict").toBool());
            auto *conflict = window->findChild<QQuickWindow *>("conflictDialog");
            QVERIFY(conflict);
            QTRY_VERIFY(conflict->isVisible());
            manager.destroyAll();
        }
        QTest::qWait(100);
        QVERIFY(task);
        QVERIFY(!task->isFinished());

        task->resolveConflict(int(Cutefish::IO::ConflictPolicy::Skip));
        QTRY_VERIFY(!task);
        QVERIFY(!canceled);
    }

    void propertiesOwnTheirDetails()
    {
        QQmlEngine engine;
        WindowManager manager(&engine);
        QTemporaryDir dir;

        QPointer<QQuickWindow> window = CutefishFM::Dialogs::properties(
                &manager, {Cutefish::IO::FileItem(QUrl::fromLocalFile(dir.path()))}, nullptr);
        QVERIFY(window);
        QPointer<FileProperties> details = window->findChild<FileProperties *>();
        QVERIFY(details);
        QCOMPARE(window->property("details").value<QObject *>(), details.data());
        QTRY_VERIFY(window->isVisible());

        window->close();
        QTRY_VERIFY(!window);
        QVERIFY(!details);
    }
};

QTEST_MAIN(WindowManagerTest)
#include "tst_windowmanager.moc"
