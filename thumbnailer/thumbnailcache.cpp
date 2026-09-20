#include "thumbnailcache.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

ThumbnailCache *ThumbnailCache::self()
{
    static ThumbnailCache cache;
    return &cache;
}

QString ThumbnailCache::requestThumbnail(const QString &filePath, const QSize &requestedSize)
{
    if (!requestedSize.isValid() || requestedSize.isEmpty())
        return {};

    const QUrl url(filePath);
    const QString path = url.isLocalFile() ? url.toLocalFile() : filePath;
    const QFileInfo info(path);
    if (!info.isFile())
        return {};

    const int extent = qMax(requestedSize.width(), requestedSize.height());
    const int cacheSize = extent <= 128 ? 128 : extent <= 256 ? 256 : extent <= 512 ? 512 : 1024;
    const QString directory =
            QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
            + QStringLiteral("/cutefish/thumbnails/") + QString::number(cacheSize);
    if (!QDir().mkpath(directory))
        return {};

    QByteArray key = QUrl::fromLocalFile(info.absoluteFilePath()).toEncoded();
    key += '\0';
    key += QByteArray::number(info.lastModified().toMSecsSinceEpoch());
    key += ':';
    key += QByteArray::number(info.size());
    const QString target = directory + '/'
            + QString::fromLatin1(QCryptographicHash::hash(key, QCryptographicHash::Md5).toHex())
            + QStringLiteral(".png");
    if (QFileInfo::exists(target) && !QImage(target).isNull())
        return target;

    return generateThumbnail(path, target, QSize(cacheSize, cacheSize));
}

QString ThumbnailCache::generateThumbnail(const QString &source, const QString &target,
                                          const QSize &requestedSize)
{
    QImageReader reader(source);
    const QSize originalSize = reader.size();
    if (!originalSize.isValid() || requestedSize.isEmpty())
        return {};

    reader.setQuality(49);
    reader.setAutoTransform(true);
    reader.setScaledSize(originalSize.scaled(requestedSize, Qt::KeepAspectRatio));
    QImage image = reader.read();
    if (image.isNull())
        return {};
    if (image.width() > requestedSize.width() || image.height() > requestedSize.height())
        image = image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return writeCacheFile(target, image);
}

QString ThumbnailCache::writeCacheFile(const QString &path, const QImage &image)
{
    if (image.isNull())
        return {};

    // Concurrent requests and separate processes must never see a partial cache entry.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || !image.save(&file, "PNG") || !file.commit())
        return {};
    return path;
}
