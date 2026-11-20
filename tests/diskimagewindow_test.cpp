// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "testutil.h"

#include "model/foldermodel.h"
#include "views/folderpage.h"
#include "views/folderwindow.h"
#include "views/gridview.h"

#include "base/dbus.h"
#include "base/url.h"
#include "ui/application.h"
#include "ui/image.h"
#include "ui/label.h"
#include "ui/window.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using namespace cutefish;
using base::dbus::Connection;
using base::dbus::MethodCall;
using base::dbus::Variant;
using base::dbus::VariantMap;
using fm::FolderWindow;
using fmtest::runUntil;
using fmtest::TemporaryDir;
using fmtest::writeFile;

extern ui::Application* s_app;

namespace {

// Records where the windows ask folders and volumes to open.
class RecordingHost : public fm::FileManagerHost
{
public:
    fm::FolderWindow* openFolderWindow(const fm::FolderPage::Start&, ui::Window*) override { return nullptr; }
    void openFolders(const std::vector<std::string>& paths) override
    {
        folders.insert(folders.end(), paths.begin(), paths.end());
    }
    void showProperties(const fm::FileItemList&, ui::Window*) override {}
    void showOpenWith(const std::string&, ui::Window*) override {}
    void confirmDelete(const std::vector<std::string>&, ui::Window*) override {}
    void confirmEmptyTrash(ui::Window*) override {}
    void confirmLaunch(const std::string&, ui::Window*) override {}
    void installBundleRunner(const std::string&, ui::Window*) override {}
    void windowClosed(fm::FolderWindow*) override {}
    void openDiskImageWindow(const std::string& path) override { diskImages.push_back(path); }

    std::vector<std::string> folders;
    std::vector<std::string> diskImages;
};

} // namespace

static ui::View* findView(ui::View* view, const std::string& name)
{
    if (view->name() == name)
        return view;
    for (const auto& child : view->children()) {
        if (ui::View* found = findView(child.get(), name))
            return found;
    }
    return nullptr;
}

static int rowNamed(const fm::FolderModel& model, const std::string& name)
{
    for (int row = 0; row < model.count(); ++row) {
        if (model.item(row)->name == name)
            return row;
    }
    return -1;
}

static void selectRow(fm::FolderModel& model, const std::string& name)
{
    const int row = rowNamed(model, name);
    ASSERT_GE(row, 0) << name;
    model.clearSelection();
    model.setSelected(row);
    model.prepareContextMenu();
}

// The window a disk image's arrangement asks for: 660 by 422 with its title
// bar, over a 660 by 400 picture.
static std::shared_ptr<fm::ArrangedFolder> ghosttyLike(const std::string& url)
{
    auto arranged = std::make_shared<fm::ArrangedFolder>();
    arranged->url = url;
    arranged->layout.windowSize = {660, 422};
    arranged->layout.iconSize = 160;
    arranged->layout.background = fm::FolderLayout::Background::Picture;
    arranged->layout.positions = {{"Ghostty.app", {180, 170}}, {"Applications", {480, 170}}};
    arranged->picture = ui::Image(660, 400);
    return arranged;
}

static void makeVolume(const TemporaryDir& volume)
{
    std::filesystem::create_directories(volume.filePath("Applications"));
    ASSERT_TRUE(writeFile(volume.filePath("Read Me")));
}

// Opened from the image, the volume's window is its picture under the window
// controls and its name: no sidebar, path bar or search, the content the
// picture's size; folders opened in it, and the one above, open in browser
// windows.
TEST(DiskImageWindowTest, OpensTheVolumeAloneAtThePicturesSize)
{
    base::EventLoop& loop = s_app->loop();
    TemporaryDir volume;
    ASSERT_TRUE(volume.isValid());
    makeVolume(volume);

    RecordingHost host;
    fm::FolderPage::Start start;
    start.url = volume.path();
    start.arrangement = ghosttyLike(volume.path());
    FolderWindow folderWindow(&host, start, {}, FolderWindow::Kind::DiskImage);
    ui::Window& window = folderWindow.window();
    folderWindow.show();
    fm::FolderModel& model = folderWindow.currentPage()->model();
    ASSERT_TRUE(runUntil(loop, [&] { return model.status() == fm::FolderModel::Status::Ready && model.count() == 2; }));
    window.grab();

    EXPECT_EQ(folderWindow.kind(), FolderWindow::Kind::DiskImage);
    EXPECT_EQ(window.size().width, 660);
    EXPECT_EQ(window.size().height, 400 + FolderWindow::kHeaderInset);
    ui::View* root = window.contentView();
    EXPECT_FALSE(findView(root, "sideBar")->isVisible());
    EXPECT_FALSE(findView(root, "sidebarDivider")->isVisible());
    EXPECT_FALSE(findView(root, "pathBar")->isVisible());
    EXPECT_FALSE(findView(root, "searchButton")->parent()->isVisible());
    EXPECT_FALSE(findView(root, "backButton")->parent()->isVisible());
    EXPECT_FALSE(findView(root, "folderTabBar")->isVisible());

    const std::string name = std::filesystem::path(volume.path()).filename().string();
    auto* title = static_cast<ui::Label*>(findView(root, "windowTitle"));
    ASSERT_NE(title, nullptr);
    EXPECT_TRUE(title->isVisible());
    EXPECT_EQ(title->text(), name);
    // Centred on the window, its capitals on the window controls' line.
    const ui::RectF titleRect = title->geometry();
    EXPECT_NEAR(titleRect.x + titleRect.width / 2, 330, 1);
    EXPECT_NEAR(titleRect.y + title->capCenter(), FolderWindow::kHeaderInset / 2, 0.5);

    // The icons sit where the image put them, from the top of the picture.
    auto* grid = dynamic_cast<fm::FolderGridView*>(folderWindow.currentPage()->view());
    ASSERT_NE(grid, nullptr);
    ASSERT_NE(grid->arrangement(), nullptr);
    const ui::RectF applications = grid->cellRect(rowNamed(model, "Applications"));
    EXPECT_EQ(applications.x + applications.width / 2, 480);

    selectRow(model, "Applications");
    EXPECT_FALSE(model.action("openInNewTab").visible);
    model.openSelected();
    ASSERT_EQ(host.folders.size(), 1u);
    EXPECT_EQ(host.folders.back(), volume.filePath("Applications"));
    EXPECT_EQ(folderWindow.currentPage()->currentUrl(), volume.path());

    model.up();
    ASSERT_EQ(host.folders.size(), 2u);
    EXPECT_EQ(folderWindow.currentPage()->currentUrl(), volume.path());
    EXPECT_EQ(folderWindow.tabCount(), 1);
}

// The same volume in a browser window keeps the sidebar and the header, and
// a folder opened there opens in the window.
TEST(DiskImageWindowTest, ABrowserWindowOnTheVolumeKeepsItsBars)
{
    base::EventLoop& loop = s_app->loop();
    TemporaryDir volume;
    ASSERT_TRUE(volume.isValid());
    makeVolume(volume);

    RecordingHost host;
    fm::FolderPage::Start start;
    start.url = volume.path();
    start.arrangement = ghosttyLike(volume.path());
    FolderWindow folderWindow(&host, start);
    ui::Window& window = folderWindow.window();
    folderWindow.show();
    fm::FolderModel& model = folderWindow.currentPage()->model();
    ASSERT_TRUE(runUntil(loop, [&] { return model.status() == fm::FolderModel::Status::Ready && model.count() == 2; }));
    window.grab();

    EXPECT_EQ(folderWindow.kind(), FolderWindow::Kind::Browser);
    ui::View* root = window.contentView();
    EXPECT_TRUE(findView(root, "sideBar")->isVisible());
    EXPECT_TRUE(findView(root, "pathBar")->isVisible());
    EXPECT_FALSE(findView(root, "windowTitle")->isVisible());
    // Still arranged by the image.
    auto* grid = dynamic_cast<fm::FolderGridView*>(folderWindow.currentPage()->view());
    ASSERT_NE(grid, nullptr);
    EXPECT_NE(grid->arrangement(), nullptr);

    selectRow(model, "Applications");
    EXPECT_TRUE(model.action("openInNewTab").visible);
    model.openSelected();
    ASSERT_TRUE(runUntil(loop, [&] { return folderWindow.currentPage()->currentUrl() == volume.filePath("Applications"); }));
    EXPECT_TRUE(host.folders.empty());
}

// Opening an image file asks mountd to mount it and hands the mounted volume
// to the host as a disk image's window, not a browser window.
TEST(DiskImageWindowTest, OpeningAnImageAsksForItsVolumesWindow)
{
    base::EventLoop& loop = s_app->loop();
    TemporaryDir folder;
    TemporaryDir volume;
    ASSERT_TRUE(folder.isValid() && volume.isValid());
    ASSERT_TRUE(writeFile(folder.filePath("Ghostty.dmg")));

    std::unique_ptr<Connection> service = Connection::open(Connection::Bus::Session, &loop);
    ASSERT_TRUE(service && service->requestName("org.cutefish.Mounts1"));
    std::vector<std::string> mounted;
    base::dbus::Object mounts(service.get(), "/org/cutefish/Mounts1", "org.cutefish.Mounts1");
    mounts.addMethod("Mount", "sa{sv}", "sa{sv}", [&](MethodCall& call) {
        std::string location;
        VariantMap options;
        call.read(location, options);
        mounted.push_back(location);
        call.reply(std::string("mounted"), VariantMap {{"path", Variant(volume.path())}});
    });
    mounts.addMethod("Mounts", "", "aa{sv}", [](MethodCall& call) { call.reply(std::vector<VariantMap> {}); });
    ASSERT_TRUE(mounts.publish());

    RecordingHost host;
    fm::FolderModel model(nullptr);
    struct ModelHost : fm::FolderModelHost
    {
        RecordingHost* host;
        void openFolders(const std::vector<std::string>& paths) override { host->openFolders(paths); }
        void showProperties(const fm::FileItemList&) override {}
        void showOpenWith(const std::string&) override {}
        void confirmDelete(const std::vector<std::string>&) override {}
        void confirmEmptyTrash() override {}
        void confirmLaunch(const std::string&) override {}
        void installBundleRunner(const std::string&) override {}
        void notify(const std::string&) override {}
        void openDiskImageWindow(const std::string& path) override { host->openDiskImageWindow(path); }
    } modelHost;
    modelHost.host = &host;
    model.setHost(&modelHost);
    model.setUrl(folder.path());
    ASSERT_TRUE(runUntil(loop, [&] { return model.status() == fm::FolderModel::Status::Ready && model.count() == 1; }));
    ASSERT_EQ(model.item(0)->mimeType, "application/x-apple-diskimage");

    selectRow(model, "Ghostty.dmg");
    model.openSelected();
    ASSERT_TRUE(runUntil(loop, [&] { return !host.diskImages.empty(); }));
    EXPECT_EQ(mounted, std::vector<std::string> {folder.filePath("Ghostty.dmg")});
    EXPECT_EQ(host.diskImages, std::vector<std::string> {volume.path()});
    EXPECT_TRUE(host.folders.empty());
}
