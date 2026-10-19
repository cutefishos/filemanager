// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "base/dbus.h"
#include "base/signal.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace cutefish::files {
class Task;
}

class DaemonTask;

// com.cutefish.FileOperations.Manager1 at /com/cutefish/FileOperations: runs
// the file manager's operations so they outlive its windows, with one undo
// history for every client.
class FileOperationsService
{
public:
    explicit FileOperationsService(cutefish::base::dbus::Connection *bus);
    ~FileOperationsService();

    bool publish();

private:
    void submit(cutefish::base::dbus::MethodCall &call);
    void submitArchive(cutefish::base::dbus::MethodCall &call);
    void history(cutefish::base::dbus::MethodCall &call, bool undo);
    std::string nextPath();
    // Registers the task and announces it; false when it could not go on the bus.
    bool announce(std::unique_ptr<DaemonTask> task, const std::string &owner);
    void watchOwner(const std::string &owner);
    void ownerLost(const std::string &owner);
    void retire(DaemonTask *task);
    void remove(const std::string &path);

    cutefish::base::dbus::Connection *m_bus;
    cutefish::base::dbus::Object m_object;
    uint64_t m_nextTaskId = 1;
    std::vector<std::string> m_taskPaths;
    std::vector<std::unique_ptr<DaemonTask>> m_tasks;
    std::map<std::string, cutefish::base::dbus::NameWatch> m_owners;
    std::vector<cutefish::base::Subscription> m_subscriptions;
    std::map<DaemonTask *, cutefish::base::Subscription> m_finishedSubscriptions;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};
