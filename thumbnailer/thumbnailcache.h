/*
 * Copyright (C) 2021 CutefishOS Team.
 *
 * Author:     Reion Wong <reionwong@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef THUMBNAILCACHE_H
#define THUMBNAILCACHE_H

#include <QObject>

class QImageReader;
class ThumbnailCache : public QObject
{
    Q_OBJECT

public:
    static ThumbnailCache *self();
    explicit ThumbnailCache(QObject *parent = nullptr);

    QString requestThumbnail(const QString &filePath, const QSize &requestedSize);
    QString generateThumbnail(const QString &source, const QString &target, const QSize &requestedSize);
    QString writeCacheFile(const QString &path, const QImage &image);

private:
    // thumbnail cache folder
    QString m_thumbnailsDir;
};

#endif // THUMBNAILCACHE_H
