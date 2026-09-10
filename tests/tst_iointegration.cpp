#include "model/foldermodel.h"
#include "qmltypes.h"
#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <task.h>

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
