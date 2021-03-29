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

#ifndef PROPERTIESDIALOG_H
#define PROPERTIESDIALOG_H

#include <QObject>
#include <QUrl>

#include <KFileItem>
#include <KIO/DirectorySizeJob>

class PropertiesDialog : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString location READ location CONSTANT)
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileNameChanged)
    Q_PROPERTY(QString iconName READ iconName NOTIFY iconNameChanged)
    Q_PROPERTY(QString mimeType READ mimeType CONSTANT)
    Q_PROPERTY(QString size READ size NOTIFY sizeChanged)
    Q_PROPERTY(QString creationTime READ creationTime CONSTANT)
    Q_PROPERTY(QString modifiedTime READ modifiedTime CONSTANT)
    Q_PROPERTY(QString accessedTime READ accessedTime CONSTANT)
    Q_PROPERTY(bool multiple READ multiple CONSTANT)
    Q_PROPERTY(bool isWritable READ isWritable CONSTANT)

public:
    explicit PropertiesDialog(const KFileItem &item, QObject *parent = nullptr);
    explicit PropertiesDialog(const KFileItemList &items, QObject *parent = nullptr);
    explicit PropertiesDialog(const QUrl &url, QObject *parent = nullptr);
    ~PropertiesDialog();

    static void showDialog(const KFileItem &item);
    static void showDialog(const KFileItemList &items);

    bool multiple() const;
    bool isWritable() const;

    QString location() const;
    QString fileName() const;
    QString iconName() const;
    QString mimeType() const;
    QString size() const;

    QString creationTime() const;
    QString modifiedTime() const;
    QString accessedTime() const;

    KFileItemList items() const;

    Q_INVOKABLE void accept(const QString &text);
    Q_INVOKABLE void reject();

signals:
    void fileNameChanged();
    void iconNameChanged();
    void sizeChanged();

private:
    void init();

private slots:
    void slotDirSizeFinished(KJob *job);

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

    KIO::DirectorySizeJob *m_dirSizeJob;

    bool m_multiple;
};

#endif // PROPERTIESDIALOG_H
