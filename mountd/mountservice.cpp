// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mountservice.h"
#include "keyring.h"
#include "mtpbackend.h"
#include "mtpmonitor.h"
#include "sftpbackend.h"
#include "smbbackend.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>

#include <thread>

static std::unique_ptr<Backend> createBackend(const QUrl &url)
{
    const QString scheme = url.scheme();
    if (scheme == QLatin1String("sftp") || scheme == QLatin1String("ssh"))
        return std::make_unique<SftpBackend>(url);
    if (scheme == QLatin1String("smb"))
        return std::make_unique<SmbBackend>(url);

    return nullptr;
}

static int defaultPort(const QString &scheme)
{
    if (scheme == QLatin1String("sftp") || scheme == QLatin1String("ssh"))
        return 22;
    if (scheme == QLatin1String("smb"))
        return 445;
    return -1;
}

// One mount per account on a server: scheme://user@host[:port].
static QString serverId(const QUrl &url, const QString &user)
{
    QUrl id;
    id.setScheme(url.scheme() == QLatin1String("ssh") ? QStringLiteral("sftp") : url.scheme());
    id.setHost(url.host().toLower());
    id.setUserName(user);
    if (url.port() > 0 && url.port() != defaultPort(id.scheme()))
        id.setPort(url.port());
    return id.toString();
}

static QString serverName(const QUrl &url, const QString &user)
{
    const QString host = url.port() > 0 && url.port() != defaultPort(url.scheme())
            ? QStringLiteral("%1:%2").arg(url.host()).arg(url.port())
            : url.host();
    return user.isEmpty() ? host : QStringLiteral("%1@%2").arg(user, host);
}

static QString joined(const QString &mountPoint, const std::string &path)
{
    if (path.empty() || path == "/")
        return mountPoint;
    return mountPoint + QFile::decodeName(QByteArray::fromStdString(path));
}

MountService::MountService(QObject *parent)
    : QObject(parent)
    , m_root(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)
             + QStringLiteral("/mounts"))
{
    qDBusRegisterMetaType<QList<QVariantMap>>();

    // Mounts a crashed run left behind answer nothing; detach them.
    QDir root(m_root);
    for (const QString &name : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden)) {
        QProcess::execute(QStringLiteral("fusermount3"),
                          {QStringLiteral("-u"), QStringLiteral("-z"), root.filePath(name)});
        root.rmdir(name);
    }

    m_monitor = new MtpMonitor(this);
    connect(m_monitor, &MtpMonitor::added, this, &MountService::addDevice);
    connect(m_monitor, &MtpMonitor::removed, this, &MountService::removeDevice);
    for (const MtpMonitor::Device &device : m_monitor->devices())
        addDevice(device.id);
}

MountService::~MountService()
{
    detachAll();
}

void MountService::detachAll()
{
    for (const auto &entry : m_entries) {
        if (entry->mount)
            entry->mount->stop(true, nullptr);
    }
}

MountService::Entry *MountService::find(const QString &id)
{
    for (const auto &entry : m_entries) {
        if (entry->id == id)
            return entry.get();
    }
    return nullptr;
}

QVariantMap MountService::describe(const Entry &entry) const
{
    return {
        {QStringLiteral("id"), entry.id},
        {QStringLiteral("kind"), entry.kind},
        {QStringLiteral("name"), entry.name},
        {QStringLiteral("icon"), entry.icon},
        {QStringLiteral("url"), entry.url.toString()},
        {QStringLiteral("path"), entry.mount ? entry.mount->mountPoint() : QString()},
        {QStringLiteral("network"), entry.network},
        {QStringLiteral("device"), entry.device},
    };
}

QList<QVariantMap> MountService::Mounts() const
{
    QList<QVariantMap> result;
    for (const auto &entry : m_entries)
        result.append(describe(*entry));
    return result;
}

QString MountService::mountPointFor(const QString &name) const
{
    QString base = name;
    base.replace(QLatin1Char('/'), QLatin1Char('_'));
    if (base.isEmpty() || base.startsWith(QLatin1Char('.')))
        base.prepend(QLatin1Char('_'));

    QString candidate = base;
    for (int suffix = 2; QFileInfo::exists(m_root + QLatin1Char('/') + candidate); ++suffix)
        candidate = QStringLiteral("%1 (%2)").arg(base).arg(suffix);

    return m_root + QLatin1Char('/') + candidate;
}

QString MountService::Mount(const QString &location, const QVariantMap &options,
                            QVariantMap &details)
{
    Entry *device = find(location);
    const QUrl url = device ? device->url : QUrl(location);

    Credentials credentials;
    credentials.user = options.value(QStringLiteral("user")).toString();
    credentials.domain = options.value(QStringLiteral("domain")).toString();
    credentials.password = options.value(QStringLiteral("password")).toString();
    credentials.anonymous = options.value(QStringLiteral("anonymous")).toBool();
    credentials.acceptedHostKey = options.value(QStringLiteral("hostKey")).toString();
    const bool remember = options.value(QStringLiteral("remember")).toBool();

    // A server already mounted for this account opens at once.
    const QUrl key(device ? QString() : serverId(url, url.userName()));
    for (const auto &entry : m_entries) {
        if (!entry->mount)
            continue;

        const QUrl mounted(entry->id);
        const bool same = device ? entry.get() == device
                                 : !entry->device && mounted.scheme() == key.scheme()
                        && mounted.host() == key.host() && mounted.port() == key.port()
                        && (key.userName().isEmpty() || mounted.userName() == key.userName());
        if (!same)
            continue;

        const std::string path = url.path().isEmpty() || url.path() == QLatin1String("/")
                ? entry->mount->backend()->initialPath()
                : url.path().toStdString();
        details.insert(QStringLiteral("path"), joined(entry->mount->mountPoint(), path));
        return QStringLiteral("mounted");
    }

    std::unique_ptr<Backend> backend;
    if (device) {
        const MtpMonitor::Device found = m_monitor->device(device->id);
        if (found.bus)
            backend = std::make_unique<MtpBackend>(found.bus, found.number);
    } else {
        backend = createBackend(url);
    }

    if (!backend) {
        details.insert(QStringLiteral("message"),
                       tr("“%1” is not a location that can be connected to.").arg(location));
        return QStringLiteral("failed");
    }

    const QString pendingKey = device ? device->id : serverId(url, credentials.user);
    if (m_pending.contains(pendingKey)) {
        details.insert(QStringLiteral("message"), tr("Already connecting to “%1”.").arg(location));
        return QStringLiteral("failed");
    }
    m_pending.insert(pendingKey);

    setDelayedReply(true);
    const QDBusMessage request = message();
    const QString deviceId = device ? device->id : QString();

    std::thread([this, request, url, deviceId, pendingKey, credentials, remember,
                 backend = std::move(backend)]() mutable {
        Credentials attempt = credentials;
        bool fromKeyring = false;
        if (attempt.password.isEmpty() && !attempt.anonymous && deviceId.isEmpty())
            fromKeyring = Keyring::find(url, &attempt);

        ConnectResult result = backend->connect(attempt);

        // A saved password that no longer works is asked for again.
        if (result.status == ConnectResult::NeedPassword && fromKeyring)
            result.message.clear();

        if (result.status == ConnectResult::Connected && remember && !attempt.anonymous)
            Keyring::store(url, attempt);

        QMetaObject::invokeMethod(this, [this, request, url, deviceId, pendingKey, result, attempt,
                                         credentials, fromKeyring,
                                         backend = std::move(backend)]() mutable {
            m_pending.remove(pendingKey);

            QString status;
            QVariantMap details;

            switch (result.status) {
            case ConnectResult::NeedPassword:
                status = QStringLiteral("password");
                details.insert(QStringLiteral("user"),
                               !attempt.user.isEmpty() ? attempt.user : url.userName());
                details.insert(QStringLiteral("domain"), attempt.domain);
                details.insert(QStringLiteral("needsDomain"), result.needsDomain);
                details.insert(QStringLiteral("anonymousAllowed"), result.anonymousAllowed);
                // Typed, or kept from before: either way it was refused.
                details.insert(QStringLiteral("retry"),
                               !credentials.password.isEmpty() || fromKeyring);
                break;
            case ConnectResult::UnknownHostKey:
                status = QStringLiteral("hostkey");
                details.insert(QStringLiteral("fingerprint"), result.fingerprint);
                details.insert(QStringLiteral("changed"), result.hostKeyChanged);
                break;
            case ConnectResult::Failed:
                status = QStringLiteral("failed");
                details.insert(QStringLiteral("message"), result.message);
                break;
            case ConnectResult::Connected: {
                Entry *entry = deviceId.isEmpty() ? nullptr : find(deviceId);
                std::unique_ptr<Entry> created;
                if (!entry) {
                    created = std::make_unique<Entry>();
                    entry = created.get();
                    const QString user = !attempt.user.isEmpty() ? attempt.user : url.userName();
                    entry->id = serverId(url, user);
                    entry->kind = QUrl(entry->id).scheme();
                    entry->name = serverName(url, user);
                    entry->icon = QStringLiteral("network-server");
                    entry->url = QUrl(entry->id);
                }

                // A device is named as it calls itself once it answers.
                if (auto *mtp = dynamic_cast<MtpBackend *>(backend.get())) {
                    const QString name = mtp->friendlyName();
                    if (!name.isEmpty() && name != entry->name) {
                        entry->name = name;
                        emit MountsChanged();
                    }
                }

                const std::string initial = url.path().isEmpty() || url.path() == QLatin1String("/")
                        ? backend->initialPath()
                        : url.path().toStdString();
                auto mount = std::make_unique<FuseMount>(
                        std::move(backend), mountPointFor(entry->name), entry->url.toString(),
                        [this](FuseMount *mount) {
                            QMetaObject::invokeMethod(this, [this, mount] {
                                finished(mount);
                            });
                        });

                QString message;
                if (!mount->start(&message)) {
                    status = QStringLiteral("failed");
                    details.insert(QStringLiteral("message"), message);
                    break;
                }

                status = QStringLiteral("mounted");
                details.insert(QStringLiteral("path"), joined(mount->mountPoint(), initial));
                entry->mount = std::move(mount);
                if (created)
                    m_entries.push_back(std::move(created));
                emit MountsChanged();
                break;
            }
            }

            QDBusConnection::sessionBus().send(
                    request.createReply({status, QVariant::fromValue(details)}));
        });
    }).detach();

    return QString();
}

void MountService::Unmount(const QString &id)
{
    Entry *entry = find(id);
    if (!entry || !entry->mount)
        return;

    QString message;
    if (!entry->mount->stop(false, &message))
        sendErrorReply(QStringLiteral("com.cutefish.Mounts1.Error.Busy"), message);
}

void MountService::finished(FuseMount *mount)
{
    for (auto it = m_retiring.begin(); it != m_retiring.end(); ++it) {
        if (it->get() == mount) {
            m_retiring.erase(it);
            return;
        }
    }

    for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
        Entry *entry = it->get();
        if (entry->mount.get() != mount)
            continue;

        entry->mount.reset();
        if (!entry->device)
            m_entries.erase(it);
        emit MountsChanged();
        return;
    }
}

void MountService::addDevice(const QString &id)
{
    if (find(id))
        return;

    const MtpMonitor::Device device = m_monitor->device(id);
    auto entry = std::make_unique<Entry>();
    entry->id = id;
    entry->kind = QStringLiteral("mtp");
    entry->name = device.name;
    entry->icon = QStringLiteral("phone");
    entry->url = QUrl(id);
    entry->network = false;
    entry->device = true;
    m_entries.push_back(std::move(entry));
    emit MountsChanged();
}

void MountService::removeDevice(const QString &id)
{
    for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
        Entry *entry = it->get();
        if (entry->id != id)
            continue;

        // Its session ends once nothing has files open there; the entry goes now.
        if (entry->mount) {
            entry->mount->stop(true, nullptr);
            m_retiring.push_back(std::move(entry->mount));
        }
        m_entries.erase(it);
        emit MountsChanged();
        return;
    }
}
