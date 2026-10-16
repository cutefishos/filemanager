// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "views/tabbar.h"
#include "views/folderview.h"
#include "views/style.h"

#include "base/event_loop.h"
#include "ui/header_button.h"
#include "ui/icon.h"
#include "ui/label.h"
#include "ui/menu.h"
#include "ui/painter.h"
#include "ui/cell_layout.h"
#include "ui/theme.h"
#include "ui/window.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace fm {

using ui::rounding::centeredStart;

static constexpr double kDetachDistance = 16;
static constexpr uint64_t kGlideNs = 150'000'000;
static constexpr uint64_t kLagNs = 120'000'000;

static uint64_t monotonicNow()
{
    return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now().time_since_epoch())
                            .count());
}

static double outCubic(double t)
{
    t = std::clamp(t, 0.0, 1.0) - 1;
    return t * t * t + 1;
}

struct FolderTabBar::State
{
    int id = 0;
    std::string title;
    ui::Label* label = nullptr;
    ui::Icon* close = nullptr;
    int lastSlot = 0;
    // Drawn at slot + shift; the shift eases to zero.
    double shift = 0;
    double shiftFrom = 0;
    uint64_t shiftStart = 0;
    bool landing = false;
};

FolderTabBar::FolderTabBar()
{
    setName("folderTabBar");
    setTracksHover(true);
    setClipsChildren(true);
    m_add = addChild<ui::HeaderButton>("add", 28);
    m_add->setName("newTab");
    m_add->onClicked = [this] {
        if (onNewRequested)
            onNewRequested();
    };

    m_autoScroll.setInterval(std::chrono::milliseconds(30));
    m_autoScroll.onTimeout = [this] {
        const ui::RectF list = listRect();
        const double delta = m_pointerX < list.x + 24 ? -8 : m_pointerX > list.x + list.width - 24 ? 8 : 0;
        if (delta == 0)
            return;
        setContentX(m_contentX + delta);
        updateDrag(m_pointerX);
    };
}

FolderTabBar::~FolderTabBar() = default;

ui::RectF FolderTabBar::listRect() const
{
    return {6, 4, std::max(0.0, geometry().width - 6 - 40), std::max(0.0, geometry().height - 8)};
}

double FolderTabBar::tabWidth() const
{
    return std::max(140.0, listRect().width / std::max<size_t>(1, m_tabs.size()));
}

double FolderTabBar::contentWidth() const
{
    return tabWidth() * double(m_tabs.size());
}

bool FolderTabBar::sliding() const
{
    return m_dragIndex >= 0 || monotonicNow() < m_settleUntil;
}

void FolderTabBar::setTabs(const std::vector<std::pair<int, std::string>>& tabs)
{
    std::vector<std::unique_ptr<State>> next;
    for (const auto& [id, title] : tabs) {
        auto found = std::find_if(m_tabs.begin(), m_tabs.end(),
                                  [id](const std::unique_ptr<State>& state) { return state && state->id == id; });
        std::unique_ptr<State> state;
        if (found != m_tabs.end()) {
            state = std::move(*found);
        } else {
            state = std::make_unique<State>();
            state->id = id;
            state->label = addChild<ui::Label>();
            state->label->setTextAlignment(ui::Label::TextAlignment::Center);
            state->label->setElide(ui::Label::Elide::Middle);
            state->label->setFontSize(12);
            state->close = addChild<ui::Icon>("close", 16);
            state->lastSlot = int(next.size());
        }
        state->title = title;
        state->label->setText(title);
        next.push_back(std::move(state));
    }
    for (auto& gone : m_tabs) {
        if (!gone)
            continue;
        std::shared_ptr<View> label = takeChild(gone->label);
        std::shared_ptr<View> close = takeChild(gone->close);
    }
    m_tabs = std::move(next);
    raiseChild(m_add);
    requestLayout();
    requestPaint();
    base::EventLoop::current()->post([this] { ensureVisible(m_currentIndex); });
}

void FolderTabBar::setCurrentIndex(int index)
{
    if (m_currentIndex == index)
        return;
    m_currentIndex = index;
    if (m_dragIndex < 0)
        ensureVisible(index);
    requestLayout();
    requestPaint();
}

void FolderTabBar::setSpringLoadIndex(int index)
{
    if (m_springLoadIndex == index)
        return;
    m_springLoadIndex = index;
    requestPaint();
}

int FolderTabBar::indexAtPosition(ui::PointF point) const
{
    const ui::RectF list = listRect();
    if (point.x < list.x || point.x >= list.x + list.width || point.y < list.y || point.y >= list.y + list.height)
        return -1;
    return std::clamp(int(std::floor((point.x - list.x + m_contentX) / tabWidth())), 0, int(m_tabs.size()) - 1);
}

// Where a tab sits while one is dragged: the dragged one is taken out and
// put back at the drop index, so the rest slide aside.
int FolderTabBar::slotFor(int index) const
{
    if (m_dragIndex < 0 || index == m_dragIndex)
        return index;
    const int without = index - (index > m_dragIndex ? 1 : 0);
    return without + (without >= m_dropIndex ? 1 : 0);
}

void FolderTabBar::setContentX(double x)
{
    m_contentX = std::clamp(x, 0.0, std::max(0.0, contentWidth() - listRect().width));
    requestLayout();
    requestPaint();
}

void FolderTabBar::ensureVisible(int index)
{
    if (index < 0 || index >= int(m_tabs.size()))
        return;
    const double width = listRect().width;
    const double left = index * tabWidth();
    double x = m_contentX;
    if (left < x)
        x = left;
    else if (left + tabWidth() > x + width)
        x = left + tabWidth() - width;
    setContentX(x);
}

void FolderTabBar::updateDrag(double x)
{
    m_pointerX = x;
    // Whole in view; at an end the others scroll under it.
    const ui::RectF list = listRect();
    const double first = m_contentX;
    m_dragPos = std::max(first, std::min(first + std::max(0.0, list.width - tabWidth()), x - list.x + first - m_dragOffset));
    m_dropIndex = std::clamp(int(std::round(m_dragPos / tabWidth())), 0, int(m_tabs.size()) - 1);
    startTicking();
    requestLayout();
    requestPaint();
}

void FolderTabBar::startTicking()
{
    if (!m_ticker && window()) {
        m_ticker = std::make_unique<FrameTicker>(window());
        m_ticker->onTick = [this](uint64_t now) { tick(now); };
    }
    if (m_ticker)
        m_ticker->start();
}

void FolderTabBar::tick(uint64_t)
{
    requestLayout();
    requestPaint();
    const uint64_t now = monotonicNow();
    bool moving = m_dragIndex >= 0 || now < m_settleUntil || now - m_dragLagStart < kLagNs
            || now - m_slotWidthStart < kGlideNs;
    for (const auto& tab : m_tabs)
        moving |= tab->shift != 0;
    if (!moving && m_ticker)
        m_ticker->stop();
}

void FolderTabBar::layout()
{
    const ui::Theme& theme = ui::Theme::current();
    const ui::RectF list = listRect();
    const uint64_t now = monotonicNow();
    const double height = list.height;

    // What the tabs are drawn at: the tab width, eased while they slide.
    const double target = tabWidth();
    if (target != m_slotWidthTarget) {
        if (sliding() && m_slotWidthTarget > 0) {
            m_slotWidthFrom = m_slotWidth;
            m_slotWidthStart = now;
            startTicking();
        } else {
            m_slotWidthFrom = target;
            m_slotWidthStart = 0;
        }
        m_slotWidthTarget = target;
    }
    m_slotWidth = m_slotWidthStart ? m_slotWidthFrom
                    + (target - m_slotWidthFrom) * outCubic(double(now - m_slotWidthStart) / kGlideNs)
                                   : target;
    if (m_slotWidthStart && now - m_slotWidthStart >= kGlideNs)
        m_slotWidthStart = 0;

    if (m_dragLag != 0) {
        const double t = double(now - m_dragLagStart) / kLagNs;
        m_dragLag = t >= 1 ? 0 : m_dragLagFrom * (1 - outCubic(t));
    }

    for (size_t i = 0; i < m_tabs.size(); ++i) {
        State& tab = *m_tabs[i];
        const int slot = slotFor(int(i));
        if (slot != tab.lastSlot) {
            // Glides from where it was drawn when its slot changes.
            const double drawn = tab.lastSlot + tab.shift;
            if (sliding()) {
                tab.shiftFrom = drawn - slot;
                tab.shiftStart = now;
                startTicking();
            } else {
                tab.shiftFrom = 0;
                tab.shiftStart = 0;
                tab.landing = false;
            }
            tab.lastSlot = slot;
        }
        if (tab.shiftStart) {
            const double t = double(now - tab.shiftStart) / kGlideNs;
            tab.shift = t >= 1 ? 0 : tab.shiftFrom * (1 - outCubic(t));
            if (t >= 1) {
                tab.shiftStart = 0;
                tab.landing = false;
            }
        } else {
            tab.shift = 0;
        }

        const bool dragged = int(i) == m_dragIndex;
        const double x = list.x - m_contentX + (dragged ? m_dragPos + m_dragLag : (slot + tab.shift) * m_slotWidth);
        const bool selected = int(i) == m_currentIndex;

        // A title long enough to reach the close button keeps clear of it
        // whether or not the button shows, so hovering never re-elides it.
        tab.label->setWeight(selected ? 500 : 400);
        tab.label->setColor(theme.text);
        const ui::SizeF hint = tab.label->sizeHint();
        const double margin = hint.width > m_slotWidth - 56 ? 28 : 12;
        tab.label->setGeometry({x + margin, list.y + centeredStart(height, hint.height),
                                std::max(0.0, m_slotWidth - margin * 2), hint.height});

        const bool hovered = int(i) == m_hoverIndex;
        const bool closeShown = m_dragIndex < 0 && (selected || hovered);
        tab.close->setVisible(closeShown);
        tab.close->setOpacity(hovered && m_closeHovered ? 1 : 0.6);
        tab.close->setGeometry({x + m_slotWidth - 8 - 16, list.y + centeredStart(height, 16), 16, 16});
    }

    m_add->setGeometry({geometry().width - 6 - 28, centeredStart(geometry().height, 28), 28, 28});
    m_add->setPlateAlphas({0.05, 0.10}, {0.05, 0.10});
}

void FolderTabBar::paint(ui::Painter& painter)
{
    const ui::Theme& theme = ui::Theme::current();
    const ui::RectF list = listRect();
    painter.drawRoundedRect(list, {list.height / 2, 0, ui::Corners::Circular}, theme.alternateBackground);

    const ui::Color pressed = style::textAlpha(0.10);
    const ui::Color hover = style::textAlpha(0.05);
    // Translucent, so the tabs sliding aside show through the one being dragged.
    const ui::Color selected = theme.darkMode ? ui::Color {255, 255, 255, 33} : ui::Color {255, 255, 255, 204};

    painter.save();
    // Crowded, the strip stays inside the track's pill.
    painter.clipRoundedRect(list, list.height / 2);
    // The dragged tab and those landing over the rest.
    std::vector<size_t> order;
    for (size_t i = 0; i < m_tabs.size(); ++i)
        order.push_back(i);
    std::stable_sort(order.begin(), order.end(), [this](size_t a, size_t b) {
        const auto rank = [this](size_t i) {
            return int(i) == m_dragIndex || m_tabs[i]->landing ? 2 : int(i) == m_currentIndex ? 1 : 0;
        };
        return rank(a) < rank(b);
    });
    for (const size_t i : order) {
        const State& tab = *m_tabs[i];
        const bool dragged = int(i) == m_dragIndex;
        const double x = list.x - m_contentX
                + (dragged ? m_dragPos + m_dragLag : (tab.lastSlot + tab.shift) * m_slotWidth);
        const ui::RectF rect {x + 2, list.y + 2, m_slotWidth - 4, list.height - 4};
        const bool isSelected = int(i) == m_currentIndex || tab.landing;
        ui::Color fill {0, 0, 0, 0};
        if (isSelected)
            fill = selected;
        else if (int(i) == m_hoverIndex || int(i) == m_springLoadIndex)
            fill = hover;
        const double border = isSelected && !theme.darkMode ? 1 : 0;
        if (fill.a || border)
            painter.drawSmoothRectangle(rect, rect.height / 2, fill, border, ui::Color {0, 0, 0, 15});
        // The close button's plate, under its glyph.
        if (int(i) == m_hoverIndex && m_closeHovered && tab.close->isVisible())
            painter.fillRoundedRect(tab.close->geometry(), 8, m_closePressed ? pressed : hover);
    }
    painter.restore();
}

void FolderTabBar::hoverMove(const ui::PointerEvent& event)
{
    const int index = indexAtPosition(event.position);
    bool closeHovered = false;
    if (index >= 0) {
        const ui::RectF close = m_tabs[size_t(index)]->close->geometry();
        closeHovered = close.contains(event.position);
    }
    if (index != m_hoverIndex || closeHovered != m_closeHovered) {
        m_hoverIndex = index;
        m_closeHovered = closeHovered;
        requestLayout();
        requestPaint();
    }
}

void FolderTabBar::pointerLeave()
{
    if (m_hoverIndex >= 0 || m_closeHovered) {
        m_hoverIndex = -1;
        m_closeHovered = false;
        requestLayout();
        requestPaint();
    }
}

void FolderTabBar::pointerPress(const ui::PointerEvent& event)
{
    m_pressedIndex = indexAtPosition(event.position);
    m_pressedButton = event.button;
    m_press = event.position;
    m_didDrag = false;
    m_detached = false;
    if (m_pressedIndex < 0)
        return;
    if (m_closeHovered && event.button == ui::MouseButton::Left) {
        m_closePressed = true;
        requestPaint();
        return;
    }
    if (event.button == ui::MouseButton::Left) {
        const ui::RectF list = listRect();
        m_dragOffset = event.position.x - (list.x - m_contentX + m_pressedIndex * m_slotWidth);
        if (onSelected)
            onSelected(m_pressedIndex);
    }
}

void FolderTabBar::pointerMove(const ui::PointerEvent& event)
{
    hoverMove(event);
    if (m_pressedIndex < 0 || m_pressedButton != ui::MouseButton::Left || m_detached || m_closePressed)
        return;

    const ui::PointF position = event.position;
    bool started = false;
    if (!m_didDrag && (std::abs(position.x - m_press.x) > 8 || std::abs(position.y - m_press.y) > 8)) {
        m_didDrag = true;
        started = true;
        m_settleUntil = monotonicNow() + kGlideNs;
        m_dragIndex = m_pressedIndex;
        m_dropIndex = m_pressedIndex;
        m_autoScroll.start();
    }
    // Upwards the title bar takes tabs back in, so a tab comes off only past
    // the window's top.
    if (m_didDrag && (position.y < -geometry().y - kDetachDistance
                      || position.y > geometry().height + kDetachDistance)) {
        m_didDrag = false;
        m_detached = true;
        m_settleUntil = monotonicNow() + kGlideNs;
        const int index = m_dragIndex;
        m_dragIndex = -1;
        m_dropIndex = -1;
        m_autoScroll.stop();
        requestLayout();
        if (onDetachRequested)
            onDetachRequested(index, mapToWindow(position));
        return;
    }
    if (m_didDrag) {
        updateDrag(position.x);
        // From its slot, rather than jumping the drag threshold at once.
        if (started) {
            m_dragLagFrom = m_dragIndex * tabWidth() - m_dragPos;
            m_dragLag = m_dragLagFrom;
            m_dragLagStart = monotonicNow();
        }
    }
}

void FolderTabBar::pointerRelease(const ui::PointerEvent& event)
{
    const int index = m_pressedIndex;
    m_pressedIndex = -1;
    m_autoScroll.stop();

    if (m_closePressed) {
        m_closePressed = false;
        requestPaint();
        if (index >= 0 && m_tabs[size_t(index)]->close->geometry().contains(event.position) && onCloseRequested)
            onCloseRequested(index);
        return;
    }

    if (m_didDrag) {
        const int from = m_dragIndex;
        const int to = m_dropIndex;
        // The tab glides from under the pointer into its slot.
        m_settleUntil = monotonicNow() + kGlideNs;
        if (from >= 0) {
            State& tab = *m_tabs[size_t(from)];
            tab.landing = true;
            tab.shiftFrom = (m_dragPos + m_dragLag) / m_slotWidth - from;
            tab.shiftStart = monotonicNow();
            tab.lastSlot = from;
        }
        m_dragIndex = -1;
        m_dropIndex = -1;
        m_didDrag = false;
        startTicking();
        if (onMoveRequested && from >= 0 && to >= 0)
            onMoveRequested(from, to);
        return;
    }
    if (m_detached || index < 0 || index != indexAtPosition(event.position))
        return;

    if (event.button == ui::MouseButton::Middle) {
        if (onCloseRequested)
            onCloseRequested(index);
    } else if (event.button == ui::MouseButton::Right) {
        m_menu = std::make_unique<ui::Menu>();
        m_menu->addItem("Open In New Tab", [this, index] {
            if (onDuplicateRequested)
                onDuplicateRequested(index);
        });
        m_menu->addItem("Close Tab", [this, index] {
            if (onCloseRequested)
                onCloseRequested(index);
        });
        m_menu->popup(window(), mapToWindow(event.position));
    }
}

bool FolderTabBar::scroll(const ui::ScrollEvent& event)
{
    const double delta = event.deltaX != 0 ? event.deltaX : event.deltaY;
    if (contentWidth() <= listRect().width)
        return false;
    setContentX(m_contentX + delta);
    return true;
}

} // namespace fm
