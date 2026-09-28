#include "model/foldermodel.h"

#include <QAbstractItemModelTester>
#include <QAction>
#include <QApplication>
#include <QFile>
#include <QMenu>
#include <QMimeData>
#include <QPersistentModelIndex>
#include <QScopeGuard>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QtTest>

#include <algorithm>

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
    void repeatedRefreshPreservesIdentity()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QMap<QString, QByteArray> expected;

        for (int i = 0; i < 800; ++i) {
            const auto name = QStringLiteral("%1.txt").arg(i, 4, 10, QLatin1Char('0'));
            expected.insert(name, QByteArray::number(i));
            QVERIFY(writeFile(directory.filePath(name), expected.value(name)));
        }

        FolderModel folder;
        folder.setIsDesktop(true);
        QAbstractItemModelTester tester(&folder,
                QAbstractItemModelTester::FailureReportingMode::QtTest);
        folder.setUrl(directory.path());
        QTRY_COMPARE(folder.status(), FolderModel::Ready);
        QCOMPARE(folder.rowCount(), expected.size());

        for (int round = 0; round < 3; ++round) {
            QMap<QString, QPersistentModelIndex> previous;

            for (int row = 0; row < folder.rowCount(); ++row)
                previous.insert(folder.fileItem(row).name(), folder.index(row, 0));

            folder.sourceModel()->dirLister()->stop();
            const auto names = expected.keys();

            for (int i = 0; i < names.size(); ++i) {
                const auto &name = names[i];

                // Cover both removals larger than one batch and separated removal ranges.
                if ((round == 0 && i < 300) || (round > 0 && i % 3 == 0)) {
                    QVERIFY(QFile::remove(directory.filePath(name)));
                    expected.remove(name);
                } else {
                    expected[name].append("-updated");
                    QVERIFY(writeFile(directory.filePath(name), expected.value(name)));
                }
            }

            for (int i = 0; i < 270; ++i) {
                const auto name = QStringLiteral("new-%1-%2.txt")
                        .arg(round).arg(i, 4, 10, QLatin1Char('0'));
                expected.insert(name, QByteArray(i % 31 + 1, 'x'));
                QVERIFY(writeFile(directory.filePath(name), expected.value(name)));
            }

            QSignalSpy completed(&folder, &FolderModel::listingCompleted);
            folder.setSortDesc(round % 2 == 0);
            folder.refresh();
            QTRY_VERIFY_WITH_TIMEOUT(!completed.isEmpty(), 15000);
            QTRY_COMPARE(folder.status(), FolderModel::Ready);
            QCOMPARE(folder.rowCount(), expected.size());
            auto ordered = expected.keys();

            if (round % 2 == 0)
                std::reverse(ordered.begin(), ordered.end());

            for (int row = 0; row < ordered.size(); ++row) {
                const auto item = folder.fileItem(row);
                const auto url = QUrl::fromLocalFile(directory.filePath(ordered[row]));
                QCOMPARE(item.url(), url);
                QCOMPARE(item.size(), quint64(expected.value(ordered[row]).size()));
                QCOMPARE(folder.indexForUrl(url), folder.index(row, 0));
                QCOMPARE(folder.mapFromSource(folder.mapToSource(folder.index(row, 0))),
                         folder.index(row, 0));
            }

            for (auto it = previous.cbegin(); it != previous.cend(); ++it) {
                QCOMPARE(it.value().isValid(), expected.contains(it.key()));

                if (it.value().isValid())
                    QCOMPARE(folder.itemForIndex(it.value()).name(), it.key());
            }
        }
    }

    void dragUrlsPreserveSpecialNamesAndRejectForeignIndexes()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QStringList names = {QStringLiteral("space # 100%.txt"),
                                   QString::fromUtf8("文件 🐟.txt"),
                                   QStringLiteral("line\nbreak.txt")};

        for (const auto &name : names)
            QVERIFY(writeFile(directory.filePath(name)));

        FolderModel folder;
        folder.setIsDesktop(true);
        folder.setUrl(directory.path());
        QTRY_COMPARE(folder.status(), FolderModel::Ready);
        FolderModel foreign;
        foreign.setIsDesktop(true);
        foreign.setUrl(directory.path());
        QTRY_COMPARE(foreign.status(), FolderModel::Ready);
        QModelIndexList indexes = {{}, foreign.index(0, 0)};
        QList<QUrl> expected;

        for (const auto &name : names) {
            const auto url = QUrl::fromLocalFile(directory.filePath(name));
            const auto index = folder.indexForUrl(url);
            QVERIFY(index.isValid());
            indexes.append(index);
            indexes.append(index);
            expected.append(url);
        }

        QScopedPointer<QMimeData> mime(folder.mimeData(indexes));
        QCOMPARE(mime->urls(), expected);
        QMimeData roundTrip;
        roundTrip.setData("text/uri-list", mime->data("text/uri-list"));
        QCOMPARE(roundTrip.urls(), expected);
    }

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

    void rubberBandSelectionModes()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        for (const QString &name : {"a.txt", "b.txt", "c.txt", "d.txt", "e.txt"})
            QVERIFY(writeFile(directory.filePath(name), "x"));

        FolderModel folder;
        folder.setIsDesktop(true);
        folder.setUrl(QUrl::fromLocalFile(directory.path()).toString());
        QTRY_COMPARE(folder.status(), FolderModel::Ready);
        QCOMPARE(folder.rowCount(), 5);

        const auto selectedRows = [&folder] {
            QList<int> rows;
            for (int row = 0; row < folder.rowCount(); ++row) {
                if (folder.isSelected(row))
                    rows.append(row);
            }
            return rows;
        };

        folder.setSelected(0);
        folder.pinSelection();

        folder.updateSelection({3, 2, 4, 99}, false);
        QCOMPARE(selectedRows(), QList<int>({2, 3, 4}));

        folder.updateSelection({2, 3}, false, true);
        QCOMPARE(selectedRows(), QList<int>({0, 2, 3}));

        folder.updateSelection({0, 1}, true);
        QCOMPARE(selectedRows(), QList<int>({1}));

        folder.updateSelection({}, false, true);
        QCOMPARE(selectedRows(), QList<int>({0}));

        folder.unpinSelection();
        folder.updateSelection({1}, true);
        QCOMPARE(selectedRows(), QList<int>({1}));
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

    void filterDuringRemovalDiscardsObsoletePublication()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        for (int i = 0; i < 800; ++i) {
            const auto name = QStringLiteral("%1.txt").arg(i, 4, 10, QLatin1Char('0'));
            QVERIFY(writeFile(directory.filePath(name)));
        }

        FolderModel folder;
        folder.setIsDesktop(true);
        QAbstractItemModelTester tester(&folder,
                QAbstractItemModelTester::FailureReportingMode::QtTest);
        folder.setUrl(directory.path());
        QTRY_COMPARE(folder.status(), FolderModel::Ready);
        const auto selectedUrl = QUrl::fromLocalFile(directory.filePath("0000.txt"));
        QPersistentModelIndex selected(folder.indexForUrl(selectedUrl));
        folder.setSelected(selected.row());
        QSignalSpy completed(&folder, &FolderModel::listingCompleted);
        bool changedQuery = false;

        connect(&folder, &QAbstractItemModel::rowsRemoved, this,
                [&](const QModelIndex &, int first, int last) {
            QVERIFY(last - first + 1 <= 256);

            if (changedQuery)
                return;

            changedQuery = true;
            QTimer::singleShot(0, &folder, [&] {
                folder.setFilterPattern("0000.txt");
            });
        });

        folder.setFilterMimeTypes({QStringLiteral("all/all")});
        folder.setFilterPattern("missing*");
        folder.setFilterMode(FolderModel::FilterShowMatches);
        QTRY_VERIFY(changedQuery);
        QTRY_VERIFY(!completed.isEmpty());
        QTRY_COMPARE(folder.status(), FolderModel::Ready);
        QCOMPARE(folder.rowCount(), 1);
        QVERIFY(selected.isValid());
        QCOMPARE(folder.itemForIndex(selected).url(), selectedUrl);
        QCOMPARE(folder.selectedUrls(), QList<QUrl>({selectedUrl}));
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

    // Menus are built per popup: neither the model nor a closed context menu
    // may leave a menu widget or an action behind.
    void contextMenuLeavesNothingBehind()
    {
        const auto menuCount = [] {
            const QWidgetList widgets = QApplication::allWidgets();

            return std::count_if(widgets.cbegin(), widgets.cend(), [](QWidget *widget) {
                return qobject_cast<QMenu *>(widget) != nullptr;
            });
        };
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        const auto menus = menuCount();

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(writeFile(directory.filePath("file.txt")));

        {
            FolderModel folder;
            QCOMPARE(menuCount(), menus);

            folder.setUrl(directory.path());
            QTRY_COMPARE(folder.status(), FolderModel::Ready);
            const auto actions = folder.findChildren<QAction *>(Qt::FindDirectChildrenOnly).size();

            folder.openContextMenu();
            auto *popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            QVERIFY(popup);
            popup->close();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QCOMPARE(menuCount(), menus);
            QCOMPARE(folder.findChildren<QAction *>(Qt::FindDirectChildrenOnly).size(), actions);
        }

        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(menuCount(), menus);
    }
};

QTEST_MAIN(FolderContentsTest)
#include "tst_foldercontents.moc"
