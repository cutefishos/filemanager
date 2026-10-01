// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "filelauncher.h"

#include "applicationlauncher.h"
#include "applicationruntime.h"
#include "desktopentry.h"

#include <QFile>
#include <QFileInfo>

FileLauncher *SELF = nullptr;

FileLauncher *FileLauncher::self()
{
    if (SELF == nullptr)
        SELF = new FileLauncher;

    return SELF;
}

FileLauncher::FileLauncher(QObject *parent)
    : QObject(parent)
{

}

bool FileLauncher::launchApp(const QString &desktopFile, const QString &fileName)
{
    const QStringList arguments = fileName.isEmpty() ? QStringList() : QStringList {fileName};

    // The runtime knows the entry: let it expand Exec and track the instance
    // under the application's own id.
    if (ApplicationRuntime::instance()->launchApplication(desktopFile, arguments))
        return true;

    // Without the runtime, or for an entry it does not know, expand Exec here.
    QFile file(desktopFile);
    DesktopEntryData data;
    if (!file.open(QIODevice::ReadOnly)
        || !DesktopEntry::parse(QFileInfo(desktopFile).completeBaseName(), desktopFile,
                                file.readAll(), &data))
        return false;

    DesktopEntry entry(data.id);
    entry.update(data);
    return ApplicationLauncher::startDetached(entry.commandForArguments(arguments),
                                              entry.workingDirectory(), entry.id());
}

bool FileLauncher::launchExecutable(const QString &fileName)
{
    // The application runtime owns every application start of the session.
    return ApplicationLauncher::startDetached({fileName});
}
