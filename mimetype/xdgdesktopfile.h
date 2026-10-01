// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef XDGDESKTOPFILE_H
#define XDGDESKTOPFILE_H

#include <QObject>
#include <QVariant>
#include <QMap>

class XdgDesktopFile
{
public:
    explicit XdgDesktopFile(const QString &fileName = QString());

    bool valid() const;

    QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;
    void setValue(const QString &key, const QVariant &value);

    bool load();
    bool save();

    QStringList keys() const;

    QString localeName() const;
    QString prefix() const;

    QString fileName() const;

private:
    bool read(const QString &prefix);

private:
    bool m_isValid;
    QString m_fileName;
    QMap<QString, QVariant> m_items;
};

#endif // XDGDESKTOPFILE_H
