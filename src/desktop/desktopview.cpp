// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/desktopview.h"
#include "core/thumbnails.h"
#include "model/foldermodel.h"
#include "views/style.h"

#include "base/url.h"
#include "ui/blurred_copy.h"
#include "ui/fast_blur.h"
#include "ui/glyph_run.h"
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

static constexpr double kIconTopMargin = 6;
static constexpr double kLabelSpacing = 5;
static constexpr double kLabelInset = 6;
static constexpr double kCellBottomMargin = 8;
static constexpr double kMinimumIconSize = 22;

static std::string iconOrFallback(const std::string& name, const std::string& fallback, int size)
{
    if (!name.empty() && !ui::themeIconPath(name, size).empty())
        return name;
    return fallback;
}

// The selection under the name: a rounded box per line, bridged where lines meet.
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

// A silhouette of the icon in black, as MultiEffect's brightness -1 makes it.
class IconSilhouette : public ui::View
{
public:
    IconSilhouette()
    {
        icon = addChild<ui::ThemeIcon>();
        icon->setColor(ui::Color {0, 0, 0, 255});
    }

    ui::ThemeIcon* icon;

protected:
    void layout() override
    {
        const ui::RectF rect = localRect();
        const double size = icon->sizeHint().width;
        icon->setGeometry({centeredStart(rect.width, size), centeredStart(rect.height, size), size, size});
    }
};

// The name in black at (8, 8), for the soft halo around it.
class LabelHalo : public ui::View
{
public:
    LabelHalo()
    {
        label = addChild<ui::Label>();
        label->setWrap(true);
        label->setTextAlignment(ui::Label::TextAlignment::Center);
        label->setColor(ui::Color {0, 0, 0, 255});
        label->setMaximumLineCount(2);
    }

    ui::Label* label;

protected:
    void layout() override
    {
        label->setGeometry({8, 8, std::max(0.0, geometry().width - 16), std::max(0.0, geometry().height - 16)});
    }
};

// One item's cell on the desktop.
class DesktopItem : public ui::View
{
public:
    explicit DesktopItem(DesktopGridView* view)
        : m_view(view)
    {
        m_shadow = addChild<ui::BlurredCopy>(16, 0.5);
        m_silhouette = m_shadow->setContent<IconSilhouette>();
        m_icon = addChild<ui::ThemeIcon>();
        m_thumbnail = addChild<ui::ImageView>();
        m_thumbnail->setFillMode(ui::ImageView::FillMode::Stretch);
        m_thumbnail->setVisible(false);
        m_emblem = addChild<ui::ThemeIcon>("emblem-symbolic-link", 16);
        m_emblem->setImageProviderLookup(true);
        m_emblem->setVisible(false);
        m_overlay = addChild<ui::ThemeIcon>();
        m_overlay->setVisible(false);
        m_halo = addChild<ui::FastBlurredCopy>(6);
        m_haloContent = m_halo->setContent<LabelHalo>();
        m_raised = addChild<ui::Label>();
        m_label = addChild<ui::Label>();
        for (ui::Label* label : {m_raised, m_label, m_haloContent->label}) {
            label->setWrap(true);
            label->setTextAlignment(ui::Label::TextAlignment::Center);
            label->setMaximumLineCount(2);
            label->setElide(ui::Label::Elide::Right);
        }
        m_raised->setColor(ui::Color {0, 0, 0, 51});
        m_raised->setSubpixelAntialiasing(false);
        m_haloContent->label->setSubpixelAntialiasing(false);
    }

    void bind(int cell, int row)
    {
        const bool fresh = cell != m_cell || row != m_row;
        m_cell = cell;
        m_row = row;
        FolderModel& model = m_view->model();
        const FileItemPtr item = model.item(row);
        if (!item)
            return;

        const double iconSize = m_view->iconSize();
        m_selected = model.isSelected(row);
        m_hidden = item->isHidden;
        m_dropTarget = m_view->isDropTarget(cell);
        m_dropAllowed = m_view->dropTargetAllowedFor(cell);
        setVisible(!model.isBlank(row));

        const std::string icon = iconOrFallback(item->iconName, item->isDir ? "folder" : "unknown", int(iconSize));
        m_icon->setIconName(icon);
        m_icon->setSize(iconSize);
        m_silhouette->icon->setIconName(icon);
        m_silhouette->icon->setSize(iconSize);
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

        const double pixelSize = m_view->labelPixelSize();
        const double labelWidth = geometry().width - kLabelInset * 2;
        const std::string text = m_view->labelText(model.displayName(row), labelWidth);
        for (ui::Label* label : {m_raised, m_label, m_haloContent->label}) {
            label->setText(text);
            label->setFontSize(pixelSize);
            label->setWeight(600);
        }
        m_haloContent->requestLayout();
        m_halo->contentChanged();
        m_label->setColor(m_selected ? style::selectedTextColor() : ui::Color::rgb(0xffffff));
        // The raised style draws the name and its copy in greyscale; only the plain
        // text on the selection takes subpixel glyphs.
        m_label->setSubpixelAntialiasing(m_selected);
        m_label->setOpacity(m_hidden ? 0.8 : 1.0);
        const bool labelShown = m_view->renamingIndex() != cell;
        m_label->setVisible(labelShown);
        // Text.Raised: the name in the style colour a pixel lower, unless selected.
        m_raised->setVisible(labelShown && !m_selected);
        m_raised->setOpacity(m_hidden ? 0.8 : 1.0);
        m_halo->setVisible(labelShown && !m_selected);
        m_halo->setOpacity(m_hidden ? 0.35 : 0.65);
        m_emblem->setVisible(item->isLink);
        requestLayout();
        requestPaint();
    }

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
        const double iconItemWidth = width - 24;
        m_iconItem = {centeredStart(width, iconItemWidth), kIconTopMargin, iconItemWidth, iconSize};
        const ui::RectF iconRect {m_iconItem.x + centeredStart(iconItemWidth, iconSize), m_iconItem.y, iconSize,
                                  iconSize};
        m_icon->setGeometry(iconRect);
        m_overlay->setGeometry(iconRect);
        m_icon->setOpacity(m_hidden ? 0.5 : 1.0);
        m_overlay->setOpacity(0.5 * (m_hidden ? 0.5 : 1.0));
        m_shadow->setGeometry({m_iconItem.x, m_iconItem.y + 1, m_iconItem.width, m_iconItem.height});
        m_shadow->setOpacity(0.15 * (m_hidden ? 0.5 : 1.0));
        m_silhouette->requestLayout();

        layoutThumbnail();
        const bool thumbnailShown = m_thumbnail->isVisible();
        m_icon->setVisible(!thumbnailShown);
        m_silhouette->icon->setVisible(!thumbnailShown);
        m_shadow->contentChanged();
        m_overlay->setVisible(m_selected && !thumbnailShown);

        const ui::RectF anchor = thumbnailShown ? m_thumbnail->geometry() : iconRect;
        const double emblem = std::max(16.0, std::round(iconSize / 4));
        m_emblem->setSize(emblem);
        m_emblem->setGeometry({anchor.x + anchor.width - emblem, anchor.y + anchor.height - emblem, emblem, emblem});
        m_emblem->setOpacity(m_hidden ? 0.5 : 1.0);

        const double labelWidth = width - kLabelInset * 2;
        const double labelHeight = m_label->heightForWidth(labelWidth);
        const ui::RectF label {centeredStart(width, labelWidth), m_iconItem.y + iconSize + kLabelSpacing, labelWidth,
                               labelHeight};
        m_label->setGeometry(label);
        m_raised->setGeometry({label.x, label.y + 1, label.width, label.height});
        m_halo->setGeometry({label.x - 8, label.y - 8 + 1, label.width + 16, label.height + 16});
    }

    void paint(ui::Painter& painter) override
    {
        const ui::Theme& theme = ui::Theme::current();
        if (m_dropTarget) {
            const double width = std::max(m_iconItem.width, paintedWidth());
            const double height = m_iconItem.height + m_label->geometry().height + 12;
            const ui::Color color = m_dropAllowed ? theme.highlight : style::kDanger;
            painter.drawRoundedRect({(geometry().width - width) / 2, m_iconItem.y, width, height},
                                    {theme.mediumRadius, 0, ui::Corners::Circular}, style::withAlpha(color, 0.16), 2,
                                    color);
        }
        if (m_selected && m_label->isVisible()) {
            const double hPadding = 4;
            const double vPadding = 1;
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
    // its aspect, cut to a tenth of its height at the corners, with a faint
    // light rim that keeps its edge readable on a wallpaper of the same tone.
    void layoutThumbnail()
    {
        if (!m_wantsThumbnail) {
            m_thumbnail->setVisible(false);
            return;
        }
        const ui::RectF box {m_iconItem.x + 6, m_iconItem.y + 6, m_iconItem.width - 12, m_iconItem.height - 6};
        const double scale = window() ? window()->scale() : 1;
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
                                 [this, alive, path, modified, width, height, scale](const ui::Bitmap& image) {
                                     if (alive.expired() || path != m_path || modified != m_modified
                                         || m_thumbnailBox != ui::SizeF(width, height))
                                         return;
                                     m_image = image;
                                     applyImage(scale);
                                     requestLayout();
                                 });
            }
            applyImage(scale);
        }
        if (m_image.isNull()) {
            m_thumbnail->setVisible(false);
            return;
        }
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

    void applyImage(double scale)
    {
        if (m_image.isNull())
            return;
        const ui::Bitmap image = m_image.converted(ui::Bitmap::Format::Argb32Premultiplied);
        m_thumbnail->setPixels(image.width(), image.height(), reinterpret_cast<const uint32_t*>(image.bits()),
                               image.stride(), true, scale);
    }

    DesktopGridView* m_view;
    int m_cell = -1;
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
    ui::BlurredCopy* m_shadow;
    IconSilhouette* m_silhouette;
    ui::ThemeIcon* m_icon;
    ui::ImageView* m_thumbnail;
    ui::ThemeIcon* m_emblem;
    ui::ThemeIcon* m_overlay;
    ui::FastBlurredCopy* m_halo;
    LabelHalo* m_haloContent;
    ui::Label* m_raised;
    ui::Label* m_label;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

DesktopGridView::DesktopGridView(FolderModel& model)
    : FolderView(model)
    , m_positioner(model)
{
    setName("DesktopGridView");
    setDesktopStyle(true);
    setScrollDirections(false, false);
    m_subscriptions.push_back(m_positioner.changed.connect([this] {
        contentView()->requestLayout();
        updateItems();
    }));
    m_subscriptions.push_back(model.move.connect([this](int x, int y, const std::vector<std::string>& urls) {
        moveToCell(x, y, urls);
    }));
}

DesktopGridView::~DesktopGridView() = default;

double DesktopGridView::labelPixelSize() const
{
    // Text steps up once per 32 px of icon, about a fifth of the icon.
    return std::min(14.0, 12 + std::floor(std::max(0.0, m_iconSize - 64) / 32));
}

double DesktopGridView::labelHeight() const
{
    const ui::Theme& theme = ui::Theme::current();
    const std::string family = theme.fontFamily.empty() ? std::string("sans-serif") : theme.fontFamily;
    // Unrounded.
    const ui::FontMetrics metrics(ui::FontSpec {family, labelPixelSize() * 3 / 4, true}, 1, false);
    return metrics.heightF();
}

std::string DesktopGridView::labelText(const std::string& name, double width) const
{
    ui::Label measure(name);
    measure.setTextLayoutMode(textLayoutMode());
    measure.setFontSize(labelPixelSize());
    measure.setWeight(600);
    const auto advance = [&measure](const std::string& text) {
        measure.setText(text);
        return measure.sizeHint().width;
    };
    // Code point boundaries of the UTF-8 name.
    std::vector<size_t> bounds;
    for (size_t i = 0; i < name.size(); ++i) {
        if ((static_cast<unsigned char>(name[i]) & 0xc0) != 0x80)
            bounds.push_back(i);
    }
    bounds.push_back(name.size());
    const auto prefix = [&](size_t count) { return name.substr(0, bounds[count]); };

    const auto elideMiddle = [&](const std::string& text, double room) {
        if (advance(text) <= room)
            return text;
        std::vector<size_t> points;
        for (size_t i = 0; i < text.size(); ++i) {
            if ((static_cast<unsigned char>(text[i]) & 0xc0) != 0x80)
                points.push_back(i);
        }
        points.push_back(text.size());
        const size_t count = points.size() - 1;
        std::string best = "…";
        for (size_t keep = count; keep > 0; --keep) {
            const size_t head = (keep + 1) / 2;
            const size_t tail = keep - head;
            const std::string candidate = text.substr(0, points[head]) + "…" + text.substr(points[count - tail]);
            if (advance(candidate) <= room) {
                best = candidate;
                break;
            }
        }
        return best;
    };

    // Measured advances can round below what the label lays out.
    const double room = width - 2;
    if (room <= 0 || advance(name) <= room)
        return name;

    const size_t count = bounds.size() - 1;
    size_t fit = 0;
    size_t low = 1;
    size_t high = count;
    while (low <= high) {
        const size_t middle = (low + high) / 2;
        if (advance(prefix(middle)) <= room) {
            fit = middle;
            low = middle + 1;
        } else {
            high = middle - 1;
        }
    }
    if (fit == 0)
        return elideMiddle(name, room);

    // Break after a word, as wrapping would, unless one word fills the line.
    size_t breakAt = fit;
    if (name[bounds[fit]] != ' ') {
        for (size_t i = fit; i > 0; --i) {
            const char c = name[bounds[i - 1]];
            if (c == ' ' || c == '-' || c == '_') {
                breakAt = i;
                break;
            }
        }
    }
    std::string first = prefix(breakAt);
    while (!first.empty() && (first.back() == ' ' || first.back() == '\t'))
        first.pop_back();
    std::string rest = name.substr(bounds[breakAt]);
    while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t'))
        rest.erase(rest.begin());
    if (advance(rest) > room)
        rest = elideMiddle(rest, room);
    return first + "\n" + rest;
}

void DesktopGridView::setIconSize(double size)
{
    size = std::clamp(size, kMinimumIconSize, m_maximumIconSize);
    if (m_iconSize == size)
        return;
    m_iconSize = size;
    requestLayout();
    if (onIconSizeChanged)
        onIconSizeChanged();
}

void DesktopGridView::setWorkArea(const ui::RectF& area)
{
    if (m_workArea == area)
        return;
    m_workArea = area;
    requestLayout();
}

void DesktopGridView::updateMetrics()
{
    m_cellWidth = std::ceil(std::max(88.0, m_iconSize + 44));
    m_cellHeight = std::ceil(kIconTopMargin + m_iconSize + kLabelSpacing + labelHeight() * 2 + kCellBottomMargin);

    // The work area, or the whole view before the compositor sends one.
    const ui::RectF area = m_workArea.width > 0 ? m_workArea : localRect();
    const double availableWidth = std::max(0.0, area.width - 24);
    const double availableHeight = std::max(0.0, area.height - 24);
    const double width = std::floor(availableWidth / m_cellWidth) * m_cellWidth;
    const double height = std::floor(availableHeight / m_cellHeight) * m_cellHeight;
    // Anchored to the work area's top right, 12 px in.
    m_grid = {area.x + area.width - 12 - width, area.y + 12, width, height};
    m_columns = std::max(1, int(std::floor(width / m_cellWidth)));
    m_rows = std::max(1, int(std::floor(height / m_cellHeight)));
    m_positioner.setGrid(m_columns, m_rows);
}

void DesktopGridView::layout()
{
    updateMetrics();
    FolderView::layout();
}

ui::RectF DesktopGridView::cellRect(int cell) const
{
    // Flowing top to bottom: a stripe is a column.
    const int column = cell / std::max(1, m_rows);
    const int row = cell % std::max(1, m_rows);
    return {m_grid.x + column * m_cellWidth, m_grid.y + row * m_cellHeight, m_cellWidth, m_cellHeight};
}

int DesktopGridView::indexAt(ui::PointF content) const
{
    const double x = content.x - m_grid.x;
    const double y = content.y - m_grid.y;
    if (x < 0 || y < 0 || x >= m_grid.width || y >= m_grid.height)
        return -1;
    const int column = int(std::floor(x / m_cellWidth));
    const int row = int(std::floor(y / m_cellHeight));
    const int cell = column * m_rows + row;
    return m_positioner.isBlank(cell) ? -1 : cell;
}

int DesktopGridView::rowOf(int index) const
{
    return index < 0 ? -1 : m_positioner.rowAt(index);
}

void DesktopGridView::selectRange(int from, int to)
{
    std::vector<int> rows;
    for (int cell = std::min(from, to); cell <= std::max(from, to); ++cell) {
        const int row = m_positioner.rowAt(cell);
        if (row >= 0)
            rows.push_back(row);
    }
    if (!rows.empty())
        model().updateSelection(rows, false, false);
}

DesktopItem* DesktopGridView::itemFor(int cell) const
{
    const auto found = m_items.find(cell);
    return found != m_items.end() ? found->second : nullptr;
}

bool DesktopGridView::hitsItem(int index, ui::PointF content) const
{
    const ui::RectF cell = cellRect(index);
    if (DesktopItem* item = itemFor(index))
        return item->containsPoint({content.x - cell.x, content.y - cell.y});
    return false;
}

int DesktopGridView::dropRowAt(ui::PointF content) const
{
    const int index = indexAt(content);
    if (index < 0)
        return -1;
    const int row = rowOf(index);
    if (model().isBlank(row))
        return -1;
    const FileItemPtr item = model().item(row);
    if (!item || !item->isDir)
        return -1;
    return hitsItem(index, content) ? row : -1;
}

int DesktopGridView::cellAt(ui::PointF content) const
{
    if (m_cellWidth <= 0 || m_cellHeight <= 0)
        return -1;
    // Clamped into the grid: a press in the margin takes the nearest edge cell.
    const int column = int(std::clamp(std::floor((content.x - m_grid.x) / m_cellWidth), 0.0, double(m_columns - 1)));
    const int row = int(std::clamp(std::floor((content.y - m_grid.y) / m_cellHeight), 0.0, double(m_rows - 1)));
    return column * m_rows + row;
}

void DesktopGridView::rectangleSelect(const ui::RectF& band)
{
    std::vector<int> indices;
    for (const auto& [cell, item] : m_items) {
        if (m_positioner.isBlank(cell) || model().isBlank(rowOf(cell)))
            continue;
        const ui::RectF rect = cellRect(cell);
        if (rect.x >= band.x + band.width || band.x >= rect.x + rect.width || rect.y >= band.y + band.height
            || band.y >= rect.y + rect.height)
            continue;
        if (item->intersectsRect({band.x - rect.x, band.y - rect.y, band.width, band.height}))
            indices.push_back(cell);
    }
    std::sort(indices.begin(), indices.end());
    applyRectangleSelection(indices);
}

std::optional<ui::RectF> DesktopGridView::labelRect(int index) const
{
    DesktopItem* item = itemFor(index);
    if (!item)
        return std::nullopt;
    const ui::RectF cell = cellRect(index);
    const ui::RectF label = item->labelBox();
    return ui::RectF {cell.x + label.x, cell.y + label.y, label.width, label.height};
}

void DesktopGridView::updateItems()
{
    View* content = contentView();
    const int cells = std::min(m_positioner.cellCount(), m_columns * m_rows);
    for (auto it = m_items.begin(); it != m_items.end();) {
        if (it->first >= cells || m_positioner.isBlank(it->first)) {
            it->second->setVisible(false);
            m_spare.push_back(it->second);
            it = m_items.erase(it);
        } else {
            ++it;
        }
    }
    for (int cell = 0; cell < cells; ++cell) {
        if (m_positioner.isBlank(cell))
            continue;
        DesktopItem* item = itemFor(cell);
        if (!item) {
            if (!m_spare.empty()) {
                item = m_spare.back();
                m_spare.pop_back();
                item->setVisible(true);
            } else {
                item = content->addChild<DesktopItem>(this);
            }
            m_items.emplace(cell, item);
        }
        item->setGeometry(cellRect(cell));
        item->bind(cell, rowOf(cell));
    }
}

bool DesktopGridView::navigate(const ui::KeyEvent& event)
{
    if ((event.key == '=' || event.key == '+') && (event.modifiers & ui::ControlModifier)) {
        setIconSize(m_iconSize + 8);
        return true;
    }
    if (event.key == '-' && (event.modifiers & ui::ControlModifier)) {
        setIconSize(m_iconSize - 8);
        return true;
    }
    // Flowing top to bottom, the positioner's rows are the view's columns.
    Positioner::Direction direction;
    switch (event.key) {
    case ui::key::Up:
        direction = Positioner::Direction::Left;
        break;
    case ui::key::Down:
        direction = Positioner::Direction::Right;
        break;
    case ui::key::Left:
        direction = Positioner::Direction::Up;
        break;
    case ui::key::Right:
        direction = Positioner::Direction::Down;
        break;
    default:
        return false;
    }
    if (isRenaming())
        return true;
    const int next = m_positioner.nearestItem(currentIndex(), direction);
    if (next != -1) {
        setCurrentIndex(next);
        updateSelection(event.modifiers);
    }
    return true;
}

bool DesktopGridView::scroll(const ui::ScrollEvent& event)
{
    if (event.modifiers & ui::ControlModifier) {
        setIconSize(m_iconSize + ((event.deltaY < 0 || event.value120Y < 0) ? 8 : -8));
        return true;
    }
    return true;
}

void DesktopGridView::forEachItemView(const std::function<void(int index, ui::View* item)>& visit) const
{
    for (const auto& [index, item] : m_items)
        visit(index, item);
}

std::optional<ui::RectF> DesktopGridView::dragRect(int index) const
{
    return cellRect(index);
}

void DesktopGridView::paintDragItem(int, ui::Painter&, const ui::RectF&)
{
}

ui::PointF DesktopGridView::cellNear(ui::PointF point) const
{
    // The nearest cell to an icon's top left: half a cell of travel moves it.
    const double column = std::clamp(std::round((point.x - m_grid.x) / m_cellWidth), 0.0, double(m_columns - 1));
    const double row = std::clamp(std::round((point.y - m_grid.y) / m_cellHeight), 0.0, double(m_rows - 1));
    return {column, row};
}

void DesktopGridView::moveToCell(int x, int y, const std::vector<std::string>& urls)
{
    // A drag that started on an icon keeps the selection's shape: every icon
    // moves as many cells as the grabbed one. A drop from elsewhere lands on
    // the cell under the pointer.
    const int anchor = model().dragging() ? dragIndex() : -1;
    const auto cellOf = [this](int cell) {
        return ui::PointF {double(cell / std::max(1, m_rows)), double(cell % std::max(1, m_rows))};
    };
    ui::PointF target;
    ui::PointF anchorCell;
    if (anchor >= 0) {
        anchorCell = cellOf(anchor);
        const ui::RectF anchorRect = cellRect(anchor);
        const ui::PointF grab {dragGrab().x - anchorRect.x, dragGrab().y - anchorRect.y};
        target = cellNear({x - grab.x, y - grab.y});
    } else {
        target = {std::clamp(std::floor((x - m_grid.x) / m_cellWidth), 0.0, double(m_columns - 1)),
                  std::clamp(std::floor((y - m_grid.y) / m_cellHeight), 0.0, double(m_rows - 1))};
    }
    std::vector<std::pair<int, int>> moves;
    for (const std::string& url : urls) {
        const int from = m_positioner.cellOfUrl(url);
        if (from < 0)
            continue;
        const ui::PointF cell = cellOf(from);
        int column = int(anchor >= 0 ? target.x + (cell.x - anchorCell.x) : target.x);
        int row = int(anchor >= 0 ? target.y + (cell.y - anchorCell.y) : target.y);
        column = std::clamp(column, 0, m_columns - 1);
        row = std::clamp(row, 0, m_rows - 1);
        moves.emplace_back(from, column * m_rows + row);
    }
    if (!moves.empty())
        m_positioner.move(moves);
}

} // namespace fm
