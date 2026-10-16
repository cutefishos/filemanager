// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/desktopwindow.h"
#include "core/config.h"
#include "core/shortcutconvention.h"
#include "core/standardfolders.h"
#include "desktop/desktopmenuexporter.h"
#include "dialogs/archiveprogressdialog.h"
#include "desktop/desktopview.h"
#include "ops/operations.h"
#include "views/foldermenu.h"
#include "views/folderwindow.h"
#include "views/previewclient.h"
#include "views/style.h"

#include "base/event_loop.h"
#include "base/json.h"
#include "base/paths.h"
#include "base/settings.h"
#include "base/url.h"
#include "ui/application.h"
#include "ui/clipboard.h"
#include "ui/keys.h"
#include "ui/menu.h"
#include "ui/trusted_connection.h"
#include "ui/window.h"

#include <cmath>

namespace fm {

static constexpr const char* kDisplayService = "com.cutefish.DisplayConfig1";
static constexpr const char* kDisplayPath = "/com/cutefish/DisplayConfig1";

static std::string desktopPath()
{
    const std::string path = base::paths::userDirectory(base::paths::UserDirectory::Desktop);
    return path.empty() ? homePath() + "/Desktop" : path;
}

static std::string settingsPath()
{
    return settingsFile();
}

// The window's content: the icons over the whole output, transparent.
class DesktopRoot : public ui::View
{
public:
    explicit DesktopRoot(DesktopWindow* desktop)
        : m_desktop(desktop)
    {
    }

    bool acceptsPointer() const override { return true; }

protected:
    void layout() override
    {
        if (m_desktop->m_view)
            m_desktop->m_view->setGeometry(localRect());
    }

private:
    DesktopWindow* m_desktop;
};

DesktopWindow::DesktopWindow(ui::TrustedConnection* connection, FileManagerHost& host)
    : m_connection(connection)
    , m_host(host)
    , m_model(std::make_unique<FolderModel>(this, true))
{
    FolderModel& model = *m_model;
    // The desktop keeps the order icons came in: sorting is the positioner's.
    model.setSortMode(-1);
    model.setUrl(base::url::fromLocalFile(desktopPath()));

    m_window = connection ? std::make_unique<ui::Window>(*connection) : std::make_unique<ui::Window>();
    m_window->setTitle("Desktop");
    m_window->setBackgroundColor(ui::Color {0, 0, 0, 0});
    auto root = std::make_unique<DesktopRoot>(this);
    m_root = root.get();
    m_window->setContentView(std::move(root));
    auto view = std::make_unique<DesktopGridView>(model);
    m_view = view.get();
    m_root->addChild(std::move(view));

    base::Settings settings(settingsPath());
    m_view->setMaximumIconSize(double(settings.intValue("Desktop/maximumIconSize", 256)));
    m_view->setIconSize(double(settings.intValue("Desktop/iconSize", 64)));
    m_view->onIconSizeChanged = [this] {
        base::Settings settings(settingsPath());
        settings.setValue("Desktop/iconSize", int(std::lround(m_view->iconSize())));
    };
    m_view->onContextMenu = [this](ui::PointF position) { showContextMenu(position); };

    // Clicking the empty desktop leaves the preview on its item.
    m_preview = std::make_unique<PreviewClient>();
    m_preview->setFollowsEmptySelection(false);
    m_preview->attach(&model, m_view);
    m_view->onKeyPressed = [this](const ui::KeyEvent& event) { return handleKey(event); };

    m_saveLayout.setSingleShot(true);
    m_saveLayout.setInterval(std::chrono::milliseconds(500));
    m_saveLayout.onTimeout = [this] { saveLayout(); };
    m_restoreLayout.setSingleShot(true);
    m_restoreLayout.setInterval(std::chrono::milliseconds(150));
    m_restoreLayout.onTimeout = [this] { loadLayout(); };
    m_subscriptions.push_back(m_view->positioner().positionsChanged.connect([this] {
        if (!m_restoring)
            m_saveLayout.start();
    }));
    m_subscriptions.push_back(m_view->positioner().changed.connect([this] {
        if (!m_layoutLoaded)
            m_restoreLayout.start();
    }));

    m_window->onWorkAreaChanged = [this] {
        const ui::RectF area = m_window->workArea();
        m_view->setWorkArea(area);
        if (!m_layoutLoaded)
            m_restoreLayout.start();
    };
    m_window->onActiveChanged = [this] {
        // Not while a name is being edited: taking the keys back closes it.
        if (m_window->isActive() && !m_view->isRenaming())
            m_view->contentView()->setFocus();
        if (m_window->isActive())
            m_preview->activate();
    };
    // The desktop is not closed.
    m_window->onCloseRequested = [] {};

    base::dbus::Connection* bus = base::dbus::Connection::session();
    if (bus) {
        m_menuExporter = std::make_unique<DesktopMenuExporter>(
                bus,
                [this] {
                    FolderModel& model = *m_model;
                    using Entry = DesktopMenuExporter::Entry;
                    const auto action = [&model](const std::string& name) {
                        const FolderModel::Action& entry = model.action(name);
                        Entry item;
                        item.label = entry.text;
                        item.enabled = entry.enabled;
                        item.action = [&model, name] { triggerAction(model, name); };
                        return item;
                    };
                    Entry separator;
                    separator.separator = true;
                    Entry newWindow;
                    newWindow.label = "New Window";
                    // A window opened from the desktop starts at home, not at ~/Desktop.
                    newWindow.action = [&model] { model.openInNewWindow(base::url::fromLocalFile(homePath())); };
                    Entry selectAll;
                    selectAll.label = "Select All";
                    selectAll.action = [&model] { model.selectAll(); };
                    Entry file;
                    file.label = "File";
                    file.children = {newWindow, action("newFolder"), separator, action("properties")};
                    Entry edit;
                    edit.label = "Edit";
                    edit.children = {action("undo"), action("redo"), separator, action("cut"), action("copy"),
                                     action("paste"), selectAll};
                    return std::vector<Entry> {file, edit};
                },
                [this] { m_model->prepareContextMenu(); });
        // The bar shows the rows it holds as a menu opens: they follow the selection, clipboard and history.
        m_menuUpdate.setSingleShot(true);
        m_menuUpdate.setInterval(std::chrono::milliseconds(50));
        m_menuUpdate.onTimeout = [this] { m_menuExporter->update(); };
        const auto update = [this] { m_menuUpdate.start(); };
        m_subscriptions.push_back(m_model->selectionChanged.connect(update));
        m_subscriptions.push_back(m_model->statusChanged.connect(update));
        m_subscriptions.push_back(ui::Clipboard::changed().connect([update](ui::ClipboardMode) { update(); }));
        m_subscriptions.push_back(OperationsClient::instance().historyChanged.connect(update));
    }

    m_archiveDialog = std::make_unique<ArchiveProgressDialog>();
    m_archiveDialog->setModel(m_model.get(), nullptr);

    followPrimaryOutput();
}

DesktopWindow::~DesktopWindow()
{
    *m_alive = false;
}

// The primary output as the display service has it, or the first one.
void DesktopWindow::followPrimaryOutput()
{
    base::dbus::Connection* bus = base::dbus::Connection::session();
    const auto fallback = [this] {
        std::string name;
        if (m_connection) {
            const auto outputs = m_connection->outputs();
            if (!outputs.empty())
                name = outputs.front().name;
        }
        placeOn(name);
    };
    if (!bus) {
        fallback();
        return;
    }
    m_display = base::dbus::Proxy(bus, kDisplayService, kDisplayPath, kDisplayService);
    const auto query = [this, fallback] {
        m_displayState = m_display.callAsync("GetState", [this, fallback](const base::dbus::Reply& reply) {
            if (reply.isError() || reply.arguments().size() < 6) {
                fallback();
                return;
            }
            // logicalDisplays: a(ssasiiiiiiiiuub), the Wayland name second and primary last.
            for (const base::dbus::Variant& display : reply.arguments()[5].items()) {
                const auto& members = display.items();
                if (members.size() >= 14 && members[13].value<bool>()) {
                    placeOn(members[1].value<std::string>());
                    return;
                }
            }
            fallback();
        });
    };
    m_displayChanged = m_display.onSignal("StateChanged", [query](const base::dbus::Message&) { query(); });
    m_displayWatch = bus->watchName(kDisplayService, [query, fallback](const std::string& owner) {
        if (owner.empty())
            fallback();
        else
            query();
    });
    if (m_connection)
        m_connection->onOutputsChanged = [query] { query(); };
}

void DesktopWindow::placeOn(const std::string& outputName)
{
    ui::SizeF size;
    if (m_connection) {
        for (const auto& output : m_connection->outputs()) {
            if (output.name == outputName)
                size = output.logicalSize;
        }
    } else {
        for (ui::Window* window : ui::Application::instance()->windows()) {
            if (window->screenSize().width > 0) {
                size = window->screenSize();
                break;
            }
        }
        if (size.width <= 0)
            size = {1280, 800};
    }
    if (size.width <= 0 || size.height <= 0)
        return;

    const bool moved = outputName != m_output;
    m_output = outputName;
    m_window->resize(size);
    if (m_window->isHidden() || !m_activated) {
        if (m_connection)
            m_window->setDesktopRole(outputName);
        m_window->show();
        if (m_connection)
            m_window->activateDesktop();
        else
            m_window->activate();
        m_view->contentView()->setFocus();
        m_activated = true;
    } else if (moved && m_connection) {
        m_window->setDesktopOutput(outputName);
    }
    if (moved) {
        // Each output keeps its own layout.
        m_layoutLoaded = false;
        m_saveLayout.stop();
        m_restoreLayout.start();
    }
}

std::string DesktopWindow::layoutKey() const
{
    std::string name = m_output;
    for (char& c : name) {
        if (c == '/' || c == '\\')
            c = '_';
    }
    return "DesktopIconLayout/layout.v3." + name;
}

void DesktopWindow::loadLayout()
{
    const ui::RectF grid = m_view->gridRect();
    if (grid.width <= 0 || grid.height <= 0 || m_layoutLoaded)
        return;
    m_restoring = true;
    base::Settings settings(settingsPath());
    const std::string raw = settings.stringValue(layoutKey());
    if (!raw.empty()) {
        if (const auto stored = base::json::Value::parse(raw); stored && stored->isArray() && stored->size() >= 5) {
            std::vector<std::string> positions;
            for (const base::json::Value& value : stored->toArray())
                positions.push_back(value.toString());
            m_view->positioner().setPositions(std::move(positions));
        }
    }
    m_layoutLoaded = true;
    m_restoring = false;
    m_saveLayout.start();
}

void DesktopWindow::saveLayout()
{
    if (!m_layoutLoaded || m_restoring || m_restoreLayout.isActive())
        return;
    const std::vector<std::string>& positions = m_view->positioner().positions();
    // Less than one record: the folder is not listed yet, and saving would
    // wipe the layout of an empty desktop.
    if (positions.size() < 5)
        return;
    base::json::Value::Array array;
    for (const std::string& position : positions)
        array.push_back(position);
    base::Settings settings(settingsPath());
    settings.setValue(layoutKey(), base::json::Value(std::move(array)).serialize());
}

void DesktopWindow::showContextMenu(ui::PointF position)
{
    m_menu = createFolderMenu(*m_model);
    m_menu->popup(m_window.get(), position);
}

// The folder keys without history, path editor, closing or refresh.
bool DesktopWindow::handleKey(const ui::KeyEvent& event)
{
    if (m_preview->filterKey(event))
        return true;

    FolderModel& model = *m_model;
    const uint32_t modifiers = event.modifiers;
    const uint32_t key = event.key;
    const bool control = modifiers & ui::ControlModifier;

    if (key == ui::key::Space && modifiers == ui::NoModifier) {
        if (!event.repeat)
            m_preview->toggle();
        return true;
    }

    if (shortcuts::commandKeys()) {
        // Command reaches us as Ctrl; outside text fields these go to the file view.
        if (modifiers == ui::ControlModifier) {
            switch (key) {
            case ui::key::BackSpace:
                model.keyDeletePress();
                return true;
            case ui::key::Down:
                model.openSelected();
                return true;
            default:
                break;
            }
        }
        // Command+Shift+.; Shift makes the key ">" on most layouts.
        if (modifiers == (ui::ControlModifier | ui::ShiftModifier) && (key == '.' || key == '>')) {
            model.setShowHiddenFiles(!model.showHiddenFiles());
            return true;
        }
    }

    if (key == ui::key::Return || key == ui::key::KeypadEnter) {
        model.openSelected();
    } else if (key == 'c' && control) {
        model.copy();
    } else if (key == 'x' && control) {
        model.cut();
    } else if (key == 'v' && control) {
        model.paste();
    } else if (key == style::key::F2) {
        model.requestRename.emit();
    } else if (key == 'a' && control) {
        model.selectAll();
    } else if (key == ui::key::Delete) {
        model.keyDeletePress();
    } else if (key == 'h' && control) {
        model.setShowHiddenFiles(!model.showHiddenFiles());
    } else if (key == 'z' && control) {
        if (modifiers & ui::ShiftModifier)
            model.redo();
        else
            model.undo();
    } else {
        if (key >= 'a' && key <= 'z' && !control)
            model.keyboardSearch(event.text);
        return false;
    }
    return true;
}

void DesktopWindow::openFolders(const std::vector<std::string>& paths)
{
    m_host.openFolders(paths);
}

void DesktopWindow::showProperties(const FileItemList& items)
{
    m_host.showProperties(items, nullptr);
}

void DesktopWindow::showOpenWith(const std::string& url)
{
    m_host.showOpenWith(url, nullptr);
}

void DesktopWindow::confirmDelete(const std::vector<std::string>& urls)
{
    m_host.confirmDelete(urls, nullptr);
}

void DesktopWindow::confirmEmptyTrash()
{
    m_host.confirmEmptyTrash(nullptr);
}

void DesktopWindow::confirmLaunch(const std::string& path)
{
    m_host.confirmLaunch(path, nullptr);
}

void DesktopWindow::notify(const std::string&)
{
}

} // namespace fm
