// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "testutil.h"

#include "desktop/desktopview.h"
#include "desktop/desktopwindow.h"
#include "model/foldermodel.h"
#include "model/places.h"
#include "ops/operations.h"
#include "views/folderwindow.h"

#include "base/dbus.h"
#include "base/process.h"
#include "base/url.h"
#include "ui/application.h"
#include "ui/clipboard.h"
#include "ui/drag.h"
#include "ui/test_input.h"
#include "ui/window.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <sys/stat.h>

using namespace cutefish;
using fmtest::runUntil;
using fmtest::TemporaryDir;

extern ui::Application* s_app;

namespace {

class WindowHost : public fm::FileManagerHost
{
public:
    fm::FolderWindow* openFolderWindow(const fm::FolderPage::Start&, ui::Window*) override { return nullptr; }
    void openFolders(const std::vector<std::string>&) override {}
    void showProperties(const fm::FileItemList&, ui::Window*) override {}
    void showOpenWith(const std::string&, ui::Window*) override {}
    void confirmDelete(const std::vector<std::string>&, ui::Window*) override {}
    void confirmEmptyTrash(ui::Window*) override {}
    void confirmLaunch(const std::string&, ui::Window*) override {}
    void installBundleRunner(const std::string&, ui::Window*) override {}
    void windowClosed(fm::FolderWindow*) override {}
};

// Another program's drag of files, with the action the compositor picked.
class FileOffer final : public ui::DropOffer
{
public:
    FileOffer(std::vector<std::string> urls, uint32_t actions)
        : m_urls(std::move(urls))
        , m_actions(actions)
    {
    }

    const std::vector<std::string>& mimeTypes() const override { return m_types; }
    uint32_t sourceActions() const override { return m_actions; }
    uint32_t action() const override { return picked; }

    void read(const std::string&, std::function<void(std::string data)> done) override
    {
        ui::MimeData mime;
        mime.setUrls(m_urls);
        const std::string* data = mime.data("text/uri-list");
        done(data ? *data : std::string());
    }

    uint32_t picked = ui::NoAction;

private:
    std::vector<std::string> m_types {"text/uri-list"};
    std::vector<std::string> m_urls;
    uint32_t m_actions;
};

// A directory on another file system than the temporary directory's.
class OtherVolumeDir
{
public:
    OtherVolumeDir()
    {
        std::string pattern = "/dev/shm/fmtest-XXXXXX";
        struct stat here;
        struct stat there;
        if (!::mkdtemp(pattern.data()))
            return;
        m_path = pattern;
        if (::stat(std::filesystem::temp_directory_path().c_str(), &here) == 0
            && ::stat(m_path.c_str(), &there) == 0 && here.st_dev != there.st_dev)
            m_otherVolume = true;
    }

    ~OtherVolumeDir()
    {
        std::error_code error;
        if (!m_path.empty())
            std::filesystem::remove_all(m_path, error);
    }

    bool isValid() const { return m_otherVolume; }
    std::string filePath(const std::string& name) const { return m_path + "/" + name; }

private:
    std::string m_path;
    bool m_otherVolume = false;
};

} // namespace

static std::string url(const std::string& path)
{
    return base::url::fromLocalFile(path);
}

static bool exists(const std::string& path)
{
    return std::filesystem::exists(path);
}

static constexpr uint32_t kCopyOrMove = ui::CopyAction | ui::MoveAction;

TEST(DropTest, FolderPrefersAMoveWithinItsVolumeAndACopyToAnother)
{
    TemporaryDir target;
    TemporaryDir same;
    OtherVolumeDir other;
    if (!other.isValid())
        GTEST_SKIP() << "no second file system at /dev/shm";
    ASSERT_TRUE(fmtest::writeFile(same.filePath("a")));
    ASSERT_TRUE(fmtest::writeFile(other.filePath("b")));
    fm::FolderModel folder;
    folder.setUrl(target.path());
    ASSERT_TRUE(runUntil(s_app->loop(), [&] { return folder.status() == fm::FolderModel::Status::Ready; }));

    const std::string a = url(same.filePath("a"));
    const std::string b = url(other.filePath("b"));
    const uint32_t all = kCopyOrMove | ui::AskAction;
    EXPECT_EQ(folder.dropActions({a}, -1, all), ui::dropActions(kCopyOrMove, ui::MoveAction));
    EXPECT_EQ(folder.dropActions({b}, -1, all), ui::dropActions(kCopyOrMove, ui::CopyAction));
    EXPECT_EQ(folder.dropActions({a, b}, -1, all), ui::dropActions(kCopyOrMove, ui::CopyAction));
    // Where the volume cannot be told, a copy.
    EXPECT_EQ(folder.dropActions({url(same.filePath("missing"))}, -1, all),
              ui::dropActions(kCopyOrMove, ui::CopyAction));
    EXPECT_EQ(folder.dropActions({"https://example.com/page"}, -1, all), ui::dropActions(kCopyOrMove, ui::CopyAction));
    // A source allowing one action gets that one wherever it goes.
    EXPECT_EQ(folder.dropActions({b}, -1, ui::MoveAction), ui::dropActions(ui::MoveAction, ui::MoveAction));
    EXPECT_EQ(folder.dropActions({a}, -1, ui::CopyAction), ui::dropActions(ui::CopyAction, ui::CopyAction));
    EXPECT_EQ(folder.dropActions({a}, -1, ui::AskAction), uint32_t(ui::NoAction));
    // Refused as before: the folder the files are in already.
    EXPECT_EQ(folder.dropActions({url(target.filePath("c"))}, -1, all), uint32_t(ui::NoAction));
}

TEST(DropTest, PlacesRowsPreferAMoveWithinTheirVolumeAndTheTrashOnlyMoves)
{
    TemporaryDir config;
    TemporaryDir target;
    TemporaryDir same;
    OtherVolumeDir other;
    if (!other.isValid())
        GTEST_SKIP() << "no second file system at /dev/shm";
    const std::string settings = config.filePath("filemanager.conf");
    ASSERT_TRUE(fmtest::writeFile(settings, "[Sidebar]\nfavorites=\n"));
    ASSERT_TRUE(fmtest::writeFile(same.filePath("a")));
    ASSERT_TRUE(fmtest::writeFile(other.filePath("b")));
    fm::PlacesModel places(false, settings);
    ASSERT_TRUE(places.addFavorites({url(target.path())}, 0));
    const int trash = places.favoriteCount();
    ASSERT_TRUE(places.at(trash).url.starts_with("trash:"));

    const std::string a = url(same.filePath("a"));
    const std::string b = url(other.filePath("b"));
    const uint32_t all = kCopyOrMove | ui::AskAction;
    EXPECT_EQ(places.dropActions({a}, 0, all), ui::dropActions(kCopyOrMove, ui::MoveAction));
    EXPECT_EQ(places.dropActions({b}, 0, all), ui::dropActions(kCopyOrMove, ui::CopyAction));
    EXPECT_EQ(places.dropActions({b}, 0, ui::MoveAction), ui::dropActions(ui::MoveAction, ui::MoveAction));
    EXPECT_EQ(places.dropActions({a}, 0, ui::CopyAction), ui::dropActions(ui::CopyAction, ui::CopyAction));
    EXPECT_EQ(places.dropActions({b}, trash, all), uint32_t(ui::MoveAction));
    EXPECT_EQ(places.dropActions({b}, trash, ui::CopyAction), uint32_t(ui::NoAction));
}

// Dropped on the desktop: a move within the volume and a copy to another
// with no key held, and whatever the keys picked otherwise.
TEST(DropTest, DesktopDropTakesTheDefaultOrThePickedAction)
{
    base::EventLoop& loop = s_app->loop();
    TemporaryDir target;
    TemporaryDir same;
    OtherVolumeDir other;
    if (!other.isValid())
        GTEST_SKIP() << "no second file system at /dev/shm";
    for (const char* name : {"a1", "a2"})
        ASSERT_TRUE(fmtest::writeFile(same.filePath(name)));
    for (const char* name : {"b1", "b2"})
        ASSERT_TRUE(fmtest::writeFile(other.filePath(name)));

    fm::OperationsClient::instance();
    base::Process daemon;
    ASSERT_TRUE(daemon.start(FILEMANAGER_DAEMON));
    auto bus = base::dbus::Connection::open(base::dbus::Connection::Bus::Session, &loop);
    ASSERT_TRUE(bus);
    const base::dbus::Proxy manager(bus.get(), "org.cutefish.FileOperations", "/org/cutefish/FileOperations",
                                    "org.cutefish.FileOperations.Manager1");
    ASSERT_TRUE(runUntil(loop, [&] { return manager.call("CanUndo").value<bool>().has_value(); }));

    WindowHost host;
    fm::DesktopWindow desktop(nullptr, host);
    fm::FolderModel& model = desktop.model();
    model.setUrl(url(target.path()));
    desktop.window()->resize({1280, 800});
    desktop.window()->show();
    ASSERT_TRUE(runUntil(loop, [&] { return model.status() == fm::FolderModel::Status::Ready; }));
    desktop.window()->grab();
    fm::DesktopGridView& view = *desktop.view();
    ASSERT_GT(view.columns(), 6);

    int cell = 2;
    const auto dropOn = [&](const std::string& path, uint32_t picked, uint32_t* answer) {
        const ui::RectF rect = view.cellRect(view.rows() * cell++);
        const ui::PointF at = view.mapToWindow({rect.x + rect.width / 2, rect.y + rect.height / 2});
        FileOffer offer({url(path)}, kCopyOrMove | ui::AskAction);
        ui::Window& window = *desktop.window();
        *answer = ui::TestInput::dragEnter(window, at, &offer);
        ui::TestInput::dragMove(window, at);
        offer.picked = picked;
        return ui::TestInput::drop(window);
    };

    uint32_t answer = ui::NoAction;
    EXPECT_EQ(dropOn(same.filePath("a1"), ui::NoAction, &answer), uint32_t(ui::MoveAction));
    EXPECT_EQ(answer, ui::dropActions(kCopyOrMove, ui::MoveAction));
    EXPECT_TRUE(runUntil(loop, [&] { return exists(target.filePath("a1")) && !exists(same.filePath("a1")); }));

    EXPECT_EQ(dropOn(other.filePath("b1"), ui::NoAction, &answer), uint32_t(ui::CopyAction));
    EXPECT_EQ(answer, ui::dropActions(kCopyOrMove, ui::CopyAction));
    EXPECT_TRUE(runUntil(loop, [&] { return exists(target.filePath("b1")); }));
    EXPECT_TRUE(exists(other.filePath("b1")));

    EXPECT_EQ(dropOn(same.filePath("a2"), ui::CopyAction, &answer), uint32_t(ui::CopyAction));
    EXPECT_TRUE(runUntil(loop, [&] { return exists(target.filePath("a2")); }));
    EXPECT_TRUE(exists(same.filePath("a2")));

    EXPECT_EQ(dropOn(other.filePath("b2"), ui::MoveAction, &answer), uint32_t(ui::MoveAction));
    EXPECT_TRUE(runUntil(loop, [&] { return exists(target.filePath("b2")) && !exists(other.filePath("b2")); }));

    daemon.terminate();
    EXPECT_TRUE(daemon.waitForFinished(std::chrono::seconds(3)));
}
