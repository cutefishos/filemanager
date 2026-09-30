#pragma once
#include <QString>
#include <QStringList>

namespace MimeApps {
QStringList associations(const QString &path, const QString &group, const QString &mimeType);
bool setDefault(const QString &path, const QString &mimeType, const QString &desktopId);

// xdg-terminals.list of the xdg-terminal-exec spec: desktop entry IDs, most preferred first.
QString defaultTerminal(const QString &path);
bool setDefaultTerminal(const QString &path, const QString &desktopId);
} // namespace MimeApps
