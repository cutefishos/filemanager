// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "views/colors.h"
#include "views/folderwindow.h"
#include "core/preferences.h"
#include "views/folderview.h"
#include "dialogs/archiveprogressdialog.h"
#include "views/foldermenu.h"
#include "views/pathbar.h"
#include "views/sidebar.h"
#include "views/style.h"
#include "views/tabbar.h"

#include "base/event_loop.h"
#include "ui/color_provider.h"
#include "ui/progress_bar.h"
#include "ui/dialog_window.h"
#include "ui/header_button.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/menu.h"
#include "ui/painter.h"
#include "ui/cell_layout.h"
#include "ui/theme.h"
#include "ui/toast.h"
#include "ui/window.h"
#include "ui/window_header.h"

#include <algorithm>
#include <cmath>

namespace fm {

using ui::rounding::centeredStart;

static constexpr double kSmallSpacing = 6;
static constexpr double kLargeSpacing = 12;
static constexpr double kButtonSize = 32;
static constexpr double kButtonsTopMargin = 4;
static constexpr double kBarHeight = 34;

// Where a search looks, home or the folder it started from.
class SearchScopeBar : public ui::View
{
public:
    SearchScopeBar()
    {
        setName("searchScopeBar");
        m_label = addChild<ui::Label>("Search:");
        m_home = addChild<ui::Label>("Home");
        m_folder = addChild<ui::Label>();
        m_folder->setElide(ui::Label::Elide::Middle);
        m_busy = addChild<ui::BusyIndicator>();
    }

    void setModel(FolderModel* model, const std::string& folderName)
    {
        m_model = model;
        m_folder->setText("“" + folderName + "”");
        requestLayout();
        requestPaint();
    }

    bool acceptsPointer() const override { return true; }

protected:
    bool folderIsHome() const
    {
        return m_model && m_model->resolvedUrl() == FolderModel::resolve("~");
    }

    void layout() override
    {
        const ui::Theme& theme = ui::Theme::current();
        const double height = geometry().height;
        const double scopeHeight = 24;
        double x = kSmallSpacing * 1.5 + kSmallSpacing;
        m_label->setColor(theme.disabledText);
        const ui::SizeF label = m_label->sizeHint();
        m_label->setGeometry({x, centeredStart(height, label.height), std::ceil(label.width), label.height});
        x += std::ceil(label.width) + kSmallSpacing;

        const bool home = folderIsHome() || (m_model && m_model->searchHome());
        const bool folder = m_model && !m_model->searchHome();
        m_homeRect = place(m_home, x, home);
        x = m_homeRect.x + m_homeRect.width + kSmallSpacing;
        m_folder->setVisible(!folderIsHome());
        if (m_folder->isVisible())
            m_folderRect = place(m_folder, x, folder, 240);
        else
            m_folderRect = {};

        m_busy->setVisible(m_model && m_model->searchBusy());
        m_busy->setGeometry({geometry().width - kLargeSpacing - 16, centeredStart(height, 16), 16, 16});
        (void)scopeHeight;
    }

    ui::RectF place(ui::Label* label, double x, bool selected, double maximum = 0)
    {
        const ui::Theme& theme = ui::Theme::current();
        const double height = geometry().height;
        const ui::SizeF hint = label->sizeHint();
        double width = std::ceil(hint.width + kLargeSpacing * 2);
        if (maximum > 0)
            width = std::min(width, maximum);
        label->setColor(selected ? theme.highlight : theme.text);
        const double textWidth = std::min(hint.width, width - kLargeSpacing * 2);
        const ui::RectF rect {x, centeredStart(height, 24), width, 24};
        label->setGeometry({rect.x + centeredStart(width, textWidth), rect.y + centeredStart(24, hint.height),
                            textWidth, hint.height});
        return rect;
    }

    void paint(ui::Painter& painter) override
    {
        const auto plate = [&](const ui::RectF& rect, bool selected, int index) {
            ui::Color fill {0, 0, 0, 0};
            if (selected)
                fill = ui::color(FmColor::SearchScopeSelected);
            else if (m_pressed == index)
                fill = style::textAlpha(0.1);
            else if (m_hovered == index)
                fill = style::textAlpha(0.05);
            if (fill.a)
                painter.drawSmoothRectangle(rect, rect.height / 2, fill);
        };
        plate(m_homeRect, folderIsHome() || (m_model && m_model->searchHome()), 0);
        if (m_folder->isVisible())
            plate(m_folderRect, m_model && !m_model->searchHome(), 1);
    }

    int scopeAt(ui::PointF point) const
    {
        if (m_homeRect.contains(point))
            return 0;
        if (m_folder->isVisible() && m_folderRect.contains(point))
            return 1;
        return -1;
    }

    void hoverMove(const ui::PointerEvent& event) override
    {
        const int hovered = scopeAt(event.position);
        if (hovered != m_hovered) {
            m_hovered = hovered;
            requestPaint();
        }
    }
    void pointerMove(const ui::PointerEvent& event) override { hoverMove(event); }
    void pointerLeave() override
    {
        m_hovered = -1;
        requestPaint();
    }
    void pointerPress(const ui::PointerEvent& event) override
    {
        m_pressed = scopeAt(event.position);
        requestPaint();
    }
    void pointerRelease(const ui::PointerEvent& event) override
    {
        const int pressed = m_pressed;
        m_pressed = -1;
        requestPaint();
        if (pressed < 0 || pressed != scopeAt(event.position) || !m_model)
            return;
        m_model->setSearchHome(pressed == 0);
        requestLayout();
    }

private:
    FolderModel* m_model = nullptr;
    ui::Label* m_label;
    ui::Label* m_home;
    ui::Label* m_folder;
    ui::BusyIndicator* m_busy;
    ui::RectF m_homeRect;
    ui::RectF m_folderRect;
    int m_hovered = -1;
    int m_pressed = -1;
};

// "Trash" and its Empty button.
class TrashBar : public ui::View
{
public:
    TrashBar()
    {
        setName("trashBar");
        m_title = addChild<ui::Label>("Trash");
        m_title->setWeight(600);
        m_empty = addChild<ui::DialogButton>("Empty");
        m_empty->setName("emptyTrashButton");
        m_empty->onClicked = [this] {
            if (m_model)
                m_model->emptyTrash();
        };
    }

    void setModel(FolderModel* model)
    {
        m_model = model;
        requestLayout();
    }

    bool acceptsPointer() const override { return true; }

protected:
    void layout() override
    {
        const double height = geometry().height;
        const ui::SizeF title = m_title->sizeHint();
        m_title->setGeometry({kSmallSpacing * 1.5 + kSmallSpacing, centeredStart(height, title.height),
                              std::ceil(title.width), title.height});
        const ui::SizeF button = m_empty->sizeHint();
        const double width = std::ceil(button.width);
        m_empty->setGeometry({geometry().width - kLargeSpacing - width, centeredStart(height, 24), width, 24});
        m_empty->setEnabled(m_model && m_model->count() > 0);
    }

private:
    FolderModel* m_model = nullptr;
    ui::Label* m_title;
    ui::DialogButton* m_empty;
};

// The header's own part: back, forward, the path bar, search and options,
// from the sidebar's edge.
class HeaderRow : public ui::View
{
public:
    HeaderRow()
    {
        back = addChild<ui::HeaderButton>(std::string(), kButtonSize);
        back->setGlyph("chevron-left", 16);
        back->setName("backButton");
        forward = addChild<ui::HeaderButton>(std::string(), kButtonSize);
        forward->setGlyph("chevron-right", 16);
        forward->setName("forwardButton");
        pathBar = addChild<PathBar>();
        search = addChild<ui::HeaderButton>(std::string(), kButtonSize);
        search->setGlyph("search", 16);
        search->setName("searchButton");
        options = addChild<ui::HeaderButton>("menu", kButtonSize);
        options->setName("optionsButton");
        for (ui::HeaderButton* button : {back, forward, search, options})
            button->setPlateAlphas({0.17, 0.26}, {0.21, 0.15});
    }

    ui::HeaderButton* back;
    ui::HeaderButton* forward;
    PathBar* pathBar;
    ui::HeaderButton* search;
    ui::HeaderButton* options;

protected:
    void layout() override
    {
        const double spacing = 1;
        double x = SideBar::kWidth + kSmallSpacing * 1.5;
        const double right = geometry().width;
        back->setGeometry({x, kButtonsTopMargin, kButtonSize, kButtonSize});
        x += kButtonSize + spacing;
        forward->setGeometry({x, kButtonsTopMargin, kButtonSize, kButtonSize});
        x += kButtonSize + spacing;
        const double end = right - spacing;
        options->setGeometry({end - kButtonSize, kButtonsTopMargin, kButtonSize, kButtonSize});
        search->setGeometry({end - kButtonSize * 2 - spacing, kButtonsTopMargin, kButtonSize, kButtonSize});
        const double pathEnd = end - kButtonSize * 2 - spacing * 2;
        pathBar->setGeometry({x, kButtonsTopMargin, std::max(0.0, pathEnd - x), kButtonSize});
    }
};

class FolderWindowRoot : public ui::View
{
public:
    explicit FolderWindowRoot(FolderWindow* window)
        : m_window(window)
    {
        sidebar = addChild<SideBar>(FolderWindow::kHeaderInset);
        pages = addChild<ui::View>();
        pages->setName("pages");
        tabBar = addChild<FolderTabBar>();
        scopeBar = addChild<SearchScopeBar>();
        trashBar = addChild<TrashBar>();
        header = addChild<ui::WindowHeader>(FolderWindow::kHeaderInset);
        row = header->setContent<HeaderRow>();
    }

    SideBar* sidebar;
    ui::View* pages;
    FolderTabBar* tabBar;
    SearchScopeBar* scopeBar;
    TrashBar* trashBar;
    ui::WindowHeader* header;
    HeaderRow* row;
    bool tabsShown = false;
    bool scopeShown = false;
    bool trashShown = false;

    double barsHeight() const
    {
        return FolderWindow::kHeaderInset + (tabsShown ? FolderTabBar::kHeight : 0) + (scopeShown ? kBarHeight : 0)
                + (trashShown ? kBarHeight : 0);
    }

protected:
    void layout() override
    {
        const double width = geometry().width;
        const double height = geometry().height;
        const double left = SideBar::kWidth;
        sidebar->setGeometry({0, 0, left, height});
        const ui::RectF content {left, 0, std::max(0.0, width - left), height};
        pages->setGeometry(content);
        for (const auto& page : pages->children())
            page->setGeometry({0, 0, content.width, content.height});

        double y = FolderWindow::kHeaderInset;
        tabBar->setVisible(tabsShown);
        tabBar->setGeometry({left, y, content.width, tabsShown ? FolderTabBar::kHeight : 0});
        y += tabsShown ? FolderTabBar::kHeight : 0;
        scopeBar->setVisible(scopeShown);
        scopeBar->setGeometry({left, y, content.width, scopeShown ? kBarHeight : 0});
        y += scopeShown ? kBarHeight : 0;
        trashBar->setVisible(trashShown);
        trashBar->setGeometry({left, y, content.width, trashShown ? kBarHeight : 0});

        header->setGeometry({0, 0, width, FolderWindow::kHeaderInset});
    }

private:
    FolderWindow* m_window;
};

FolderWindow::FolderWindow(FileManagerHost* host, const FolderPage::Start& start)
    : m_host(host)
    , m_window(std::make_unique<ui::Window>())
{
    m_window->setTitle("Files");
    m_window->resize({900, 580});
    m_window->setMinimumSize({600, 500});
    m_window->setRestoresSize("main");

    auto root = std::make_unique<FolderWindowRoot>(this);
    m_root = root.get();
    m_window->setContentView(std::move(root));
    m_sidebar = m_root->sidebar;
    m_sidebar->setTitle("Files");
    m_pathBar = m_root->row->pathBar;
    m_tabBar = m_root->tabBar;
    m_back = m_root->row->back;
    m_forward = m_root->row->forward;
    m_options = m_root->row->options;
    m_toast = std::make_unique<ui::Toast>(m_window.get());

    m_back->onClicked = [this] {
        if (FolderPage* page = currentPage())
            page->goBack();
    };
    m_forward->onClicked = [this] {
        if (FolderPage* page = currentPage())
            page->goForward();
    };
    m_root->row->search->onClicked = [this] { m_pathBar->openSearch(); };
    m_options->onClicked = [this] { showOptionsMenu(); };

    m_pathBar->onItemClicked = [this](const std::string& path) {
        if (FolderPage* page = currentPage())
            page->openUrl(path);
    };
    m_pathBar->onEditorAccepted = [this](const std::string& path) {
        if (FolderPage* page = currentPage())
            page->openUrl(path);
    };
    m_pathBar->onOpenInNewTab = [this](const std::string& path) { openTab(path); };
    m_pathBar->onOpenInNewWindow = [this](const std::string& path) {
        if (FolderPage* page = currentPage())
            page->model().openInNewWindow(path);
    };
    m_pathBar->onFinished = [this] {
        if (FolderPage* page = currentPage())
            page->focusView();
    };

    m_sidebar->onClicked = [this](const std::string& path) {
        if (FolderPage* page = currentPage())
            page->openUrl(path);
    };
    m_sidebar->onOpenInNewTab = [this](const std::string& path) { openTab(path); };
    m_sidebar->onOpenInNewWindow = [this](const std::string& path) {
        if (FolderPage* page = currentPage())
            page->model().openInNewWindow(path);
    };

    m_tabBar->onSelected = [this](int index) { setCurrentTab(index); };
    m_tabBar->onCloseRequested = [this](int index) { closeTab(index); };
    m_tabBar->onNewRequested = [this] {
        if (FolderPage* page = currentPage())
            openTab(page->currentUrl());
    };
    m_tabBar->onDuplicateRequested = [this](int index) {
        if (index >= 0 && index < int(m_tabs.size()))
            openTab(m_tabs[size_t(index)].page->currentUrl());
    };
    m_tabBar->onMoveRequested = [this](int from, int to) { moveTab(from, to); };

    m_window->onActiveChanged = [this] {
        updateBackground();
        if (m_window->isActive()) {
            if (FolderPage* page = currentPage())
                page->activatePreview();
        }
    };
    m_window->onStatesChanged = [this] { updateBackground(); };
    m_window->onCloseRequested = [this] { close(); };
    m_subscriptions.push_back(Preferences::instance().changed.connect([this] { updateHeader(); }));

    addShortcuts();
    openTabSelecting(start);
    updateBackground();
}

FolderWindow::~FolderWindow()
{
    *m_alive = false;
}

FolderPage* FolderWindow::currentPage() const
{
    return m_tabs.empty() ? nullptr : m_tabs[size_t(std::clamp(m_currentTab, 0, int(m_tabs.size()) - 1))].page;
}

void FolderWindow::show()
{
    m_window->show();
}

void FolderWindow::close()
{
    // The window goes on the next turn, not from inside its own handlers.
    m_window->hide();
    base::EventLoop::current()->post([this, alive = m_alive] {
        if (*alive)
            m_host->windowClosed(this);
    });
}

void FolderWindow::addShortcuts()
{
    ui::Window& window = *m_window;
    window.addShortcut(ui::ControlModifier, 't', [this] {
        if (FolderPage* page = currentPage())
            openTab(page->currentUrl());
    });
    window.addShortcut(ui::ControlModifier, 'w', [this] { closeTab(m_currentTab); });
    window.addShortcut(ui::ControlModifier, ui::key::Tab, [this] {
        setCurrentTab((m_currentTab + 1) % std::max(1, tabCount()));
    });
    window.addShortcut(ui::ControlModifier | ui::ShiftModifier, ui::key::Tab, [this] {
        setCurrentTab((m_currentTab + tabCount() - 1) % std::max(1, tabCount()));
    });
    window.addShortcut(ui::ControlModifier, 'f', [this] { m_pathBar->openSearch(); });
    window.addShortcut(ui::ControlModifier, 'k', [this] { m_sidebar->connectToServer(); });
}

void FolderWindow::openTab(const std::string& url)
{
    openTabSelecting({url, {}, {}, {}});
}

void FolderWindow::openTabSelecting(const FolderPage::Start& start, int index)
{
    auto page = std::make_unique<FolderPage>(this, start);
    FolderPage* raw = page.get();
    m_root->pages->addChild(std::move(page));

    Tab tab {m_nextTabId++, raw, {}};
    tab.subscriptions.push_back(raw->currentUrlChanged.connect([this, raw] {
        if (currentPage() == raw)
            syncNavigation();
        updateTabBar();
    }));
    tab.subscriptions.push_back(raw->model().historyChanged.connect([this, raw] {
        if (currentPage() == raw)
            updateHeader();
    }));
    tab.subscriptions.push_back(raw->model().notification.connect([this](const std::string& message) {
        notify(message);
    }));
    tab.subscriptions.push_back(raw->model().countChanged.connect([this, raw] {
        if (currentPage() == raw)
            m_root->trashBar->requestLayout();
    }));
    tab.subscriptions.push_back(raw->model().searchChanged.connect([this, raw] {
        if (currentPage() == raw)
            updateHeader();
        updateTabBar();
    }));
    raw->onOpenInNewTab = [this](const std::string& url) { openTab(url); };
    raw->onCloseRequested = [this] { closeTab(m_currentTab); };
    raw->onRequestPathEditor = [this] { m_pathBar->openEditor(); };
    raw->onContextMenu = [this](ui::PointF position) { showContextMenu(position); };

    const int at = index < 0 || index > int(m_tabs.size()) ? int(m_tabs.size()) : index;
    m_tabs.insert(m_tabs.begin() + at, std::move(tab));
    m_currentTab = at;
    syncNavigation();
}

void FolderWindow::closeTab(int index)
{
    if (index < 0 || index >= int(m_tabs.size()))
        return;
    if (m_tabs.size() == 1) {
        close();
        return;
    }
    FolderPage* page = m_tabs[size_t(index)].page;
    m_tabs.erase(m_tabs.begin() + index);
    m_currentTab = std::max(0, m_currentTab - (index <= m_currentTab ? 1 : 0));
    std::shared_ptr<ui::View> gone = m_root->pages->takeChild(page);
    base::EventLoop::current()->post([gone] {});
    syncNavigation();
}

void FolderWindow::setCurrentTab(int index)
{
    if (index < 0 || index >= int(m_tabs.size()) || index == m_currentTab)
        return;
    m_currentTab = index;
    syncNavigation();
    if (FolderPage* page = currentPage())
        page->activatePreview();
}

void FolderWindow::moveTab(int from, int to)
{
    const int count = int(m_tabs.size());
    if (from == to || from < 0 || to < 0 || from >= count || to >= count)
        return;
    FolderPage* active = currentPage();
    Tab tab = std::move(m_tabs[size_t(from)]);
    m_tabs.erase(m_tabs.begin() + from);
    m_tabs.insert(m_tabs.begin() + to, std::move(tab));
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].page == active)
            m_currentTab = int(i);
    }
    syncNavigation();
}

void FolderWindow::syncNavigation()
{
    FolderPage* page = currentPage();
    if (!page)
        return;
    for (const Tab& tab : m_tabs)
        tab.page->setVisible(tab.page == page);
    m_pathBar->setFolderModel(&page->model());
    if (!m_archiveDialog)
        m_archiveDialog = std::make_unique<ArchiveProgressDialog>();
    m_archiveDialog->setModel(&page->model(), m_window.get());
    m_pathBar->closeEditor();
    m_sidebar->updateSelection(page->currentUrl());
    m_pathBar->updateUrl(page->currentUrl());
    updateTabBar();
    updateHeader();
    page->focusView();
}

void FolderWindow::updateTabBar()
{
    std::vector<std::pair<int, std::string>> tabs;
    for (const Tab& tab : m_tabs)
        tabs.push_back({tab.id, tab.page->tabTitle()});
    m_tabBar->setTabs(tabs);
    m_tabBar->setCurrentIndex(m_currentTab);
}

double FolderWindow::headerHeight() const
{
    return m_root->barsHeight();
}

void FolderWindow::updateHeader()
{
    FolderPage* page = currentPage();
    if (!page)
        return;
    FolderModel& model = page->model();
    m_back->setEnabled(model.canGoBack());
    m_forward->setEnabled(model.canGoForward());

    m_root->tabsShown = m_tabs.size() > 1;
    m_root->scopeShown = !model.searchText().empty();
    m_root->trashShown = model.isTrash() && model.searchText().empty();
    m_root->scopeBar->setModel(&model, page->folderName());
    m_root->trashBar->setModel(&model);
    const double height = headerHeight();
    for (const Tab& tab : m_tabs)
        tab.page->setHeaderHeight(height);
    m_root->requestLayout();
}

void FolderWindow::updateBackground()
{
    const ui::Theme& theme = ui::Theme::current();
    const bool active = m_window->isActive();
    m_window->setBackgroundColor(ui::color(active ? ui::ColorId::TranslucentWindowBackground
                                                  : ui::ColorId::TranslucentWindowBackgroundInactive));
    const ui::SizeF size = m_window->size();
    m_window->setBackgroundBlur({0, 0, size.width, size.height}, theme.windowRadius);
    // Views colour their text by it as they lay out.
    m_window->requestLayout();
}

void FolderWindow::showContextMenu(ui::PointF position)
{
    FolderPage* page = currentPage();
    if (!page)
        return;
    m_menu = createFolderMenu(page->model());
    m_menu->popup(m_window.get(), position);
}

void FolderWindow::showOptionsMenu()
{
    FolderPage* page = currentPage();
    if (!page)
        return;
    Preferences& preferences = Preferences::instance();
    const bool trash = page->model().isTrash();
    const int sortMode = trash ? preferences.trashSortMode() : preferences.sortMode();
    const auto sortBy = [trash](int mode) {
        Preferences& preferences = Preferences::instance();
        if (trash)
            preferences.setTrashSort(mode, preferences.trashSortDesc());
        else
            preferences.setSort(mode, preferences.sortDesc());
    };

    m_menu = std::make_unique<ui::Menu>();
    const size_t icons = m_menu->addItem("Icons", [] { Preferences::instance().setViewMethod(Preferences::Grid); });
    m_menu->setItemGlyph(icons, "grid");
    m_menu->setItemChecked(icons, preferences.viewMethod() == Preferences::Grid);
    const size_t list = m_menu->addItem("List", [] { Preferences::instance().setViewMethod(Preferences::List); });
    m_menu->setItemGlyph(list, "list");
    m_menu->setItemChecked(list, preferences.viewMethod() == Preferences::List);
    m_menu->addSeparator();
    const auto sortItem = [&](const std::string& text, int mode) {
        const size_t index = m_menu->addItem(text, [sortBy, mode] { sortBy(mode); });
        m_menu->setItemChecked(index, sortMode == mode);
    };
    sortItem("Name", Preferences::Name);
    if (trash)
        sortItem("Original Location", Preferences::OriginalPath);
    sortItem(trash ? "Date Deleted" : "Date", trash ? Preferences::DeletionTime : Preferences::ModifiedTime);
    if (!trash)
        sortItem("Type", Preferences::Type);
    sortItem("Size", Preferences::Size);

    m_options->setChecked(true);
    m_menu->onClosed = [this] { m_options->setChecked(false); };
    // Under the button's round plate, as popupFor(_optionsButton.background):
    // RoundImageButton's plate, inset a tenth and kept to the icon's pixel parity.
    const double ratio = m_window->scale() > 0 ? m_window->scale() : 1;
    const double iconSize = 24;
    double pixels = std::round((kButtonSize - std::round(kButtonSize * 0.1) * 2) * ratio);
    if (int(pixels - std::round(iconSize * ratio)) % 2 != 0)
        pixels -= 1;
    const double plate = pixels / ratio;
    const double iconOrigin = std::round((kButtonSize - iconSize) / 2);
    const double plateOrigin = iconOrigin + (iconSize - plate) / 2;
    m_menu->popupFor(m_options, kSmallSpacing / 2, ui::RectF {plateOrigin, plateOrigin, plate, plate});
}

void FolderWindow::openFolders(const std::vector<std::string>& paths)
{
    m_host->openFolders(paths);
}

void FolderWindow::showProperties(const FileItemList& items)
{
    m_host->showProperties(items, m_window.get());
}

void FolderWindow::showOpenWith(const std::string& url)
{
    m_host->showOpenWith(url, m_window.get());
}

void FolderWindow::confirmDelete(const std::vector<std::string>& urls)
{
    m_host->confirmDelete(urls, m_window.get());
}

void FolderWindow::confirmEmptyTrash()
{
    m_host->confirmEmptyTrash(m_window.get());
}

void FolderWindow::confirmLaunch(const std::string& path)
{
    m_host->confirmLaunch(path, m_window.get());
}

void FolderWindow::notify(const std::string& message)
{
    m_toast->show(message, std::chrono::milliseconds(3000));
}

} // namespace fm
