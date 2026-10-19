// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "core/fileitem.h"
#include "dialogs/framelessdialog.h"

#include "base/thread_pool.h"

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace cutefish::ui {
class Label;
}

namespace fm {

// The item's icon and
// name, then type, location, size (counted on a worker for folders and
// several items), and its times.
class PropertiesDialog : public FramelessDialog
{
public:
    explicit PropertiesDialog(FileItemList items);
    ~PropertiesDialog() override;

    void retranslate() override;

    std::function<void()> onDone;

private:
    std::unique_ptr<ui::View> createContent();

    FileItemList m_items;
    std::string m_fileName;
    std::string m_iconName;
    std::string m_mimeType;
    std::string m_location;
    std::string m_size;
    // What m_size reads, once known.
    std::optional<uint64_t> m_bytes;
    std::string m_created;
    std::string m_modified;
    std::string m_accessed;
    ui::Label* m_sizeLabel = nullptr;
    base::Job m_sizeJob;
    std::shared_ptr<std::atomic_bool> m_cancelled = std::make_shared<std::atomic_bool>(false);
};

} // namespace fm
