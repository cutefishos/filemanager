#include "thumbnailprovider.h"
#include "thumbnailcache.h"

#include <QImage>
#include <QQuickTextureFactory>
#include <QThreadPool>
#include <atomic>

namespace {
QThreadPool &thumbnailPool()
{
    static QThreadPool pool;
    static const bool configured = [] {
        pool.setMaxThreadCount(2);
        return true;
    }();
    Q_UNUSED(configured)
    return pool;
}

class ThumbnailResponse final : public QQuickImageResponse
{
public:
    ThumbnailResponse(const QString &id, const QSize &requestedSize)
    {
        thumbnailPool().start([this, id, requestedSize] {
            if (!m_cancelled.load() && requestedSize.isValid()) {
                const QString path = ThumbnailCache::self()->requestThumbnail(id, requestedSize);
                if (!m_cancelled.load() && !path.isEmpty())
                    m_image = QImage(path);
            }
            // Qt keeps the response alive until finished, including cancelled requests.
            emit finished();
        });
    }

    void cancel() override
    {
        m_cancelled.store(true);
    }

    QQuickTextureFactory *textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

private:
    QImage m_image;
    std::atomic_bool m_cancelled = false;
};
}

QQuickImageResponse *ThumbnailProvider::requestImageResponse(const QString &id,
                                                             const QSize &requestedSize)
{
    return new ThumbnailResponse(id, requestedSize);
}
