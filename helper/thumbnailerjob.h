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

#ifndef THUMBNAILERJOB_H
#define THUMBNAILERJOB_H

#include <QObject>
#include <QThread>
#include <QSize>
#include <QUrl>

class ThumbnailerJob : public QThread
{
    Q_OBJECT

public:
    explicit ThumbnailerJob(const QString &fileName, const QSize &size, QObject *parent = nullptr);
    ~ThumbnailerJob();

    void run() override;

signals:
    void gotPreview(const QPixmap &pixmap);
    void failed();

private:
    void emitPreview(const QImage &image);

private:
    QUrl m_url;
    QSize m_size;

    // thumbnail cache folder
    QString m_thumbnailsDir;
    QString m_thumbnailsPath;
    QString m_thumbnailsName;

    uchar *shmaddr;
    size_t shmsize;
    int shmid;
};

#endif // THUMBNAILERJOB_H
