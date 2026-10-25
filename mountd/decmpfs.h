// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "diskvolume.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

// A file compressed in place: the com.apple.decmpfs attribute holds the header and
// small contents, the resource fork the blocks of larger ones.
struct Decmpfs
{
    uint32_t type = 0;
    // Of the contents, uncompressed.
    uint64_t size = 0;
    std::vector<uint8_t> inlineData;
};

// Reads size bytes of the resource fork at offset; false past its end or on a damaged volume.
using ReadResource = std::function<bool(uint64_t offset, void *buffer, size_t size)>;

// False when value is no decmpfs header.
bool parseDecmpfs(const std::vector<uint8_t> &value, Decmpfs *header);

// 0, or a negative errno.
int openDecmpfs(const Decmpfs &header, uint64_t resourceSize, ReadResource readResource,
                std::unique_ptr<DiskVolume::File> *file);
