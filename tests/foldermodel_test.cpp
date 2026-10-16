// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "testutil.h"

#include "core/preferences.h"
#include "model/foldermodel.h"

#include "base/url.h"
#include "ui/application.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>

using namespace cutefish;
using fm::FolderModel;
using fmtest::runUntil;
using fmtest::TemporaryDir;
using fmtest::writeFile;

extern ui::Application* s_app;

static base::EventLoop& loop()
{
    return s_app->loop();
}

static std::vector<std::string> names(const FolderModel& folder)
{
    std::vector<std::string> result;
    for (int row = 0; row < folder.count(); ++row)
        result.push_back(folder.item(row)->name);
    return result;
}

static bool ready(const FolderModel& folder)
{
    return folder.status() == FolderModel::Status::Ready;
}

static std::string url(const std::string& path)
{
    return base::url::fromLocalFile(path);
}

TEST(FolderModelTest, NamesSortNumerically)
{
    TemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    for (const char* name : {"img10.png", "img2.png", "img1.png", "img100.png", "Img3.png"})
        ASSERT_TRUE(writeFile(directory.filePath(name)));
    const std::vector<std::string> expected = {"img1.png", "img2.png", "Img3.png", "img10.png", "img100.png"};

    FolderModel folder;
    folder.setSortMode(fm::Preferences::Name);
    folder.setSortDesc(false);
    folder.setUrl(directory.path());
    ASSERT_TRUE(runUntil(loop(), [&] { return ready(folder) && folder.count() == 5; }));
    EXPECT_EQ(names(folder), expected);

    std::vector<std::string> reversed(expected.rbegin(), expected.rend());
    folder.setSortDesc(true);
    EXPECT_TRUE(runUntil(loop(), [&] { return names(folder) == reversed; }));
}

// A refresh merges changes into the rows: what did not change keeps its item.
TEST(FolderModelTest, RefreshKeepsUnchangedItems)
{
    TemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    std::map<std::string, std::string> expected;
    for (int i = 0; i < 800; ++i) {
        char name[16];
        std::snprintf(name, sizeof(name), "%04d.txt", i);
        expected[name] = std::to_string(i);
        ASSERT_TRUE(writeFile(directory.filePath(name), expected[name]));
    }

    FolderModel folder;
    folder.setSortMode(fm::Preferences::Name);
    folder.setSortDesc(false);
    folder.setUrl(directory.path());
    ASSERT_TRUE(runUntil(loop(), [&] { return ready(folder) && folder.count() == int(expected.size()); }, 15000));

    for (int round = 0; round < 3; ++round) {
        std::map<std::string, fm::FileItemPtr> previous;
        for (int row = 0; row < folder.count(); ++row)
            previous[folder.item(row)->name] = folder.item(row);

        std::vector<std::string> keys;
        for (const auto& [name, contents] : expected)
            keys.push_back(name);
        std::map<std::string, bool> changed;
        for (size_t i = 0; i < keys.size(); ++i) {
            const std::string& name = keys[i];
            if ((round == 0 && i < 300) || (round > 0 && i % 3 == 0)) {
                std::filesystem::remove(directory.filePath(name));
                expected.erase(name);
            } else if (i % 5 == 0) {
                expected[name] += "-updated";
                changed[name] = true;
                ASSERT_TRUE(writeFile(directory.filePath(name), expected[name]));
            }
        }
        for (int i = 0; i < 270; ++i) {
            char name[32];
            std::snprintf(name, sizeof(name), "new-%d-%04d.txt", round, i);
            expected[name] = std::string(size_t(i % 31 + 1), 'x');
            ASSERT_TRUE(writeFile(directory.filePath(name), expected[name]));
        }

        bool completed = false;
        const base::Subscription subscription = folder.listingCompleted.connect([&] { completed = true; });
        folder.refresh();
        ASSERT_TRUE(runUntil(loop(), [&] { return completed && ready(folder) && folder.count() == int(expected.size()); },
                             15000));

        int row = 0;
        for (const auto& [name, contents] : expected) {
            const fm::FileItemPtr item = folder.item(row);
            ASSERT_EQ(item->name, name);
            EXPECT_EQ(item->size, contents.size());
            EXPECT_EQ(folder.rowForUrl(url(directory.filePath(name))), row);
            const auto before = previous.find(name);
            // Unchanged files keep their item, so views keep what they made for it.
            if (before != previous.end() && !changed.count(name))
                EXPECT_EQ(before->second, item) << name;
            ++row;
        }
    }
}

TEST(FolderModelTest, SortingKeepsTheSelection)
{
    TemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    for (const char* name : {"a.txt", "b.txt", "c.txt", "d.txt"})
        ASSERT_TRUE(writeFile(directory.filePath(name)));

    FolderModel folder;
    folder.setSortMode(fm::Preferences::Name);
    folder.setSortDesc(false);
    folder.setUrl(directory.path());
    ASSERT_TRUE(runUntil(loop(), [&] { return ready(folder) && folder.count() == 4; }));
    folder.setSelected(folder.rowForUrl(url(directory.filePath("b.txt"))));
    folder.toggleSelected(folder.rowForUrl(url(directory.filePath("d.txt"))));

    folder.setSortDesc(true);
    ASSERT_TRUE(runUntil(loop(), [&] { return folder.item(0)->name == "d.txt"; }));
    std::vector<std::string> selected = folder.selectedUrls();
    std::sort(selected.begin(), selected.end());
    EXPECT_EQ(selected, (std::vector<std::string> {url(directory.filePath("b.txt")), url(directory.filePath("d.txt"))}));
}

TEST(FolderModelTest, OpeningAFileShowsItsFolder)
{
    TemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    ASSERT_TRUE(writeFile(directory.filePath("file.txt")));

    FolderModel folder;
    folder.setUrl(directory.filePath("file.txt"));
    ASSERT_TRUE(runUntil(loop(), [&] { return ready(folder) && folder.count() == 1; }));
    EXPECT_EQ(folder.resolvedUrl(), url(directory.path()));
    EXPECT_FALSE(folder.canGoBack());

    std::vector<std::string> errors;
    const base::Subscription subscription
            = folder.notification.connect([&](const std::string& message) { errors.push_back(message); });
    folder.setUrl(directory.filePath("missing"));
    EXPECT_TRUE(runUntil(loop(), [&] { return !errors.empty(); }));
}

TEST(FolderModelTest, BackAndForwardKeepViewPositions)
{
    TemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    std::filesystem::create_directories(directory.filePath("a/b"));
    const std::string root = url(directory.path());
    const std::string a = url(directory.filePath("a"));
    const std::string b = url(directory.filePath("a/b"));

    FolderModel folder;
    int leaves = 0;
    const base::Subscription subscription
            = folder.aboutToLeaveFolder.connect([&] { folder.setViewPosition(++leaves * 100); });

    folder.setUrl(root);
    EXPECT_TRUE(std::isnan(folder.viewPosition()));
    folder.setUrl(a);
    folder.setUrl(b);

    folder.goBack();
    EXPECT_EQ(folder.resolvedUrl(), a);
    EXPECT_TRUE(folder.canGoBack());
    EXPECT_TRUE(folder.canGoForward());
    EXPECT_EQ(folder.viewPosition(), 300.0);

    folder.goBack();
    EXPECT_EQ(folder.resolvedUrl(), root);
    EXPECT_FALSE(folder.canGoBack());
    EXPECT_EQ(folder.viewPosition(), 200.0);

    folder.goForward();
    EXPECT_EQ(folder.resolvedUrl(), a);
    EXPECT_EQ(folder.viewPosition(), 500.0);
    folder.goForward();
    EXPECT_EQ(folder.resolvedUrl(), b);
    EXPECT_FALSE(folder.canGoForward());
    EXPECT_EQ(folder.viewPosition(), 400.0);

    folder.goBack();
    folder.setUrl(root);
    EXPECT_FALSE(folder.canGoForward());
    EXPECT_TRUE(std::isnan(folder.viewPosition()));
}

TEST(FolderModelTest, BackForwardAndUpRestoreTheSelection)
{
    TemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    std::filesystem::create_directories(directory.filePath("a/b"));
    std::filesystem::create_directories(directory.filePath("c"));
    ASSERT_TRUE(writeFile(directory.filePath("a/file.txt")));
    const std::string root = url(directory.path());
    const std::string a = url(directory.filePath("a"));
    const std::string b = url(directory.filePath("a/b"));
    const std::string file = url(directory.filePath("a/file.txt"));
    using Urls = std::vector<std::string>;

    FolderModel folder;
    folder.setUrl(root);
    ASSERT_TRUE(runUntil(loop(), [&] { return ready(folder) && folder.rowForUrl(a) >= 0; }));
    folder.setSelected(folder.rowForUrl(a));
    folder.setUrl(a);
    ASSERT_TRUE(runUntil(loop(), [&] { return ready(folder) && folder.rowForUrl(b) >= 0; }));
    folder.setSelected(folder.rowForUrl(b));
    folder.setUrl(b);
    ASSERT_TRUE(runUntil(loop(), [&] { return ready(folder); }));

    folder.goBack();
    EXPECT_TRUE(runUntil(loop(), [&] { return folder.selectedUrls() == Urls {b}; }));
    folder.clearSelection();
    ASSERT_TRUE(runUntil(loop(), [&] { return folder.rowForUrl(file) >= 0; }));
    folder.setSelected(folder.rowForUrl(file));

    folder.goBack();
    EXPECT_TRUE(runUntil(loop(), [&] { return folder.selectedUrls() == Urls {a}; }));

    folder.goForward();
    EXPECT_TRUE(runUntil(loop(), [&] { return folder.selectedUrls() == Urls {file}; }));

    folder.goForward();
    ASSERT_TRUE(runUntil(loop(), [&] { return ready(folder) && folder.resolvedUrl() == b; }));
    folder.up();
    EXPECT_TRUE(runUntil(loop(), [&] { return folder.resolvedUrl() == a; }));
    EXPECT_TRUE(runUntil(loop(), [&] { return folder.selectedUrls() == Urls {b}; }));
}

// The menus follow the selection: a file, a folder, and the folder itself.
TEST(FolderModelTest, ContextMenuActionsFollowTheSelection)
{
    TemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    ASSERT_TRUE(writeFile(directory.filePath("notes.txt")));
    std::filesystem::create_directories(directory.filePath("folder"));

    FolderModel folder;
    folder.setUrl(directory.path());
    ASSERT_TRUE(runUntil(loop(), [&] { return ready(folder) && folder.count() == 2; }));

    folder.setSelected(folder.rowForUrl(url(directory.filePath("notes.txt"))));
    folder.prepareContextMenu();
    EXPECT_TRUE(folder.action("open").visible);
    EXPECT_TRUE(folder.action("openWith").visible);
    EXPECT_TRUE(folder.action("copy").enabled);
    EXPECT_TRUE(folder.action("rename").visible);
    EXPECT_EQ(folder.action("compress").text, "Compress “notes.txt”");

    folder.clearSelection();
    folder.setSelected(folder.rowForUrl(url(directory.filePath("folder"))));
    folder.prepareContextMenu();
    EXPECT_FALSE(folder.action("open").visible);
    EXPECT_FALSE(folder.action("openWith").visible);

    folder.clearSelection();
    folder.prepareContextMenu();
    EXPECT_TRUE(folder.action("newFolder").enabled);
    EXPECT_TRUE(folder.action("newTextFile").visible);
}

