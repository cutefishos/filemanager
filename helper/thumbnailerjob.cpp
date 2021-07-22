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

#include "thumbnailerjob.h"
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QFile>
#include <QDir>
#include <QImage>
#include <QPixmap>
#include <QSaveFile>
#include <QDebug>
#include <QImageReader>

#include <sys/ipc.h>
#include <sys/shm.h>

ThumbnailerJob::ThumbnailerJob(const QString &fileName, const QSize &size, QObject *parent)
    : QThread(parent)
    , m_url(QUrl::fromUserInput(fileName))
    , m_size(size)
    , shmaddr(nullptr)
    , shmid(-1)
{
    // http://specifications.freedesktop.org/thumbnail-spec/thumbnail-spec-latest.html#DIRECTORY
    m_thumbnailsDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QLatin1String("/thumbnails/");
}

ThumbnailerJob::~ThumbnailerJob()
{
    if (shmaddr) {
        shmdt((char *)shmaddr);
        shmctl(shmid, IPC_RMID, nullptr);
    }
}

void ThumbnailerJob::run()
{
    if (!QFile::exists(m_url.toLocalFile())) {
        emit failed();
        return;
    }

    int width = m_size.width();
    int height = m_size.height();
    int cacheSize = 0;
    bool needCache = false;

    // 首先需要找到目录
    if (width <= 128 && height <= 128)
        cacheSize = 128;
    else if (width <= 256 && height <= 256)
        cacheSize = 256;
    else
        cacheSize = 512;

    struct CachePool {
        QString path;
        int minSize;
    };

    const static auto pools = {
        CachePool{QStringLiteral("/normal/"), 128},
        CachePool{QStringLiteral("/large/"), 256},
        CachePool{QStringLiteral("/x-large/"), 512},
        CachePool{QStringLiteral("/xx-large/"), 1024},
    };

    QString thumbDir;
    int wants = /*devicePixelRatio **/ cacheSize;
    for (const auto &p : pools) {
        if (p.minSize < wants) {
            continue;
        } else {
            thumbDir = p.path;
            break;
        }
    }

    m_thumbnailsPath = m_thumbnailsDir + thumbDir;

    // 不存在需要创建路径
    if (!QDir(m_thumbnailsPath).exists()) {
        if (QDir().mkpath(m_thumbnailsPath)) {
            QFile f(m_thumbnailsPath);
            f.setPermissions(QFile::ReadUser | QFile::WriteUser | QFile::ExeUser); // 0700
        }
    }

    // 求出文件的 md5
    QByteArray origName;
    const QFileInfo info(m_url.toLocalFile());
    const QString canonicalPath = info.canonicalFilePath();
    origName = QUrl::fromLocalFile(canonicalPath).toEncoded(QUrl::RemovePassword | QUrl::FullyEncoded);

    if (origName.isEmpty()) {
        emit failed();
        return;
    }

    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(origName);
    m_thumbnailsName = QString::fromLatin1(md5.result().toHex()) + QLatin1String(".png");

    // 是否需要生成缓存
    needCache = !QFile::exists(m_thumbnailsPath + m_thumbnailsName);

    if (needCache) {
        QFile f(m_url.toLocalFile());
        if (f.open(QIODevice::ReadOnly)) {
            QByteArray data = f.readAll();
            QImage thumb;
            thumb.loadFromData(data);
            thumb = thumb.scaled(m_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

            // 保存 cache 文件
            QSaveFile saveFile(m_thumbnailsPath + m_thumbnailsName);
            if (saveFile.open(QIODevice::WriteOnly)) {
                if (thumb.save(&saveFile, "PNG")) {
                    saveFile.commit();
                }
            }

            emitPreview(thumb);
        }
    } else {
        QFile f(m_thumbnailsPath + m_thumbnailsName);
        if (f.open(QIODevice::ReadOnly)) {
            QByteArray data = f.readAll();
            QImage thumb;
            thumb.loadFromData(data);
            emitPreview(thumb);
        }
    }
}

void ThumbnailerJob::emitPreview(const QImage &image)
{
    QPixmap pixmap;

    if (image.width() > m_size.width() || image.height() > m_size.height()) {
        pixmap = QPixmap::fromImage(image.scaled(m_size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        pixmap = QPixmap::fromImage(image);
    }

    emit gotPreview(pixmap);
}
