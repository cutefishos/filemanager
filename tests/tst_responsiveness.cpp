#include "model/foldersizecache.h"
#include "model/foldermodel.h"
#include "thumbnailer/thumbnailcache.h"
#include "thumbnailer/thumbnailprovider.h"

#include <directory.h>
#include <fileenumerator.h>
#include <QFile>
#include <QQuickTextureFactory>
#include <QSemaphore>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QtTest>
#include <memory>

class ResponsivenessTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_cacheDirectory.isValid());
        qputenv("XDG_CACHE_HOME", m_cacheDirectory.path().toUtf8());
    }

    void folderCountsAreDeferredAndInvalidated()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile file(directory.filePath("visible"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        QFile hidden(directory.filePath(".hidden"));
        QVERIFY(hidden.open(QIODevice::WriteOnly));
        hidden.close();

        FolderSizeCache cache;
        QSignalSpy ready(&cache, &FolderSizeCache::ready);
        QCOMPARE(cache.count(directory.path()), -1);
        QCOMPARE(cache.count(directory.path()), -1);
        QTRY_COMPARE(ready.count(), 1);
        QCOMPARE(cache.count(directory.path()), 1);

        cache.clear();
        QCOMPARE(cache.count(directory.path()), -1);
        cache.clear();
        QVERIFY(QDir(directory.path()).mkdir("child"));
        QCOMPARE(cache.count(directory.path()), -1);
        QTRY_COMPARE(ready.count(), 2);
        QCOMPARE(cache.count(directory.path()), 2);
    }

    void closingListerDoesNotWaitForQueuedScan()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QThreadPool *pool = QThreadPool::globalInstance();
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
        auto *lister = new Cutefish::IO::DirectoryLister;
        lister->openUrl(QUrl::fromLocalFile(directory.path()));

        // Release from another thread even if a blocking destructor regresses.
        QThreadPool releasePool;
        releasePool.start([gate] {
            QThread::msleep(1000);
            gate->release();
        });
        QElapsedTimer elapsed;
        elapsed.start();
        delete lister;
        const qint64 teardownTime = elapsed.elapsed();
        releasePool.waitForDone();
        pool->waitForDone();
        pool->setMaxThreadCount(previousLimit);
        QVERIFY2(teardownTime < 500, "Closing the lister waited for filesystem work");
    }

    void listingPublishesOneInsertionAndCachedMetadata()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        for (int i = 0; i < 200; ++i) {
            QFile file(directory.filePath(QString::number(i) + ".txt"));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("plain text\n");
        }
        Cutefish::IO::DirectoryLister lister;
        Cutefish::IO::DirectoryModel model;
        model.setDirLister(&lister);
        QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
        lister.openUrl(QUrl::fromLocalFile(directory.path()));
        QTRY_COMPARE(model.rowCount(), 200);
        QCOMPARE(inserted.count(), 1);
        const auto item = model.itemForIndex(model.index(0, 0));
        QVERIFY(QFile::remove(item.localPath()));
        QCOMPARE(item.mimetype(), QStringLiteral("text/plain"));
        QCOMPARE(item.size(), quint64(11));
    }

    void navigationStartsWithoutWaitingForPreviousDelivery()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(QDir(directory.path()).mkdir("first"));
        QVERIFY(QDir(directory.path()).mkdir("second"));
        QFile file(directory.filePath("second/only.txt"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();

        Cutefish::IO::DirectoryLister lister;
        QSignalSpy started(&lister, &Cutefish::IO::DirectoryLister::started);
        QSignalSpy completed(&lister, &Cutefish::IO::DirectoryLister::completed);
        lister.openUrl(QUrl::fromLocalFile(directory.filePath("first")));
        lister.openUrl(QUrl::fromLocalFile(directory.filePath("second")));
        QCOMPARE(started.count(), 2);
        QTRY_COMPARE(completed.count(), 1);
        QCOMPARE(lister.items().size(), 1);
        QCOMPARE(lister.items().first().name(), QStringLiteral("only.txt"));
    }

    void navigationFromItemsChangedDoesNotCompleteOldListing()
    {
        QTemporaryDir first;
        QTemporaryDir second;
        QVERIFY(first.isValid());
        QVERIFY(second.isValid());
        Cutefish::IO::DirectoryLister lister;
        QSignalSpy completed(&lister, &Cutefish::IO::DirectoryLister::completed);
        int deliveries = 0;
        connect(&lister, &Cutefish::IO::DirectoryLister::itemsChanged, this, [&] {
            if (++deliveries == 1)
                lister.openUrl(QUrl::fromLocalFile(second.path()));
        });
        lister.openUrl(QUrl::fromLocalFile(first.path()));
        QTRY_COMPARE(deliveries, 2);
        QCOMPARE(completed.count(), 1);
        QCOMPARE(lister.url(), QUrl::fromLocalFile(second.path()));
    }

    void collationTiesHaveStrictOrdering()
    {
        const QLocale previousLocale;
        const auto restoreLocale = qScopeGuard([previousLocale] {
            QLocale::setDefault(previousLocale);
        });
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
        const QString first = QString::fromUtf8("\xc3\xa9.txt");
        const QString second = QString::fromUtf8("e\xcc\x81.txt");
        if (QCollator().compare(first, second) != 0)
            QSKIP("The platform collator does not equate canonical Unicode forms");
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        for (const QString &name : {first, second}) {
            QFile file(directory.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        FolderModel folder;
        folder.setUrl(QUrl::fromLocalFile(directory.path()).toString());
        QTRY_COMPARE(folder.rowCount(), 2);
        QTRY_COMPARE(folder.status(), FolderModel::Ready);
        const auto left = folder.fileItem(0).url();
        const auto right = folder.fileItem(1).url();
        QVERIFY(left.toString() < right.toString());

        QSignalSpy sorted(&folder, &FolderModel::listingCompleted);
        folder.setSortDesc(true);
        QTRY_COMPARE(sorted.count(), 1);
        QCOMPARE(folder.fileItem(0).url(), right);
        QCOMPARE(folder.fileItem(1).url(), left);
    }

    void enumerationChecksCancellationBetweenEntries()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        for (int i = 0; i < 3; ++i) {
            QFile file(directory.filePath(QString::number(i)));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        Cutefish::IO::OperationContext context;
        int visited = 0;
        context.currentItem = [&](const QString &) {
            ++visited;
            context.canceled.store(true);
        };
        QVERIFY_EXCEPTION_THROWN(
                Cutefish::IO::FileEnumerator::list(QUrl::fromLocalFile(directory.path()),
                                                   true, &context),
                Cutefish::IO::Failure);
        QCOMPARE(visited, 1);
    }

    void thumbnailCacheHandlesUrlsAndRepairsCorruption()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString source = directory.filePath("image # %.png");
        QImage image(600, 300, QImage::Format_ARGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(source));
        auto *cache = ThumbnailCache::self();
        const QString url = QUrl::fromLocalFile(source).toString(QUrl::FullyEncoded);
        const QString first = cache->requestThumbnail(url, QSize(80, 80));
        QVERIFY(!first.isEmpty());
        QCOMPARE(QImage(first).size(), QSize(128, 64));
        QCOMPARE(cache->requestThumbnail(url, QSize(100, 100)), first);
        QFile damaged(first);
        QVERIFY(damaged.open(QIODevice::WriteOnly | QIODevice::Truncate));
        damaged.write("incomplete");
        damaged.close();
        QCOMPARE(cache->requestThumbnail(url, QSize(80, 80)), first);
        QVERIFY(!QImage(first).isNull());
        QVERIFY(cache->writeCacheFile(directory.filePath("invalid.png"), QImage()).isEmpty());
        QVERIFY(QFile::remove(first));
    }

    void asynchronousThumbnailsSurviveProviderDestruction()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString source = directory.filePath("image.png");
        QImage image(256, 128, QImage::Format_RGB32);
        image.fill(Qt::blue);
        QVERIFY(image.save(source));

        auto provider = std::make_unique<ThumbnailProvider>();
        std::unique_ptr<QQuickImageResponse> response(
                provider->requestImageResponse(source, QSize(64, 64)));
        QSignalSpy finished(response.get(), &QQuickImageResponse::finished);
        std::unique_ptr<QQuickImageResponse> cancelled(
                provider->requestImageResponse(source, QSize(64, 64)));
        QSignalSpy cancelledFinished(cancelled.get(), &QQuickImageResponse::finished);
        cancelled->cancel();
        provider.reset();
        QTRY_COMPARE(finished.count(), 1);
        QTRY_COMPARE(cancelledFinished.count(), 1);
        std::unique_ptr<QQuickTextureFactory> texture(response->textureFactory());
        QVERIFY(texture);
        QCOMPARE(texture->image().pixelColor(0, 0), QColor(Qt::blue));
        QVERIFY(QFile::remove(ThumbnailCache::self()->requestThumbnail(source, QSize(64, 64))));
    }
private:
    QTemporaryDir m_cacheDirectory;
};

QTEST_MAIN(ResponsivenessTest)
#include "tst_responsiveness.moc"
