// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "xdgdesktopfile.h"
#include <QTextStream>
#include <QFile>
#include <QLocale>
#include <QDebug>

XdgDesktopFile::XdgDesktopFile(const QString &fileName)
    : m_isValid(false)
    , m_fileName(fileName)
{
    if (!m_fileName.isEmpty())
        load();
}

bool XdgDesktopFile::valid() const
{
    return m_isValid;
}

QVariant XdgDesktopFile::value(const QString &key, const QVariant &defaultValue) const
{
    QString path = (!prefix().isEmpty()) ? prefix() + QLatin1Char('/') + key : key;
    QVariant res = m_items.value(path, defaultValue);
    return res;
}

bool XdgDesktopFile::load()
{
    if (!QFile::exists(m_fileName))
        return false;

    m_items.clear();

    read("Desktop Entry");

    return m_isValid;
}

QStringList XdgDesktopFile::keys() const
{
    return m_items.keys();
}

QString XdgDesktopFile::localeName() const
{
    QString localeKey = QString("Name[%1]").arg(QLocale::system().name());

    if (XdgDesktopFile::value(localeKey).toString().isEmpty())
        return XdgDesktopFile::value("Name").toString();;

    return XdgDesktopFile::value(localeKey).toString();
}

QString XdgDesktopFile::prefix() const
{
    return QLatin1String("Desktop Entry");
}

QString XdgDesktopFile::fileName() const
{
    return m_fileName;
}

bool XdgDesktopFile::read(const QString &prefix)
{
    QFile file(m_fileName);

    // Can't open file.
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream stream(&file);
    QString section;
    bool prefixExists = false;

    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();

        // Skip comments.
        if (line.startsWith("#"))
            continue;

        // Find the prefix string.
        if (line.startsWith(QChar('[')) && line.endsWith(QChar(']'))) {
            section = line.mid(1, line.length() - 2);

            if (section == prefix)
                prefixExists = true;

            continue;
        }

        QString key = line.section(QLatin1Char('='), 0, 0).trimmed();
        QString value = line.section(QLatin1Char('='), 1).trimmed();

        if (key.isEmpty())
            continue;

        m_items[section + QLatin1Char('/') + key] = QVariant(value);
    }

    m_isValid = (prefix.isEmpty()) || prefixExists;
    return m_isValid;
}
