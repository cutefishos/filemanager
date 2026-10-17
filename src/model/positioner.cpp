// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-FileCopyrightText: 2014 Eike Hein <hein@kde.org>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "model/positioner.h"
#include "core/standardfolders.h"
#include "model/foldercontents.h"
#include "model/foldermodel.h"

#include "base/collator.h"
#include "base/url.h"

#include <algorithm>
#include <cstdlib>
#include <set>

namespace fm {

static std::string nameOf(const std::string& url)
{
    return fileName(base::url::toLocalFile(url).value_or(url));
}

static bool parseInt(const std::string& text, int* value)
{
    if (text.empty())
        return false;
    char* end = nullptr;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (*end != '\0')
        return false;
    *value = int(parsed);
    return true;
}

Positioner::Positioner(FolderModel& model)
    : m_model(model)
{
    m_update.setSingleShot(true);
    m_update.setInterval(std::chrono::milliseconds(0));
    m_update.onTimeout = [this] { updatePositions(); };

    m_subscriptions.push_back(model.rowsChanged.connect([this](const ContentsChange& change) {
        contentsChanged(change);
    }));
    m_subscriptions.push_back(model.statusChanged.connect([this] {
        if (m_model.status() != FolderModel::Status::Listing)
            listed();
    }));
    m_subscriptions.push_back(model.urlChanged.connect([this] { reset(); }));
    // A rename keeps the icon where it is, under its new URL.
    m_subscriptions.push_back(model.renaming.connect([this](const std::string& from, const std::string& to) {
        m_renames[from] = to;
    }));
    m_subscriptions.push_back(model.renameFailed.connect([this](const std::string& from, const std::string& to) {
        if (const auto found = m_renames.find(from); found != m_renames.end() && found->second == to)
            m_renames.erase(found);
    }));
    initMaps();
}

Positioner::~Positioner() = default;

void Positioner::setGrid(int stripes, int perStripe)
{
    if (stripes == m_stripes && perStripe == m_perStripe)
        return;
    if (m_update.isActive()) {
        m_update.stop();
        updatePositions();
    }
    m_stripes = stripes;
    m_perStripe = perStripe;
    // Without saved records every icon sits on its default cell, which depends on the grid.
    if (m_positions.empty()) {
        if (!m_deferApply)
            initMaps();
    } else if (m_stripes > 0 && m_perStripe > 0 && !m_cells.empty()) {
        applyPositions();
    }
    changed.emit();
}

int Positioner::lastCell() const
{
    return m_cells.empty() ? -1 : m_cells.rbegin()->first;
}

int Positioner::cellCount() const
{
    return lastCell() + 1;
}

int Positioner::rowAt(int cell) const
{
    const auto found = m_cells.find(cell);
    return found == m_cells.end() ? -1 : m_model.rowForUrl(found->second);
}

int Positioner::cellOfRow(int row) const
{
    const FileItemPtr item = m_model.item(row);
    return item ? cellOfUrl(item->url) : -1;
}

int Positioner::cellOfUrl(const std::string& url) const
{
    const auto found = m_urls.find(url);
    return found == m_urls.end() ? -1 : found->second;
}

void Positioner::place(int cell, const std::string& url)
{
    m_cells[cell] = url;
    m_urls[url] = cell;
}

// A cell is usable only while it is inside the grid the view has room for;
// anything outside is reflowed or it would sit off screen.
bool Positioner::fitsGrid(int stripe, int pos) const
{
    if (stripe < 0 || pos < 0)
        return false;
    if (m_perStripe > 0 && pos >= m_perStripe)
        return false;
    if (m_stripes > 0 && stripe >= m_stripes)
        return false;
    return true;
}

// The nth default slot: the rightmost stripe first, filled from its start,
// then the stripe next to it. Plain packing while the grid is unknown.
int Positioner::defaultCell(int ordinal) const
{
    if (m_perStripe <= 0 || m_stripes <= 0)
        return ordinal;
    // More icons than the grid has room for spill past it.
    if (ordinal >= m_perStripe * m_stripes)
        return ordinal;
    const int stripe = m_stripes - 1 - ordinal / m_perStripe;
    return stripe * m_perStripe + ordinal % m_perStripe;
}

int Positioner::nextDefaultCell(const std::map<int, std::string>& occupied) const
{
    if (m_perStripe <= 0 || m_stripes <= 0)
        return -1;
    for (int ordinal = 0; ordinal < m_perStripe * m_stripes; ++ordinal) {
        const int cell = defaultCell(ordinal);
        if (!occupied.contains(cell))
            return cell;
    }
    return -1;
}

// The first default slot, else the first gap, else past the last cell.
int Positioner::freeCell(const std::map<int, std::string>& occupied) const
{
    const int cell = nextDefaultCell(occupied);
    if (cell != -1)
        return cell;
    const int last = occupied.empty() ? -1 : occupied.rbegin()->first;
    for (int i = 0; i <= last; ++i) {
        if (!occupied.contains(i))
            return i;
    }
    return last + 1;
}

void Positioner::initMaps()
{
    m_cells.clear();
    m_urls.clear();
    for (int row = 0; row < m_model.count(); ++row)
        place(defaultCell(row), m_model.item(row)->url);
}

void Positioner::reset()
{
    m_update.stop();
    m_renames.clear();
    m_deferredMoves.clear();
    initMaps();
    const bool hadPositions = !m_positions.empty();
    m_positions.clear();
    if (hadPositions)
        positionsChanged.emit();
    changed.emit();
}

void Positioner::contentsChanged(const ContentsChange& change)
{
    // Waiting for the listing to apply the saved layout to all of it.
    if (m_deferApply)
        return;
    if (change.removed.empty() && change.added.empty())
        return;

    std::set<std::string> renamed;
    for (const std::string& url : change.removed) {
        const auto cell = m_urls.find(url);
        if (cell == m_urls.end())
            continue;
        const auto rename = m_renames.find(url);
        const int at = cell->second;
        m_urls.erase(cell);
        m_cells.erase(at);
        if (rename != m_renames.end()) {
            place(at, rename->second);
            renamed.insert(rename->second);
            m_renames.erase(rename);
        }
    }

    const bool first = m_cells.empty();
    std::set<int> takenNow;
    int ordinal = 0;
    for (const std::string& url : change.added) {
        if (renamed.contains(url) || m_urls.contains(url))
            continue;
        // A cell asked for up front, such as a new folder on the clicked cell,
        // wins over the default order as long as nothing sits on it.
        int cell = m_model.newDocumentCell(nameOf(url));
        const int cells = m_perStripe > 0 && m_stripes > 0 ? m_perStripe * m_stripes : 0;
        if (cell >= 0 && cells > 0 && cell >= cells)
            cell = -1;
        if (cell >= 0 && m_cells.contains(cell))
            cell = -1;
        if (cell < 0)
            cell = first ? defaultCell(ordinal) : freeCell(m_cells);
        while (m_cells.contains(cell) || takenNow.contains(cell))
            cell = freeCell(m_cells);
        takenNow.insert(cell);
        place(cell, url);
        ++ordinal;
    }

    changed.emit();
    scheduleUpdate();
}

void Positioner::listed()
{
    if (m_deferApply || m_positions.size() >= 5)
        applyPositions();
    if (!m_deferredMoves.empty()) {
        const auto moves = std::move(m_deferredMoves);
        m_deferredMoves.clear();
        move(moves);
    }
}

void Positioner::setPositions(std::vector<std::string> positions)
{
    if (positions == m_positions)
        return;
    m_update.stop();
    m_positions = std::move(positions);
    positionsChanged.emit();
    applyPositions();
}

void Positioner::applyPositions()
{
    if (m_model.status() == FolderModel::Status::Listing) {
        m_deferApply = true;
        return;
    }

    std::map<int, std::string> cells;
    if (!computeMaps(&cells)) {
        // Nothing to apply: whatever came while waiting gets default cells.
        if (m_deferApply) {
            m_deferApply = false;
            initMaps();
            changed.emit();
        }
        return;
    }
    m_deferApply = false;
    if (cells != m_cells) {
        m_cells = std::move(cells);
        m_urls.clear();
        for (const auto& [cell, url] : m_cells)
            m_urls[url] = cell;
        changed.emit();
    }
    // A reflow is temporary; only file changes and moves replace saved cells.
}

// Which file belongs on which cell by the saved records, without touching
// the live maps.
bool Positioner::computeMaps(std::map<int, std::string>* cells) const
{
    if (m_positions.size() < 5)
        return false;
    int storedColumns = 0;
    if (!parseInt(m_positions[0], &storedColumns) || storedColumns <= 0)
        return false;
    if ((m_positions.size() - 2) % 3 != 0)
        return false;

    cells->clear();
    std::set<std::string> unplaced;
    for (int row = 0; row < m_model.count(); ++row)
        unplaced.insert(m_model.item(row)->url);

    // The free cell closest to (stripe, pos), clamped into the grid: an icon
    // that no longer fits stays near where it was.
    const auto nearestFree = [&](int stripe, int pos) {
        if (m_perStripe <= 0 || m_stripes <= 0)
            return -1;
        stripe = std::clamp(stripe, 0, m_stripes - 1);
        pos = std::clamp(pos, 0, m_perStripe - 1);
        for (int radius = 0; radius < m_stripes + m_perStripe; ++radius) {
            int best = -1;
            int bestDistance = 0;
            for (int ds = -radius; ds <= radius; ++ds) {
                for (int dp = -radius; dp <= radius; ++dp) {
                    if (std::max(std::abs(ds), std::abs(dp)) != radius)
                        continue;
                    const int s = stripe + ds;
                    const int p = pos + dp;
                    if (s < 0 || p < 0 || s >= m_stripes || p >= m_perStripe)
                        continue;
                    const int cell = s * m_perStripe + p;
                    if (cells->contains(cell))
                        continue;
                    const int distance = ds * ds + dp * dp;
                    if (best == -1 || distance < bestDistance) {
                        best = cell;
                        bestDistance = distance;
                    }
                }
            }
            if (best != -1)
                return best;
        }
        return -1;
    };

    struct Spill
    {
        std::string url;
        int stripe;
        int pos;
    };
    std::vector<Spill> spilled;

    for (size_t offset = 2; offset + 2 < m_positions.size(); offset += 3) {
        const std::string& url = m_positions[offset];
        int storedStripe = 0;
        int pos = 0;
        if (!parseInt(m_positions[offset + 1], &storedStripe) || !parseInt(m_positions[offset + 2], &pos))
            continue;
        // The distance from the right edge stays when the grid changes width.
        const int stripe = storedStripe + m_stripes - storedColumns;
        if (!unplaced.contains(url))
            continue;
        if (!fitsGrid(stripe, pos)) {
            spilled.push_back({url, stripe, pos});
            continue;
        }
        const int cell = stripe * m_perStripe + pos;
        if (cells->contains(cell)) {
            spilled.push_back({url, stripe, pos});
            continue;
        }
        (*cells)[cell] = url;
        unplaced.erase(url);
    }

    for (const Spill& spill : spilled) {
        if (!unplaced.contains(spill.url))
            continue;
        int cell = nearestFree(spill.stripe, spill.pos);
        if (cell < 0)
            cell = freeCell(*cells);
        (*cells)[cell] = spill.url;
        unplaced.erase(spill.url);
    }

    // New files fill the free cells in the order the folder view lists
    // names; names the collator equates ("a1", "a01") keep one fixed order.
    std::vector<std::string> newUrls(unplaced.begin(), unplaced.end());
    base::Collator collator;
    collator.setNumericMode(true);
    std::sort(newUrls.begin(), newUrls.end(), [&collator](const std::string& a, const std::string& b) {
        const int order = collator.compare(nameOf(a), nameOf(b));
        return order != 0 ? order < 0 : nameOf(a) < nameOf(b);
    });
    for (const std::string& url : newUrls) {
        // A renamed file has no record under its new URL but already has a cell.
        int cell = cellOfUrl(url);
        if (cell < 0 || cells->contains(cell)
            || (m_perStripe > 0 && !fitsGrid(cell / m_perStripe, cell % m_perStripe)))
            cell = freeCell(*cells);
        (*cells)[cell] = url;
    }
    return true;
}

void Positioner::move(const std::vector<std::pair<int, int>>& moves)
{
    // Not while listing: the rows are about to change under the cells.
    if (m_model.status() == FolderModel::Status::Listing) {
        m_deferredMoves = moves;
        return;
    }

    std::vector<int> from;
    std::vector<int> to;
    std::vector<std::string> urls;
    for (const auto& [source, target] : moves) {
        from.push_back(source);
        to.push_back(target);
        const auto found = m_cells.find(source);
        urls.push_back(found == m_cells.end() ? std::string() : found->second);
    }
    const auto contains = [](const std::vector<int>& list, int value) {
        return std::find(list.begin(), list.end(), value) != list.end();
    };

    for (size_t i = 0; i < from.size(); ++i) {
        const int source = from[i];
        int target = to[i];
        if (urls[i].empty() || source == target)
            continue;
        if (target == -1)
            target = freeCell(m_cells);

        if (!contains(from, target) && !isBlank(target)) {
            // The next blank cell; two icons never land on one.
            const int cells = m_perStripe > 0 && m_stripes > 0 ? m_perStripe * m_stripes : 0;
            int tried = 0;
            while ((!isBlank(target) && source != target) || contains(to, target)) {
                ++target;
                // Wrapping inside the grid keeps the icon off columns the desktop cannot show.
                if (cells > 0 && target >= cells)
                    target = 0;
                if (cells > 0 && ++tried > cells) {
                    target = lastCell() + 1;
                    break;
                }
            }
        }
        to[i] = target;

        if (!contains(to, source)) {
            const auto found = m_cells.find(source);
            if (found != m_cells.end() && found->second == urls[i])
                m_cells.erase(found);
        }
        place(target, urls[i]);
    }

    changed.emit();
    scheduleUpdate();
}

int Positioner::nearestItem(int cell, Direction direction) const
{
    if (m_cells.empty())
        return -1;
    if (cell < 0)
        return m_cells.begin()->first;
    if (m_perStripe <= 0)
        return -1;

    int along = 0;
    int across = 0;
    switch (direction) {
    case Direction::Left:
        across = -1;
        break;
    case Direction::Right:
        across = 1;
        break;
    case Direction::Up:
        along = -1;
        break;
    case Direction::Down:
        along = 1;
        break;
    }

    // Positions as (x, y) on a grid of perStripe columns; the view maps
    // arrows into it.
    const int currentX = cell % m_perStripe;
    const int currentY = cell / m_perStripe;
    int nearest = -1;
    int lastDistance = -1;
    for (const auto& [candidate, url] : m_cells) {
        if (candidate == cell)
            continue;
        const int x = candidate % m_perStripe;
        const int y = candidate / m_perStripe;
        const int distance = std::abs(x - currentX) + std::abs(y - currentY);
        if (across == 0) {
            if (along * y > along * currentY
                && (nearest == -1 || distance < lastDistance || (distance == lastDistance && x == currentX))) {
                nearest = candidate;
                lastDistance = distance;
            }
        } else if (across * x > across * currentX
                   && (nearest == -1 || distance < lastDistance || (distance == lastDistance && y == currentY))) {
            nearest = candidate;
            lastDistance = distance;
        }
    }
    return nearest;
}

void Positioner::scheduleUpdate()
{
    m_update.start();
}

void Positioner::updatePositions()
{
    std::vector<std::string> positions;
    if (!m_cells.empty() && m_perStripe > 0) {
        positions.push_back(std::to_string(m_stripes));
        positions.push_back(std::to_string(m_perStripe));
        // Sorted by cell, so an unchanged desktop saves the same list.
        for (const auto& [cell, url] : m_cells) {
            positions.push_back(url);
            positions.push_back(std::to_string(std::max(0, cell / m_perStripe)));
            positions.push_back(std::to_string(std::max(0, cell % m_perStripe)));
        }
    }
    if (positions != m_positions) {
        m_positions = std::move(positions);
        positionsChanged.emit();
    }
}

} // namespace fm
