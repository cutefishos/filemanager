// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "views/gridview.h"
#include "core/preferences.h"
#include "core/thumbnails.h"
#include "model/foldermodel.h"
#include "views/style.h"

#include "ui/icon.h"
#include "ui/image_view.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/path.h"
#include "ui/cell_layout.h"
#include "ui/theme.h"
#include "ui/window.h"

#include <algorithm>
#include <cmath>

namespace fm {

using ui::rounding::centeredStart;

static constexpr double kSmallSpacing = 6;
static constexpr double kLargeSpacing = 12;
static constexpr double kMinimumIconSize = 22;

// An icon name the theme has, else the fallback, as IconItem's fallback.
static std::string iconOrFallback(const std::string& name, const std::string& fallback, int size)
{
    if (!name.empty() && !ui::themeIconPath(name, size).empty())
        return name;
    return fallback;
}

// The selection's ColorOverlay over a thumbnail: its rounded shape in the colour.
class ThumbnailTint : public ui::View
{
public:
    void setRadius(double radius)
    {
        m_radius = radius;
        requestPaint();
    }

protected:
    void paint(ui::Painter& painter) override
    {
        painter.drawSmoothRectangle(localRect(), m_radius, ui::Theme::current().highlight);
    }

private:
    double m_radius = 0;
};

// One item's cell in the grid.
class GridItem : public ui::View
{
public:
    explicit GridItem(FolderGridView* view)
        : m_view(view)
    {
        m_icon = addChild<ui::ThemeIcon>();
        m_thumbnail = addChild<ui::ImageView>();
        m_thumbnail->setFillMode(ui::ImageView::FillMode::Stretch);
        m_thumbnail->setVisible(false);
        m_emblem = addChild<ui::ThemeIcon>("emblem-symbolic-link", 16);
        m_emblem->setImageProviderLookup(true);
        m_emblem->setVisible(false);
        // Over the emblem as well: the overlay's source is the whole icon item.
        m_overlay = addChild<ui::ThemeIcon>();
        m_overlay->setVisible(false);
        m_thumbnailTint = addChild<ThumbnailTint>();
        m_thumbnailTint->setVisible(false);
        m_label = addChild<ui::Label>();
        m_label->setWrap(true);
        m_label->setTextAlignment(ui::Label::TextAlignment::Center);
        m_label->setElide(ui::Label::Elide::Right);
    }

    int index() const { return m_index; }
    int row() const { return m_row; }

    void bind(int index, int row)
    {
        const bool fresh = index != m_index || row != m_row;
        m_index = index;
        m_row = row;
        FolderModel& model = m_view->model();
        const FileItemPtr item = model.item(row);
        if (!item)
            return;

        const double iconSize = m_view->iconSize();
        m_selected = model.isSelected(row);
        m_hidden = item->isHidden;
        m_dropTarget = m_view->isDropTarget(index);
        m_dropAllowed = m_view->dropTargetAllowedFor(index);

        const std::string icon = iconOrFallback(item->iconName, item->isDir ? "folder" : "unknown", int(iconSize));
        m_icon->setIconName(icon);
        m_icon->setSize(iconSize);
        m_overlay->setIconName(icon);
        m_overlay->setSize(iconSize);
        m_overlay->setColor(ui::Theme::current().highlight);

        const bool thumbnail = model.showsThumbnail(row);
        if (fresh || thumbnail != m_wantsThumbnail || item->modified != m_modified || item->path != m_path) {
            m_wantsThumbnail = thumbnail;
            m_modified = item->modified;
            m_path = item->path;
            m_image = {};
            m_thumbnailBox = {};
        }

        m_emblem->setVisible(item->isLink);
        m_label->setText(model.displayName(row));
        m_label->setMaximumLineCount(m_selected ? 3 : 2);
        m_label->setColor(m_selected ? style::selectedTextColor() : ui::Theme::current().text);
        m_label->setOpacity(m_hidden ? 0.8 : 1.0);
        m_label->setVisible(m_view->renamingIndex() != index);
        requestLayout();
        requestPaint();
    }

    // The icon, or the thumbnail as drawn, in item coordinates.
    ui::RectF iconRect() const
    {
        if (m_thumbnail->isVisible())
            return m_thumbnail->geometry();
        return m_icon->geometry();
    }

    // The name as drawn, narrower than its box when it is short.
    ui::RectF labelRect() const
    {
        const ui::RectF box = m_label->geometry();
        const double width = paintedWidth();
        return {box.x + (box.width - width) / 2, box.y, width, box.height};
    }

    ui::RectF labelBox() const { return m_label->geometry(); }

    bool containsPoint(ui::PointF point) const
    {
        const auto inside = [&point](const ui::RectF& rect) {
            return point.x >= rect.x && point.y >= rect.y && point.x <= rect.x + rect.width
                    && point.y <= rect.y + rect.height;
        };
        return inside(iconRect()) || inside(labelRect());
    }

    bool intersectsRect(const ui::RectF& rect) const
    {
        const auto intersects = [&rect](const ui::RectF& other) {
            return other.x < rect.x + rect.width && rect.x < other.x + other.width && other.y < rect.y + rect.height
                    && rect.y < other.y + other.height;
        };
        return intersects(iconRect()) || intersects(labelRect());
    }

protected:
    void layout() override
    {
        const double width = geometry().width;
        const double iconSize = m_view->iconSize();
        const double layoutWidth = m_view->minimumCellWidth();
        const double iconItemWidth = layoutWidth - kLargeSpacing * 2;
        const double iconItemX = centeredStart(width, iconItemWidth);
        m_iconItem = {iconItemX, kSmallSpacing, iconItemWidth, iconSize};

        const ui::RectF iconRect {iconItemX + centeredStart(iconItemWidth, iconSize), kSmallSpacing, iconSize,
                                  iconSize};
        m_icon->setGeometry(iconRect);
        m_overlay->setGeometry(iconRect);
        m_icon->setOpacity(m_hidden ? 0.5 : 1.0);
        m_overlay->setOpacity(0.5 * (m_hidden ? 0.5 : 1.0));

        layoutThumbnail();
        const bool thumbnailShown = m_thumbnail->isVisible();
        m_icon->setVisible(!thumbnailShown);
        m_overlay->setVisible(m_selected && !thumbnailShown);
        m_thumbnailTint->setVisible(m_selected && thumbnailShown);
        if (thumbnailShown) {
            m_thumbnailTint->setGeometry(m_thumbnail->geometry());
            m_thumbnailTint->setRadius(m_thumbnail->geometry().height * 0.1);
            m_thumbnailTint->setOpacity(0.5 * (m_hidden ? 0.5 : 1.0));
        }

        const ui::RectF anchor = thumbnailShown ? m_thumbnail->geometry() : iconRect;
        m_emblem->setGeometry({anchor.x + anchor.width - 16, anchor.y + anchor.height - 16, 16, 16});
        m_emblem->setOpacity(m_hidden ? 0.5 : 1.0);

        const double labelWidth = layoutWidth - kLargeSpacing * 2 - kSmallSpacing;
        const double labelHeight = m_label->heightForWidth(labelWidth);
        m_label->setGeometry({centeredStart(width, labelWidth), kSmallSpacing + iconSize + kSmallSpacing, labelWidth,
                              labelHeight});
    }

    void paint(ui::Painter& painter) override
    {
        const ui::Theme& theme = ui::Theme::current();
        if (m_dropTarget) {
            const double width = std::max(m_iconItem.width, paintedWidth());
            const double height = m_iconItem.height + m_label->geometry().height + kLargeSpacing;
            const ui::Color color = m_dropAllowed ? theme.highlight : style::kDanger;
            painter.drawRoundedRect({(geometry().width - width) / 2, m_iconItem.y, width, height},
                                    {theme.mediumRadius, 0, ui::Corners::Circular}, style::withAlpha(color, 0.16), 2,
                                    color);
        }

        // The selection behind the name: a rounded box per line, bridged
        // where lines meet.
        if (m_selected && m_label->isVisible()) {
            const double hPadding = kSmallSpacing / 2;
            const double vPadding = 0.5;
            const ui::RectF box = m_label->geometry();
            const ui::PointF origin {box.x - hPadding, box.y - vPadding};
            ui::Path path;
            std::optional<ui::RectF> previous;
            for (size_t i = 0;; ++i) {
                const auto line = m_label->lineGeometry(i);
                if (!line)
                    break;
                const double lineWidth = std::max(0.0, std::min(box.width, line->width));
                const ui::RectF rect {(box.width - lineWidth) / 2, line->top, lineWidth, line->height};
                if (rect.width <= 0 || rect.height <= 0)
                    continue;
                addRoundedRect(path, {origin.x + rect.x, origin.y + rect.y, rect.width + hPadding * 2,
                                      rect.height + vPadding * 2},
                               4);
                if (previous) {
                    const double left = std::max(previous->x, rect.x);
                    const double right = std::min(previous->x + previous->width, rect.x + rect.width) + hPadding * 2;
                    const double boundary = rect.y + vPadding;
                    if (right > left)
                        path.addRect({origin.x + left, origin.y + boundary - 2, right - left, 4});
                }
                previous = rect;
            }
            painter.fillPath(path, theme.highlight);
        }
    }

private:
    static void addRoundedRect(ui::Path& path, const ui::RectF& rect, double radius)
    {
        // Quadratic corners.
        const double r = std::min({radius, rect.width / 2, rect.height / 2});
        const double x = rect.x;
        const double y = rect.y;
        const double w = rect.width;
        const double h = rect.height;
        path.moveTo({x + r, y});
        path.lineTo({x + w - r, y});
        path.quadTo({x + w, y}, {x + w, y + r});
        path.lineTo({x + w, y + h - r});
        path.quadTo({x + w, y + h}, {x + w - r, y + h});
        path.lineTo({x + r, y + h});
        path.quadTo({x, y + h}, {x, y + h - r});
        path.lineTo({x, y + r});
        path.quadTo({x, y}, {x + r, y});
        path.close();
    }

    double paintedWidth() const
    {
        double width = 0;
        for (size_t i = 0;; ++i) {
            const auto line = m_label->lineGeometry(i);
            if (!line)
                break;
            width = std::max(width, line->width);
        }
        return std::min(width, m_label->geometry().width);
    }

    // The thumbnail fills the icon box less 6 px at the top and sides, keeping
    // its aspect, cut to a tenth of its height at the corners.
    void layoutThumbnail()
    {
        if (!m_wantsThumbnail) {
            m_thumbnail->setVisible(false);
            return;
        }

        const ui::RectF box {m_iconItem.x + kSmallSpacing, m_iconItem.y + kSmallSpacing,
                             m_iconItem.width - kSmallSpacing * 2, m_iconItem.height - kSmallSpacing};
        const ui::Window* window = this->window();
        const double scale = window ? window->scale() : 1;
        // The size an Image asks its provider for: its logical size, rounded, at the scale.
        const int width = int(std::round(std::round(box.width) * scale));
        const int height = int(std::round(std::round(box.height) * scale));
        if (width <= 0 || height <= 0) {
            m_thumbnail->setVisible(false);
            return;
        }

        if (m_thumbnailBox != ui::SizeF(width, height)) {
            m_thumbnailBox = ui::SizeF(width, height);
            m_image = thumbnails::cached(m_path, m_modified, width, height);
            if (m_image.isNull()) {
                const std::weak_ptr<bool> alive = m_alive;
                const std::string path = m_path;
                const int64_t modified = m_modified;
                thumbnails::load(path, modified, width, height,
                                 [this, alive, path, modified, width, height](const ui::Bitmap& image) {
                                     if (alive.expired() || path != m_path || modified != m_modified
                                         || m_thumbnailBox != ui::SizeF(width, height))
                                         return;
                                     m_image = image;
                                     requestLayout();
                                 });
            }
            if (!m_image.isNull()) {
                const ui::Bitmap image = m_image.converted(ui::Bitmap::Format::Argb32Premultiplied);
                m_thumbnail->setPixels(image.width(), image.height(), reinterpret_cast<const uint32_t*>(image.bits()),
                                       image.stride(), true, scale);
            }
        }

        if (m_image.isNull()) {
            m_thumbnail->setVisible(false);
            return;
        }

        // PreserveAspectFit, centred in the box.
        const double imageWidth = m_image.width() / scale;
        const double imageHeight = m_image.height() / scale;
        const double fit = std::min(box.width / imageWidth, box.height / imageHeight);
        const double paintedWidth = std::min(box.width, imageWidth * fit);
        const double paintedHeight = std::min(box.height, imageHeight * fit);
        m_thumbnail->setGeometry({box.x + (box.width - paintedWidth) / 2, box.y + (box.height - paintedHeight) / 2,
                                  paintedWidth, paintedHeight});
        m_thumbnail->setCornerRadius(paintedHeight * 0.1);
        m_thumbnail->setOpacity(m_hidden ? 0.5 : 1.0);
        m_thumbnail->setVisible(true);
    }

    FolderGridView* m_view;
    int m_index = -1;
    int m_row = -1;
    bool m_selected = false;
    bool m_hidden = false;
    bool m_dropTarget = false;
    bool m_dropAllowed = false;
    bool m_wantsThumbnail = false;
    std::string m_path;
    int64_t m_modified = 0;
    ui::SizeF m_thumbnailBox;
    ui::Bitmap m_image;
    ui::RectF m_iconItem;
    ui::ThemeIcon* m_icon;
    ui::ImageView* m_thumbnail;
    ui::ThemeIcon* m_overlay;
    ThumbnailTint* m_thumbnailTint;
    ui::ThemeIcon* m_emblem;
    ui::Label* m_label;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

FolderGridView::FolderGridView(FolderModel& model)
    : FolderView(model)
{
    setName("FolderGridView");
    m_iconSize = Preferences::instance().gridIconSize();
    updateMetrics();
}

FolderGridView::~FolderGridView() = default;

void FolderGridView::setIconSize(double size)
{
    if (m_iconSize == size)
        return;
    m_iconSize = size;
    Preferences::instance().setGridIconSize(size);
    requestLayout();
}

void FolderGridView::increaseIconSize()
{
    const double maximum = Preferences::instance().maximumIconSize();
    setIconSize(m_iconSize >= maximum ? maximum : std::min(maximum, m_iconSize + m_iconSize * 0.1));
}

void FolderGridView::decreaseIconSize()
{
    setIconSize(m_iconSize <= kMinimumIconSize ? kMinimumIconSize
                                               : std::max(kMinimumIconSize, m_iconSize - m_iconSize * 0.1));
}

void FolderGridView::updateMetrics()
{
    const int fromColumns = m_columns;
    const double fromRowHeight = m_cellHeight;

    const double available = geometry().width - leftMargin() - rightMargin();
    // The columns come from the width alone and share it: counting them from
    // the cell width, which follows the width too, dropped a column for an
    // instant on every pixel of a resize.
    m_columns = std::max(1, int(std::floor(available / minimumCellWidth())));
    // Rounded down a 64th of a pixel, so the cells never add up to more than
    // the width; each item snaps its own drawing to the pixel grid.
    m_cellWidth = std::max(minimumCellWidth(), std::floor(available / m_columns * 64) / 64);
    m_cellHeight = m_iconSize + style::fontHeight() * 2 + kLargeSpacing * 2;

    if (fromRowHeight > 0)
        reflow(fromColumns, fromRowHeight);
}

void FolderGridView::reflow(int fromColumns, double fromRowHeight)
{
    if (fromColumns <= 0 || fromRowHeight <= 0 || (fromColumns == m_columns && fromRowHeight == m_cellHeight))
        return;

    // The offset of the view's top below the first row, in content
    // coordinates, which start at the first row.
    const double top = scrollPosition().y - topMargin() + headerHeight();
    if (top <= 0)
        return;
    const int row = int(std::floor(top / fromRowHeight));
    const double within = (top - row * fromRowHeight) * m_cellHeight / fromRowHeight;
    const double y = std::floor(double(row) * fromColumns / m_columns) * m_cellHeight + within - headerHeight();
    // The content height changes with the new metrics: lay it out first.
    ScrollView::layout();
    setScrollPosition({0, std::max(0.0, y + topMargin())});
}

void FolderGridView::layout()
{
    updateMetrics();
    FolderView::layout();
}

double FolderGridView::minimumContentHeight() const
{
    const int count = model().count();
    const int rows = (count + m_columns - 1) / std::max(1, m_columns);
    return std::max(geometry().height, topMargin() + rows * m_cellHeight + bottomMargin());
}

ui::RectF FolderGridView::cellRect(int index) const
{
    const int column = index % std::max(1, m_columns);
    const int row = index / std::max(1, m_columns);
    const ui::Window* window = this->window();
    const double scale = window ? window->scale() : 1;
    // Cells are a fraction of a pixel wide; each sits on whole device pixels.
    const double x = std::round(column * m_cellWidth * scale) / scale;
    return {leftMargin() + x, topMargin() + row * m_cellHeight, m_cellWidth, m_cellHeight};
}

int FolderGridView::indexAt(ui::PointF content) const
{
    const double x = content.x - leftMargin();
    const double y = content.y - topMargin();
    if (x < 0 || y < 0)
        return -1;
    const int column = int(std::floor(x / m_cellWidth));
    const int row = int(std::floor(y / m_cellHeight));
    if (column >= m_columns)
        return -1;
    const int index = row * m_columns + column;
    return index < model().count() ? index : -1;
}

GridItem* FolderGridView::itemFor(int index) const
{
    const auto found = m_items.find(index);
    return found != m_items.end() ? found->second : nullptr;
}

bool FolderGridView::hitsItem(int index, ui::PointF content) const
{
    const ui::RectF cell = cellRect(index);
    const ui::PointF local {content.x - cell.x, content.y - cell.y};
    if (GridItem* item = itemFor(index))
        return item->containsPoint(local);
    return false;
}

int FolderGridView::dropRowAt(ui::PointF content) const
{
    // Only a folder's icon and name take a drop, so the rest of every cell
    // still reaches the folder shown when icons fill it.
    const int index = indexAt(content);
    if (index < 0 || model().isBlank(rowOf(index)))
        return -1;
    const FileItemPtr item = model().item(rowOf(index));
    if (!item || !item->isDir)
        return -1;
    return hitsItem(index, content) ? rowOf(index) : -1;
}

void FolderGridView::rectangleSelect(const ui::RectF& band)
{
    const int count = model().count();
    const double cellWidth = m_cellWidth - 2 * kSmallSpacing;
    const double cellHeight = m_cellHeight - 2 * kSmallSpacing;
    // In content coordinates, which start at the first cell.
    const double x = band.x - leftMargin();
    const double y = band.y - topMargin();
    const int perStripe = std::max(1, int(std::floor(geometry().width / m_cellWidth)));
    const int stripes = (count + perStripe - 1) / perStripe;
    const int firstColumn = std::max(0, int(std::floor(x / m_cellWidth)) - 1);
    const int lastColumn = int(std::floor((x + band.width) / m_cellWidth)) + 1;
    const int firstRow = std::max(0, int(std::floor(y / m_cellHeight)) - 1);
    const int lastRow = std::min(stripes - 1, int(std::floor((y + band.height) / m_cellHeight)) + 1);
    const int lastCell = std::min(perStripe - 1, lastColumn);

    std::vector<int> indices;
    for (int stripe = firstRow; stripe <= lastRow; ++stripe) {
        for (int cell = firstColumn; cell <= lastCell; ++cell) {
            const int index = stripe * perStripe + cell;
            if (index >= count)
                break;
            if (model().isBlank(rowOf(index)))
                continue;

            const double cellX = cell * m_cellWidth + kSmallSpacing;
            const double cellY = stripe * m_cellHeight + kSmallSpacing;
            // The cell first, which is cheaper than the item.
            if (cellX >= x + band.width || x >= cellX + cellWidth || cellY >= y + band.height
                || y >= cellY + cellHeight)
                continue;

            // An item answers for its icon and name; one not made has its cell.
            GridItem* item = itemFor(index);
            const ui::RectF itemCell = cellRect(index);
            if (!item || item->intersectsRect({band.x - itemCell.x, band.y - itemCell.y, band.width, band.height}))
                indices.push_back(index);
        }
    }
    applyRectangleSelection(indices);
}

std::optional<ui::RectF> FolderGridView::labelRect(int index) const
{
    GridItem* item = itemFor(index);
    if (!item)
        return std::nullopt;
    const ui::RectF cell = cellRect(index);
    const ui::RectF label = item->labelBox();
    const ui::PointF origin = fromContent({cell.x + label.x, cell.y + label.y});
    return ui::RectF {origin.x, origin.y, label.width, label.height};
}

void FolderGridView::revealIndex(int index)
{
    if (index < 0)
        return;
    const ui::RectF cell = cellRect(index);
    const double top = scrollPosition().y + headerHeight();
    const double bottom = scrollPosition().y + geometry().height - bottomMargin();
    if (cell.y < top)
        setContentY(cell.y - headerHeight());
    else if (cell.y + cell.height > bottom)
        setContentY(cell.y + cell.height - geometry().height + bottomMargin());
}

void FolderGridView::updateItems()
{
    View* content = contentView();
    const int count = model().count();
    const double top = scrollPosition().y;
    const double bottom = top + geometry().height;
    int first = 0;
    int last = 0;
    if (count > 0 && m_cellHeight > 0) {
        const int firstRow = std::max(0, int(std::floor((top - topMargin()) / m_cellHeight)));
        const int lastRow = std::max(0, int(std::ceil((bottom - topMargin()) / m_cellHeight)));
        first = std::min(count, firstRow * m_columns);
        last = std::min(count, lastRow * m_columns);
    }

    for (auto it = m_items.begin(); it != m_items.end();) {
        if (it->first < first || it->first >= last) {
            it->second->setVisible(false);
            m_spare.push_back(it->second);
            it = m_items.erase(it);
        } else {
            ++it;
        }
    }

    for (int index = first; index < last; ++index) {
        GridItem* item = itemFor(index);
        if (!item) {
            if (!m_spare.empty()) {
                item = m_spare.back();
                m_spare.pop_back();
                item->setVisible(true);
            } else {
                item = content->addChild<GridItem>(this);
            }
            m_items.emplace(index, item);
        }
        item->setGeometry(cellRect(index));
        item->bind(index, rowOf(index));
    }
}

bool FolderGridView::navigate(const ui::KeyEvent& event)
{
    if ((event.key == '=' || event.key == '+') && (event.modifiers & ui::ControlModifier)) {
        increaseIconSize();
        return true;
    }
    if (event.key == '-' && (event.modifiers & ui::ControlModifier)) {
        decreaseIconSize();
        return true;
    }

    int step = 0;
    if (event.key == ui::key::Left)
        step = -1;
    else if (event.key == ui::key::Right)
        step = 1;
    else if (event.key == ui::key::Up)
        step = -m_columns;
    else if (event.key == ui::key::Down)
        step = m_columns;
    else
        return false;

    const int count = model().count();
    if (count <= 0 || m_columns <= 0)
        return true;
    // A packed grid: plain index arithmetic, nothing past either end.
    const int next = currentIndex() < 0 ? 0 : currentIndex() + step;
    if (next < 0 || next >= count)
        return true;
    setCurrentIndex(next);
    updateSelection(event.modifiers);
    return true;
}

bool FolderGridView::scroll(const ui::ScrollEvent& event)
{
    if (event.modifiers & ui::ControlModifier) {
        if (event.deltaY < 0 || event.value120Y < 0)
            increaseIconSize();
        else
            decreaseIconSize();
        return true;
    }
    return FolderView::scroll(event);
}

void FolderGridView::forEachItemView(const std::function<void(int index, ui::View* item)>& visit) const
{
    for (const auto& [index, item] : m_items)
        visit(index, item);
}

std::optional<ui::RectF> FolderGridView::dragRect(int index) const
{
    return cellRect(index);
}

void FolderGridView::paintDragItem(int, ui::Painter&, const ui::RectF&)
{
}

} // namespace fm
