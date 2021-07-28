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

#include "xdgdesktopfile.h"
#include <QTextStream>
#include <QFile>
#include <QLocale>

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

void XdgDesktopFile::setValue(const QString &key, const QVariant &value)
{
    QString path = (!prefix().isEmpty()) ? prefix() + QLatin1Char('/') + key : key;

    if (value.type() == QVariant::String) {
        QString s = value.toString();
        m_items[path] = QVariant(s);
    } else {
        m_items[path] = value;
    }
}

bool XdgDesktopFile::load()
{
    if (!QFile::exists(m_fileName))
        return false;

    m_items.clear();

    read("Desktop Entry");

    return m_isValid;
}

bool XdgDesktopFile::save()
{
    QFile file(m_fileName);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream stream(&file);
    QMap<QString, QVariant>::const_iterator i = m_items.constBegin();
    QString section;

    while (i != m_items.constBegin()) {
        QString path = i.key();
        QString sect = path.section(QChar('/'), 0, 0);

        if (sect != section) {
            section = sect;
            stream << QLatin1Char('[') << section << QChar(']') << Qt::endl;
        }

        QString key = path.section(QChar('/'), 1);
        stream << key << QLatin1Char('=') << i.value().toString() << Qt::endl;
        ++i;
    }

    return true;
}

QString XdgDesktopFile::localeName() const
{
    QString localeKey = QString("Name[%1]").arg(QLocale::system().name());

    if (m_items.contains(localeKey)) {
        return m_items[localeKey].toString();
    }

    return m_items["Name"].toString();
}

QString XdgDesktopFile::prefix() const
{
    return QLatin1String("Desktop Entry");
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
