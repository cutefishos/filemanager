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

#include "propertiesdialog.h"
#include "../desktopiconprovider.h"

#include <KFileItemListProperties>
#include <KIO/CopyJob>

#include <QDir>
#include <QFileInfo>

#include <QQmlApplicationEngine>
#include <QQuickView>
#include <QQmlContext>
#include <QDebug>

inline QString concatPaths(const QString &path1, const QString &path2)
{
    Q_ASSERT(!path2.startsWith(QLatin1Char('/')));

    if (path1.isEmpty()) {
        return path2;
    } else if (!path1.endsWith(QLatin1Char('/'))) {
        return path1 + QLatin1Char('/') + path2;
    } else {
        return path1 + path2;
    }
}

PropertiesDialog::PropertiesDialog(const KFileItem &item, QObject *parent)
    : QObject(parent)
{
    m_items.append(item);
    init();
}

PropertiesDialog::PropertiesDialog(const KFileItemList &items, QObject *parent)
    : QObject(parent)
{
    m_items = items;
    init();
}

PropertiesDialog::PropertiesDialog(const QUrl &url, QObject *parent)
    : QObject(parent)
{
    m_items.append(KFileItem(url));
    init();
}

PropertiesDialog::~PropertiesDialog()
{
    if (m_dirSizeJob)
        m_dirSizeJob->kill();
}

void PropertiesDialog::showDialog(const KFileItem &item)
{
    PropertiesDialog *dlg = new PropertiesDialog(item);
    QQmlApplicationEngine *engine = new QQmlApplicationEngine;
    engine->addImageProvider(QStringLiteral("icontheme"), new DesktopIconProvider());
    engine->rootContext()->setContextProperty("main", dlg);
    engine->load(QUrl("qrc:/qml/Dialogs/PropertiesDialog.qml"));
}

void PropertiesDialog::showDialog(const KFileItemList &items)
{
    PropertiesDialog *dlg = new PropertiesDialog(items);
    QQmlApplicationEngine *engine = new QQmlApplicationEngine;
    engine->addImageProvider(QStringLiteral("icontheme"), new DesktopIconProvider());
    engine->rootContext()->setContextProperty("main", dlg);
    engine->load(QUrl("qrc:/qml/Dialogs/PropertiesDialog.qml"));
}

bool PropertiesDialog::multiple() const
{
    return m_multiple;
}

bool PropertiesDialog::isWritable() const
{
    return m_items.first().isWritable();
}

QString PropertiesDialog::location() const
{
    return m_location;
}

QString PropertiesDialog::fileName() const
{
    return m_fileName;
}

QString PropertiesDialog::iconName() const
{
    return m_iconName;
}

QString PropertiesDialog::mimeType() const
{
    return m_mimeType;
}

QString PropertiesDialog::size() const
{
    return m_size;
}

QString PropertiesDialog::creationTime() const
{
    return m_creationTime;
}

QString PropertiesDialog::modifiedTime() const
{
    return m_modifiedTime;
}

QString PropertiesDialog::accessedTime() const
{
    return m_accessedTime;
}

KFileItemList PropertiesDialog::items() const
{
    return m_items;
}

void PropertiesDialog::accept(const QString &text)
{
    KFileItemList list = items();

    if (list.size() == 1) {
        KFileItem item = list.first();

        QString n = text;
        while (!n.isEmpty() && n[n.length() - 1].isSpace())
            n.chop(1);

        if (n.isEmpty())
            return;

        QString newFileName = KIO::encodeFileName(n);

        if (fileName() != newFileName) {
            QUrl newUrl;

            if (!location().isEmpty()) {
                newUrl = location();
                newUrl.setPath(concatPaths(newUrl.path(), newFileName));
                newUrl.setScheme(item.url().scheme());

                auto job = KIO::move(item.url(), newUrl, KIO::HideProgressInfo);
                job->start();
            }
        }
    }
}

void PropertiesDialog::reject()
{
}

void PropertiesDialog::init()
{
    m_multiple = m_items.count() > 1;

    m_dirSizeJob = KIO::directorySize(m_items);

    connect(m_dirSizeJob, &KIO::DirectorySizeJob::result, this, &PropertiesDialog::slotDirSizeFinished);

    if (!m_multiple) {
        KFileItem item = m_items.first();

        QString path;
        m_fileName = m_items.first().name();

        if (item.isDir())
            m_iconName = "folder";
        else
            m_iconName = m_items.first().iconName();

        m_mimeType = m_items.first().mimetype();
        m_size = KIO::convertSize(m_items.first().size());
        m_location = QFileInfo(m_items.first().localPath()).dir().path();

        m_creationTime = item.time(KFileItem::CreationTime).toString();
        m_modifiedTime = item.time(KFileItem::ModificationTime).toString();
        m_accessedTime = item.time(KFileItem::AccessTime).toString();
    } else {
        m_fileName = tr("%1 files").arg(m_items.count());
        m_location = QFileInfo(m_items.first().localPath()).dir().path();
    }
}

void PropertiesDialog::slotDirSizeFinished(KJob *job)
{
    if (job->error())
        return;

    m_size = KIO::convertSize(m_dirSizeJob->totalSize());

    m_dirSizeJob = 0;

    emit sizeChanged();
}
