// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QVariantMap>

#include <functional>

// The phones and servers cutefish-mountd serves, shared by every view of the process.
class MountsClient : public QObject
{
    Q_OBJECT

public:
    struct Mount
    {
        QString id;
        QString name;
        QString icon;
        QString path;
        bool network = false;
        bool device = false;
    };

    static MountsClient *instance();

    QList<Mount> mounts() const;
    Mount mount(const QString &id) const;
    // The mount a local path lies in, or an empty one.
    Mount mountAt(const QString &path) const;

    using Reply = std::function<void(const QString &status, const QVariantMap &details)>;
    void requestMount(const QString &location, const QVariantMap &options, Reply reply);
    void requestUnmount(const QString &id, std::function<void(const QString &error)> reply);

    // smb://server/share for "server/share" or \\server\share; empty when not a location.
    static QString normalizedAddress(const QString &address);

signals:
    void mountsChanged();

private slots:
    void refresh();

private:
    explicit MountsClient(QObject *parent = nullptr);

    QList<Mount> m_mounts;
};
