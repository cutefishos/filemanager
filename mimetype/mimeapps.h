#pragma once
#include <QString>
#include <QStringList>

namespace MimeApps {
QStringList associations(const QString &path, const QString &group, const QString &mimeType);
bool setDefault(const QString &path, const QString &mimeType, const QString &desktopId);
} // namespace MimeApps
