// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "model/positioner.h"
#include "views/folderview.h"

#include <map>
#include <memory>
#include <vector>

namespace fm {

class DesktopItem;

// The desktop's folder view: icons on fixed cells of a grid
// anchored to the top right of the work area, in columns filled from the
// top, white names over the wallpaper. The view covers the whole output and
// takes presses outside the grid too.
class DesktopGridView : public FolderView
{
public:
    explicit DesktopGridView(FolderModel& model);
    ~DesktopGridView() override;

    Positioner& positioner() { return m_positioner; }

    double iconSize() const { return m_iconSize; }
    void setIconSize(double size);
    void setMaximumIconSize(double size) { m_maximumIconSize = size; }
    // The work area in the view's coordinates; the grid keeps 12 px inside it.
    void setWorkArea(const ui::RectF& area);
    // The grid as laid out, in the view's coordinates.
    const ui::RectF& gridRect() const { return m_grid; }

    double cellWidth() const { return m_cellWidth; }
    double cellHeight() const { return m_cellHeight; }
    int columns() const { return m_columns; }
    int rows() const { return m_rows; }
    ui::RectF cellRect(int cell) const;

    // Desktop label metrics, shared with the items.
    double labelPixelSize() const;
    double labelHeight() const;
    // A name on at most two lines, the middle of the second elided, so a long
    // name keeps how it starts and its extension.
    std::string labelText(const std::string& name, double width) const;

    void revealIndex(int) override {}

    // For the items.
    bool isDropTarget(int cell) const { return dropTargetIndex() == cell; }
    bool dropTargetAllowedFor(int cell) const { return isDropTarget(cell) && dropTargetAllowed(); }
    int renamingIndex() const { return isRenaming() ? currentIndex() : -1; }

    std::function<void()> onIconSizeChanged;

protected:
    int indexAt(ui::PointF content) const override;
    int rowOf(int index) const override;
    int indexOfRow(int row) const override { return m_positioner.cellOfRow(row); }
    void selectRange(int from, int to) override;
    bool hitsItem(int index, ui::PointF content) const override;
    int dropRowAt(ui::PointF content) const override;
    int cellAt(ui::PointF content) const override;
    void rectangleSelect(const ui::RectF& band) override;
    std::optional<ui::RectF> labelRect(int index) const override;
    bool centeredRename() const override { return true; }
    void updateItems() override;
    bool navigate(const ui::KeyEvent& event) override;
    double minimumContentHeight() const override { return geometry().height; }
    bool autoScrolls() const override { return false; }
    std::optional<ui::RectF> dragRect(int index) const override;
    void forEachItemView(const std::function<void(int index, ui::View* item)>& visit) const override;
    void paintDragItem(int index, ui::Painter& painter, const ui::RectF& rect) override;
    void layout() override;
    bool scroll(const ui::ScrollEvent& event) override;

private:
    void updateMetrics();
    DesktopItem* itemFor(int cell) const;
    // A drop of the files at a point: the icons go to the cell under it,
    // keeping their arrangement when the drag started on one of them.
    void moveToCell(int x, int y, const std::vector<std::string>& urls);
    ui::PointF cellNear(ui::PointF point) const;

    Positioner m_positioner;
    double m_iconSize = 64;
    double m_maximumIconSize = 256;
    ui::RectF m_workArea;
    ui::RectF m_grid;
    double m_cellWidth = 108;
    double m_cellHeight = 0;
    int m_columns = 1;
    int m_rows = 1;
    std::map<int, DesktopItem*> m_items;
    std::vector<DesktopItem*> m_spare;
    std::vector<base::Subscription> m_subscriptions;
};

} // namespace fm
