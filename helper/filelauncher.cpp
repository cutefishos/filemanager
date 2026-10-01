// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "filelauncher.h"

#include "applicationlauncher.h"
#include "applicationruntime.h"

#include <QSettings>
#include <QFile>
#include <QDir>
#include <QDebug>

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
    // The runtime knows the entry: let it expand Exec and track the instance
    // under the application's own id.
    if (ApplicationRuntime::instance()->launchApplication(
                desktopFile, fileName.isEmpty() ? QStringList() : QStringList{fileName}))
        return true;

    QSettings settings(desktopFile, QSettings::IniFormat);
    settings.beginGroup("Desktop Entry");

    QStringList list = settings.value("Exec").toString().split(' ');
    QStringList args;

    if (list.isEmpty() || list.size() < 0)
        return false;

    QString exec = list.first();
    list.removeOne(exec);

    for (const QString &arg : list) {
        QString newArg = arg;

        if (newArg.startsWith("%F", Qt::CaseInsensitive))
            newArg.replace("%F", fileName, Qt::CaseInsensitive);

        if (newArg.startsWith("%U", Qt::CaseInsensitive))
            newArg.replace("%U", fileName, Qt::CaseInsensitive);

        args.append(newArg);
    }

    qDebug() << "launchApp()" << exec << args;

    return startDetached(exec, args);
}

bool FileLauncher::launchExecutable(const QString &fileName)
{
    return startDetached(fileName);
}

bool FileLauncher::startDetached(const QString &exec, QStringList args)
{
    return startDetached(exec, QString(), args);
}

bool FileLauncher::startDetached(const QString &exec, const QString &workingDir, QStringList args)
{
    // The application runtime owns every application start of the session.
    return ApplicationLauncher::startDetached(QStringList{exec} + args, workingDir);
}
