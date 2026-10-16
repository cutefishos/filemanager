// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "views/folderview.h"

#include <map>
#include <vector>

namespace fm {

class GridItem;

// The browser's icon grid. The columns follow the width
// (at least icon + 48 wide each, sharing the width evenly in 1/64 px), the
// rows the icon size and two lines of name; items are made for the rows in
// sight only.
class FolderGridView : public FolderView
{
public:
    explicit FolderGridView(FolderModel& model);
    ~FolderGridView() override;

    double iconSize() const { return m_iconSize; }
    void setIconSize(double size);
    void increaseIconSize();
    void decreaseIconSize();

    int columns() const { return m_columns; }
    double cellWidth() const { return m_cellWidth; }
    double cellHeight() const { return m_cellHeight; }
    double minimumCellWidth() const { return m_iconSize + 48; }
    // The left and right margins inside the view, and the top one under the bars.
    double leftMargin() const { return 6; }
    double rightMargin() const { return 12; }
    double topMargin() const { return topInset() + 6; }
    double bottomMargin() const { return 6 + bottomInset(); }

    // An item's cell in content coordinates, its x on the device grid.
    ui::RectF cellRect(int index) const;
    void revealIndex(int index) override;

    // For the items.
    bool isDropTarget(int index) const { return dropTargetIndex() == index; }
    bool dropTargetAllowedFor(int index) const { return isDropTarget(index) && dropTargetAllowed(); }
    int renamingIndex() const { return isRenaming() ? currentIndex() : -1; }

protected:
    int indexAt(ui::PointF content) const override;
    bool hitsItem(int index, ui::PointF content) const override;
    int dropRowAt(ui::PointF content) const override;
    void rectangleSelect(const ui::RectF& band) override;
    std::optional<ui::RectF> labelRect(int index) const override;
    bool centeredRename() const override { return true; }
    void updateItems() override;
    bool navigate(const ui::KeyEvent& event) override;
    double minimumContentHeight() const override;
    std::optional<ui::RectF> dragRect(int index) const override;
    void forEachItemView(const std::function<void(int index, ui::View* item)>& visit) const override;
    void paintDragItem(int index, ui::Painter& painter, const ui::RectF& rect) override;
    void layout() override;
    bool scroll(const ui::ScrollEvent& event) override;

private:
    void updateMetrics();
    // A new column count or icon size moves every row; the one at the top
    // of the view stays there instead of whatever lands under it.
    void reflow(int fromColumns, double fromRowHeight);
    GridItem* itemFor(int index) const;

    double m_iconSize = 64;
    int m_columns = 1;
    double m_cellWidth = 112;
    double m_cellHeight = 0;
    std::map<int, GridItem*> m_items;
    std::vector<GridItem*> m_spare;
    base::Subscription m_preferences;
};

} // namespace fm
