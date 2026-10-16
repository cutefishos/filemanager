// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "base/dbus.h"
#include "base/event_loop.h"
#include "base/file.h"
#include "base/paths.h"
#include "base/process.h"
#include "base/url.h"

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <sys/stat.h>
#include <thread>

using namespace cutefish::base;
using dbus::Variant;
using dbus::VariantMap;

static constexpr char kService[] = "com.cutefish.FileOperations";
static constexpr char kManagerPath[] = "/com/cutefish/FileOperations";
static constexpr char kManager[] = "com.cutefish.FileOperations.Manager1";
static constexpr char kTask[] = "com.cutefish.FileOperations.Task1";

namespace {

class FileOperationsDaemonTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        bus = dbus::Connection::open(dbus::Connection::Bus::Session, &loop);
        ASSERT_TRUE(bus);
        ASSERT_TRUE(daemon.start(FILEMANAGER_DAEMON));
        ASSERT_TRUE(waitFor([&] { return manager().call("CanUndo").value<bool>().has_value(); }, 3000));
    }

    void TearDown() override
    {
        daemon.terminate();
        EXPECT_TRUE(daemon.waitForFinished(std::chrono::seconds(3)));
    }

    dbus::Proxy manager(dbus::Connection* connection = nullptr)
    {
        return dbus::Proxy(connection ? connection : bus.get(), kService, kManagerPath, kManager);
    }

    dbus::Proxy task(const std::string& path) { return dbus::Proxy(bus.get(), kService, path, kTask); }

    static bool waitFor(const std::function<bool()>& condition, int milliseconds)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
        while (std::chrono::steady_clock::now() < deadline) {
            if (condition())
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        return condition();
    }

    std::optional<VariantMap> snapshot(const std::string& path) { return task(path).call("GetSnapshot").value<VariantMap>(); }

    static bool flag(const VariantMap& map, const std::string& key)
    {
        const auto found = map.find(key);
        return found != map.end() && found->second.value<bool>();
    }

    std::string submit(int32_t operation, const std::vector<std::string>& sources, const std::string& destination,
                       int32_t conflict, dbus::Connection* connection = nullptr)
    {
        const auto path = manager(connection).call("Submit", operation, sources, destination, conflict)
                                  .value<dbus::ObjectPath>();
        return path ? path->path : std::string();
    }

    EventLoop loop;
    std::unique_ptr<dbus::Connection> bus;
    Process daemon;
};

} // namespace

TEST_F(FileOperationsDaemonTest, OperationOutlivesSubmittingClient)
{
    file::TemporaryDirectory source(paths::runtimeDirectory(), "source");
    file::TemporaryDirectory destination(paths::runtimeDirectory(), "destination");
    const std::string payload(16 * 1024 * 1024, 'x');
    ASSERT_TRUE(file::writeAtomically(source.path() + "/payload.bin", payload));

    std::string path;
    {
        auto submitter = dbus::Connection::open(dbus::Connection::Bus::Session, &loop);
        path = submit(0, {url::fromLocalFile(source.path() + "/payload.bin")}, url::fromLocalFile(destination.path()),
                      4, submitter.get());
        ASSERT_FALSE(path.empty());
    }

    VariantMap result;
    ASSERT_TRUE(waitFor([&] {
        const auto current = snapshot(path);
        if (current)
            result = *current;
        return current && flag(*current, "finished");
    }, 10000));
    EXPECT_EQ(result["errorCode"].value<int32_t>(), 0);
    struct stat st;
    ASSERT_EQ(::stat((destination.path() + "/payload.bin").c_str(), &st), 0);
    EXPECT_EQ(uint64_t(st.st_size), payload.size());
}

TEST_F(FileOperationsDaemonTest, LiveTasksAreDiscoverable)
{
    file::TemporaryDirectory directory(paths::runtimeDirectory(), "conflict");
    ASSERT_TRUE(file::writeAtomically(directory.path() + "/source", "new"));
    ASSERT_TRUE(file::writeAtomically(directory.path() + "/destination", "old"));

    const std::string path = submit(5, {url::fromLocalFile(directory.path() + "/source")},
                                    url::fromLocalFile(directory.path() + "/destination"), 4);
    ASSERT_FALSE(path.empty());
    ASSERT_TRUE(waitFor([&] {
        const auto current = snapshot(path);
        return current && flag(*current, "waitingForConflict");
    }, 3000));

    VariantMap waiting = *snapshot(path);
    const VariantMap progress = waiting["progress"].value<VariantMap>();
    ASSERT_FALSE(progress.empty());
    EXPECT_TRUE(flag(progress, "totalsKnown"));
    EXPECT_EQ(progress.at("totalItems").value<uint64_t>(), 1u);

    const auto paths = manager().call("TaskPaths").value<std::vector<std::string>>();
    ASSERT_TRUE(paths);
    EXPECT_NE(std::find(paths->begin(), paths->end(), path), paths->end());

    task(path).call("ResolveConflict", int32_t(1), false);
    EXPECT_TRUE(waitFor([&] {
        const auto current = snapshot(path);
        return current && flag(*current, "finished");
    }, 3000));
}

TEST_F(FileOperationsDaemonTest, FinishedTasksLeaveTheBus)
{
    file::TemporaryDirectory directory(paths::runtimeDirectory(), "mkdir");
    const std::string created = directory.path() + "/created";
    const std::string path = submit(4, {}, url::fromLocalFile(created), 0);
    ASSERT_FALSE(path.empty());

    // A client attaching just after the task ended still reads its result.
    ASSERT_TRUE(waitFor([&] {
        const auto current = snapshot(path);
        return current && flag(*current, "finished");
    }, 3000));
    struct stat st;
    ASSERT_EQ(::stat(created.c_str(), &st), 0);
    EXPECT_TRUE(S_ISDIR(st.st_mode));

    EXPECT_TRUE(waitFor([&] { return !snapshot(path); }, 15000));
}

TEST_F(FileOperationsDaemonTest, ConflictOfVanishedSubmitterIsCanceled)
{
    file::TemporaryDirectory directory(paths::runtimeDirectory(), "vanished");
    const std::string source = directory.path() + "/source";
    const std::string destination = directory.path() + "/destination";
    ASSERT_TRUE(file::writeAtomically(source, "new"));
    ASSERT_TRUE(file::writeAtomically(destination, "old"));

    std::string path;
    {
        auto submitter = dbus::Connection::open(dbus::Connection::Bus::Session, &loop);
        path = submit(5, {url::fromLocalFile(source)}, url::fromLocalFile(destination), 4, submitter.get());
        ASSERT_FALSE(path.empty());
        ASSERT_TRUE(waitFor([&] {
            const auto current = snapshot(path);
            return current && flag(*current, "waitingForConflict");
        }, 3000));
        // The submitter leaves the bus without answering the conflict.
    }

    VariantMap result;
    ASSERT_TRUE(waitFor([&] {
        const auto current = snapshot(path);
        if (current)
            result = *current;
        return current && flag(*current, "finished");
    }, 3000));
    EXPECT_TRUE(flag(result, "canceled"));
    EXPECT_EQ(file::read(destination).value_or(""), "old");
    struct stat st;
    EXPECT_EQ(::stat(source.c_str(), &st), 0);
}

TEST_F(FileOperationsDaemonTest, UnknownConflictAnswerKeepsWaiting)
{
    file::TemporaryDirectory directory(paths::runtimeDirectory(), "unknown");
    const std::string source = directory.path() + "/source";
    const std::string destination = directory.path() + "/destination";
    ASSERT_TRUE(file::writeAtomically(source, "new"));
    ASSERT_TRUE(file::writeAtomically(destination, "old"));

    std::string path;
    {
        auto submitter = dbus::Connection::open(dbus::Connection::Bus::Session, &loop);
        path = submit(5, {url::fromLocalFile(source)}, url::fromLocalFile(destination), 4, submitter.get());
        ASSERT_FALSE(path.empty());
        ASSERT_TRUE(waitFor([&] {
            const auto current = snapshot(path);
            return current && flag(*current, "waitingForConflict");
        }, 3000));

        for (const int32_t policy : {int32_t(-1), int32_t(4), int32_t(99)})
            EXPECT_TRUE(task(path).call("ResolveConflict", policy, false).isError()) << policy;
        const auto current = snapshot(path);
        ASSERT_TRUE(current);
        EXPECT_TRUE(flag(*current, "waitingForConflict"));
        EXPECT_FALSE(flag(*current, "finished"));
    }

    // Still waiting, so the submitter leaving cancels it.
    VariantMap result;
    ASSERT_TRUE(waitFor([&] {
        const auto current = snapshot(path);
        if (current)
            result = *current;
        return current && flag(*current, "finished");
    }, 3000));
    EXPECT_TRUE(flag(result, "canceled"));
    EXPECT_EQ(file::read(destination).value_or(""), "old");
}
