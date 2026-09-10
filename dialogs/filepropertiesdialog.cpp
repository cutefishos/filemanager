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

#include "filepropertiesdialog.h"

#include <QDir>
#include <QFileInfo>
#include <QQmlContext>

FilePropertiesDialog::FilePropertiesDialog(const Cutefish::IO::FileItem &item, QObject *parent)
    : Window(parent)
{
    m_items.append(item);
    init();
}

FilePropertiesDialog::FilePropertiesDialog(const Cutefish::IO::FileItemList &items, QObject *parent)
    : Window(parent)
    , m_items(items)
{
    init();
}

FilePropertiesDialog::FilePropertiesDialog(const QUrl &url, QObject *parent)
    : Window(parent)
{
    m_items.append(Cutefish::IO::FileItem(url));

    init();
}

FilePropertiesDialog::~FilePropertiesDialog()
{
    if (m_sizeJob) {
        m_sizeJob->stop();
        m_sizeJob.reset();
    }
}

bool FilePropertiesDialog::multiple() const
{
    return m_multiple;
}

QString FilePropertiesDialog::location() const
{
    return m_location;
}

QString FilePropertiesDialog::fileName() const
{
    return m_fileName;
}

QString FilePropertiesDialog::iconName() const
{
    return m_iconName;
}

QString FilePropertiesDialog::mimeType() const
{
    return m_mimeType;
}

QString FilePropertiesDialog::fileSize() const
{
    return m_size;
}

QString FilePropertiesDialog::creationTime() const
{
    return m_creationTime;
}

QString FilePropertiesDialog::modifiedTime() const
{
    return m_modifiedTime;
}

QString FilePropertiesDialog::accessedTime() const
{
    return m_accessedTime;
}

void FilePropertiesDialog::init()
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

    // Populate the properties before loading the QML window. Its size is
    // derived from the content, so changing these values after load can
    // resize the window during its first expose.
    rootContext()->setContextProperty("main", this);
    load(QUrl("qrc:/qml/Dialogs/PropertiesDialog.qml"));

    m_sizeJob = std::shared_ptr<CFileSizeJob>(new CFileSizeJob);
    connect(m_sizeJob.get(), &CFileSizeJob::sizeChanged, this,
            &FilePropertiesDialog::updateTotalSize);
    connect(m_sizeJob.get(), &CFileSizeJob::result, this, &FilePropertiesDialog::updateTotalSize);
    m_sizeJob->start(list);
}

void FilePropertiesDialog::updateTotalSize()
{
    if (!m_sizeJob)
        return;

    m_size = Cutefish::IO::convertSize(m_sizeJob->totalSize());
    emit fileSizeChanged();
}
