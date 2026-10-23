// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "fusemount.h"
#include "remoteurl.h"

#include "base/dbus.h"
#include "base/signal.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

class MtpMonitor;

// org.cutefish.Mounts1 at /org/cutefish/Mounts1.
class MountService
{
public:
    explicit MountService(cutefish::base::dbus::Connection *bus);
    ~MountService();

    MountService(const MountService &) = delete;
    MountService &operator=(const MountService &) = delete;

    bool publish();
    // Detaches every mount, for the session's end.
    void detachAll();

private:
    struct Entry
    {
        std::string id;
        std::string kind;
        std::string name;
        std::string icon;
        RemoteUrl url;
        bool network = true;
        // Devices stay listed while unmounted.
        bool device = false;
        // Disk images go away when ejected, as devices do when pulled out.
        bool ejectable = false;
        std::unique_ptr<FuseMount> mount;
    };

    std::vector<cutefish::base::dbus::VariantMap> mounts() const;
    // location is a URL (sftp://, smb://), the id of a device from Mounts() or the path of
    // a disk image. Answers (status, details): "mounted" with path; "password" with user,
    // domain, needsDomain, anonymousAllowed and retry; "hostkey" with fingerprint and
    // changed; "failed" with message. options carry the answers: user, domain, password,
    // anonymous, remember, hostKey.
    void mount(cutefish::base::dbus::MethodCall &call);
    void unmount(cutefish::base::dbus::MethodCall &call);
    void mountsChanged();

    Entry *find(const std::string &id);
    cutefish::base::dbus::VariantMap describe(const Entry &entry) const;
    std::string mountPointFor(const std::string &name) const;
    void finished(FuseMount *mount);
    void addDevice(const std::string &id);
    void removeDevice(const std::string &id);

    cutefish::base::dbus::Connection *m_bus;
    cutefish::base::dbus::Object m_object;
    std::string m_root;
    std::vector<std::unique_ptr<Entry>> m_entries;
    std::set<std::string> m_pending;
    // Mount calls waiting for their connection, answered later.
    std::map<int, cutefish::base::dbus::MethodCall> m_calls;
    int m_nextCall = 1;
    // Mounts of devices pulled out, kept until their sessions end.
    std::vector<std::unique_ptr<FuseMount>> m_retiring;
    std::unique_ptr<MtpMonitor> m_monitor;
    cutefish::base::Subscription m_added;
    cutefish::base::Subscription m_removed;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};
