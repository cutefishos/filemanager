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

#ifndef FILEPROPERTIESDIALOG_H
#define FILEPROPERTIESDIALOG_H

#include "../window.h"
#include <QTimer>
#include <QUrl>
#include <QSharedPointer>

#include <KFileItem>
#include <KIO/DirectorySizeJob>

#include "cio/cfilesizejob.h"

class FilePropertiesDialog : public Window
{
    Q_OBJECT
    Q_PROPERTY(QString location READ location NOTIFY locationChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileNameChanged)
    Q_PROPERTY(QString iconName READ iconName NOTIFY iconNameChanged)
    Q_PROPERTY(QString mimeType READ mimeType NOTIFY mimeTypeChanged)
    Q_PROPERTY(QString fileSize READ fileSize NOTIFY fileSizeChanged)
    Q_PROPERTY(QString creationTime READ creationTime NOTIFY creationTimeChanged)
    Q_PROPERTY(QString modifiedTime READ modifiedTime NOTIFY modifiedTimeChanged)
    Q_PROPERTY(QString accessedTime READ accessedTime NOTIFY accessedTimeChanged)
    Q_PROPERTY(bool multiple READ multiple CONSTANT)

public:
    explicit FilePropertiesDialog(const KFileItem &item, QObject *parent = nullptr);
    explicit FilePropertiesDialog(const KFileItemList &items, QObject *parent = nullptr);
    explicit FilePropertiesDialog(const QUrl &url, QObject *parent = nullptr);
    ~FilePropertiesDialog();

    bool multiple() const;

    QString location() const;
    QString fileName() const;
    QString iconName() const;
    QString mimeType() const;
    QString fileSize() const;

    QString creationTime() const;
    QString modifiedTime() const;
    QString accessedTime() const;

signals:
    void locationChanged();
    void fileNameChanged();
    void iconNameChanged();
    void mimeTypeChanged();
    void fileSizeChanged();

    void creationTimeChanged();
    void modifiedTimeChanged();
    void accessedTimeChanged();

private:
    void init();

private slots:
    void updateTotalSize();

private:
    KFileItemList m_items;
    QString m_location;
    QString m_fileName;
    QString m_iconName;
    QString m_mimeType;
    QString m_size;
    QString m_creationTime;
    QString m_modifiedTime;
    QString m_accessedTime;

    std::shared_ptr<CFileSizeJob> m_sizeJob;

    bool m_multiple = false;
};

#endif // FILEPROPERTIESDIALOG_H
