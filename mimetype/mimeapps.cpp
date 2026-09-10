#include "mimeapps.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QSaveFile>

namespace {
QStringList readEntries(const QStringList &lines, const QString &group, const QString &key)
{
    QString current;
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith('[') && trimmed.endsWith(']'))
            current = trimmed.mid(1, trimmed.size() - 2);
        else if (current == group && !trimmed.startsWith('#')) {
            const auto equals = line.indexOf('=');
            if (equals >= 0 && line.left(equals).trimmed() == key)
                return line.mid(equals + 1).trimmed().split(';', Qt::SkipEmptyParts);
        }
    }
    return {};
}

void writeEntry(QStringList &lines, const QString &group, const QString &key,
                const QStringList &values)
{
    const QString entry = key + '=' + values.join(';') + ';';
    QString current;
    int insertion = -1;
    for (int i = 0; i < lines.size(); ++i) {
        const QString trimmed = lines.at(i).trimmed();
        if (trimmed.startsWith('[') && trimmed.endsWith(']')) {
            if (current == group)
                break;
            current = trimmed.mid(1, trimmed.size() - 2);
            if (current == group)
                insertion = i + 1;
        } else if (current == group) {
            const auto equals = lines.at(i).indexOf('=');
            if (equals >= 0 && lines.at(i).left(equals).trimmed() == key) {
                lines.removeAt(i--);
            } else {
                insertion = i + 1;
            }
        }
    }
    if (insertion < 0) {
        lines.append('[' + group + ']');
        insertion = lines.size();
    }
    lines.insert(insertion, entry);
}
} // namespace

QStringList MimeApps::associations(const QString &path, const QString &group,
                                   const QString &mimeType)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return readEntries(QString::fromUtf8(file.readAll()).split('\n'), group, mimeType);
}

bool MimeApps::setDefault(const QString &path, const QString &mimeType, const QString &desktopId)
{
    if (mimeType.isEmpty() || mimeType.contains('\n') || mimeType.contains('\r')
        || mimeType.contains('=') || desktopId.isEmpty() || desktopId.contains('\n')
        || desktopId.contains('\r') || desktopId.contains(';'))
        return false;
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QLockFile lock(path + ".lock");
    if (!lock.tryLock())
        return false;
    QFile input(path);
    QStringList lines;
    if (input.exists()) {
        if (!input.open(QIODevice::ReadOnly))
            return false;
        lines = QString::fromUtf8(input.readAll()).split('\n');
        input.close();
    }
    auto added = readEntries(lines, "Added Associations", mimeType);
    added.removeAll(desktopId);
    added.prepend(desktopId);
    writeEntry(lines, "Default Applications", mimeType, {desktopId});
    writeEntry(lines, "Added Associations", mimeType, added);
    auto removed = readEntries(lines, "Removed Associations", mimeType);
    if (removed.removeAll(desktopId))
        writeEntry(lines, "Removed Associations", mimeType, removed);
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly))
        return false;
    const auto data = (lines.join('\n') + '\n').toUtf8();
    return output.write(data) == data.size() && output.commit();
}
