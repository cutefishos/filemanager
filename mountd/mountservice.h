// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "fusemount.h"

#include <QDBusContext>
#include <QObject>
#include <QSet>
#include <QUrl>
#include <QVariantMap>

#include <memory>

class MtpMonitor;

class MountService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.cutefish.Mounts1")

public:
    explicit MountService(QObject *parent = nullptr);
    ~MountService() override;

    // Detaches every mount, for the session's end.
    void detachAll();

public slots:
    QList<QVariantMap> Mounts() const;
    // location is a URL (sftp://, smb://) or the id of a device from Mounts(). Answers
    // (status, details): "mounted" with path; "password" with user, domain, needsDomain,
    // anonymousAllowed and retry; "hostkey" with fingerprint and changed; "failed" with
    // message. options carry the answers: user, domain, password, anonymous, remember,
    // hostKey.
    QString Mount(const QString &location, const QVariantMap &options, QVariantMap &details);
    void Unmount(const QString &id);

signals:
    void MountsChanged();

private:
    struct Entry
    {
        QString id;
        QString kind;
        QString name;
        QString icon;
        QUrl url;
        bool network = true;
        // Devices stay listed while unmounted.
        bool device = false;
        std::unique_ptr<FuseMount> mount;
    };

    Entry *find(const QString &id);
    QVariantMap describe(const Entry &entry) const;
    QString mountPointFor(const QString &name) const;
    void finished(FuseMount *mount);
    void addDevice(const QString &id);
    void removeDevice(const QString &id);

    QString m_root;
    std::vector<std::unique_ptr<Entry>> m_entries;
    QSet<QString> m_pending;
    // Mounts of devices pulled out, kept until their sessions end.
    std::vector<std::unique_ptr<FuseMount>> m_retiring;
    MtpMonitor *m_monitor = nullptr;
};
