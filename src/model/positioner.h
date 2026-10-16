// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-FileCopyrightText: 2014 Eike Hein <hein@kde.org>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/common.h"

#include "base/signal.h"
#include "base/timer.h"

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace fm {

class FolderModel;

// The desktop's icons on fixed cells. The grid runs
// in stripes (columns, as the desktop flows top to bottom) of perStripe
// cells; cell = stripe * perStripe + pos. New icons take the rightmost
// stripe first, filled from the top. Cells are kept by URL, saved as
// [stripes, perStripe, then url, stripe, pos for each icon].
class Positioner
{
public:
    enum class Direction {
        Left,
        Right,
        Up,
        Down,
    };

    explicit Positioner(FolderModel& model);
    ~Positioner();

    void setGrid(int stripes, int perStripe);
    int stripes() const { return m_stripes; }
    int perStripe() const { return m_perStripe; }

    // One past the last cell in use: the cells a view lays out.
    int cellCount() const;
    // The model row on a cell, or -1 for a blank one.
    int rowAt(int cell) const;
    int cellOfRow(int row) const;
    int cellOfUrl(const std::string& url) const;
    bool isBlank(int cell) const { return rowAt(cell) < 0; }

    // The saved layout; applied once the folder is listed.
    const std::vector<std::string>& positions() const { return m_positions; }
    void setPositions(std::vector<std::string> positions);
    // Pairs of cells: each icon on the first moves to the second, the next
    // free one when that is taken, -1 for the first free cell.
    void move(const std::vector<std::pair<int, int>>& moves);
    // The icon nearest the cell in a direction, or -1.
    int nearestItem(int cell, Direction direction) const;
    // Every icon on its default cell, the saved layout forgotten.
    void reset();

    // Cells changed: icons came, went or moved.
    base::Signal<> changed;
    base::Signal<> positionsChanged;

private:
    void contentsChanged(const struct ContentsChange& change);
    void listed();
    void initMaps();
    void place(int cell, const std::string& url);
    int lastCell() const;
    int defaultCell(int ordinal) const;
    int nextDefaultCell(const std::map<int, std::string>& occupied) const;
    int freeCell(const std::map<int, std::string>& occupied) const;
    bool fitsGrid(int stripe, int pos) const;
    bool computeMaps(std::map<int, std::string>* cells) const;
    void applyPositions();
    void scheduleUpdate();
    void updatePositions();

    FolderModel& m_model;
    int m_stripes = 0;
    int m_perStripe = 0;
    std::map<int, std::string> m_cells;
    std::unordered_map<std::string, int> m_urls;
    std::map<std::string, std::string> m_renames;
    std::vector<std::string> m_positions;
    bool m_deferApply = false;
    std::vector<std::pair<int, int>> m_deferredMoves;
    base::Timer m_update;
    std::vector<base::Subscription> m_subscriptions;
};

} // namespace fm
