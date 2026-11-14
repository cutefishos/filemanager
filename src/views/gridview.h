// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "views/icongridview.h"

namespace fm {

// The columns of the browser's icon grid across a width: cells of a fixed
// width, as many as fit with 10 px beside each, and the width left over
// shared evenly among the gaps between them and at both ends.
struct GridColumns
{
    static GridColumns forWidth(double width, double iconSize);

    // The left edge of a column's cell, unrounded.
    double x(int column) const { return gap + column * (cellWidth + gap); }

    int count = 1;
    double cellWidth = 104;
    double gap = 0;
};

// The browser's icon grid. The columns follow the width as GridColumns
// shares it, the rows the icon size and two lines of name; items are made
// for the rows in sight only.
class FolderGridView : public IconGridView
{
public:
    explicit FolderGridView(FolderModel& model);
    ~FolderGridView() override;

    void setIconSize(double size);
    void increaseIconSize();
    void decreaseIconSize();

    int columns() const { return m_grid.count; }
    double cellWidth() const { return m_grid.cellWidth; }
    double cellHeight() const { return m_cellHeight; }
    // The top margin under the bars.
    double topMargin() const { return topInset() + 6; }
    double bottomMargin() const { return 6 + bottomInset(); }

    // The cell sits on whole device pixels.
    ui::RectF cellRect(int index) const override;
    void revealIndex(int index) override;

protected:
    std::unique_ptr<IconItem> makeItem() override;
    int indexAt(ui::PointF content) const override;
    void rectangleSelect(const ui::RectF& band) override;
    void updateItems() override;
    bool navigate(const ui::KeyEvent& event) override;
    double minimumContentHeight() const override;
    void layout() override;
    bool scroll(const ui::ScrollEvent& event) override;

private:
    void updateMetrics();
    // A new icon size moves every row; the one at the top of the view stays
    // there instead of whatever lands under it. A new column count alone
    // keeps the scroll offset.
    void reflow(int fromColumns, double fromRowHeight);
    // The column under a content x, or -1 in a gap or past the last column.
    int columnAt(double x) const;

    GridColumns m_grid;
    double m_cellHeight = 0;
    base::Subscription m_preferences;
};

} // namespace fm
