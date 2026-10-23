// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "model/foldermodel.h"

#include "base/dbus.h"
#include "base/signal.h"
#include "base/timer.h"
#include "ui/geometry.h"

#include <memory>
#include <string>
#include <vector>

namespace cutefish::ui {
struct KeyEvent;
class Menu;
class TrustedConnection;
class Window;
}

namespace fm {

class DesktopGridView;
class DesktopMenuExporter;
class DesktopRoot;
class FileManagerHost;
class PreviewClient;

// The desktop: the icons of ~/Desktop in a transparent window with the
// desktop role on the primary output, over the compositor's wallpaper, on
// the session's trusted connection.
class DesktopWindow : public FolderModelHost
{
public:
    // Without a connection, an ordinary window of the output's size, for
    // trying the desktop out in a session that has one already.
    DesktopWindow(ui::TrustedConnection* connection, FileManagerHost& host);
    ~DesktopWindow() override;

    ui::Window* window() const { return m_window.get(); }
    DesktopGridView* view() const { return m_view; }
    FolderModel& model() { return *m_model; }

    // FolderModelHost.
    void openFolders(const std::vector<std::string>& paths) override;
    void showProperties(const FileItemList& items) override;
    void showOpenWith(const std::string& url) override;
    void confirmDelete(const std::vector<std::string>& urls) override;
    void confirmEmptyTrash() override;
    void confirmLaunch(const std::string& path) override;
    void notify(const std::string& message) override;

private:
    friend class DesktopRoot;

    void followPrimaryOutput();
    void placeOn(const std::string& outputName);
    void retranslate();
    void showContextMenu(ui::PointF position);
    bool handleKey(const ui::KeyEvent& event);
    void loadLayout();
    void saveLayout();
    std::string layoutKey() const;

    ui::TrustedConnection* m_connection;
    FileManagerHost& m_host;
    std::unique_ptr<FolderModel> m_model;
    std::unique_ptr<ui::Window> m_window;
    DesktopRoot* m_root = nullptr;
    DesktopGridView* m_view = nullptr;
    std::unique_ptr<PreviewClient> m_preview;
    std::unique_ptr<ui::Menu> m_menu;
    std::unique_ptr<DesktopMenuExporter> m_menuExporter;
    std::unique_ptr<class ArchivePasswordPrompt> m_archivePassword;
    std::string m_output;
    bool m_layoutLoaded = false;
    bool m_restoring = false;
    bool m_activated = false;
    base::Timer m_saveLayout;
    base::Timer m_restoreLayout;
    base::Timer m_menuUpdate;
    base::dbus::Proxy m_display;
    base::dbus::Match m_displayChanged;
    base::dbus::PendingCall m_displayState;
    base::dbus::NameWatch m_displayWatch;
    std::vector<base::Subscription> m_subscriptions;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

} // namespace fm
