#include "draganddrop/declarativedragdropevent.h"
#include "draganddrop/declarativedroparea.h"
#include "model/foldermodel.h"
#include "qmltypes.h"
#include <QApplication>
#include <QClipboard>
#include <QDropEvent>
#include <QFile>
#include <QMimeData>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <task.h>

class TestDropArea : public DeclarativeDropArea
{
public:
    using DeclarativeDropArea::dragEnterEvent;
    using DeclarativeDropArea::dragMoveEvent;
    using DeclarativeDropArea::dropEvent;
};

class IOIntegrationTest : public QObject
{
    Q_OBJECT
    QTemporaryDir m_data;
private slots:

    void initTestCase()
    {
        QVERIFY(m_data.isValid());
        qputenv("XDG_DATA_HOME", m_data.path().toUtf8());
        CutefishFM::registerQmlTypes();
        CutefishFM::initResources();
    }

    void folderOperations()
    {
        QTemporaryDir dir;
        QFile file(dir.filePath("original.txt"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("payload"), 7);
        file.close();
        FolderModel folder;
        folder.setUrl(QUrl::fromLocalFile(dir.path()).toString());
        QTRY_COMPARE(folder.count(), 1);
        QCOMPARE(folder.data(folder.index(0, 0), FolderModel::FileNameRole).toString(),
                 QString("original.txt"));
        folder.setSelected(0);
        folder.copy();
        folder.paste();
        QTRY_COMPARE(folder.count(), 2);
        QVERIFY(QFileInfo::exists(dir.filePath("original (1).txt")));
        const int original = folder.indexForKeyboardSearch("original.txt", 0);
        QVERIFY(original >= 0);
        folder.rename(original, "renamed.txt");
        QTRY_VERIFY(QFileInfo::exists(dir.filePath("renamed.txt")));
        QTRY_VERIFY(folder.indexForKeyboardSearch("renamed.txt", 0) >= 0);
        folder.clearSelection();
        folder.setSelected(folder.indexForKeyboardSearch("renamed.txt", 0));
        folder.moveSelectedToTrash();
        QTRY_COMPARE(folder.count(), 1);
        QVERIFY(!QFileInfo::exists(dir.filePath("renamed.txt")));
        FolderModel trash;
        trash.setUrl("trash:/");
        QTRY_COMPARE(trash.count(), 1);
        trash.setSelected(0);
        trash.restoreFromTrash();
        QTRY_COMPARE(trash.count(), 0);
        QTRY_COMPARE(folder.count(), 2);
        folder.newFolder();
        QTRY_COMPARE(folder.count(), 3);
        folder.clearSelection();
        folder.selectAll();
        folder.moveSelectedToTrash();
        QTRY_COMPARE(folder.count(), 0);
        QTRY_COMPARE(trash.count(), 3);
        trash.selectAll();
        trash.deleteSelected();
        QTRY_COMPARE(trash.count(), 0);
        QApplication::clipboard()->clear();
    }

    void acceptedDropAction_data()
    {
        QTest::addColumn<int>("action");
        QTest::addColumn<bool>("isDesktop");
        QTest::newRow("move-to-desktop") << int(Qt::MoveAction) << true;
        QTest::newRow("move-to-window") << int(Qt::MoveAction) << false;
        QTest::newRow("ctrl-copy-to-desktop") << int(Qt::CopyAction) << true;
        QTest::newRow("ctrl-copy-to-window") << int(Qt::CopyAction) << false;
    }

    void acceptedDropAction()
    {
        QFETCH(int, action);
        QFETCH(bool, isDesktop);
        QTemporaryDir source;
        QTemporaryDir destination;
        QVERIFY(source.isValid());
        QVERIFY(destination.isValid());
        QVERIFY(QDir(source.path()).mkdir("folder"));
        FolderModel desktop;
        desktop.setIsDesktop(isDesktop);
        desktop.setUrl(QUrl::fromLocalFile(destination.path()).toString());
        QTRY_VERIFY(desktop.status() != FolderModel::Listing);

        QMimeData mimeData;
        mimeData.setUrls({QUrl::fromLocalFile(source.filePath("folder"))});
        const auto modifiers = action == Qt::CopyAction ? Qt::ControlModifier : Qt::NoModifier;
        TestDropArea area;
        QDragEnterEvent enter(QPoint(), Qt::CopyAction | Qt::MoveAction, &mimeData, Qt::LeftButton,
                              modifiers);
        area.dragEnterEvent(&enter);
        QCOMPARE(enter.dropAction(), Qt::DropAction(action));
        QDragMoveEvent move(QPoint(1, 1), Qt::CopyAction | Qt::MoveAction, &mimeData,
                            Qt::LeftButton, modifiers);
        area.dragMoveEvent(&move);
        QCOMPARE(move.dropAction(), Qt::DropAction(action));
        QDropEvent drop(QPointF(240, 360), Qt::CopyAction | Qt::MoveAction, &mimeData,
                        Qt::LeftButton, modifiers);
        connect(&area, &DeclarativeDropArea::drop, &desktop, [&](DeclarativeDragDropEvent *event) {
            desktop.drop(nullptr, event, -1);
        });
        QSignalSpy finished(Cutefish::IO::TaskController::instance(),
                            &Cutefish::IO::TaskController::taskFinished);
        QSignalSpy positioned(&desktop, &FolderModel::move);
        area.dropEvent(&drop);
        QCOMPARE(drop.dropAction(), Qt::DropAction(action));
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(QFileInfo(destination.filePath("folder")).isDir());
        QCOMPARE(QFileInfo::exists(source.filePath("folder")), action == Qt::CopyAction);
        if (isDesktop) {
            QTRY_COMPARE(positioned.count(), 1);
            QCOMPARE(positioned.first().at(0).toInt(), 240);
            QCOMPARE(positioned.first().at(1).toInt(), 360);
            QCOMPARE(positioned.first().at(2).value<QList<QUrl>>(),
                     QList<QUrl> {QUrl::fromLocalFile(destination.filePath("folder"))});
        } else {
            QCOMPARE(positioned.count(), 0);
        }
    }

    void copyThenNavigateAndPaste()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(QDir(directory.path()).mkdir("Documents"));
        QFile file(directory.filePath("item.txt"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("payload");
        file.close();
        FolderModel folder;
        folder.setUrl(QUrl::fromLocalFile(directory.path()).toString());
        QTRY_COMPARE(folder.count(), 2);
        QQmlEngine engine;
        QQmlComponent menuComponent(&engine, QUrl("qrc:/qml/FolderContextMenu.qml"));
        QVERIFY2(menuComponent.isReady(), qPrintable(menuComponent.errorString()));
        QScopedPointer<QObject> menu(menuComponent.createWithInitialProperties(
                {{"folderModel", QVariant::fromValue(&folder)}}));
        QVERIFY2(menu, qPrintable(menuComponent.errorString()));
        folder.setSelected(folder.indexForKeyboardSearch("item.txt", 0));
        QVERIFY(menu->property("hasSelection").toBool());
        folder.copy();
        QVERIFY(folder.action("paste")->isEnabled());
        folder.setUrl(QUrl::fromLocalFile(directory.filePath("Documents")).toString());
        QTRY_COMPARE(folder.status(), FolderModel::Ready);
        QCOMPARE(folder.selectionCount(), 0);
        QVERIFY(!menu->property("hasSelection").toBool());
        QVERIFY(folder.action("paste")->isVisible());
        QVERIFY(folder.action("paste")->isEnabled());
        folder.action("paste")->trigger();
        QTRY_VERIFY(QFileInfo::exists(directory.filePath("Documents/item.txt")));
        QVERIFY(QFileInfo::exists(directory.filePath("item.txt")));
        QApplication::clipboard()->clear();
        QVERIFY(!folder.action("paste")->isEnabled());
    }

    void dropBackOntoSource()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkdir("folder"));
        FolderModel folder;
        folder.setUrl(QUrl::fromLocalFile(dir.path()).toString());
        QTRY_COMPARE(folder.count(), 1);

        QMimeData mimeData;
        mimeData.setUrls({QUrl::fromLocalFile(dir.filePath("folder"))});
        QObject event;
        event.setProperty("mimeData", QVariant::fromValue<QObject *>(&mimeData));
        event.setProperty("proposedAction", int(Qt::MoveAction));
        QSignalSpy tasks(Cutefish::IO::TaskController::instance(),
                         &Cutefish::IO::TaskController::taskAdded);
        QVERIFY(tasks.isValid());

        folder.drop(nullptr, &event, 0);
        QCOMPARE(tasks.count(), 0);
        folder.drop(nullptr, &event, -1);
        QCOMPARE(tasks.count(), 0);
        mimeData.setUrls({QUrl::fromLocalFile(dir.filePath("folder/"))});
        folder.drop(nullptr, &event, 0);
        QCOMPARE(tasks.count(), 0);
        QVERIFY(QFileInfo(dir.filePath("folder")).isDir());
    }

    void progressDialogPresentation()
    {
        Cutefish::IO::Task task({Cutefish::IO::Operation::Copy,
                                 {QUrl::fromLocalFile("/example/Photos")},
                                 QUrl::fromLocalFile("/example/Documents")});
        QQmlEngine engine;
        CutefishFM::registerImageProviders(&engine);
        engine.rootContext()->setContextProperty("ioTask", &task);
        QQmlComponent component(&engine, QUrl("qrc:/qml/Dialogs/IoTaskDialog.qml"));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> dialog(component.create());
        QVERIFY2(dialog, qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(dialog.data());
        QVERIFY(window);
        auto *bar = dialog->findChild<QObject *>("ioProgressBar");
        auto *cancel = dialog->findChild<QObject *>("ioCancelButton");
        QVERIFY(bar);
        QVERIFY(cancel);
        QVERIFY(bar->property("indeterminate").toBool());
        Cutefish::IO::TaskProgress progress;
        progress.preparing = false;
        progress.totalsKnown = true;
        progress.totalBytes = 96 * 1024 * 1024;
        progress.processedBytes = 32 * 1024 * 1024;
        progress.totalItems = 72;
        progress.processedItems = 24;
        progress.elapsedMs = 2000;
        progress.currentPath = "/example/Photos/DSC_2048.jpg";
        QVERIFY(dialog->setProperty("info", QVariant::fromValue(progress)));
        QVERIFY(!bar->property("indeterminate").toBool());
        QVERIFY(qAbs(bar->property("value").toDouble() - 1.0 / 3.0) < 0.001);
        QCOMPARE(dialog->property("currentName").toString(), QString("DSC_2048.jpg"));
        QVERIFY(dialog->property("summary").toString().contains(
                task.formattedSize(progress.totalBytes)));
        QVERIFY(dialog->property("summary").toString().contains("remaining"));
        QVERIFY(dialog->property("heading").toString().contains("Photos"));
        QVERIFY(dialog->property("heading").toString().contains("Documents"));
        QCOMPARE(window->minimumWidth(), window->maximumWidth());
        QCOMPARE(window->minimumHeight(), window->maximumHeight());
        QVERIFY(QMetaObject::invokeMethod(dialog.data(), "toggleMaximized"));
        QVERIFY(window->visibility() != QWindow::Maximized);
        window->show();
        window->showMaximized();
        QTRY_VERIFY(window->visibility() != QWindow::Maximized);
        QVERIFY(QMetaObject::invokeMethod(cancel, "clicked"));
        QVERIFY(dialog->property("canceling").toBool());
        QVERIFY(!cancel->property("enabled").toBool());
        QCOMPARE(dialog->property("summary").toString(), QString("Stopping…"));
        QSignalSpy done(&task, &Cutefish::IO::Task::result);
        task.start();
        QTRY_COMPARE(done.size(), 1);
        QVERIFY(task.wasCanceled());
    }

    void jobDialogLoadsAndHandlesConflict()
    {
        QTemporaryDir dir;
        QFile source(dir.filePath("source"));
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("new");
        source.close();
        QFile target(dir.filePath("target"));
        QVERIFY(target.open(QIODevice::WriteOnly));
        target.write("old");
        target.close();
        Cutefish::IO::Task task({Cutefish::IO::Operation::Rename,
                                 {QUrl::fromLocalFile(source.fileName())},
                                 QUrl::fromLocalFile(target.fileName()),
                                 Cutefish::IO::ConflictPolicy::Ask});
        QQmlEngine engine;
        CutefishFM::registerImageProviders(&engine);
        engine.rootContext()->setContextProperty("ioTask", &task);
        QQmlComponent component(&engine, QUrl("qrc:/qml/Dialogs/IoTaskDialog.qml"));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> dialog(component.create());
        QVERIFY2(dialog, qPrintable(component.errorString()));
        task.start();
        QTRY_VERIFY(dialog->property("conflict").toBool());
        task.resolveConflict(int(Cutefish::IO::ConflictPolicy::Skip));
        QTRY_VERIFY(dialog->property("finished").toBool());
        QVERIFY(QFileInfo::exists(source.fileName()));
    }
};
QTEST_MAIN(IOIntegrationTest)
#include "tst_iointegration.moc"
