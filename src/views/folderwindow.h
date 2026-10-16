// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/common.h"
#include "model/foldermodel.h"
#include "views/folderpage.h"

#include "base/signal.h"
#include "base/timer.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cutefish::ui {
class HeaderButton;
class Menu;
class Toast;
class Window;
}

namespace fm {

class FolderTabBar;
class FolderWindowRoot;
class PathBar;
class SideBar;

// What a browser window asks of the process: other windows and the dialogs.
class FileManagerHost
{
public:
    virtual ~FileManagerHost() = default;

    virtual void openFolderWindow(const FolderPage::Start& start, ui::Window* from) = 0;
    virtual void openFolders(const std::vector<std::string>& paths) = 0;
    virtual void showProperties(const FileItemList& items, ui::Window* parent) = 0;
    virtual void showOpenWith(const std::string& url, ui::Window* parent) = 0;
    virtual void confirmDelete(const std::vector<std::string>& urls, ui::Window* parent) = 0;
    virtual void confirmEmptyTrash(ui::Window* parent) = 0;
    virtual void confirmLaunch(const std::string& path, ui::Window* parent) = 0;
    virtual void windowClosed(class FolderWindow* window) = 0;
};

// The browser window. The sidebar, the header with back, forward,
// the path bar, search and the options, the tab bar, the search scope and
// trash bars over the pages, which run up under them.
class FolderWindow : public FolderModelHost
{
public:
    FolderWindow(FileManagerHost* host, const FolderPage::Start& start);
    ~FolderWindow() override;

    ui::Window& window() const { return *m_window; }
    FolderPage* currentPage() const;
    int tabCount() const { return int(m_tabs.size()); }
    int currentTab() const { return m_currentTab; }

    void openTab(const std::string& url);
    void openTabSelecting(const FolderPage::Start& start, int index = -1);
    void closeTab(int index);
    void setCurrentTab(int index);
    void moveTab(int from, int to);
    void show();
    void close();

    // FolderModelHost.
    void openFolders(const std::vector<std::string>& paths) override;
    void showProperties(const FileItemList& items) override;
    void showOpenWith(const std::string& url) override;
    void confirmDelete(const std::vector<std::string>& urls) override;
    void confirmEmptyTrash() override;
    void confirmLaunch(const std::string& path) override;
    void notify(const std::string& message) override;

    // The header's height: buttons 32 with 4 above and below.
    static constexpr double kHeaderInset = 40;

private:
    friend class FolderWindowRoot;

    struct Tab
    {
        int id;
        FolderPage* page;
        std::vector<base::Subscription> subscriptions;
    };

    void syncNavigation();
    void updateHeader();
    void updateBackground();
    void updateTabBar();
    void showContextMenu(ui::PointF position);
    void showOptionsMenu();
    double headerHeight() const;
    void addShortcuts();

    FileManagerHost* m_host;
    std::unique_ptr<ui::Window> m_window;
    FolderWindowRoot* m_root = nullptr;
    SideBar* m_sidebar = nullptr;
    PathBar* m_pathBar = nullptr;
    FolderTabBar* m_tabBar = nullptr;
    ui::HeaderButton* m_back = nullptr;
    ui::HeaderButton* m_forward = nullptr;
    ui::HeaderButton* m_options = nullptr;
    std::vector<Tab> m_tabs;
    int m_currentTab = 0;
    int m_nextTabId = 1;
    std::unique_ptr<ui::Menu> m_menu;
    std::unique_ptr<ui::Toast> m_toast;
    std::unique_ptr<class ArchiveProgressDialog> m_archiveDialog;
    std::vector<base::Subscription> m_pageSubscriptions;
    std::vector<base::Subscription> m_subscriptions;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

} // namespace fm
