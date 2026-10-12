// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mountservice.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QFile>
#include <QSocketNotifier>
#include <QStandardPaths>

#include <libmtp.h>
#include <libssh/libssh.h>

#include <csignal>
#include <fcntl.h>
#include <sys/eventfd.h>
#include <sys/file.h>
#include <unistd.h>

static int s_quitFd = -1;

static void requestQuit(int)
{
    const uint64_t one = 1;
    (void)!::write(s_quitFd, &one, sizeof(one));
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("cutefish-mountd"));

    // One instance per user: a second one, started on a private bus by a test, would
    // take the first one's mounts for leftovers and detach them.
    const QByteArray lockPath = QFile::encodeName(
            QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)
            + QStringLiteral("/cutefish-mountd.lock"));
    const int lock = ::open(lockPath.constData(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (lock < 0 || ::flock(lock, LOCK_EX | LOCK_NB) < 0)
        return 1;

    ssh_init();
    LIBMTP_Init();

    MountService service;
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerObject(QStringLiteral("/com/cutefish/Mounts1"), &service,
                            QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals)
        || !bus.registerService(QStringLiteral("com.cutefish.Mounts1")))
        return 1;

    // The session's end detaches every mount, so none is left answering nothing.
    s_quitFd = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    QSocketNotifier quitNotifier(s_quitFd, QSocketNotifier::Read);
    QObject::connect(&quitNotifier, &QSocketNotifier::activated, &app, &QCoreApplication::quit);
    std::signal(SIGTERM, requestQuit);
    std::signal(SIGINT, requestQuit);
    std::signal(SIGHUP, requestQuit);

    const int result = app.exec();
    service.detachAll();
    ssh_finalize();
    // Sessions still serving open files end on their own; nothing waits for them.
    ::_exit(result);
}
