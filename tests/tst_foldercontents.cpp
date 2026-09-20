#include "model/foldermodel.h"

#include <QAbstractItemModelTester>
#include <QFile>
#include <QPersistentModelIndex>
#include <QScopeGuard>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QtTest>

class FolderContentsTest : public QObject
{
    Q_OBJECT

private:
    static bool writeFile(const QString &path, const QByteArray &contents = "text\n")
    {
        QFile file(path);

        return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
    }

private slots:
    void largeDirectoryPublishesBetweenEventLoopTurns()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        for (int i = 0; i < 2048; ++i) {
            const auto name = QStringLiteral("%1.txt").arg(i, 5, 10, QLatin1Char('0'));
            QVERIFY(writeFile(directory.filePath(name)));
        }

        FolderModel folder;
        folder.setIsDesktop(true);
        QAbstractItemModelTester tester(&folder,
                QAbstractItemModelTester::FailureReportingMode::QtTest);
        QSignalSpy finished(&folder, &FolderModel::listingCompleted);
        int batches = 0;
        bool dispatched = false;

        connect(&folder, &QAbstractItemModel::rowsInserted, this,
                [&](const QModelIndex &, int first, int last) {
            QVERIFY(last - first + 1 <= 256);

            if (++batches == 1) {
                QTimer::singleShot(0, &folder, [&] { dispatched = true; });
            } else {
                QVERIFY(dispatched);
            }
        });

        folder.setUrl(QUrl::fromLocalFile(directory.path()).toString());
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 15000);
        QCOMPARE(folder.rowCount(), 2048);
        QCOMPARE(batches, 8);
        QCOMPARE(folder.fileItem(0).name(), QStringLiteral("00000.txt"));
        QCOMPARE(folder.fileItem(2047).name(), QStringLiteral("02047.txt"));
    }

    void sortingIsDeferredAndPreservesSelection()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(writeFile(directory.filePath("a.txt"), "a"));
        QVERIFY(writeFile(directory.filePath("b.txt"), "longer"));
        QVERIFY(QDir(directory.path()).mkdir("directory"));

        FolderModel folder;
        folder.setIsDesktop(true);
        folder.setUrl(QUrl::fromLocalFile(directory.path()).toString());
        QTRY_COMPARE(folder.status(), FolderModel::Ready);
        QAbstractItemModelTester tester(&folder,
                QAbstractItemModelTester::FailureReportingMode::QtTest);
        const auto url = QUrl::fromLocalFile(directory.filePath("a.txt"));
        QPersistentModelIndex selected(folder.indexForUrl(url));
        folder.setSelected(selected.row());

        auto *pool = QThreadPool::globalInstance();
        pool->waitForDone();
        const int previousLimit = pool->maxThreadCount();
        pool->setMaxThreadCount(1);
        auto gate = std::make_shared<QSemaphore>();
        QSemaphore started;

        pool->start([gate, &started] {
            started.release();
            gate->acquire();
        });
        started.acquire();

        const auto release = qScopeGuard([&] {
            gate->release();
            pool->waitForDone();
            pool->setMaxThreadCount(previousLimit);
        });
        QSignalSpy sorted(&folder, &FolderModel::listingCompleted);
        QSignalSpy preparing(&folder, &FolderContentsModel::preparationStarted);
        folder.setSortDesc(true);
        QTRY_COMPARE(preparing.count(), 1);
        QCOMPARE(sorted.count(), 0);
        QCOMPARE(folder.fileItem(1).name(), QStringLiteral("a.txt"));

        gate->release();
        QTRY_COMPARE(sorted.count(), 1);
        QCOMPARE(folder.fileItem(0).name(), QStringLiteral("directory"));
        QCOMPARE(folder.fileItem(1).name(), QStringLiteral("b.txt"));
        QVERIFY(selected.isValid());
        QCOMPARE(folder.itemForIndex(selected).url(), url);
        QCOMPARE(folder.selectedUrls(), QList<QUrl>({url}));

        folder.setSortMode(Cutefish::IO::DirectoryModel::Size);
        folder.setSortDesc(false);
        QTRY_COMPARE(sorted.count(), 2);
        QCOMPARE(folder.fileItem(1).name(), QStringLiteral("a.txt"));
    }

    void latestFilterAndRefreshWin()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(writeFile(directory.filePath("a.txt")));
        QVERIFY(writeFile(directory.filePath("b.log")));

        FolderModel folder;
        folder.setIsDesktop(true);
        folder.setUrl(QUrl::fromLocalFile(directory.path()).toString());
        QTRY_COMPARE(folder.status(), FolderModel::Ready);
        QAbstractItemModelTester tester(&folder,
                QAbstractItemModelTester::FailureReportingMode::QtTest);
        QSignalSpy finished(&folder, &FolderModel::listingCompleted);

        folder.setFilterMimeTypes({QStringLiteral("all/all")});
        folder.setFilterMode(FolderModel::FilterShowMatches);
        folder.setFilterPattern("*.log");
        folder.setFilterPattern("*.txt");
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(folder.rowCount(), 1);
        QCOMPARE(folder.fileItem(0).name(), QStringLiteral("a.txt"));

        QPersistentModelIndex retained(folder.index(0, 0));
        QVERIFY(writeFile(directory.filePath("a.txt"), "updated contents"));
        QVERIFY(writeFile(directory.filePath("new.txt")));
        folder.refresh();
        QTRY_COMPARE(folder.rowCount(), 2);
        QTRY_COMPARE(folder.itemForIndex(retained).size(), quint64(16));
        QVERIFY(retained.isValid());

        folder.setFilterPattern("missing*");
        QTRY_COMPARE(folder.rowCount(), 0);
        QVERIFY(!retained.isValid());
    }

    void navigationDuringPublicationDiscardsOldRows()
    {
        QTemporaryDir first;
        QTemporaryDir second;
        QVERIFY(first.isValid());
        QVERIFY(second.isValid());

        for (int i = 0; i < 600; ++i)
            QVERIFY(writeFile(first.filePath(QString::number(i) + ".txt")));

        QVERIFY(writeFile(second.filePath("destination.txt")));
        FolderModel folder;
        folder.setIsDesktop(true);
        bool navigated = false;

        connect(&folder, &QAbstractItemModel::rowsInserted, this, [&] {
            if (navigated)
                return;

            navigated = true;
            QTimer::singleShot(0, &folder, [&] {
                folder.setUrl(QUrl::fromLocalFile(second.path()).toString());
            });
        });

        folder.setUrl(QUrl::fromLocalFile(first.path()).toString());
        QTRY_VERIFY(navigated);
        QTRY_COMPARE(folder.resolvedUrl(), QUrl::fromLocalFile(second.path()));
        QTRY_COMPARE(folder.status(), FolderModel::Ready);
        QCOMPARE(folder.rowCount(), 1);
        QCOMPARE(folder.fileItem(0).name(), QStringLiteral("destination.txt"));
    }

    void desktopRolesUsePreparedSnapshot()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("shortcut.desktop");
        QVERIFY(writeFile(path, "[Desktop Entry]\nType=Link\nName=Cached name\n"
                                "Icon=cached-icon\nURL=trash:/\n"));

        FolderModel folder;
        folder.setIsDesktop(true);
        folder.setUrl(QUrl::fromLocalFile(directory.path()).toString());
        QTRY_COMPARE(folder.status(), FolderModel::Ready);
        folder.sourceModel()->dirLister()->stop();
        QVERIFY(QFile::remove(path));

        const auto index = folder.index(0, 0);
        QCOMPARE(folder.data(index, FolderModel::DisplayNameRole).toString(),
                 QStringLiteral("Cached name"));
        QCOMPARE(folder.data(index, FolderModel::IconNameRole).toString(),
                 QStringLiteral("cached-icon"));
        QCOMPARE(folder.itemForIndex(index).targetUrl(), QUrl(QStringLiteral("trash:/")));
    }

    void openingFileResolvesParentWithoutDuplicateHistory()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("file.txt");
        QVERIFY(writeFile(path));

        FolderModel folder;
        folder.setIsDesktop(true);
        folder.setUrl(directory.path());
        QTRY_COMPARE(folder.status(), FolderModel::Ready);

        QSignalSpy finished(&folder, &FolderModel::listingCompleted);
        folder.setUrl(path);
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(folder.resolvedUrl(), QUrl::fromLocalFile(directory.path()));
        QCOMPARE(folder.rowCount(), 1);
        QVERIFY(!folder.canGoBack());

        QSignalSpy errors(&folder, &FolderModel::notification);
        folder.setUrl(directory.filePath("missing"));
        QTRY_COMPARE(folder.status(), FolderModel::Canceled);
        QVERIFY(!errors.isEmpty());
    }

    void filterChangesWaitForRunningScan()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(writeFile(directory.filePath("match.txt")));

        FolderModel folder;
        folder.setIsDesktop(true);
        auto gate = std::make_shared<QSemaphore>();
        auto entered = std::make_shared<std::atomic_bool>(false);
        folder.sourceModel()->dirLister()->setItemPreparer([gate, entered](auto &) {
            if (!entered->exchange(true))
                gate->acquire();
        });
        const auto release = qScopeGuard([gate] { gate->release(); });

        folder.setUrl(directory.path());
        QTRY_VERIFY(entered->load());
        QSignalSpy completed(&folder, &FolderModel::listingCompleted);
        folder.setFilterMode(FolderModel::FilterShowMatches);
        folder.setFilterMimeTypes({QStringLiteral("all/all")});
        folder.setFilterPattern("*.txt");
        QCoreApplication::processEvents();
        QCOMPARE(completed.count(), 0);
        QCOMPARE(folder.status(), FolderModel::Listing);

        gate->release();
        QTRY_COMPARE(completed.count(), 1);
        QCOMPARE(folder.rowCount(), 1);
    }
};

QTEST_MAIN(FolderContentsTest)
#include "tst_foldercontents.moc"
