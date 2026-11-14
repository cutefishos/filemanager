// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "views/gridview.h"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

using fm::GridColumns;

namespace {

// The left edges of the icons in the first row, in whole pixels.
std::vector<int> iconEdges(double width, double iconSize)
{
    const GridColumns columns = GridColumns::forWidth(width, iconSize);
    std::vector<int> edges;
    for (int column = 0; column < columns.count; ++column)
        edges.push_back(int(std::floor(columns.x(column) + (columns.cellWidth - iconSize) / 2)));
    return edges;
}

} // namespace

// A column is added once the width reaches it, and the icons spread out
// again from the narrowest gaps.
TEST(GridColumnsTest, AddsAColumnAtEachStep)
{
    EXPECT_EQ(iconEdges(575, 64), (std::vector<int> {51, 187, 323, 459}));
    EXPECT_EQ(iconEdges(576, 64), (std::vector<int> {29, 142, 256, 369, 482}));
    EXPECT_EQ(iconEdges(689, 64), (std::vector<int> {48, 180, 312, 444, 576}));
    EXPECT_EQ(iconEdges(690, 64), (std::vector<int> {29, 142, 256, 369, 483, 596}));
    EXPECT_EQ(iconEdges(347, 64), (std::vector<int> {66, 216}));
    EXPECT_EQ(iconEdges(348, 64), (std::vector<int> {29, 142, 255}));
}

// Small icons keep the cell a name needs; large ones widen it.
TEST(GridColumnsTest, CellWidthFollowsLargeIcons)
{
    EXPECT_EQ(GridColumns::forWidth(800, 32).cellWidth, 104);
    EXPECT_EQ(GridColumns::forWidth(800, 64).cellWidth, 104);
    EXPECT_EQ(GridColumns::forWidth(800, 96).cellWidth, 110);
    EXPECT_EQ(GridColumns::forWidth(800, 128).cellWidth, 146);
    EXPECT_EQ(GridColumns::forWidth(629, 128).count, 3);
    EXPECT_EQ(GridColumns::forWidth(630, 128).count, 4);
}

// One column at least, however narrow.
TEST(GridColumnsTest, KeepsOneColumn)
{
    const GridColumns columns = GridColumns::forWidth(80, 64);
    EXPECT_EQ(columns.count, 1);
    EXPECT_GE(columns.gap, 0);
}
