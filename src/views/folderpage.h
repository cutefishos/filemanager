// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/common.h"

#include "base/signal.h"
#include "ui/view.h"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace cutefish::ui {
class HeaderBackdrop;
class Label;
}

namespace fm {

class FolderModel;
class FolderModelHost;
class FolderView;
class PreviewClient;

// One tab's folder, its model and its grid or list view,
// which runs up under the window's bars.
class FolderPage : public ui::View
{
public:
    struct Start
    {
        std::string url;
        std::vector<std::string> selection;
        std::vector<std::string> back;
        std::vector<std::string> forward;
    };

    FolderPage(FolderModelHost* host, const Start& start);
    ~FolderPage() override;

    FolderModel& model() const { return *m_model; }
    FolderView* view() const { return m_view; }
    std::string currentUrl() const;
    std::string folderName() const;
    // "Searching “…”" while searching, else the folder's name.
    std::string tabTitle() const;

    // The bars over the page's top: the view starts its items under them.
    void setHeaderHeight(double height);
    void openUrl(const std::string& url);
    void focusView();
    void goBack();
    void goForward();
    // Quick Look follows this page while it is the window's current one.
    void activatePreview();

    std::function<void(const std::string& url)> onOpenInNewTab;
    std::function<void()> onCloseRequested;
    std::function<void()> onRequestPathEditor;
    std::function<void(ui::PointF windowPosition)> onContextMenu;
    base::Signal<> currentUrlChanged;

protected:
    void layout() override;
    void paint(ui::Painter& painter) override;

private:
    void createView();
    void restoreViewPosition(bool complete);
    bool handleKey(const ui::KeyEvent& event);

    std::unique_ptr<FolderModel> m_model;
    std::unique_ptr<PreviewClient> m_preview;
    FolderView* m_view = nullptr;
    ui::HeaderBackdrop* m_backdrop = nullptr;
    ui::Label* m_noResults;
    class SearchPathStrip* m_pathStrip = nullptr;
    double m_headerHeight = 0;
    int m_viewMethod = -1;
    double m_pendingViewPosition = NAN;
    std::vector<base::Subscription> m_subscriptions;
};

} // namespace fm
