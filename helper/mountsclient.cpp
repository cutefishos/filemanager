// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mountsclient.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QUrl>

static const QString s_service = QStringLiteral("com.cutefish.Mounts1");
static const QString s_path = QStringLiteral("/com/cutefish/Mounts1");
static const QString s_interface = QStringLiteral("com.cutefish.Mounts1");

// Connecting asks the server and maybe the keyring; a slow server answers late.
static constexpr int kMountTimeoutMs = 120000;

MountsClient *MountsClient::instance()
{
    static auto *client = new MountsClient(QCoreApplication::instance());
    return client;
}

MountsClient::MountsClient(QObject *parent)
    : QObject(parent)
{
    qDBusRegisterMetaType<QList<QVariantMap>>();
    QDBusConnection::sessionBus().connect(s_service, s_path, s_interface,
                                          QStringLiteral("MountsChanged"), this, SLOT(refresh()));
    refresh();
}

void MountsClient::refresh()
{
    const auto message = QDBusMessage::createMethodCall(s_service, s_path, s_interface,
                                                        QStringLiteral("Mounts"));
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message),
                                                this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        watcher->deleteLater();
        const QDBusPendingReply<QList<QVariantMap>> reply = *watcher;
        if (reply.isError())
            return;

        QList<Mount> mounts;
        for (const QVariantMap &entry : reply.value()) {
            Mount mount;
            mount.id = entry.value(QStringLiteral("id")).toString();
            mount.name = entry.value(QStringLiteral("name")).toString();
            mount.icon = entry.value(QStringLiteral("icon")).toString();
            mount.path = entry.value(QStringLiteral("path")).toString();
            mount.network = entry.value(QStringLiteral("network")).toBool();
            mount.device = entry.value(QStringLiteral("device")).toBool();
            mounts.append(mount);
        }

        m_mounts = mounts;
        emit mountsChanged();
    });
}

QList<MountsClient::Mount> MountsClient::mounts() const
{
    return m_mounts;
}

MountsClient::Mount MountsClient::mount(const QString &id) const
{
    for (const Mount &mount : m_mounts) {
        if (mount.id == id)
            return mount;
    }
    return {};
}

MountsClient::Mount MountsClient::mountAt(const QString &path) const
{
    for (const Mount &mount : m_mounts) {
        if (!mount.path.isEmpty()
            && (path == mount.path || path.startsWith(mount.path + QLatin1Char('/'))))
            return mount;
    }
    return {};
}

void MountsClient::requestMount(const QString &location, const QVariantMap &options, Reply reply)
{
    auto message = QDBusMessage::createMethodCall(s_service, s_path, s_interface,
                                                  QStringLiteral("Mount"));
    message.setArguments({location, options});
    auto *watcher = new QDBusPendingCallWatcher(
            QDBusConnection::sessionBus().asyncCall(message, kMountTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [watcher, reply] {
        watcher->deleteLater();
        const QDBusPendingReply<QString, QVariantMap> answer = *watcher;
        if (answer.isError()) {
            reply(QStringLiteral("failed"),
                  {{QStringLiteral("message"), answer.error().message()}});
            return;
        }
        reply(answer.argumentAt<0>(), answer.argumentAt<1>());
    });
}

void MountsClient::requestUnmount(const QString &id, std::function<void(const QString &)> reply)
{
    auto message = QDBusMessage::createMethodCall(s_service, s_path, s_interface,
                                                  QStringLiteral("Unmount"));
    message.setArguments({id});
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message),
                                                this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [watcher, reply] {
        watcher->deleteLater();
        const QDBusPendingReply<> answer = *watcher;
        if (reply)
            reply(answer.isError() ? answer.error().message() : QString());
    });
}

QString MountsClient::normalizedAddress(const QString &address)
{
    QString text = address.trimmed();
    if (text.isEmpty())
        return QString();

    // \\server\share, as Windows writes it.
    if (text.startsWith(QLatin1String("\\\\"))) {
        text = text.mid(2);
        text.replace(QLatin1Char('\\'), QLatin1Char('/'));
        text.prepend(QLatin1String("smb://"));
    } else if (!text.contains(QLatin1String("://"))) {
        text.prepend(QLatin1String("smb://"));
    }

    QUrl url(text, QUrl::TolerantMode);
    if (url.scheme() == QLatin1String("ssh"))
        url.setScheme(QStringLiteral("sftp"));

    if (!url.isValid() || url.host().isEmpty()
        || (url.scheme() != QLatin1String("smb") && url.scheme() != QLatin1String("sftp")))
        return QString();

    return url.toString(QUrl::StripTrailingSlash);
}
