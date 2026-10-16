// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/launcher.h"

#include "base/process.h"

#include <gtest/gtest.h>

#include <climits>
#include <csignal>
#include <filesystem>
#include <string>

#include <sys/socket.h>
#include <unistd.h>

using namespace cutefish;

static std::string target(const std::string& link)
{
    char buffer[PATH_MAX];
    const ssize_t length = ::readlink(link.c_str(), buffer, sizeof(buffer));
    return length > 0 ? std::string(buffer, size_t(length)) : std::string();
}

// Whether the program's descriptors include the socket this process holds at fd.
static bool childHolds(pid_t pid, int fd)
{
    const std::string socket = target("/proc/self/fd/" + std::to_string(fd));
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator("/proc/" + std::to_string(pid) + "/fd", error)) {
        if (target(entry.path().string()) == socket)
            return true;
    }
    return false;
}

static bool spawnedChildHolds(int fd)
{
    // posix_spawn returns once the program is executed, so its descriptors are final.
    const pid_t pid = base::Process::startDetached("sleep", {"30"});
    EXPECT_GT(pid, 0);
    if (pid <= 0)
        return false;
    const bool holds = childHolds(pid, fd);
    ::kill(pid, SIGKILL);
    return holds;
}

TEST(LauncherTest, HandedDescriptorStaysOutOfChildren)
{
    int pair[2];
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, pair), 0);

    // As the session hands it over: inheritable.
    EXPECT_TRUE(spawnedChildHolds(pair[0]));

    ASSERT_TRUE(fm::launcher::keepFromChildren(pair[0]));
    EXPECT_FALSE(spawnedChildHolds(pair[0]));
    EXPECT_FALSE(fm::launcher::keepFromChildren(-1));

    ::close(pair[0]);
    ::close(pair[1]);
}
