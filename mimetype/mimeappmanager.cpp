// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mimeappmanager.h"

#include "applicationlauncher.h"
#include "applicationregistry.h"
#include "mimeapps.h"

#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>
#include <QUrl>

static MimeAppManager *s_self = nullptr;

// Hidden entries only mask a system entry; programs that need a terminal are not
// offered for opening files.
static bool opensFiles(const DesktopEntry *entry)
{
    return !entry->hidden() && !entry->terminal();
}

// mimeapps.list and xdg-terminals.list name applications by desktop file ID.
static DesktopEntry *entryForDesktopId(QString desktopId)
{
    if (desktopId.endsWith(QLatin1String(".desktop")))
        desktopId.chop(QLatin1String(".desktop").size());

    DesktopEntry *entry = ApplicationRegistry::instance()->byId(desktopId);
    return entry && opensFiles(entry) ? entry : nullptr;
}

static QString desktopId(const DesktopEntry *entry)
{
    return entry->id() + QLatin1String(".desktop");
}

MimeAppManager *MimeAppManager::self()
{
    if (!s_self)
        s_self = new MimeAppManager;

    return s_self;
}

MimeAppManager::MimeAppManager(QObject *parent)
    : QObject(parent)
{
    // Starts the background scan, so the lists are ready by the first right click.
    ApplicationRegistry::instance();
}

QString MimeAppManager::getDefaultAppByFilePath(const QString &filePath)
{
    return getDefaultAppByMimeType(QMimeDatabase().mimeTypeForFile(filePath));
}

QString MimeAppManager::getDefaultAppByMimeType(const QMimeType &mimeType)
{
    return MimeApps::chosenApplication(MimeApps::userListPath(), mimeType);
}

bool MimeAppManager::setDefaultAppForType(const QString &mimeType, const QString &app)
{
    const DesktopEntry *entry = ApplicationRegistry::instance()->byPath(app);
    if (!entry)
        return false;
    return MimeApps::setDefault(MimeApps::userListPath(), mimeType, desktopId(entry));
}

bool MimeAppManager::setDefaultAppForFile(const QString &filePath, const QString &desktop)
{
    if (!QFileInfo::exists(filePath))
        return false;
    return setDefaultAppForType(QMimeDatabase().mimeTypeForFile(filePath).name(), desktop);
}

QStringList MimeAppManager::getRecommendedAppsByFilePath(const QString &filePath)
{
    return getRecommendedAppsByMimeType(QMimeDatabase().mimeTypeForFile(filePath));
}

QStringList MimeAppManager::getRecommendedAppsByMimeType(const QMimeType &mimeType)
{
    return MimeApps::applications(mimeType);
}

QVariantList MimeAppManager::recommendedApps(const QUrl &url)
{
    QVariantList list;

    if (!url.isValid())
        return list;

    const QString filePath = url.isLocalFile() ? url.toLocalFile() : url.toString();
    QStringList applications = getRecommendedAppsByFilePath(filePath);
    QString defaultApp = getDefaultAppByFilePath(filePath);

    // Match openSelected(): without an explicit association, Open uses
    // the first recommended application.
    if (defaultApp.isEmpty() && !applications.isEmpty())
        defaultApp = applications.first();

    if (!defaultApp.isEmpty()) {
        applications.removeAll(defaultApp);
        applications.prepend(defaultApp);
    }

    for (const QString &path : std::as_const(applications)) {
        const DesktopEntry *entry = ApplicationRegistry::instance()->byPath(path);
        if (!entry)
            continue;

        QVariantMap item;
        item["icon"] = entry->icon();
        item["name"] = entry->name();
        item["desktopFile"] = path;

        list << item;
    }

    return list;
}

void MimeAppManager::launchTerminal(const QString &path)
{
    const QString preferred = MimeApps::defaultTerminal(MimeApps::userTerminalsListPath());
    const DesktopEntry *terminal = entryForDesktopId(preferred);

    if (!terminal || !terminal->categories().contains(QStringLiteral("TerminalEmulator"))) {
        terminal = nullptr;
        for (const DesktopEntry *entry : ApplicationRegistry::instance()->entries()) {
            if (opensFiles(entry)
                && entry->categories().contains(QStringLiteral("TerminalEmulator"))) {
                terminal = entry;
                break;
            }
        }
    }

    if (terminal)
        ApplicationLauncher::startDetached(terminal->commandForArguments({}), path, terminal->id());
}
