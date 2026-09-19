#include "model/foldermodel.h"
#include "qmltypes.h"

#include <QApplication>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlExpression>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>
#include <functional>

class TabsTest : public QObject
{
    Q_OBJECT

private slots:

    void headerDragScroll_data()
    {
        QTest::addColumn<int>("viewMethod");
        QTest::newRow("list") << 0;
        QTest::newRow("grid") << 1;
    }

    void headerDragScroll()
    {
        QFETCH(int, viewMethod);
        QTemporaryDir folders;
        QVERIFY(folders.isValid());
        for (int i = 0; i < 120; ++i)
            QVERIFY(QDir(folders.path()).mkdir(QString::number(i)));
        QQmlApplicationEngine engine;
        CutefishFM::registerImageProviders(&engine);
        engine.rootContext()->setContextProperty("arg", folders.path());
        engine.load(QUrl("qrc:/qml/main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        auto *settings = window->property("settings").value<QObject *>();
        QVERIFY(settings);
        settings->setProperty("viewMethod", viewMethod);
        auto *page = window->property("_folderPage").value<QObject *>();
        QVERIFY(page);
        QQuickItem *view = nullptr;
        QTRY_VERIFY((view = page->property("currentView").value<QQuickItem *>()));
        QTRY_COMPARE(view->objectName(),
                     viewMethod == 0 ? QString("FolderListView") : QString("FolderGridView"));
        QTRY_VERIFY(view->property("contentHeight").toReal() > view->height() + 600);
        view->setProperty("contentY", 500);
        QMimeData mime;
        mime.setUrls({QUrl::fromLocalFile(folders.filePath("0"))});
        const QPoint headerPoint(window->width() / 2, 20);
        QDragEnterEvent enter(headerPoint, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &enter);
        QVERIFY(enter.isAccepted());
        QDragMoveEvent move(headerPoint, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &move);
        QTRY_VERIFY(view->property("dragScrollActive").toBool());
        QVERIFY(view->property("dragPositionY").toReal() < 0);
        QTRY_VERIFY(view->property("contentY").toReal() < 480);
        QDragLeaveEvent leave;
        QCoreApplication::sendEvent(window, &leave);
        QTRY_VERIFY(!view->property("dragScrollActive").toBool());
        const qreal stopped = view->property("contentY").toReal();
        QTest::qWait(100);
        QCOMPARE(view->property("contentY").toReal(), stopped);

        const qreal dragEdge = view->property("dragScrollEdge").toReal();
        const qreal rubberBandEdge = view->property("rubberBandScrollEdge").toReal();
        QVERIFY(rubberBandEdge < dragEdge);
        view->setProperty("rubberBand", QVariant::fromValue(view));
        const qreal headerHeight = view->property("headerHeight").toReal();
        QVERIFY(QMetaObject::invokeMethod(
                view, "updateDragScrollPosition",
                Q_ARG(QVariant, headerHeight + (dragEdge + rubberBandEdge) / 2)));
        QCOMPARE(view->property("dragScrollDirection").toInt(), 0);
        QVERIFY(QMetaObject::invokeMethod(
                view, "updateDragScrollPosition",
                Q_ARG(QVariant, headerHeight + rubberBandEdge - 1)));
        QCOMPARE(view->property("dragScrollDirection").toInt(), -1);
        QVERIFY(QMetaObject::invokeMethod(view, "stopDragScroll"));
        view->setProperty("rubberBand", QVariant::fromValue<QQuickItem *>(nullptr));
    }

    void springLoadedNavigation()
    {
        QTemporaryDir folders;
        QVERIFY(folders.isValid());
        QDir dir(folders.path());
        QVERIFY(dir.mkdir("first"));
        QVERIFY(dir.mkdir("second"));

        QQmlApplicationEngine engine;
        CutefishFM::registerImageProviders(&engine);
        engine.rootContext()->setContextProperty("arg", folders.path());
        engine.load(QUrl("qrc:/qml/main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        window->resize(900, 600);

        auto *firstPage = window->property("_folderPage").value<QObject *>();
        QVERIFY(firstPage);
        auto *firstModel = qobject_cast<FolderModel *>(
                firstPage->property("model").value<QObject *>());
        QVERIFY(firstModel);
        const QString initialUrl = firstModel->url();
        firstModel->setUrl(QUrl::fromLocalFile(dir.filePath("first")).toString());
        QTRY_VERIFY(firstModel->canGoBack());

        QVERIFY(QMetaObject::invokeMethod(window, "openTab",
                                          Q_ARG(QVariant, dir.filePath("second"))));
        QCOMPARE(window->property("currentTab").toInt(), 1);

        auto *bar = window->findChild<QQuickItem *>("folderTabBar");
        QVERIFY(bar);
        QTRY_VERIFY(bar->width() > 0);
        std::function<QQuickItem *(QQuickItem *, const QString &)> findItem;
        findItem = [&](QQuickItem *parent, const QString &name) -> QQuickItem * {
            if (parent->objectName() == name)
                return parent;
            for (auto *child : parent->childItems()) {
                if (auto *found = findItem(child, name))
                    return found;
            }
            return nullptr;
        };
        QTRY_VERIFY(findItem(bar, "folderTab0"));
        auto *firstTab = findItem(bar, "folderTab0");
        QTRY_VERIFY(firstTab->width() > 0 && firstTab->height() > 0);
        const QPointF tabPoint = firstTab->mapToItem(
                bar, QPointF(firstTab->width() / 2, firstTab->height() / 2));
        QVariant tabIndex;
        QVERIFY(QMetaObject::invokeMethod(bar, "indexAtPosition",
                                          Q_RETURN_ARG(QVariant, tabIndex),
                                          Q_ARG(QVariant, tabPoint.x()),
                                          Q_ARG(QVariant, tabPoint.y())));
        QCOMPARE(tabIndex.toInt(), 0);

        QVERIFY(QMetaObject::invokeMethod(window, "updateDragHoverPosition",
                                          Q_ARG(QVariant, QVariant::fromValue(bar)),
                                          Q_ARG(QVariant, tabPoint.x()),
                                          Q_ARG(QVariant, tabPoint.y())));
        QCOMPARE(bar->property("springLoadIndex").toInt(), 0);
        QTest::qWait(300);
        QCOMPARE(window->property("currentTab").toInt(), 1);
        QTRY_COMPARE(window->property("currentTab").toInt(), 0);
        QVERIFY(QMetaObject::invokeMethod(window, "clearDragHoverTarget"));

        auto *backButton = window->findChild<QQuickItem *>("backButton");
        QVERIFY(backButton);
        QVERIFY(backButton->property("enabled").toBool());
        QVERIFY(QMetaObject::invokeMethod(window, "updateDragHoverPosition",
                                          Q_ARG(QVariant, QVariant::fromValue(backButton)),
                                          Q_ARG(QVariant, backButton->width() / 2),
                                          Q_ARG(QVariant, backButton->height() / 2)));
        QVERIFY(backButton->property("highlighted").toBool());
        QTRY_COMPARE(firstModel->url(), initialUrl);
        QVERIFY(QMetaObject::invokeMethod(window, "clearDragHoverTarget"));
        QVERIFY(!backButton->property("highlighted").toBool());
    }

    void navigationAndLifetime()
    {
        QTemporaryDir folders;
        QVERIFY(folders.isValid());
        QDir dir(folders.path());
        QVERIFY(dir.mkdir("first"));
        QVERIFY(dir.mkdir("second"));

        QQmlApplicationEngine engine;
        CutefishFM::registerImageProviders(&engine);
        engine.rootContext()->setContextProperty("arg", folders.path());
        engine.load(QUrl("qrc:/qml/main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        QObject *window = engine.rootObjects().first();
        QObject *menuBar = window->findChild<QObject *>("applicationMenuBar");
        QVERIFY(menuBar);
        QObject *sideBar = window->findChild<QObject *>("sideBar");
        QVERIFY(sideBar);
        QSignalSpy menuWindowChanges(menuBar, SIGNAL(windowChanged()));
        QVERIFY(menuWindowChanges.isValid());
        auto evaluate = [&](const QString &code) {
            QQmlExpression expression(engine.rootContext(), window, code);
            const QVariant result = expression.evaluate();
            if (expression.hasError())
                qWarning() << expression.error();
            return result;
        };
        auto activePage = [&]() {
            return window->property("_folderPage").value<QObject *>();
        };
        auto modelFor = [](QObject *page) {
            return qobject_cast<FolderModel *>(page->property("model").value<QObject *>());
        };

        QCOMPARE(evaluate("tabs.length").toInt(), 1);
        auto *bar = window->findChild<QQuickItem *>("folderTabBar");
        QVERIFY(bar);
        QVERIFY(!bar->isVisible());
        QCOMPARE(bar->height(), 0);
        QObject *firstPage = activePage();
        QVERIFY(firstPage);
        FolderModel *firstModel = modelFor(firstPage);
        QVERIFY(firstModel);
        QTRY_COMPARE(firstModel->rowCount(), 2);
        const QString initialUrl = firstModel->url();
        firstModel->setUrl(QUrl::fromLocalFile(dir.filePath("first")).toString());
        QTRY_VERIFY(firstModel->canGoBack());
        const QString firstUrl = firstModel->url();

        QVERIFY(QMetaObject::invokeMethod(window, "openTab",
                                          Q_ARG(QVariant, dir.filePath("second"))));
        QCOMPARE(evaluate("tabs.length").toInt(), 2);
        QCOMPARE(window->property("currentTab").toInt(), 1);
        QVERIFY(bar->isVisible());
        QCOMPARE(bar->height(), 38);
        QObject *secondPage = activePage();
        QVERIFY(secondPage != firstPage);
        QVERIFY(!firstPage->property("visible").toBool());
        QVERIFY(secondPage->property("visible").toBool());
        QCOMPARE(secondPage->property("tabTitle").toString(), QStringLiteral("second"));
        FolderModel *secondModel = modelFor(secondPage);
        QVERIFY(secondModel);
        const QString secondUrl = secondModel->url();

        window->setProperty("currentTab", 0);
        QCOMPARE(activePage(), firstPage);
        // Switching tabs has to move the sidebar selection with it.
        QCOMPARE(sideBar->property("selectedPath").toString(), firstUrl);
        QCOMPARE(firstModel->url(), firstUrl);
        QVERIFY(firstModel->canGoBack());
        firstModel->goBack();
        QTRY_COMPARE(firstModel->url(), initialUrl);
        QCOMPARE(secondModel->url(), secondUrl);

        QTRY_COMPARE(firstModel->rowCount(), 2);
        firstModel->selectAll();
        firstModel->prepareContextMenu();
        QVERIFY(firstModel->action("openInNewTab")->isVisible());
        firstModel->openInNewTab();
        QCOMPARE(evaluate("tabs.length").toInt(), 4);
        QCOMPARE(firstModel->selectionCount(), 2);

        QPointer<QObject> closing = activePage();
        QVERIFY(QMetaObject::invokeMethod(window, "closeTab", Q_ARG(QVariant, 3)));
        QTRY_VERIFY(closing.isNull());
        QCOMPARE(evaluate("tabs.length").toInt(), 3);
        evaluate("closeTab(2); closeTab(1)");
        QCOMPARE(activePage(), firstPage);
        QCOMPARE(evaluate("tabs.length").toInt(), 1);
        QVERIFY(!bar->isVisible());
        QCOMPARE(bar->height(), 0);

        auto *quickWindow = qobject_cast<QQuickWindow *>(window);
        QVERIFY(quickWindow);
        QTest::keyClick(quickWindow, Qt::Key_T, Qt::ControlModifier);
        QTRY_COMPARE(evaluate("tabs.length").toInt(), 2);
        QTest::keyClick(quickWindow, Qt::Key_Tab, Qt::ControlModifier);
        QTRY_COMPARE(window->property("currentTab").toInt(), 0);
        QCOMPARE(sideBar->property("selectedPath").toString(), initialUrl);
        QTest::keyClick(quickWindow, Qt::Key_W, Qt::ControlModifier);
        QTRY_COMPARE(evaluate("tabs.length").toInt(), 1);

        QVERIFY(QMetaObject::invokeMethod(window, "openTab",
                                          Q_ARG(QVariant, dir.filePath("second"))));
        QObject *draggedPage = activePage();
        std::function<QQuickItem *(QQuickItem *, const QString &)> findItem;
        findItem = [&](QQuickItem *parent, const QString &name) -> QQuickItem * {
            if (parent->objectName() == name)
                return parent;
            for (auto *child : parent->childItems()) {
                if (auto *found = findItem(child, name))
                    return found;
            }
            return nullptr;
        };
        QTRY_VERIFY(findItem(bar, "folderTab1"));
        auto *draggedTab = findItem(bar, "folderTab1");
        auto *targetTab = findItem(bar, "folderTab0");
        QVERIFY(targetTab);
        auto *closeButton = draggedTab->findChild<QQuickItem *>("tabCloseButton");
        QVERIFY(closeButton);
        QVERIFY(closeButton->x() > draggedTab->width() / 2);
        const QPoint from =
                draggedTab->mapToScene(QPointF(draggedTab->width() / 2, draggedTab->height() / 2))
                        .toPoint();
        const QPoint to =
                targetTab->mapToScene(QPointF(targetTab->width() / 2, targetTab->height() / 2))
                        .toPoint();
        QTest::mousePress(quickWindow, Qt::LeftButton, Qt::NoModifier, from);
        QTest::mouseMove(quickWindow, to, 30);
        QTRY_COMPARE(bar->property("dragIndex").toInt(), 1);
        QTest::mouseRelease(quickWindow, Qt::LeftButton, Qt::NoModifier, to);
        QTRY_COMPARE(window->property("currentTab").toInt(), 0);
        QCOMPARE(activePage(), draggedPage);
        QCOMPARE(evaluate("tabs.length").toInt(), 2);
        QCOMPARE(menuWindowChanges.count(), 0);
        QCOMPARE(window->findChildren<QObject *>("applicationMenuBar").count(), 1);
        if (qEnvironmentVariableIsSet("FILEMANAGER_TEST_SCREENSHOT")) {
            QTest::qWait(150);
            QVERIFY(quickWindow->grabWindow().save(
                    qEnvironmentVariable("FILEMANAGER_TEST_SCREENSHOT")));
        }
    }
};

int main(int argc, char **argv)
{
    QTemporaryDir config;
    qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
    QApplication app(argc, argv);
    app.setOrganizationName("cutefish-tests");
    app.setApplicationName("tabs");
    CutefishFM::registerQmlTypes();
    CutefishFM::initResources();
    TabsTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_tabs.moc"
