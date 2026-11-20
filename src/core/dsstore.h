// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "core/common.h"

#include "ui/geometry.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace fm {

// How a folder's window was arranged on the platform its .DS_Store file
// comes from, as disk images that install an application carry it: the
// window's size, the icons' size and places, and what is behind them.
// Lengths are points.
struct FolderLayout
{
    enum class Background { Default, Color, Picture };

    // The window's size, its title bar included; empty when not stored.
    ui::SizeF windowSize;
    // 0 when not stored.
    double iconSize = 0;
    // The names' text size; 0 when not stored.
    double textSize = 0;
    // Names under the icons, else beside them.
    bool labelOnBottom = true;
    Background background = Background::Default;
    ui::Color color;
    // The picture's absolute path, when it was found.
    std::string picture;
    // Each item's icon centre from the content's top left, by file name.
    std::map<std::string, ui::PointF> positions;
};

// The layout the file's records give the folder; nothing when the data is
// no .DS_Store file or places no icon.
std::optional<FolderLayout> parseFolderLayout(std::string_view data, const std::string& folder);
// The folder's own .DS_Store file read; reads the disk.
std::optional<FolderLayout> readFolderLayout(const std::string& folder);

} // namespace fm
