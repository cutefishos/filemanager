// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/common.h"

#include "base/timer.h"
#include "ui/view.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cutefish::ui {
class HeaderButton;
class Menu;
}

namespace fm {

class FrameTicker;

// The tabs as pills on a track, at least 140 wide and
// scrolled when crowded; dragged to reorder, dragged off to detach, closed
// with their button or a middle click.
class FolderTabBar : public ui::View
{
public:
    static constexpr double kHeight = 38;

    FolderTabBar();
    ~FolderTabBar() override;

    // The tabs by an id that stays with a tab while others come and go.
    void setTabs(const std::vector<std::pair<int, std::string>>& tabs);
    void setCurrentIndex(int index);
    int currentIndex() const { return m_currentIndex; }
    // A tab lit while a file drag hovers it, before it springs open.
    void setSpringLoadIndex(int index);
    // The tab under a point in the bar's coordinates, or -1.
    int indexAtPosition(ui::PointF point) const;

    std::function<void(int index)> onSelected;
    std::function<void(int index)> onCloseRequested;
    std::function<void()> onNewRequested;
    std::function<void(int index)> onDuplicateRequested;
    std::function<void(int from, int to)> onMoveRequested;
    // position is the pointer, in the window.
    std::function<void(int index, ui::PointF position)> onDetachRequested;

    bool acceptsPointer() const override { return true; }

protected:
    void layout() override;
    void paint(ui::Painter& painter) override;
    void pointerPress(const ui::PointerEvent& event) override;
    void pointerMove(const ui::PointerEvent& event) override;
    void pointerRelease(const ui::PointerEvent& event) override;
    void pointerLeave() override;
    void hoverMove(const ui::PointerEvent& event) override;
    bool scroll(const ui::ScrollEvent& event) override;

private:
    class Tab;
    struct State;

    ui::RectF listRect() const;
    double tabWidth() const;
    double contentWidth() const;
    int slotFor(int index) const;
    void updateDrag(double x);
    void ensureVisible(int index);
    void setContentX(double x);
    void startTicking();
    void tick(uint64_t now);
    bool sliding() const;

    std::vector<std::unique_ptr<State>> m_tabs;
    int m_currentIndex = 0;
    int m_springLoadIndex = -1;
    int m_dragIndex = -1;
    int m_dropIndex = -1;
    int m_pressedIndex = -1;
    int m_hoverIndex = -1;
    bool m_closeHovered = false;
    bool m_closePressed = false;
    bool m_didDrag = false;
    bool m_detached = false;
    ui::MouseButton m_pressedButton = ui::MouseButton::Left;
    ui::PointF m_press;
    double m_pointerX = 0;
    double m_dragPos = 0;
    double m_dragOffset = 0;
    double m_dragLag = 0;
    double m_dragLagFrom = 0;
    uint64_t m_dragLagStart = 0;
    double m_contentX = 0;
    double m_slotWidth = 0;
    double m_slotWidthFrom = 0;
    double m_slotWidthTarget = 0;
    uint64_t m_slotWidthStart = 0;
    uint64_t m_settleUntil = 0;
    uint64_t m_lastTick = 0;
    ui::HeaderButton* m_add;
    std::unique_ptr<FrameTicker> m_ticker;
    base::Timer m_autoScroll;
    std::unique_ptr<ui::Menu> m_menu;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

} // namespace fm
