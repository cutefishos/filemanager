// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "core/common.h"

#include "base/signal.h"
#include "base/thread_pool.h"

#include <atomic>
#include <map>
#include <memory>
#include <string>

namespace fm {

// How many entries the folders of a listing hold, counted on two threads
// of their own as the list view asks; -1 until counted.
class FolderSizeCache
{
public:
    FolderSizeCache();
    ~FolderSizeCache();

    // Has each folder counted again when next asked for, which answers with
    // the count it had until the new one arrives.
    void clear();
    // Forgets them and drops those under way, for another folder.
    void reset();
    int64_t count(const std::string& path);

    base::Signal<std::string> ready;

private:
    struct Count
    {
        int64_t entries = -1;
        bool stale = false;
    };

    std::map<std::string, Count> m_counts;
    std::shared_ptr<std::atomic_bool> m_cancelled;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

} // namespace fm
