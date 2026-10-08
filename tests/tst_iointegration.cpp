#include "cio/archivejob.h"
#include "draganddrop/declarativedragdropevent.h"
#include "draganddrop/declarativedroparea.h"
#include "helper/dialogs.h"
#include "helper/filelauncher.h"
#include "model/foldermodel.h"
#include "operations/fileoperationsclient.h"
#include "operations/operationtask.h"
#include "qmltypes.h"
#include "windowmanager.h"
#include <QApplication>
#include <QClipboard>
#include <QDropEvent>
#include <QFile>
#include <QMimeData>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>
#include <task.h>

#include <archive.h>
#include <archive_entry.h>
#include <sys/stat.h>

class TestDropArea : public DeclarativeDropArea
{
public:
    using DeclarativeDropArea::dragEnterEvent;
    using DeclarativeDropArea::dragMoveEvent;
    using DeclarativeDropArea::dropEvent;
};

struct FixtureEntry {
    QByteArray path;
    mode_t type;
    // The data of a file, or the target of a link.
    QByteArray contents;
    mode_t permissions = 0644;
    bool hardLink = false;
};

static bool writeArchive(const QString &path, int format, int filter,
                         const QList<FixtureEntry> &entries, const char *options = nullptr)
{
    struct archive *writer = archive_write_new();
    auto freeWriter = qScopeGuard([writer] { archive_write_free(writer); });
    if (archive_write_set_format(writer, format) != ARCHIVE_OK
        || (filter != ARCHIVE_FILTER_NONE && archive_write_add_filter(writer, filter) != ARCHIVE_OK))
        return false;

    if (options && (archive_write_set_options(writer, options) != ARCHIVE_OK
                    || archive_write_set_passphrase(writer, "secret") != ARCHIVE_OK))
        return false;

    if (archive_write_open_filename(writer, QFile::encodeName(path).constData()) != ARCHIVE_OK)
        return false;

    for (const FixtureEntry &fixture : entries) {
        struct archive_entry *entry = archive_entry_new();
        archive_entry_set_pathname(entry, fixture.path.constData());
        archive_entry_set_filetype(entry, fixture.type);
        archive_entry_set_perm(entry, fixture.permissions);
        archive_entry_set_mtime(entry, 1700000000, 0);
        if (fixture.hardLink)
            archive_entry_set_hardlink(entry, fixture.contents.constData());
        else if (fixture.type == AE_IFLNK)
            archive_entry_set_symlink(entry, fixture.contents.constData());
        else if (fixture.type == AE_IFREG)
            archive_entry_set_size(entry, fixture.contents.size());

        const bool written = archive_write_header(writer, entry) == ARCHIVE_OK
                && (fixture.type != AE_IFREG || fixture.hardLink
                    || archive_write_data(writer, fixture.contents.constData(), fixture.contents.size())
                            == fixture.contents.size());
        archive_entry_free(entry);
        if (!written)
            return false;
    }

    return archive_write_close(writer) == ARCHIVE_OK;
}

static bool runArchiveJob(ArchiveJob::Operation operation, const QStringList &inputs,
                          const QString &outputPath, QString *error)
{
    ArchiveJob job(operation, inputs, outputPath);
    QSignalSpy completed(&job, &ArchiveJob::completed);
    job.start();
    job.wait();
    if (completed.size() != 1) {
        *error = QStringLiteral("The job did not complete");
        return false;
    }

    *error = completed.first().at(2).toString();
    return completed.first().at(0).toBool();
}

static QString readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

static QStringList extractionLeftovers(const QString &directory)
{
    return QDir(directory).entryList({QStringLiteral(".cutefish-extract-*")},
                                     QDir::AllEntries | QDir::Hidden | QDir::System);
}

class IOIntegrationTest : public QObject
{
    Q_OBJECT
    QTemporaryDir m_data;
private slots:

    void initTestCase()
    {
        QVERIFY(m_data.isValid());
        qputenv("XDG_DATA_HOME", m_data.path().toUtf8());
        // Standard folders are read once per process: before a test points
        // XDG_CONFIG_HOME away from the user-dirs.dirs dropEligibility compares with.
        Cutefish::IO::standardFolderIconName(QDir::homePath());
        CutefishFM::registerQmlTypes();
        CutefishFM::initResources();
    }

    void desktopFoldersUseCutefish()
    {
        QTemporaryDir directory;
        QTemporaryDir config;
        QVERIFY(directory.isValid());
        QVERIFY(config.isValid());
        QVERIFY(QDir(directory.path()).mkdir("folder with spaces"));

        QVERIFY(QDir().mkpath(m_data.filePath("applications")));
        QFile other(m_data.filePath("applications/other-manager.desktop"));
        QVERIFY(other.open(QIODevice::WriteOnly));
        other.write("[Desktop Entry]\nType=Application\nName=Other Manager\n"
                    "Exec=/bin/false\nMimeType=inode/directory;\n");
        other.close();
        QFile defaults(config.filePath("mimeapps.list"));
        QVERIFY(defaults.open(QIODevice::WriteOnly));
        defaults.write("[Default Applications]\ninode/directory=other-manager.desktop;\n");
        defaults.close();

        QStringList opened;
        CutefishFM::setFolderOpener([&](const QStringList &paths, const QStringList &) {
            opened += paths;
        });

        const QByteArray originalConfig = qgetenv("XDG_CONFIG_HOME");
        const auto restore = qScopeGuard([&] {
            CutefishFM::setFolderOpener(nullptr);
            if (originalConfig.isNull())
                qunsetenv("XDG_CONFIG_HOME");
            else
                qputenv("XDG_CONFIG_HOME", originalConfig);
            other.remove();
        });
        qputenv("XDG_CONFIG_HOME", config.path().toUtf8());

        // A folder opened from the desktop goes to the file manager, not to the
        // default inode/directory handler.
        FolderModel folder;
        folder.setIsDesktop(true);
        folder.setUrl(QUrl::fromLocalFile(directory.path()).toString());
        QTRY_COMPARE(folder.count(), 1);
        folder.setSelected(0);
        folder.openSelected();
        QCOMPARE(opened, QStringList {directory.filePath("folder with spaces")});
        QCOMPARE(folder.url(), directory.path());
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

    void destructiveOperationsFollowSelectionAfterReordering()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QStringList names = {QStringLiteral("a-keep.txt"),
                                   QStringLiteral("m-target #%.txt"),
                                   QStringLiteral("z-keep.txt")};

        for (const auto &name : names) {
            QFile file(directory.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(name.toUtf8()), name.toUtf8().size());
        }

        FolderModel folder;
        folder.setUrl(directory.path());
        QTRY_COMPARE(folder.status(), FolderModel::Ready);
        const auto target = QUrl::fromLocalFile(directory.filePath(names[1]));
        folder.setSelected(folder.indexForUrl(target).row());
        QSignalSpy completed(&folder, &FolderModel::listingCompleted);
        QVERIFY(QFile::remove(directory.filePath(names[0])));
        folder.refresh();
        folder.setSortDesc(true);
        QTRY_VERIFY(!completed.isEmpty());
        QTRY_COMPARE(folder.status(), FolderModel::Ready);
        QTRY_COMPARE(folder.rowCount(), 2);
        QCOMPARE(folder.selectedUrls(), QList<QUrl>({target}));

        const auto renamed = QString::fromUtf8("renamed 文件 #%.txt");
        QList<Cutefish::IO::TaskResult> results;
        connect(Cutefish::IO::TaskController::instance(),
                &Cutefish::IO::TaskController::taskFinished, &folder,
                [&](Cutefish::IO::Task *task) {
            const auto operation = task->request().operation;

            if (operation == Cutefish::IO::Operation::Rename
                || operation == Cutefish::IO::Operation::Trash) {
                results.append(task->outcome());
            }
        });
        folder.rename(folder.indexForUrl(target).row(), renamed);
        QTRY_COMPARE(results.size(), 1);
        QCOMPARE(results[0].error.code, 0);
        QCOMPARE(results[0].completedSources, QList<QUrl>({target}));
        QVERIFY(!QFileInfo::exists(target.toLocalFile()));
        const auto renamedUrl = QUrl::fromLocalFile(directory.filePath(renamed));
        QTRY_VERIFY(folder.indexForUrl(renamedUrl).isValid());
        folder.clearSelection();
        folder.setSelected(folder.indexForUrl(renamedUrl).row());

        completed.clear();
        folder.setSortDesc(false);
        QTRY_VERIFY(!completed.isEmpty());
        QCOMPARE(folder.selectedUrls(), QList<QUrl>({renamedUrl}));
        folder.moveSelectedToTrash();
        QTRY_COMPARE(results.size(), 2);
        QCOMPARE(results[1].error.code, 0);
        QCOMPARE(results[1].completedSources, QList<QUrl>({renamedUrl}));
        QVERIFY(!QFileInfo::exists(renamedUrl.toLocalFile()));
        QTRY_COMPARE(folder.rowCount(), 1);
        QCOMPARE(folder.selectionCount(), 0);

        folder.deleteSelected();
        QCOMPARE(results.size(), 2);

        QFile kept(directory.filePath(names[2]));
        QVERIFY(kept.open(QIODevice::ReadOnly));
        QCOMPARE(kept.readAll(), names[2].toUtf8());

        FolderModel trash;
        trash.setUrl("trash:/");
        QTRY_COMPARE(trash.status(), FolderModel::Ready);
        QCOMPARE(results[1].outputs.size(), 1);
        const auto trashed = trash.indexForUrl(results[1].outputs.first());
        QVERIFY(trashed.isValid());
        trash.setSelected(trashed.row());
        trash.restoreFromTrash();
        QTRY_VERIFY(QFileInfo::exists(renamedUrl.toLocalFile()));
        QTRY_COMPARE(trash.rowCount(), 0);
        QFile restored(renamedUrl.toLocalFile());
        QVERIFY(restored.open(QIODevice::ReadOnly));
        QCOMPARE(restored.readAll(), names[1].toUtf8());
    }

    void removedSelectionCannotDeleteReplacementRow()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        for (const auto &name : {QStringLiteral("a.txt"), QStringLiteral("b.txt")}) {
            QFile file(directory.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write("keep"), 4);
        }

        FolderModel folder;
        folder.setUrl(directory.path());
        QTRY_COMPARE(folder.status(), FolderModel::Ready);
        folder.setSelected(0);
        QCOMPARE(folder.fileItem(0).name(), QStringLiteral("a.txt"));
        QVERIFY(QFile::remove(directory.filePath("a.txt")));
        folder.refresh();
        QTRY_COMPARE(folder.rowCount(), 1);
        QCOMPARE(folder.fileItem(0).name(), QStringLiteral("b.txt"));
        QCOMPARE(folder.selectionCount(), 0);
        QSignalSpy added(Cutefish::IO::TaskController::instance(),
                         &Cutefish::IO::TaskController::taskAdded);
        folder.deleteSelected();
        folder.moveSelectedToTrash();
        folder.rename(-1, "invalid.txt");
        folder.rename(folder.rowCount(), "invalid.txt");
        folder.rename(0, "../escaped.txt");
        folder.rename(0, ".");
        folder.rename(0, "..");
        folder.rename(0, QString());
        folder.rename(0, QStringLiteral("null") + QChar(0) + QStringLiteral("name"));
        QCOMPARE(added.count(), 0);

        QFile file(directory.filePath("b.txt"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("keep"));
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
        QVERIFY(!menu->property("pointerGrabEnabled").toBool());
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

        QVERIFY(!folder.canDrop(&mimeData, 0, Qt::MoveAction));
        QVERIFY(!folder.canDrop(&mimeData, -1, Qt::MoveAction));
        folder.drop(nullptr, &event, 0);
        QCOMPARE(tasks.count(), 0);
        folder.drop(nullptr, &event, -1);
        QCOMPARE(tasks.count(), 0);
        mimeData.setUrls({QUrl::fromLocalFile(dir.filePath("folder/"))});
        folder.drop(nullptr, &event, 0);
        QCOMPARE(tasks.count(), 0);
        QVERIFY(QFileInfo(dir.filePath("folder")).isDir());
    }

    void dropEligibility()
    {
        QTemporaryDir directory;
        QTemporaryDir source;
        QVERIFY(directory.isValid());
        QVERIFY(source.isValid());
        QVERIFY(QDir(directory.path()).mkdir("writable"));
        QVERIFY(QDir(directory.path()).mkdir("readonly"));
        QFile plainFile(directory.filePath("plain.txt"));
        QVERIFY(plainFile.open(QIODevice::WriteOnly));
        plainFile.close();
        QFile sourceFile(source.filePath("item.txt"));
        QVERIFY(sourceFile.open(QIODevice::WriteOnly));
        sourceFile.close();

        FolderModel folder;
        folder.setUrl(QUrl::fromLocalFile(directory.path()).toString());
        QTRY_COMPARE(folder.count(), 3);
        const int writable = folder.indexForKeyboardSearch("writable", 0);
        const int readonly = folder.indexForKeyboardSearch("readonly", 0);
        const int file = folder.indexForKeyboardSearch("plain.txt", 0);
        QVERIFY(writable >= 0);
        QVERIFY(readonly >= 0);
        QVERIFY(file >= 0);

        QMimeData mimeData;
        mimeData.setUrls({QUrl::fromLocalFile(sourceFile.fileName())});
        QVERIFY(folder.canDrop(&mimeData, writable, Qt::MoveAction));
        QVERIFY(folder.canDrop(&mimeData, -1, Qt::CopyAction));
        QVERIFY(!folder.canDrop(&mimeData, file, Qt::MoveAction));

        const QString readonlyPath = directory.filePath("readonly");
        QVERIFY(QFile::setPermissions(readonlyPath, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
        QVERIFY(!QFileInfo(readonlyPath).isWritable());
        QVERIFY(!folder.canDrop(&mimeData, readonly, Qt::MoveAction));

        QObject event;
        event.setProperty("mimeData", QVariant::fromValue<QObject *>(&mimeData));
        event.setProperty("dropAction", int(Qt::MoveAction));
        QSignalSpy tasks(Cutefish::IO::TaskController::instance(),
                         &Cutefish::IO::TaskController::taskAdded);
        folder.drop(nullptr, &event, readonly);
        QCOMPARE(tasks.count(), 0);
        QVERIFY(QFile::setPermissions(
                readonlyPath,
                QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));

        const QUrl home = QUrl::fromLocalFile(
                QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
        const QUrl documents = QUrl::fromLocalFile(
                QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
        QVERIFY(!Cutefish::IO::canMoveStandardFolderTo(home, home));
        if (documents != home) {
            QVERIFY(Cutefish::IO::isStandardFolder(documents));
            QVERIFY(Cutefish::IO::canMoveStandardFolderTo(documents, home));
            QVERIFY(!Cutefish::IO::canMoveStandardFolderTo(
                    documents, QUrl::fromLocalFile(directory.filePath("writable"))));
            mimeData.setUrls({documents});
            QVERIFY(!folder.canDrop(&mimeData, writable, Qt::MoveAction));
            QVERIFY(folder.canDrop(&mimeData, writable, Qt::CopyAction));
        }
    }

    void progressDialogPresentation()
    {
        Cutefish::IO::Task task({Cutefish::IO::Operation::Copy,
                                 {QUrl::fromLocalFile("/example/Photos")},
                                 QUrl::fromLocalFile("/example/Documents")});
        QQmlEngine engine;
        CutefishFM::registerImageProviders(&engine);
        QQmlComponent component(&engine, QUrl("qrc:/qml/Dialogs/IoTaskDialog.qml"));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> dialog(component.createWithInitialProperties(
                {{"ioTask", QVariant::fromValue<QObject *>(&task)}}));
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
        QQmlComponent component(&engine, QUrl("qrc:/qml/Dialogs/IoTaskDialog.qml"));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> dialog(component.createWithInitialProperties(
                {{"ioTask", QVariant::fromValue<QObject *>(&task)}}));
        QVERIFY2(dialog, qPrintable(component.errorString()));
        task.start();
        QTRY_VERIFY(dialog->property("conflict").toBool());
        task.resolveConflict(int(Cutefish::IO::ConflictPolicy::Skip));
        QTRY_VERIFY(dialog->property("finished").toBool());
        QVERIFY(QFileInfo::exists(source.fileName()));
    }

    void conflictDialogReplacesThroughTheClient()
    {
        QTemporaryDir dir;
        QVERIFY(QDir().mkpath(dir.filePath("target")));
        QFile source(dir.filePath("photo.svg"));
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("<svg/>");
        source.close();
        QFile target(dir.filePath("target/photo.svg"));
        QVERIFY(target.open(QIODevice::WriteOnly));
        target.write("<svg>old</svg>");
        target.close();

        QPointer<OperationTask> task = FileOperationsClient::instance()->copy(
                {QUrl::fromLocalFile(source.fileName())},
                QUrl::fromLocalFile(dir.filePath("target")));
        QQmlEngine engine;
        CutefishFM::registerImageProviders(&engine);
        QQmlComponent component(&engine, QUrl("qrc:/qml/Dialogs/IoTaskDialog.qml"));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> dialog(component.createWithInitialProperties(
                {{"ioTask", QVariant::fromValue<QObject *>(task.data())}}));
        QVERIFY2(dialog, qPrintable(component.errorString()));

        QTRY_VERIFY(dialog->property("conflict").toBool());
        auto *conflictDialog = dialog->findChild<QQuickWindow *>("conflictDialog");
        QVERIFY(conflictDialog);
        QTRY_VERIFY(conflictDialog->isVisible());
        QVERIFY(!qobject_cast<QQuickWindow *>(dialog.data())->isVisible());
        QVERIFY(conflictDialog->property("heading").toString().contains("photo.svg"));
        QVERIFY(!conflictDialog->findChild<QObject *>("conflictSkipButton")->property("visible").toBool());
        auto *replace = conflictDialog->findChild<QObject *>("conflictReplaceButton");
        QVERIFY(replace);
        QVERIFY(QMetaObject::invokeMethod(replace, "clicked"));
        QTRY_VERIFY(!task);
        QVERIFY(!conflictDialog->isVisible());
        QVERIFY(dialog->property("errorMessage").toString().isEmpty());
        QFile replaced(dir.filePath("target/photo.svg"));
        QVERIFY(replaced.open(QIODevice::ReadOnly));
        QCOMPARE(replaced.readAll(), QByteArray("<svg/>"));
    }

    void activeTaskCountFollowsOwnTasks()
    {
        QTemporaryDir dir;
        auto *client = FileOperationsClient::instance();
        QCOMPARE(client->activeTaskCount(), 0);
        QSignalSpy changes(client, &FileOperationsClient::activeTaskCountChanged);
        QSignalSpy added(client, &FileOperationsClient::taskAdded);
        int countWhenAdded = -1;
        const auto connection = connect(client, &FileOperationsClient::taskAdded, this,
                                        [&] { countWhenAdded = client->activeTaskCount(); });
        const auto disconnect = qScopeGuard([&] { QObject::disconnect(connection); });

        QPointer<OperationTask> task = client->mkdir(QUrl::fromLocalFile(dir.filePath("created")));
        QVERIFY(task);
        QCOMPARE(added.size(), 1);
        QCOMPARE(countWhenAdded, 1);
        QCOMPARE(client->activeTaskCount(), 1);
        QTRY_VERIFY(!task);
        QCOMPARE(client->activeTaskCount(), 0);
        QCOMPARE(changes.size(), 2);
        QCOMPARE(changes.last().first().toInt(), 0);
        QVERIFY(QFileInfo(dir.filePath("created")).isDir());
    }

    void finishedTasksAreDeleted()
    {
        QTemporaryDir dir;
        QFile source(dir.filePath("notes.txt"));
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("notes");
        source.close();

        auto *client = FileOperationsClient::instance();
        QPointer<OperationTask> created = client->mkdir(QUrl::fromLocalFile(dir.filePath("created")));
        QPointer<OperationTask> compressed = client->compress({source.fileName()},
                                                              dir.filePath("notes.zip"));
        QVERIFY(created);
        QVERIFY(compressed);
        QTRY_VERIFY(!created);
        QTRY_VERIFY(!compressed);
        QVERIFY(QFileInfo(dir.filePath("created")).isDir());
        QVERIFY(QFileInfo(dir.filePath("notes.zip")).isFile());
    }

    void extractsTarGzWithFoldersAndLinks()
    {
        QTemporaryDir dir;
        const QString archivePath = dir.filePath("bundle.tar.gz");
        QVERIFY(writeArchive(archivePath, ARCHIVE_FORMAT_TAR_PAX_RESTRICTED, ARCHIVE_FILTER_GZIP,
                             {{"./", AE_IFDIR, {}, 0755},
                              {"./docs/", AE_IFDIR, {}, 0700},
                              {"./docs/readme.txt", AE_IFREG, "hello"},
                              {"./run.sh", AE_IFREG, "#!/bin/sh\n", 04755},
                              {"./latest", AE_IFLNK, "docs/readme.txt", 0777}}));

        const QString output = dir.filePath("bundle");
        QString error;
        QVERIFY2(runArchiveJob(ArchiveJob::Extract, {archivePath}, output, &error), qPrintable(error));

        QCOMPARE(readFile(output + "/docs/readme.txt"), QString("hello"));
        QCOMPARE(QFileInfo(output + "/docs/readme.txt").lastModified().toSecsSinceEpoch(),
                 qint64(1700000000));

        struct stat info;
        QCOMPARE(::stat(QFile::encodeName(output + "/run.sh").constData(), &info), 0);
        QVERIFY(info.st_mode & S_IXUSR);
        QVERIFY(!(info.st_mode & (S_ISUID | S_ISGID)));
        QCOMPARE(::stat(QFile::encodeName(output + "/docs").constData(), &info), 0);
        QCOMPARE(info.st_mode & 0777, mode_t(0700));

        const QFileInfo link(output + "/latest");
        QVERIFY(link.isSymLink());
        QCOMPARE(readFile(link.filePath()), QString("hello"));
        QVERIFY(extractionLeftovers(dir.path()).isEmpty());
    }

    void extractsArchiveFormats_data()
    {
        QTest::addColumn<int>("format");
        QTest::addColumn<int>("filter");
        QTest::addColumn<QString>("fileName");

        QTest::newRow("tar.xz") << ARCHIVE_FORMAT_TAR_PAX_RESTRICTED << ARCHIVE_FILTER_XZ << "pack.tar.xz";
        QTest::newRow("tar.zst") << ARCHIVE_FORMAT_TAR_PAX_RESTRICTED << ARCHIVE_FILTER_ZSTD << "pack.tar.zst";
        QTest::newRow("tar.bz2") << ARCHIVE_FORMAT_TAR_PAX_RESTRICTED << ARCHIVE_FILTER_BZIP2 << "pack.tar.bz2";
        QTest::newRow("tar") << ARCHIVE_FORMAT_TAR_GNUTAR << ARCHIVE_FILTER_NONE << "pack.tar";
        QTest::newRow("7z") << ARCHIVE_FORMAT_7ZIP << ARCHIVE_FILTER_NONE << "pack.7z";
        QTest::newRow("zip") << ARCHIVE_FORMAT_ZIP << ARCHIVE_FILTER_NONE << "pack.zip";
    }

    void extractsArchiveFormats()
    {
        QFETCH(int, format);
        QFETCH(int, filter);
        QFETCH(QString, fileName);

        QTemporaryDir dir;
        const QString archivePath = dir.filePath(fileName);
        QVERIFY(writeArchive(archivePath, format, filter,
                             {{"pack/", AE_IFDIR, {}, 0755},
                              {"pack/a.txt", AE_IFREG, "alpha"},
                              {"pack/sub/b.txt", AE_IFREG, QByteArray(300 * 1024, 'b')},
                              {"pack/link", AE_IFLNK, "a.txt", 0777}}));

        const QString output = dir.filePath("pack");
        QString error;
        QVERIFY2(runArchiveJob(ArchiveJob::Extract, {archivePath}, output, &error), qPrintable(error));
        QCOMPARE(readFile(output + "/pack/a.txt"), QString("alpha"));
        QCOMPARE(QFileInfo(output + "/pack/sub/b.txt").size(), qint64(300 * 1024));
        QVERIFY(QFileInfo(output + "/pack/link").isSymLink());
        QCOMPARE(QFileInfo(output + "/pack/link").symLinkTarget(), QFileInfo(output + "/pack/a.txt").filePath());
        QVERIFY(extractionLeftovers(dir.path()).isEmpty());
    }

    void extractsSingleCompressedFile_data()
    {
        QTest::addColumn<int>("filter");
        QTest::addColumn<QString>("fileName");

        QTest::newRow("gz") << ARCHIVE_FILTER_GZIP << "notes.txt.gz";
        QTest::newRow("xz") << ARCHIVE_FILTER_XZ << "notes.txt.xz";
        QTest::newRow("zst") << ARCHIVE_FILTER_ZSTD << "notes.txt.zst";
        QTest::newRow("bz2") << ARCHIVE_FILTER_BZIP2 << "notes.txt.bz2";
    }

    void extractsSingleCompressedFile()
    {
        QFETCH(int, filter);
        QFETCH(QString, fileName);

        QTemporaryDir dir;
        const QString archivePath = dir.filePath(fileName);
        QVERIFY(writeArchive(archivePath, ARCHIVE_FORMAT_RAW, filter,
                             {{"notes.txt", AE_IFREG, "plain notes"}}));

        const QString output = dir.filePath("notes.txt");
        QString error;
        QVERIFY2(runArchiveJob(ArchiveJob::Extract, {archivePath}, output, &error), qPrintable(error));
        QVERIFY(QFileInfo(output).isFile());
        QCOMPARE(readFile(output), QString("plain notes"));
        QVERIFY(extractionLeftovers(dir.path()).isEmpty());
    }

    void refusesPlainFile()
    {
        QTemporaryDir dir;
        QFile plain(dir.filePath("fake.zip"));
        QVERIFY(plain.open(QIODevice::WriteOnly));
        plain.write("not an archive at all");
        plain.close();

        QString error;
        QVERIFY(!runArchiveJob(ArchiveJob::Extract, {plain.fileName()}, dir.filePath("fake"), &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!QFileInfo::exists(dir.filePath("fake")));
        QVERIFY(extractionLeftovers(dir.path()).isEmpty());
    }

    void roundTripsCompressedZip()
    {
        QTemporaryDir dir;
        QVERIFY(QDir(dir.path()).mkpath("source/inner"));
        QFile file(dir.filePath("source/inner/data.txt"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("zip data");
        file.close();
        QVERIFY(QFile::link("inner/data.txt", dir.filePath("source/shortcut")));

        QString error;
        QVERIFY2(runArchiveJob(ArchiveJob::Compress, {dir.filePath("source")},
                               dir.filePath("source.zip"), &error), qPrintable(error));
        QVERIFY2(runArchiveJob(ArchiveJob::Extract, {dir.filePath("source.zip")},
                               dir.filePath("restored"), &error), qPrintable(error));
        QCOMPARE(readFile(dir.filePath("restored/source/inner/data.txt")), QString("zip data"));
        QVERIFY(QFileInfo(dir.filePath("restored/source/shortcut")).isSymLink());
        QCOMPARE(readFile(dir.filePath("restored/source/shortcut")), QString("zip data"));
    }

    void refusesUnsafeArchives_data()
    {
        QTest::addColumn<QByteArrayList>("paths");
        QTest::addColumn<QList<int>>("types");
        QTest::addColumn<QByteArrayList>("contents");
        QTest::addColumn<QString>("expectedError");

        // "@outside" stands for a folder next to the work folder.
        QTest::newRow("parent") << QByteArrayList {"ok.txt", "../evil"}
                                << QList<int> {AE_IFREG, AE_IFREG}
                                << QByteArrayList {"ok", "evil"} << "Unsafe path";
        QTest::newRow("absolute") << QByteArrayList {"@outside/evil"} << QList<int> {AE_IFREG}
                                  << QByteArrayList {"evil"} << "Unsafe path";
        QTest::newRow("file through link") << QByteArrayList {"escape", "escape/evil"}
                                           << QList<int> {AE_IFLNK, AE_IFREG}
                                           << QByteArrayList {"@outside", "evil"} << "Duplicate path";
        QTest::newRow("link under link") << QByteArrayList {"escape", "escape/evil"}
                                         << QList<int> {AE_IFLNK, AE_IFLNK}
                                         << QByteArrayList {"@outside", "/etc/passwd"}
                                         << "Symbolic link parent";
        QTest::newRow("fifo") << QByteArrayList {"pipe"} << QList<int> {AE_IFIFO}
                              << QByteArrayList {QByteArray()} << "Special files";
    }

    void refusesUnsafeArchives()
    {
        QFETCH(QByteArrayList, paths);
        QFETCH(QList<int>, types);
        QFETCH(QByteArrayList, contents);
        QFETCH(QString, expectedError);

        QTemporaryDir root;
        QVERIFY(QDir(root.path()).mkpath("work"));
        QVERIFY(QDir(root.path()).mkpath("outside"));
        const QByteArray outside = QFile::encodeName(root.filePath("outside"));
        const QString work = root.filePath("work");

        QList<FixtureEntry> entries;
        for (qsizetype index = 0; index < paths.size(); ++index) {
            QByteArray path = paths.at(index);
            QByteArray data = contents.at(index);
            path.replace("@outside", outside);
            data.replace("@outside", outside);
            entries.append({path, static_cast<mode_t>(types.at(index)), data,
                            static_cast<mode_t>(types.at(index) == AE_IFLNK ? 0777 : 0644)});
        }

        const QString archivePath = work + "/unsafe.tar";
        QVERIFY(writeArchive(archivePath, ARCHIVE_FORMAT_TAR_PAX_RESTRICTED, ARCHIVE_FILTER_NONE, entries));

        QString error;
        QVERIFY(!runArchiveJob(ArchiveJob::Extract, {archivePath}, work + "/unsafe", &error));
        QVERIFY2(error.contains(expectedError), qPrintable(error));
        QVERIFY(!QFileInfo::exists(work + "/unsafe"));
        QVERIFY(!QFileInfo::exists(work + "/evil"));
        QVERIFY(!QFileInfo::exists(root.filePath("outside/evil")));
        QVERIFY(extractionLeftovers(work).isEmpty());
    }

    void refusesHardLinks()
    {
        QTemporaryDir dir;
        const QString archivePath = dir.filePath("links.tar");
        QVERIFY(writeArchive(archivePath, ARCHIVE_FORMAT_TAR_PAX_RESTRICTED, ARCHIVE_FILTER_NONE,
                             {{"target.txt", AE_IFREG, "data"},
                              {"copy.txt", AE_IFREG, "target.txt", 0644, true}}));

        QString error;
        QVERIFY(!runArchiveJob(ArchiveJob::Extract, {archivePath}, dir.filePath("links"), &error));
        QVERIFY2(error.contains("Hard links"), qPrintable(error));
        QVERIFY(!QFileInfo::exists(dir.filePath("links")));
        QVERIFY(extractionLeftovers(dir.path()).isEmpty());
    }

    void refusesPasswordProtectedZip()
    {
        QTemporaryDir dir;
        const QString archivePath = dir.filePath("secret.zip");
        if (!writeArchive(archivePath, ARCHIVE_FORMAT_ZIP, ARCHIVE_FILTER_NONE,
                          {{"secret.txt", AE_IFREG, "hidden"}}, "zip:encryption=zipcrypt"))
            QSKIP("libarchive cannot write encrypted zip archives here");

        QString error;
        QVERIFY(!runArchiveJob(ArchiveJob::Extract, {archivePath}, dir.filePath("secret"), &error));
        QVERIFY2(error.contains("Password-protected"), qPrintable(error));
        QVERIFY(!QFileInfo::exists(dir.filePath("secret")));
        QVERIFY(extractionLeftovers(dir.path()).isEmpty());
    }

    void openExtractsNextToArchive()
    {
        QTemporaryDir dir;
        QVERIFY(writeArchive(dir.filePath("bundle.tar.gz"), ARCHIVE_FORMAT_TAR_PAX_RESTRICTED,
                             ARCHIVE_FILTER_GZIP, {{"inside.txt", AE_IFREG, "inside"}}));
        QVERIFY(writeArchive(dir.filePath("notes.txt.xz"), ARCHIVE_FORMAT_RAW, ARCHIVE_FILTER_XZ,
                             {{"notes.txt", AE_IFREG, "notes"}}));

        // A fresh model each time: the last one selects what it extracted.
        const auto open = [&dir](const QString &name) {
            FolderModel folder;
            folder.setUrl(QUrl::fromLocalFile(dir.path()).toString());
            QTRY_VERIFY(folder.indexForKeyboardSearch(name, 0) >= 0);
            folder.setSelected(folder.indexForKeyboardSearch(name, 0));
            folder.openSelected();
            QVERIFY(folder.archiveBusy());
            QTRY_VERIFY(!folder.archiveBusy());
        };

        open("bundle.tar.gz");
        QVERIFY(QFileInfo(dir.filePath("bundle")).isDir());
        QCOMPARE(readFile(dir.filePath("bundle/inside.txt")), QString("inside"));

        open("notes.txt.xz");
        QVERIFY(QFileInfo(dir.filePath("notes.txt")).isFile());
        QCOMPARE(readFile(dir.filePath("notes.txt")), QString("notes"));

        // A second open of the same archive picks a free name.
        open("bundle.tar.gz");
        QVERIFY(QFileInfo(dir.filePath("bundle 1")).isDir());
        open("notes.txt.xz");
        QVERIFY(QFileInfo(dir.filePath("notes 1.txt")).isFile());
    }

    void closingProgressDialogDoesNotCancelTask()
    {
        QTemporaryDir sourceDirectory;
        QTemporaryDir destinationDirectory;
        QFile source(sourceDirectory.filePath("payload"));
        QVERIFY(source.open(QIODevice::WriteOnly));
        QCOMPARE(source.write(QByteArray(1024 * 1024, 'x')), qint64(1024 * 1024));
        source.close();

        Cutefish::IO::Task task({Cutefish::IO::Operation::Copy,
                                 {QUrl::fromLocalFile(source.fileName())},
                                 QUrl::fromLocalFile(destinationDirectory.path())});
        QQmlEngine engine;
        CutefishFM::registerImageProviders(&engine);
        QQmlComponent component(&engine, QUrl("qrc:/qml/Dialogs/IoTaskDialog.qml"));
        QScopedPointer<QObject> dialog(component.createWithInitialProperties(
                {{"ioTask", QVariant::fromValue<QObject *>(&task)}}));
        QVERIFY2(dialog, qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(dialog.data());
        QVERIFY(window);

        window->show();
        QSignalSpy done(&task, &Cutefish::IO::Task::result);
        window->close();
        // Hidden, not closed: a conflict or an error later still needs it.
        QVERIFY(!window->isVisible());
        QVERIFY(window->handle());
        QVERIFY(dialog->property("dismissed").toBool());
        task.start();
        QTRY_COMPARE(done.size(), 1);
        QVERIFY(!task.wasCanceled());
        QVERIFY(QFileInfo::exists(destinationDirectory.filePath("payload")));
        // Nothing to show any more: now a close goes through.
        QTest::qWait(800);
        QVERIFY(!window->isVisible());
    }

    void untrustedLaunchersAsk()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString program = dir.filePath("program");
        QVERIFY(QFile::copy("/bin/true", program));
        QVERIFY(QFile::setPermissions(program, QFile::ReadOwner | QFile::WriteOwner));
        const QString marker = dir.filePath("launched");
        const QString launcher = dir.filePath("launcher.desktop");
        QFile entry(launcher);
        QVERIFY(entry.open(QIODevice::WriteOnly));
        entry.write("[Desktop Entry]\nType=Application\nName=Launcher\nExec=touch " + marker.toUtf8()
                    + "\n");
        entry.close();
        QVERIFY(QFile::setPermissions(launcher, QFile::ReadOwner | QFile::WriteOwner));

        QVERIFY(!FileLauncher::isTrusted(program));
        QVERIFY(!FileLauncher::isTrusted(launcher));

        QQmlEngine engine;
        CutefishFM::registerImageProviders(&engine);
        WindowManager manager(&engine);
        WindowManager::setDefault(&manager);
        const auto restore = qScopeGuard([] { WindowManager::setDefault(nullptr); });

        auto launchDialogs = [] {
            QList<QWindow *> dialogs;
            for (QWindow *window : QGuiApplication::topLevelWindows()) {
                if (window->property("desktopFile").isValid())
                    dialogs.append(window);
            }
            return dialogs;
        };

        FolderModel folder;
        folder.setUrl(QUrl::fromLocalFile(dir.path()).toString());
        QTRY_COMPARE(folder.count(), 2);
        folder.selectAll();
        folder.openSelected();

        // Neither runs, nor turns executable, before the user agrees.
        QTRY_COMPARE(launchDialogs().size(), 2);
        QTest::qWait(300);
        QVERIFY(!QFileInfo::exists(marker));
        QVERIFY(!(QFile::permissions(program) & QFile::ExeOwner));
        QVERIFY(!(QFile::permissions(launcher) & QFile::ExeOwner));

        QWindow *launcherDialog = nullptr;
        for (QWindow *dialog : launchDialogs()) {
            if (dialog->property("desktopFile").toBool())
                launcherDialog = dialog;
            else
                QVERIFY(QMetaObject::invokeMethod(dialog, "rejected"));
        }
        QVERIFY(launcherDialog);
        QCOMPARE(launcherDialog->property("name").toString(), QString("launcher.desktop"));
        QVERIFY(QMetaObject::invokeMethod(launcherDialog, "accepted"));
        QTRY_VERIFY(QFileInfo::exists(marker));
        QVERIFY(QFile::permissions(launcher) & QFile::ExeOwner);
        QVERIFY(!(QFile::permissions(program) & QFile::ExeOwner));
        QVERIFY(FileLauncher::isTrusted(launcher));

        for (QWindow *dialog : launchDialogs())
            manager.release(qobject_cast<QQuickWindow *>(dialog));
        QTRY_COMPARE(launchDialogs().size(), 0);

        // A trusted file runs at once.
        QTRY_COMPARE(folder.count(), 3);
        QVERIFY(QFile::remove(marker));
        QTRY_COMPARE(folder.count(), 2);
        folder.clearSelection();
        // Launchers show their Name, so the row is found by file name.
        int launcherRow = -1;
        for (int row = 0; row < folder.count(); ++row) {
            if (folder.data(folder.index(row, 0), FolderModel::FileNameRole) == "launcher.desktop")
                launcherRow = row;
        }
        QVERIFY(launcherRow >= 0);
        folder.setSelected(launcherRow);
        folder.openSelected();
        QTRY_VERIFY(QFileInfo::exists(marker));
        QTest::qWait(100);
        QCOMPARE(launchDialogs().size(), 0);
    }

    void launchTrust()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString program = dir.filePath("program");
        QVERIFY(QFile::copy("/bin/true", program));
        QVERIFY(QFile::setPermissions(program, QFile::ReadOwner | QFile::WriteOwner));
        QVERIFY(!FileLauncher::isTrusted(program));
        QVERIFY(QFile::setPermissions(program, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(FileLauncher::isTrusted(program));

        // Menu entries are launchers by definition, executable or not.
        QVERIFY(QDir().mkpath(m_data.filePath("applications")));
        const QString installed = m_data.filePath("applications/installed.desktop");
        QFile entry(installed);
        QVERIFY(entry.open(QIODevice::WriteOnly));
        entry.write("[Desktop Entry]\nType=Application\nName=Installed\nExec=/bin/true\n");
        entry.close();
        const auto remove = qScopeGuard([&] { QFile::remove(installed); });
        QVERIFY(QFile::setPermissions(installed, QFile::ReadOwner | QFile::WriteOwner));
        QVERIFY(FileLauncher::isTrusted(installed));

        // Outside the menu the same file needs the executable bit.
        const QString copy = dir.filePath("installed.desktop");
        QVERIFY(QFile::copy(installed, copy));
        QVERIFY(QFile::setPermissions(copy, QFile::ReadOwner | QFile::WriteOwner));
        QVERIFY(!FileLauncher::isTrusted(copy));
    }

    void emptyTrashProgressUsesShortDelay()
    {
        Cutefish::IO::Task task({Cutefish::IO::Operation::EmptyTrash});
        QQmlEngine engine;
        CutefishFM::registerImageProviders(&engine);
        QQmlComponent component(&engine, QUrl("qrc:/qml/Dialogs/IoTaskDialog.qml"));
        QScopedPointer<QObject> dialog(component.createWithInitialProperties(
                {{"ioTask", QVariant::fromValue<QObject *>(&task)}}));
        QVERIFY2(dialog, qPrintable(component.errorString()));

        auto *delay = dialog->findChild<QObject *>("ioShowDelay");
        QVERIFY(delay);
        QCOMPARE(delay->property("interval").toInt(), 300);

        auto *window = qobject_cast<QQuickWindow *>(dialog.data());
        QVERIFY(window);
        QVERIFY(!window->isVisible());
        QSignalSpy done(&task, &Cutefish::IO::Task::result);
        task.start();
        QTRY_COMPARE(done.size(), 1);
        QTest::qWait(350);
        QVERIFY(!window->isVisible());
    }
};
QTEST_MAIN(IOIntegrationTest)
#include "tst_iointegration.moc"
