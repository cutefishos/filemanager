/*
 * Copyright (C) 2021 CutefishOS Team.
 *
 * Author:     revenmartin <revenmartin@gmail.com>
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

#include "thumbnailer.h"

#include <KIO/PreviewJob>

#include "thumbnailerjob.h"

#include <QDebug>
#include <QImage>

QQuickImageResponse *Thumbnailer::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    AsyncImageResponse *response = new AsyncImageResponse(id, requestedSize);
    return response;
}

AsyncImageResponse::AsyncImageResponse(const QString &id, const QSize &requestedSize)
    : m_id(id)
    , m_requestedSize(requestedSize)
{
    auto job_ = new ThumbnailerJob(QUrl::fromUserInput(id).toLocalFile(), requestedSize);
    connect(job_, &ThumbnailerJob::gotPreview, [this] (QPixmap pixmap) {
        m_image = pixmap.toImage();
        emit this->finished();
    });

    connect(job_, &ThumbnailerJob::failed, [this] {
        emit this->cancel();
        emit this->finished();
    });

    job_->start();



//    QStringList plugins = KIO::PreviewJob::defaultPlugins();
//    auto job = new KIO::PreviewJob(KFileItemList() << KFileItem(QUrl::fromUserInput(id)), requestedSize, &plugins);

//    connect(job, &KIO::PreviewJob::gotPreview, [this](KFileItem, QPixmap pixmap) {
//        m_image = pixmap.toImage();
//        emit this->finished();
//    });

//    connect(job, &KIO::PreviewJob::failed, [this](KFileItem) {
//        emit this->cancel();
//        emit this->finished();
//    });

//    job->start();
}

QQuickTextureFactory *AsyncImageResponse::textureFactory() const
{
    return QQuickTextureFactory::textureFactoryForImage(m_image);
}
