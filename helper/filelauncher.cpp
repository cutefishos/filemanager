// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "filelauncher.h"

#include "applicationlauncher.h"
#include "applicationruntime.h"
#include "desktopentry.h"

#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QStandardPaths>

#include <linux/magic.h>
#include <sys/vfs.h>
#include <unistd.h>

// Not in linux/magic.h: the ntfs3 driver and the old in-kernel ntfs.
static constexpr long NTFS3_SUPER_MAGIC = 0x7366746e;
static constexpr long NTFS_SB_MAGIC = 0x5346544e;

static bool isDesktopFile(const QString &path)
{
    return QMimeDatabase().mimeTypeForFile(path).inherits(QStringLiteral("application/x-desktop"));
}

static bool isInApplicationsDirectory(const QString &path)
{
    const QString directory = QFileInfo(path).canonicalPath();
    const QStringList locations =
            QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    for (const QString &location : locations) {
        const QString canonical = QFileInfo(location).canonicalFilePath();
        if (!canonical.isEmpty()
            && (directory == canonical || directory.startsWith(canonical + QLatin1Char('/'))))
            return true;
    }

    return false;
}

// These mounts show every file as executable, so the bit says nothing about the user's intent.
static bool keepsPermissions(const QString &path)
{
    struct statfs fs;
    if (::statfs(QFile::encodeName(path).constData(), &fs) != 0)
        return false;

    switch (fs.f_type) {
    case MSDOS_SUPER_MAGIC:
    case EXFAT_SUPER_MAGIC:
    case NTFS3_SUPER_MAGIC:
    case NTFS_SB_MAGIC:
    case FUSE_SUPER_MAGIC:
        return false;
    default:
        return true;
    }
}

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

bool FileLauncher::launchFile(const QString &path)
{
    if (isDesktopFile(path))
        return launchApp(path, QString());

    return launchExecutable(path);
}

bool FileLauncher::isTrusted(const QString &path)
{
    if (isDesktopFile(path) && isInApplicationsDirectory(path))
        return true;

    const QFileInfo info(path);
    if (!info.isFile() || !(info.permissions() & QFile::ExeOwner))
        return false;

    // Someone else's executable file says nothing about what this user wants to run.
    if (info.ownerId() != ::getuid() && info.ownerId() != 0)
        return false;

    return keepsPermissions(path);
}

bool FileLauncher::trustAndLaunch(const QString &path)
{
    QFile file(path);
    if (!file.exists())
        return false;

    // Fails on mounts without permissions; the user agreed to run it all the same.
    if (!(file.permissions() & QFile::ExeOwner))
        file.setPermissions(file.permissions() | QFile::ExeOwner);

    return launchFile(path);
}
