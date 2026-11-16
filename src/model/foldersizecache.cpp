// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "model/foldersizecache.h"

#include "base/event_loop.h"

#include <dirent.h>

namespace fm {

static base::ThreadPool& countPool()
{
    static base::ThreadPool pool(2);
    return pool;
}

FolderSizeCache::FolderSizeCache()
    : m_cancelled(std::make_shared<std::atomic_bool>(false))
{
}

FolderSizeCache::~FolderSizeCache()
{
    *m_alive = false;
    m_cancelled->store(true);
}

void FolderSizeCache::clear()
{
    for (auto& [path, count] : m_counts)
        count.stale = true;
}

void FolderSizeCache::reset()
{
    m_cancelled->store(true);
    m_cancelled = std::make_shared<std::atomic_bool>(false);
    m_counts.clear();
}

int64_t FolderSizeCache::count(const std::string& path)
{
    const auto [found, added] = m_counts.try_emplace(path);
    Count& known = found->second;
    if (!added && !known.stale)
        return known.entries;

    known.stale = false;
    const auto cancelled = m_cancelled;
    const base::LoopHandle loop = base::EventLoop::current()->handle();
    countPool().post([this, alive = m_alive, cancelled, loop, path] {
        int64_t count = 0;
        if (DIR* directory = ::opendir(path.c_str())) {
            while (!cancelled->load()) {
                dirent* entry = ::readdir(directory);
                if (!entry)
                    break;
                // Hidden entries, "." and ".." among them, are not counted.
                if (entry->d_name[0] != '.')
                    ++count;
            }
            ::closedir(directory);
        }
        loop.post([this, alive, cancelled, path, count] {
            if (!*alive || cancelled->load())
                return;
            Count& counted = m_counts[path];
            counted.stale = false;
            if (counted.entries == count)
                return;
            counted.entries = count;
            ready.emit(path);
        });
    });
    return known.entries;
}

} // namespace fm
