#include "qmltypes.h"
#include "windowmanager.h"

#include <QApplication>
#include <QDir>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>

#include <cmath>

// The icon grid while its window is resized: the columns share the width
// evenly and follow it smoothly, and a change of column count keeps the row
// at the top of the view.
class GridResizeTest : public QObject
{
    Q_OBJECT

private:
    struct Grid {
        QQmlEngine engine;
        WindowManager windows {&engine};
        QQuickWindow *window = nullptr;
        QQuickItem *view = nullptr;
    };

    bool open(Grid &grid, const QTemporaryDir &folders, int width)
    {
        grid.windows.installIncubationController();
        grid.window = grid.windows.create(QUrl("qrc:/qml/main.qml"), {{"startPath", folders.path()}});
        if (!grid.window)
            return false;
        grid.window->resize(width, 600);
        grid.window->property("settings").value<QObject *>()->setProperty("viewMethod", 1);
        auto *page = grid.window->property("_folderPage").value<QObject *>();
        if (!page)
            return false;
        return QTest::qWaitFor([&] {
            grid.view = page->property("currentView").value<QQuickItem *>();
            return grid.view && grid.view->objectName() == "FolderGridView"
                    && grid.view->property("count").toInt() > 0 && item(grid, 0);
        });
    }

    static QQuickItem *item(Grid &grid, int index)
    {
        QQuickItem *found = nullptr;
        QMetaObject::invokeMethod(grid.view, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, found),
                                  Q_ARG(int, index));
        return found;
    }

    // Resizes the window and lays the grid out at once, before any animation
    // has had a frame.
    static void resize(Grid &grid, int width)
    {
        grid.window->resize(width, 600);
        // The window takes the size at once, its content with the resize event.
        QTRY_COMPARE(grid.window->contentItem()->width(), qreal(width));
        QMetaObject::invokeMethod(grid.view, "forceLayout");
    }

    static int columns(Grid &grid) { return grid.view->property("gridColumns").toInt(); }

    static QPointF drawnAt(QQuickItem *item)
    {
        return item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
    }

    static QPointF laidAt(Grid &grid, QQuickItem *item)
    {
        return grid.view->property("contentItem").value<QQuickItem *>()->mapToScene(
                QPointF(item->x() + item->width() / 2, item->y() + item->height() / 2));
    }

    static void makeFolders(const QTemporaryDir &folders, int count)
    {
        for (int i = 0; i < count; ++i)
            QVERIFY(QDir(folders.path()).mkdir(QString("%1").arg(i, 3, 10, QChar('0'))));
    }

private slots:

    void columnsFollowTheWidthSmoothly()
    {
        QTemporaryDir folders;
        makeFolders(folders, 40);
        Grid grid;
        QVERIFY(open(grid, folders, 900));

        int previousColumns = 0;
        qreal previousX = 0;
        for (int width = 900; width >= 700; --width) {
            resize(grid, width);
            const int count = columns(grid);
            const qreal cell = grid.view->property("cellWidth").toReal();
            const qreal available = grid.view->width() - grid.view->property("leftMargin").toReal()
                    - grid.view->property("rightMargin").toReal();
            // Every column that fits is laid out, and together they fill the width.
            QVERIFY2(count * cell <= available + 1e-9, qPrintable(QString::number(width)));
            QVERIFY2(available - count * cell < 1, qPrintable(QString::number(width)));

            // The last column moves with the edge a pixel at a time, not several
            // pixels every few pixels.
            QQuickItem *last = item(grid, count - 1);
            QVERIFY(last);
            if (count == previousColumns)
                QVERIFY2(std::abs(last->x() - previousX) <= 1.0, qPrintable(QString::number(width)));
            previousColumns = count;
            previousX = last->x();
        }
    }

    void aNewColumnCountPutsTheItemsStraightIntoTheirCells()
    {
        QTemporaryDir folders;
        makeFolders(folders, 40);
        Grid grid;
        QVERIFY(open(grid, folders, 900));

        const int before = columns(grid);
        // The last item of the first row goes down to the second.
        QQuickItem *moving = item(grid, before - 1);
        QVERIFY(moving);
        int width = 900;
        QPointF drawn;
        while (columns(grid) == before && width > 400) {
            drawn = drawnAt(moving);
            resize(grid, --width);
        }
        QVERIFY(columns(grid) < before);
        QCOMPARE(item(grid, before - 1), moving);

        QVERIFY(std::abs(laidAt(grid, moving).y() - drawn.y()) > 50);
        QVERIFY(std::abs(drawnAt(moving).x() - laidAt(grid, moving).x()) <= 1);
        QVERIFY(std::abs(drawnAt(moving).y() - laidAt(grid, moving).y()) <= 1);
    }

    void theTopRowStaysAtTheTopWhenTheColumnsChange()
    {
        QTemporaryDir folders;
        makeFolders(folders, 300);
        Grid grid;
        QVERIFY(open(grid, folders, 900));
        QTRY_VERIFY(grid.view->property("contentHeight").toReal() > 3000);

        const qreal rowHeight = grid.view->property("cellHeight").toReal();
        const qreal header = grid.view->property("headerHeight").toReal();
        const int before = columns(grid);
        grid.view->setProperty("contentY", 10 * rowHeight - header + 7);
        QMetaObject::invokeMethod(grid.view, "forceLayout");
        const int top = 10 * before;

        int width = 900;
        while (columns(grid) == before && width > 400)
            resize(grid, --width);
        QVERIFY(columns(grid) < before);

        const qreal line = grid.view->property("contentY").toReal() + header;
        QCOMPARE(int(std::floor(line / rowHeight)), top / columns(grid));
        QVERIFY(std::abs((line - std::floor(line / rowHeight) * rowHeight) - 7) < 0.5);
    }
};

int main(int argc, char **argv)
{
    QTemporaryDir config;
    qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
    QApplication app(argc, argv);
    app.setOrganizationName("cutefish-tests");
    app.setApplicationName("gridresize");
    CutefishFM::registerQmlTypes();
    CutefishFM::initResources();
    GridResizeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_gridresize.moc"
