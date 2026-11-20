// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "core/dsstore.h"
#include "views/icongridview.h"

#include "ui/image.h"

#include <memory>

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

// A folder as its .DS_Store arranges it, with the picture behind the icons
// decoded.
struct ArrangedFolder
{
    std::string url;
    FolderLayout layout;
    ui::Image picture;
    // Device pixels per point of the picture.
    double pictureScale = 1;

    // What the window shows under its title bar: the picture's size when it
    // was made for the window, else the stored size less a title bar.
    ui::SizeF contentSize() const;

    // The folder's arrangement with its picture decoded, or null when it
    // keeps none; reads the disk, so it runs on the pool.
    static std::shared_ptr<const ArrangedFolder> read(const std::string& url, const std::string& folder);
};

// The browser's icon grid. The columns follow the width as GridColumns
// shares it, the rows the icon size and two lines of name; items are made
// for the rows in sight only. An arranged folder puts each icon where its
// layout places it instead, over its background.
class FolderGridView : public IconGridView
{
public:
    explicit FolderGridView(FolderModel& model);
    ~FolderGridView() override;

    void setIconSize(double size);
    // Null for the plain grid.
    void setArrangement(std::shared_ptr<const ArrangedFolder> arrangement);
    const ArrangedFolder* arrangement() const { return m_arrangement.get(); }
    // The names' text size the arrangement asks for; 0 follows the theme.
    double labelSize() const { return m_arrangement ? m_arrangement->layout.textSize : 0; }
    // Names beside the icons, as an arrangement may ask.
    bool labelsBeside() const { return m_arrangement && !m_arrangement->layout.labelOnBottom; }
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
    void paint(ui::Painter& painter) override;

private:
    void updateMetrics();
    // Each index's cell as the arrangement places it; the items it does not
    // place in rows under the others.
    void arrangeCells();
    int arrangedIndexAt(ui::PointF content) const;
    bool navigateArranged(int dx, int dy, uint32_t modifiers);
    // A new icon size moves every row; the one at the top of the view stays
    // there instead of whatever lands under it. A new column count alone
    // keeps the scroll offset.
    void reflow(int fromColumns, double fromRowHeight);
    // The column under a content x, or -1 in a gap or past the last column.
    int columnAt(double x) const;

    GridColumns m_grid;
    double m_cellHeight = 0;
    std::shared_ptr<const ArrangedFolder> m_arrangement;
    std::vector<ui::RectF> m_arrangedCells;
    base::Subscription m_preferences;
};

} // namespace fm
