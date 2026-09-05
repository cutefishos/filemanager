#include "qmltypes.h"
#include "model/foldermodel.h"

#include <QApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlExpression>
#include <QTemporaryDir>
#include <QtTest>
#include <functional>

class TabsTest : public QObject
{
    Q_OBJECT

private slots:
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

        QVERIFY(QMetaObject::invokeMethod(window, "openTab", Q_ARG(QVariant, dir.filePath("second"))));
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
        QTest::keyClick(quickWindow, Qt::Key_W, Qt::ControlModifier);
        QTRY_COMPARE(evaluate("tabs.length").toInt(), 1);

        QVERIFY(QMetaObject::invokeMethod(window, "openTab", Q_ARG(QVariant, dir.filePath("second"))));
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
        const QPoint from = draggedTab->mapToScene(QPointF(draggedTab->width() / 2, draggedTab->height() / 2)).toPoint();
        const QPoint to = targetTab->mapToScene(QPointF(targetTab->width() / 2, targetTab->height() / 2)).toPoint();
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
            // The software renderer cannot render FishUI's rounded-window shader mask.
            window->setProperty("windowRadius", 0);
            QTest::qWait(150);
            QVERIFY(quickWindow->grabWindow().save(qEnvironmentVariable("FILEMANAGER_TEST_SCREENSHOT")));
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
