// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FILEPROPERTIES_H
#define FILEPROPERTIES_H

#include <QObject>
#include <QSharedPointer>
#include <QTimer>
#include <QUrl>
#include <filesystem.h>

#include "cio/cfilesizejob.h"

/**
 * What the properties dialog shows about one or more items. The total size is
 * computed in the background and updates fileSize as it grows.
 */
class FileProperties : public QObject
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
    explicit FileProperties(const Cutefish::IO::FileItemList &items, QObject *parent = nullptr);
    ~FileProperties() override;

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
    Cutefish::IO::FileItemList m_items;
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

#endif // FILEPROPERTIES_H
