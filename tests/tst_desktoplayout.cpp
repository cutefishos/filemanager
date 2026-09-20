#include "model/foldermodel.h"
#include "model/positioner.h"
#include "qmltypes.h"

#include <QApplication>
#include <QFile>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExpression>
#include <QQmlPropertyMap>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>

class DesktopLayoutTest : public QObject
{
    Q_OBJECT

private slots:

    void desktopGeometry()
    {
        QQmlEngine engine;
        CutefishFM::registerImageProviders(&engine);
        QQmlPropertyMap desktopView;
        desktopView.insert("leftMargin", 0);
        desktopView.insert("topMargin", 28);
        desktopView.insert("rightMargin", 0);
        desktopView.insert("bottomMargin", 64);
        engine.rootContext()->setContextProperty("desktopView", &desktopView);
        QQmlComponent component(&engine, QUrl("qrc:/qml/Desktop/Main.qml"));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> object(
                component.createWithInitialProperties({{"width", 1920}, {"height", 1080}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *root = qobject_cast<QQuickItem *>(object.data());
        QVERIFY(root);
        QQuickWindow window;
        window.resize(1920, 1080);
        root->setParentItem(window.contentItem());
        window.show();
        auto *grid = root->findChild<QQuickItem *>("FolderGridView");
        QVERIFY(grid);
        QTest::qWait(200);
        const qreal cellWidth = grid->property("cellWidth").toReal();
        const qreal cellHeight = grid->property("cellHeight").toReal();
        QCOMPARE(grid->x() + grid->width(), 1908.0);
        QCOMPARE(grid->y(), 40.0);
        QCOMPARE(grid->width(), grid->property("gridColumns").toInt() * cellWidth);
        QCOMPARE(grid->height(), grid->property("gridRows").toInt() * cellHeight);

        root->setWidth(1366);
        root->setHeight(768);
        QTest::qWait(200);
        QCOMPARE(grid->x() + grid->width(), 1354.0);
        QCOMPARE(grid->property("cellWidth").toReal(), cellWidth);
        QCOMPARE(grid->property("cellHeight").toReal(), cellHeight);
        QVERIFY(grid->y() + grid->height() <= 768 - 64 - 12);
        QQmlExpression expression(qmlContext(grid), grid,
                                  "cellAt(width - cellWidth / 2, cellHeight / 2)");
        QCOMPARE(expression.evaluate().toPointF(),
                 QPointF(grid->property("gridColumns").toInt() - 1, 0));
        QVERIFY(!expression.hasError());
        desktopView.insert("rightMargin", 80);
        desktopView.insert("topMargin", 36);
        desktopView.insert("bottomMargin", 0);
        QTRY_COMPARE(grid->x() + grid->width(), 1366.0 - 80 - 12);
        QTRY_COMPARE(grid->y(), 48.0);
        QCOMPARE(grid->property("availableHeight").toReal(), 768.0 - 36 - 24);
        desktopView.insert("leftMargin", 72);
        QCOMPARE(grid->property("availableWidth").toReal(), 1366.0 - 72 - 80 - 24);
        desktopView.insert("leftMargin", 0);
        desktopView.insert("rightMargin", 0);
        desktopView.insert("topMargin", 28);
        desktopView.insert("bottomMargin", 64);
        const int iconSize = grid->property("iconSize").toInt();
        grid->forceActiveFocus();
        QTest::keyClick(&window, Qt::Key_Minus, Qt::ControlModifier);
        QCOMPARE(grid->property("iconSize").toInt(), iconSize - 8);
        QTest::qWait(200);
        QTest::keyClick(&window, Qt::Key_Plus, Qt::ControlModifier);
        QCOMPARE(grid->property("iconSize").toInt(), iconSize);
        QCOMPARE(grid->property("cellWidth").toReal(), cellWidth);
        QCOMPARE(grid->property("cellHeight").toReal(), cellHeight);
        QTest::keyClick(&window, Qt::Key_Minus, Qt::ControlModifier);
        QTest::keyClick(&window, Qt::Key_Equal, Qt::ControlModifier);
        QCOMPARE(grid->property("iconSize").toInt(), iconSize);

        QTest::mouseMove(&window, QPoint(5, 760));
        QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, QPoint(5, 760));
        QTest::mouseMove(&window, QPoint(6, 759));
        QVERIFY(!grid->property("rubberBand").value<QQuickItem *>());
        QVERIFY(grid->property("pressActive").toBool());
        QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, QPoint(6, 759));

        QTest::mouseMove(&window, QPoint(5, 760));
        QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, QPoint(5, 760));
        QTest::mouseMove(&window, QPoint(10, 750));
        QTest::mouseMove(&window, QPoint(1300, 5));
        auto *rubberBand = grid->property("rubberBand").value<QQuickItem *>();
        QVERIFY(rubberBand);
        const QRectF selection = rubberBand->mapRectToItem(root, rubberBand->boundingRect());
        QCOMPARE(selection, QRectF(5, 5, 1295, 755));
        QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, QPoint(1300, 5));
        QVERIFY(!grid->property("pressActive").toBool());

        QSignalSpy contextMenuSpy(grid, SIGNAL(contextMenuRequested()));
        QTest::mouseClick(&window, Qt::RightButton, Qt::NoModifier, QPoint(5, 760));
        QCOMPARE(contextMenuSpy.count(), 1);
        object.reset();
    }

    void resizeAndRestore()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QStringList urls;
        for (const QString &name :
             {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")}) {
            QFile file(directory.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            urls.append(QUrl::fromLocalFile(file.fileName()).toString());
        }

        FolderModel folder;
        Positioner positioner;
        positioner.setPerStripe(8);
        positioner.setStripes(12);
        positioner.setFolderModel(&folder);
        positioner.setEnabled(true);
        folder.setUrl(QUrl::fromLocalFile(directory.path()).toString());
        QTRY_COMPARE(folder.rowCount(), 3);
        QTRY_VERIFY(folder.status() != FolderModel::Listing);
        QTRY_VERIFY(positioner.positions().size() >= 5);

        const QStringList original = {"12", "8", urls[0], "11", "0", urls[1],
                                      "10", "2", urls[2], "3",  "7"};
        positioner.setPositions(original);
        QTest::qWait(20);
        const QStringList saved = positioner.positions();
        auto cell = [&](int item) {
            return positioner.indexForUrl(QUrl(urls[item]));
        };
        QCOMPARE(cell(0), 11 * 8);

        positioner.setStripes(16);
        positioner.setPerStripe(10);
        QTRY_COMPARE(cell(0), 15 * 10);
        QCOMPARE(cell(1), 14 * 10 + 2);
        QCOMPARE(cell(2), 7 * 10 + 7);
        QTest::qWait(20);
        QCOMPARE(positioner.positions(), saved);

        positioner.setStripes(4);
        positioner.setPerStripe(3);
        QTRY_COMPARE(cell(0), 3 * 3);
        QCOMPARE(cell(1), 2 * 3 + 2);
        QVERIFY(cell(2) >= 0 && cell(2) < 12);
        QVERIFY(cell(0) != cell(2));
        QVERIFY(cell(1) != cell(2));

        QTest::qWait(20);
        QCOMPARE(positioner.positions(), saved);
        positioner.setStripes(12);
        positioner.setPerStripe(8);
        QCOMPARE(cell(0), 11 * 8);
        QCOMPARE(cell(1), 10 * 8 + 2);
        QCOMPARE(cell(2), 3 * 8 + 7);

        // A desktop with an empty rightmost column still stores the full width.
        positioner.move({cell(0), 9 * 8});
        QTRY_COMPARE(positioner.positions().first(), QStringLiteral("12"));
        QTest::qWait(10);
        positioner.setStripes(14);
        QCOMPARE(cell(0), 11 * 8);
        QCOMPARE(cell(1), 12 * 8 + 2);
    }

    // A new folder appears straight on the right-clicked cell, and a renamed
    // file comes back on the cell it was on -- neither passes through the
    // default cell first.
    void cellsSurviveCreateAndRename()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile file(directory.filePath("a"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();

        FolderModel folder;
        folder.setIsDesktop(true);
        folder.setSortMode(-1);
        Positioner positioner;
        positioner.setPerStripe(8);
        positioner.setStripes(12);
        positioner.setFolderModel(&folder);
        positioner.setEnabled(true);
        folder.setUrl(QUrl::fromLocalFile(directory.path()).toString());
        QTRY_COMPARE(folder.rowCount(), 1);
        QTRY_VERIFY(folder.status() != FolderModel::Listing);
        QTRY_VERIFY(positioner.positions().size() >= 5);

        const QUrl a = QUrl::fromLocalFile(directory.filePath("a"));
        QVERIFY(positioner.indexForUrl(a) >= 8);

        // Every cell the row passes through is visible to the view, so record
        // where the file sits each time the view is told something changed.
        QUrl watched;
        QSet<int> seen;
        auto record = [&] {
            const int cell = positioner.indexForUrl(watched);
            if (cell >= 0)
                seen.insert(cell);
        };
        connect(&positioner, &QAbstractItemModel::rowsInserted, this, record);
        connect(&positioner, &QAbstractItemModel::dataChanged, this, record);
        connect(&positioner, &QAbstractItemModel::modelReset, this, record);

        watched = QUrl::fromLocalFile(directory.filePath(folder.tr("New Folder")));
        folder.setNewDocumentPosition(1, 1, 2);
        folder.newFolder();
        QTRY_COMPARE(folder.rowCount(), 2);
        QTRY_COMPARE(positioner.indexForUrl(watched), 2);
        QCOMPARE(seen, QSet<int>{2});
        const QUrl created = watched;

        positioner.move({positioner.indexForUrl(a), 1});
        QCOMPARE(positioner.indexForUrl(a), 1);
        QTest::qWait(20);

        // The rename is an in-place update: the delegate stays, so the icon
        // cannot blink out between the old row leaving and the new one arriving.
        QSignalSpy removed(&positioner, &QAbstractItemModel::rowsRemoved);
        QSignalSpy inserted(&positioner, &QAbstractItemModel::rowsInserted);
        watched = QUrl::fromLocalFile(directory.filePath("b"));
        seen.clear();
        folder.rename(folder.indexForUrl(a).row(), "b");
        QTRY_VERIFY(positioner.indexForUrl(watched) >= 0);
        QCOMPARE(positioner.indexForUrl(watched), 1);
        QCOMPARE(seen, QSet<int>{1});
        QCOMPARE(removed.count(), 0);
        QCOMPARE(inserted.count(), 0);
        QCOMPARE(folder.rowCount(), 2);
        QVERIFY(folder.indexForUrl(watched).isValid());
        QVERIFY(!folder.indexForUrl(a).isValid());
        QVERIFY(positioner.indexForUrl(a) < 0);
        QCOMPARE(positioner.indexForUrl(created), 2);
    }
};

int main(int argc, char **argv)
{
    QTemporaryDir config;
    qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
    QApplication app(argc, argv);
    CutefishFM::registerQmlTypes();
    CutefishFM::initResources();
    DesktopLayoutTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_desktoplayout.moc"
