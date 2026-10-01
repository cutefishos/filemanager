// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fileproperties.h"

#include <QDir>
#include <QFileInfo>

FileProperties::FileProperties(const Cutefish::IO::FileItemList &items, QObject *parent)
    : QObject(parent)
    , m_items(items)
{
    init();
}

FileProperties::~FileProperties()
{
    if (m_sizeJob) {
        m_sizeJob->stop();
        m_sizeJob.reset();
    }
}

bool FileProperties::multiple() const
{
    return m_multiple;
}

QString FileProperties::location() const
{
    return m_location;
}

QString FileProperties::fileName() const
{
    return m_fileName;
}

QString FileProperties::iconName() const
{
    return m_iconName;
}

QString FileProperties::mimeType() const
{
    return m_mimeType;
}

QString FileProperties::fileSize() const
{
    return m_size;
}

QString FileProperties::creationTime() const
{
    return m_creationTime;
}

QString FileProperties::modifiedTime() const
{
    return m_modifiedTime;
}

QString FileProperties::accessedTime() const
{
    return m_accessedTime;
}

void FileProperties::init()
{
    m_multiple = m_items.count() > 1;

    QList<QUrl> list;
    for (Cutefish::IO::FileItem item : m_items) {
        list.append(item.url());
    }

    if (!m_multiple) {
        Cutefish::IO::FileItem item = m_items.first();
        QFileInfo info(item.localPath());

        QString path;
        m_fileName = m_items.first().name();

        if (item.isDir())
            m_iconName = "folder";
        else
            m_iconName = m_items.first().iconName();

        m_mimeType = m_items.first().mimetype();
        m_size = Cutefish::IO::convertSize(m_items.first().size());
        m_location = info.dir().path();

        m_creationTime = item.time(Cutefish::IO::FileItem::CreationTime).toString();
        m_modifiedTime = item.time(Cutefish::IO::FileItem::ModificationTime).toString();
        m_accessedTime = item.time(Cutefish::IO::FileItem::AccessTime).toString();

        emit fileNameChanged();
        emit iconNameChanged();
        emit mimeTypeChanged();
        emit fileSizeChanged();
        emit locationChanged();
        emit creationTimeChanged();
        emit modifiedTimeChanged();
        emit accessedTimeChanged();
    } else {
        m_fileName = tr("%1 files").arg(m_items.count());
        m_location = QFileInfo(m_items.first().localPath()).dir().path();
        m_iconName = "unknown";

        emit fileNameChanged();
        emit locationChanged();
        emit iconNameChanged();
    }

    m_sizeJob = std::shared_ptr<CFileSizeJob>(new CFileSizeJob);
    connect(m_sizeJob.get(), &CFileSizeJob::sizeChanged, this,
            &FileProperties::updateTotalSize);
    connect(m_sizeJob.get(), &CFileSizeJob::result, this, &FileProperties::updateTotalSize);
    m_sizeJob->start(list);
}

void FileProperties::updateTotalSize()
{
    if (!m_sizeJob)
        return;

    m_size = Cutefish::IO::convertSize(m_sizeJob->totalSize());
    emit fileSizeChanged();
}
