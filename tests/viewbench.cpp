// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

// The folder views' costs on the UI thread, frame by frame:
//   viewbench DIR grid|list|desktop [verify] [SCENARIO...]
// Each scenario prints its frames, the UI thread's time and a hash of every
// frame presented, which two builds that draw the same share. With verify,
// every frame is also compared with the window laid out and painted whole,
// which tells a view that kept something stale; the times then mean nothing.
// The churn scenario makes and removes files of its own in DIR.
#include "testutil.h"

#include "core/config.h"
#include "core/preferences.h"
#include "desktop/desktopview.h"
#include "model/foldermodel.h"
#include "views/folderview.h"
#include "views/folderwindow.h"
#include "views/gridview.h"
#include "views/listview.h"

#include "base/url.h"
#include "ui/application.h"
#include "ui/drag.h"
#include "ui/keys.h"
#include "ui/test_input.h"
#include "ui/theme.h"
#include "ui/window.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <vector>

using namespace cutefish;

class Host : public fm::FileManagerHost
{
public:
    fm::FolderWindow* openFolderWindow(const fm::FolderPage::Start&, ui::Window*) override { return nullptr; }
    void openFolders(const std::vector<std::string>&) override {}
    void showProperties(const fm::FileItemList&, ui::Window*) override {}
    void showOpenWith(const std::string&, ui::Window*) override {}
    void confirmDelete(const std::vector<std::string>&, ui::Window*) override {}
    void confirmEmptyTrash(ui::Window*) override {}
    void confirmLaunch(const std::string&, ui::Window*) override {}
    void windowClosed(fm::FolderWindow*) override {}
};

static double threadMs()
{
    timespec ts;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

class Bench
{
public:
    Bench(ui::Application& app, ui::Window& window, std::function<fm::FolderView*()> view, std::string folder,
          bool desktop, bool verify)
        : m_app(app)
        , m_window(window)
        , m_view(std::move(view))
        , m_folder(std::move(folder))
        , m_desktop(desktop)
        , m_verify(verify)
    {
    }

    int mismatches() const { return m_mismatches; }

    fm::FolderView* view() const { return m_view(); }
    fm::FolderModel& model() const { return view()->model(); }
    ui::PointF inWindow(ui::PointF inView) const { return view()->mapToWindow(inView); }

    void begin(const char* name)
    {
        m_name = name;
        m_frames = 0;
        m_total = 0;
        m_worst = 0;
        m_peakSelection = 0;
        m_hash = 1469598103934665603ull;
    }

    // One frame: the action, whatever the loop has for the UI thread, and the frame.
    template <typename Action>
    void frame(Action&& action)
    {
        const double start = threadMs();
        action();
        m_app.loop().dispatch();
        m_clock += 16666667;
        const ui::Image image = ui::TestInput::presentFrame(m_window, m_clock);
        const double spent = threadMs() - start;
        m_total += spent;
        m_worst = std::max(m_worst, spent);
        ++m_frames;
        m_peakSelection = std::max(m_peakSelection, model().selectionCount());
        if (!image.isNull()) {
            const uint32_t* pixels = image.bits();
            const size_t count = size_t(image.width()) * image.height();
            for (size_t i = 0; i < count; ++i)
                m_hash = (m_hash ^ pixels[i]) * 1099511628211ull;
        }
        if (m_verify)
            verify(image);
    }

    // The frame as presented against the same frame laid out and painted whole.
    void verify(const ui::Image& presented)
    {
        m_window.requestLayout();
        m_window.requestPaint();
        const ui::Image whole = ui::TestInput::presentFrame(m_window, m_clock);
        if (presented.isNull() || whole.isNull() || presented.width() != whole.width()
            || presented.height() != whole.height())
            return;
        int differing = 0;
        int left = whole.width();
        int top = whole.height();
        int right = 0;
        int bottom = 0;
        for (int y = 0; y < whole.height(); ++y) {
            const uint32_t* a = presented.bits() + size_t(y) * whole.width();
            const uint32_t* b = whole.bits() + size_t(y) * whole.width();
            for (int x = 0; x < whole.width(); ++x) {
                if (a[x] == b[x])
                    continue;
                ++differing;
                left = std::min(left, x);
                top = std::min(top, y);
                right = std::max(right, x + 1);
                bottom = std::max(bottom, y + 1);
            }
        }
        if (!differing)
            return;
        ++m_mismatches;
        std::printf("%-12s frame %4d  differs in %d pixels within %d,%d %dx%d\n", m_name, m_frames, differing, left,
                    top, right - left, bottom - top);
        if (m_mismatches <= 6) {
            const std::string stem = "viewbench-" + std::string(m_name) + "-" + std::to_string(m_frames);
            presented.savePng(stem + "-presented.png");
            whole.savePng(stem + "-whole.png");
        }
    }

    void end()
    {
        std::printf("%-12s frames %4d  gui %8.1f ms  per frame %6.2f ms  worst %6.2f ms  selected %5d  hash %016llx\n",
                    m_name, m_frames, m_total, m_frames ? m_total / m_frames : 0, m_worst, m_peakSelection,
                    static_cast<unsigned long long>(m_hash));
        std::fflush(stdout);
    }

    // Frames with nothing asked of them, until what the workers owe has arrived.
    void settle(int frames = 30)
    {
        for (int i = 0; i < frames; ++i) {
            fmtest::runFor(m_app.loop(), 15);
            m_clock += 16666667;
            ui::TestInput::presentFrame(m_window, m_clock);
        }
    }

    void scroll()
    {
        begin("scroll");
        const double maximum = std::min(view()->maximumContentY(), 6000.0);
        for (double y = 0; y < maximum; y += 37)
            frame([&] { view()->setContentY(y); });
        for (double y = maximum; y > 0; y -= 151)
            frame([&] { view()->setContentY(y); });
        frame([&] { view()->setContentY(0); });
        end();
    }

    void wheel()
    {
        begin("wheel");
        const ui::PointF at = inWindow({view()->geometry().width / 2, view()->geometry().height / 2});
        for (int notch = 0; notch < 40; ++notch) {
            frame([&] {
                ui::ScrollEvent event;
                event.deltaY = 15;
                event.value120Y = 120;
                ui::TestInput::scroll(m_window, at, event);
            });
            for (int i = 0; i < 5; ++i)
                frame([] {});
        }
        for (int i = 0; i < 30; ++i)
            frame([] {});
        frame([&] { view()->setContentY(0); });
        end();
    }

    void band()
    {
        begin("band");
        const double width = view()->geometry().width;
        const double height = view()->geometry().height;
        // From a corner without items, as a press on one drags it: the
        // desktop fills from the top right.
        const ui::PointF start = m_desktop ? ui::PointF {8, height - 8} : ui::PointF {8, view()->headerHeight() + 2};
        const ui::PointF finish = m_desktop ? ui::PointF {width - 12, 40} : ui::PointF {width - 12, height - 8};
        frame([&] { ui::TestInput::press(m_window, inWindow(start)); });
        const int steps = 120;
        for (int i = 1; i <= steps; ++i) {
            const ui::PointF at {start.x + (finish.x - start.x) * i / steps, start.y + (finish.y - start.y) * i / steps};
            frame([&] { ui::TestInput::movePointer(m_window, inWindow(at)); });
        }
        // Held at the edge, where a folder's view scrolls under the band.
        for (int i = 0; i < 90; ++i)
            frame([] {});
        frame([&] { ui::TestInput::release(m_window, inWindow(finish)); });
        for (int i = 0; i < 12; ++i)
            frame([] {});
        frame([&] {
            model().clearSelection();
            view()->setContentY(0);
        });
        end();
    }

    void selectAll()
    {
        begin("selectall");
        for (int i = 0; i < 20; ++i) {
            frame([&] { model().selectAll(); });
            frame([&] { model().clearSelection(); });
        }
        end();
    }

    void keys()
    {
        begin("keys");
        frame([&] { view()->sendKey(ui::KeyEvent {.key = ui::key::Down}); });
        for (int i = 0; i < 150; ++i)
            frame([&] { view()->sendKey(ui::KeyEvent {.key = ui::key::Down}); });
        for (int i = 0; i < 60; ++i)
            frame([&] { view()->sendKey(ui::KeyEvent {.key = ui::key::Down, .modifiers = ui::ShiftModifier}); });
        for (int i = 0; i < 100; ++i)
            frame([&] { view()->sendKey(ui::KeyEvent {.key = ui::key::Up}); });
        frame([&] {
            model().clearSelection();
            view()->setCurrentIndex(-1);
            view()->setContentY(0);
        });
        end();
    }

    void click()
    {
        begin("click");
        const double width = view()->geometry().width;
        const double height = view()->geometry().height;
        for (int i = 0; i < 60; ++i) {
            const ui::PointF at {40 + std::fmod(i * 97.0, width - 80),
                                 view()->headerHeight() + 20 + std::fmod(i * 61.0, height - view()->headerHeight() - 40)};
            frame([&] { ui::TestInput::press(m_window, inWindow(at)); });
            frame([&] { ui::TestInput::release(m_window, inWindow(at)); });
        }
        frame([&] {
            ui::TestInput::leavePointer(m_window);
            model().clearSelection();
            view()->setCurrentIndex(-1);
        });
        end();
    }

    void resize()
    {
        begin("resize");
        const ui::SizeF size = m_window.size();
        for (double width = size.width; width < size.width + 300; width += 7)
            frame([&] { ui::TestInput::resizeInteractively(m_window, {width, size.height}); });
        for (double width = size.width + 300; width > size.width - 200; width -= 11)
            frame([&] { ui::TestInput::resizeInteractively(m_window, {width, size.height}); });
        frame([&] { ui::TestInput::resizeInteractively(m_window, size); });
        end();
    }

    void idle()
    {
        begin("idle");
        for (int i = 0; i < 120; ++i)
            frame([] {});
        end();
    }

    void zoom()
    {
        if (auto* desktop = dynamic_cast<fm::DesktopGridView*>(view())) {
            begin("zoom");
            const double size = desktop->iconSize();
            frame([&] { model().setSelected(1); });
            for (const double step : {16.0, 32.0, 64.0, -16.0, 0.0})
                frame([&] { desktop->setIconSize(size + step); });
            frame([&] { model().clearSelection(); });
            end();
            return;
        }
        auto* grid = dynamic_cast<fm::FolderGridView*>(view());
        if (!grid)
            return;
        begin("zoom");
        frame([&] { model().setSelected(1); });
        for (int i = 0; i < 6; ++i)
            frame([&] { grid->increaseIconSize(); });
        for (int i = 0; i < 12; ++i)
            frame([&] { grid->decreaseIconSize(); });
        frame([&] {
            grid->setIconSize(64);
            model().clearSelection();
        });
        end();
    }

    void columns()
    {
        auto* list = dynamic_cast<fm::FolderListView*>(view());
        if (!list)
            return;
        begin("columns");
        const fm::ListColumns before = list->columns();
        frame([&] { model().setSelected(1); });
        for (double width = before.dateWidth; width < before.dateWidth + 120; width += 9)
            frame([&] { list->setColumnWidth(1, width, false); });
        for (double width = before.sizeWidth; width > 64; width -= 5)
            frame([&] { list->setColumnWidth(2, width, false); });
        for (double width = before.typeWidth; width < before.typeWidth + 400; width += 31)
            frame([&] { list->setColumnWidth(3, width, false); });
        frame([&] {
            list->setColumnWidth(1, before.dateWidth, false);
            list->setColumnWidth(2, before.sizeWidth, false);
            list->setColumnWidth(3, before.typeWidth, false);
            model().clearSelection();
        });
        end();
    }

    void theme()
    {
        begin("theme");
        frame([&] {
            model().setSelected(1);
            model().setSelected(2);
        });
        frame([] { ui::Theme::setCurrent(ui::Theme::dark()); });
        for (int i = 0; i < 3; ++i)
            frame([] {});
        frame([&] { model().setSelected(4); });
        frame([] { ui::Theme::setCurrent(ui::Theme::light()); });
        for (int i = 0; i < 3; ++i)
            frame([] {});
        frame([&] { model().clearSelection(); });
        end();
    }

    void sort()
    {
        begin("sort");
        frame([&] { model().setSelected(3); });
        const auto wait = [&] {
            for (int i = 0; i < 8; ++i)
                frame([&] { fmtest::runFor(m_app.loop(), 20); });
        };
        // The way the header and the menu sort: every page follows the preference.
        fm::Preferences& preferences = fm::Preferences::instance();
        frame([&] { preferences.setSort(fm::Preferences::Name, true); });
        wait();
        frame([&] { preferences.setSort(fm::Preferences::Size, true); });
        wait();
        frame([&] { preferences.setSort(fm::Preferences::ModifiedTime, false); });
        wait();
        frame([&] { preferences.setSort(fm::Preferences::Name, false); });
        wait();
        frame([&] {
            model().clearSelection();
            view()->setContentY(0);
        });
        end();
    }

    void rename()
    {
        begin("rename");
        for (const int index : {1, 6}) {
            frame([&] {
                model().clearSelection();
                model().setSelected(index);
                view()->setCurrentIndex(index);
            });
            frame([&] { view()->rename(); });
            for (int i = 0; i < 4; ++i)
                frame([] {});
            frame([&] { view()->cancelRename(); });
            for (int i = 0; i < 3; ++i)
                frame([] {});
        }
        frame([&] {
            model().clearSelection();
            view()->setCurrentIndex(-1);
        });
        end();
    }

    void hidden()
    {
        begin("hidden");
        const auto wait = [&] {
            for (int i = 0; i < 8; ++i)
                frame([&] { fmtest::runFor(m_app.loop(), 20); });
        };
        frame([&] { model().setShowHiddenFiles(true); });
        wait();
        frame([&] { model().selectAll(); });
        frame([&] { model().clearSelection(); });
        frame([&] { model().setShowHiddenFiles(false); });
        wait();
        end();
    }

    // A drag from elsewhere passing over the items, folders among them.
    void drop()
    {
        begin("drop");
        const double width = view()->geometry().width;
        const double height = view()->geometry().height;
        const std::vector<std::string> urls {"file:///nonexistent/viewbench-dragged"};
        for (int pass = 0; pass < 2; ++pass) {
            for (double y = view()->headerHeight() + 6; y < height - 6; y += 9) {
                const double x = 30 + std::fmod(y * 3.7, width - 60);
                frame([&] { view()->updateDropTarget(urls, ui::CopyAction | ui::MoveAction, {x, y}); });
            }
        }
        frame([&] { view()->clearDropTarget(); });
        frame([&] { view()->setContentY(0); });
        end();
    }

    // Files coming, changing and going while the folder shows.
    void churn()
    {
        begin("churn");
        const auto wait = [&] {
            for (int i = 0; i < 10; ++i)
                frame([&] { fmtest::runFor(m_app.loop(), 20); });
        };
        const auto name = [&](int i, const char* suffix) {
            return m_folder + "/viewbench-churn-" + std::to_string(i) + suffix;
        };
        std::error_code error;
        frame([&] { model().setSelected(2); });
        for (int i = 0; i < 4; ++i) {
            std::ofstream(name(i, ".txt")) << "one\n";
            std::filesystem::create_directory(name(i, ""), error);
        }
        wait();
        for (int i = 0; i < 4; ++i)
            std::ofstream(name(i, ".txt")) << std::string(5000 * (i + 1), 'x');
        wait();
        frame([&] { model().selectAll(); });
        for (int i = 0; i < 4; ++i) {
            std::filesystem::rename(name(i, ".txt"), name(i, ".md"), error);
            std::filesystem::create_symlink(name(i, ".md"), name(i, ".link"), error);
        }
        wait();
        frame([&] { model().clearSelection(); });
        for (int i = 0; i < 4; ++i) {
            std::filesystem::remove(name(i, ".md"), error);
            std::filesystem::remove(name(i, ".link"), error);
            std::filesystem::remove(name(i, ""), error);
        }
        wait();
        end();
    }

private:
    ui::Application& m_app;
    ui::Window& m_window;
    std::function<fm::FolderView*()> m_view;
    std::string m_folder;
    bool m_desktop = false;
    bool m_verify = false;
    int m_mismatches = 0;
    const char* m_name = "";
    int m_frames = 0;
    double m_total = 0;
    double m_worst = 0;
    int m_peakSelection = 0;
    uint64_t m_hash = 0;
    uint64_t m_clock = 1000000000000ull;
};

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s DIR grid|list|desktop [verify] [SCENARIO...]\n", argv[0]);
        return 1;
    }
    const std::string folder = argv[1];
    const bool list = std::strcmp(argv[2], "list") == 0;
    const bool desktop = std::strcmp(argv[2], "desktop") == 0;
    bool verify = false;
    std::vector<std::string> scenarios;
    for (int i = 3; i < argc; ++i) {
        if (std::strcmp(argv[i], "verify") == 0)
            verify = true;
        else
            scenarios.push_back(argv[i]);
    }
    if (scenarios.empty()) {
        scenarios = desktop ? std::vector<std::string> {"idle", "band", "selectall", "keys", "click"}
                            : std::vector<std::string> {"idle",      "scroll", "wheel", "band",
                                                        "selectall", "keys",   "click", "resize"};
    }

    // Neither the preferences nor the desktop's layout reach the user's settings.
    fmtest::TemporaryDir config;
    fm::setSettingsFile(config.filePath("filemanager.conf"));
    ui::Application app(argc, argv, ui::Application::Platform::Headless);
    ui::Theme::setCurrent(ui::Theme::light());
    fm::Preferences::instance().setViewMethod(list ? fm::Preferences::List : fm::Preferences::Grid);
    fm::Preferences::instance().setSort(fm::Preferences::Name, false);

    Host host;
    std::unique_ptr<fm::FolderWindow> folderWindow;
    std::unique_ptr<fm::FolderModel> desktopModel;
    std::unique_ptr<ui::Window> desktopWindow;
    ui::Window* window = nullptr;
    std::function<fm::FolderView*()> view;
    if (desktop) {
        desktopModel = std::make_unique<fm::FolderModel>(nullptr, true);
        desktopModel->setSortMode(-1);
        desktopModel->setUrl(base::url::fromLocalFile(folder));
        desktopWindow = std::make_unique<ui::Window>();
        window = desktopWindow.get();
        window->setBackgroundColor(ui::Color::rgb(0x4a6a8a));
        auto content = std::make_unique<fm::DesktopGridView>(*desktopModel);
        fm::DesktopGridView* raw = content.get();
        window->setContentView(std::move(content));
        raw->setWorkArea({0, 32, 1000, 648});
        view = [raw] { return raw; };
    } else {
        folderWindow = std::make_unique<fm::FolderWindow>(&host, fm::FolderPage::Start {folder, {}, {}, {}, {}});
        window = &folderWindow->window();
        view = [&folderWindow] { return folderWindow->currentPage()->view(); };
    }
    window->setScale(1.5);
    window->resize({1000, 680});
    window->show();

    const double openStart = threadMs();
    if (!fmtest::runUntil(app.loop(),
                          [&] {
                              if (desktop)
                                  return desktopModel->status() == fm::FolderModel::Status::Ready;
                              fm::FolderPage* page = folderWindow->currentPage();
                              return page && page->model().status() == fm::FolderModel::Status::Ready;
                          },
                          120000)) {
        std::fprintf(stderr, "the folder never listed\n");
        return 1;
    }
    ui::TestInput::setKeyboardFocus(*window, true);
    Bench bench(app, *window, view, folder, desktop, verify);
    bench.begin("open");
    bench.frame([] {});
    bench.end();
    std::printf("%-12s gui %8.1f ms  rows %d\n", "listed", threadMs() - openStart, bench.model().count());
    bench.settle();

    for (const std::string& scenario : scenarios) {
        if (scenario == "idle")
            bench.idle();
        else if (scenario == "scroll")
            bench.scroll();
        else if (scenario == "wheel")
            bench.wheel();
        else if (scenario == "band")
            bench.band();
        else if (scenario == "selectall")
            bench.selectAll();
        else if (scenario == "keys")
            bench.keys();
        else if (scenario == "click")
            bench.click();
        else if (scenario == "resize")
            bench.resize();
        else if (scenario == "zoom")
            bench.zoom();
        else if (scenario == "columns")
            bench.columns();
        else if (scenario == "theme")
            bench.theme();
        else if (scenario == "sort")
            bench.sort();
        else if (scenario == "rename")
            bench.rename();
        else if (scenario == "drop")
            bench.drop();
        else if (scenario == "hidden")
            bench.hidden();
        else if (scenario == "churn")
            bench.churn();
        else
            std::fprintf(stderr, "unknown scenario %s\n", scenario.c_str());
        bench.settle(10);
    }
    if (verify)
        std::printf("%d frames differ from the window painted whole\n", bench.mismatches());
    return bench.mismatches() ? 2 : 0;
}
