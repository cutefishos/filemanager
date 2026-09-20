#pragma once

#include <QImage>
#include <QSize>
#include <QString>

class ThumbnailCache
{
public:
    static ThumbnailCache *self();

    QString requestThumbnail(const QString &filePath, const QSize &requestedSize);
    QString generateThumbnail(const QString &source, const QString &target,
                              const QSize &requestedSize);
    QString writeCacheFile(const QString &path, const QImage &image);
};
