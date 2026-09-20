#include "foldersizecache.h"

#include <QDirIterator>
#include <QFutureWatcher>
#include <QPromise>
#include <QThreadPool>

namespace {
QThreadPool &countPool()
{
    static QThreadPool pool;
    static const bool configured = [] {
        pool.setMaxThreadCount(2);
        return true;
    }();
    Q_UNUSED(configured)
    return pool;
}
}

FolderSizeCache::FolderSizeCache(QObject *parent)
    : QObject(parent)
    , m_cancelled(std::make_shared<std::atomic_bool>(false))
{
}

FolderSizeCache::~FolderSizeCache()
{
    m_cancelled->store(true);
}

void FolderSizeCache::clear()
{
    m_cancelled->store(true);
    m_cancelled = std::make_shared<std::atomic_bool>(false);
    m_counts.clear();
}

qint64 FolderSizeCache::count(const QString &path)
{
    const auto existing = m_counts.constFind(path);
    if (existing != m_counts.cend())
        return existing.value();

    m_counts.insert(path, -1);
    const auto cancelled = m_cancelled;
    auto promise = std::make_shared<QPromise<qint64>>();
    promise->start();
    auto *watcher = new QFutureWatcher<qint64>(this);
    connect(watcher, &QFutureWatcher<qint64>::finished, this,
            [this, watcher, cancelled, path] {
        if (!cancelled->load()) {
            m_counts.insert(path, watcher->result());
            emit ready(path);
        }
        watcher->deleteLater();
    });
    watcher->setFuture(promise->future());
    countPool().start([promise, cancelled, path] {
        qint64 count = 0;
        if (!cancelled->load()) {
            QDirIterator entries(path, QDir::AllEntries | QDir::NoDotAndDotDot);
            while (!cancelled->load() && entries.hasNext()) {
                entries.next();
                ++count;
            }
        }
        promise->addResult(count);
        promise->finish();
    });
    return -1;
}
