// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "dialogs/propertiesdialog.h"
#include "core/standardfolders.h"

#include "base/i18n.h"
#include "base/locale.h"
#include "ui/icon.h"
#include "ui/label.h"
#include "ui/cell_layout.h"
#include "ui/theme.h"

#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>

using cutefish::i18n::tr;
using cutefish::i18n::trn;
using cutefish::i18n::trNoop;

namespace fm {

static constexpr double kSmallSpacing = 6;
static constexpr double kLargeSpacing = 12;

// "Fri Oct 16 09:30:00 2026", local time.
static std::string textDate(int64_t milliseconds)
{
    if (milliseconds <= 0)
        return {};
    const base::DateTime time = base::DateTime::fromSecondsSinceEpoch(milliseconds / 1000);
    return base::Locale("C").toString(time, "ddd MMM d HH:mm:ss yyyy");
}

// What a copy of everything would take: files' sizes, folders walked.
static uint64_t totalSize(const std::string& path, const std::atomic_bool& cancelled)
{
    struct stat st;
    if (::lstat(path.c_str(), &st) != 0)
        return 0;
    if (!S_ISDIR(st.st_mode))
        return uint64_t(st.st_size);
    uint64_t total = 0;
    if (DIR* directory = ::opendir(path.c_str())) {
        while (dirent* entry = ::readdir(directory)) {
            if (cancelled.load())
                break;
            const std::string name = entry->d_name;
            if (name == "." || name == "..")
                continue;
            total += totalSize(path + '/' + name, cancelled);
        }
        ::closedir(directory);
    }
    return total;
}

PropertiesDialog::PropertiesDialog(FileItemList items)
    : FramelessDialog(tr("Properties"), true)
    , m_items(std::move(items))
{
    const bool multiple = m_items.size() > 1;
    const FileItem& first = *m_items.front();
    if (!multiple) {
        m_fileName = first.name;
        m_iconName = first.isDir ? "folder" : first.iconName;
        m_mimeType = first.mimeType;
        m_size = formatSize(first.size);
        m_location = parentPath(first.path);
        m_created = textDate(first.created);
        m_modified = textDate(first.modified);
        m_accessed = textDate(first.accessed);
    } else {
        m_fileName = trn("{n} file", "{n} files", m_items.size());
        m_location = parentPath(first.path);
        m_iconName = "unknown";
    }

    std::vector<std::string> paths;
    for (const FileItemPtr& item : m_items)
        paths.push_back(item->path);
    m_sizeJob = base::ThreadPool::global().run(
            [paths, cancelled = m_cancelled] {
                uint64_t total = 0;
                for (const std::string& path : paths)
                    total += totalSize(path, *cancelled);
                return total;
            },
            [this](uint64_t total) {
                m_size = formatSize(total);
                if (m_sizeLabel) {
                    m_sizeLabel->setText(m_size);
                    fit();
                }
            });

    setHeader(40, 4);
    setPadding(kLargeSpacing * 1.5, kSmallSpacing, kLargeSpacing * 2 - kSmallSpacing);
    setContentFactory([this] { return createContent(); }, 0, 0);
    onCloseRequested = [this] {
        hide();
        if (onDone)
            onDone();
    };
}

PropertiesDialog::~PropertiesDialog()
{
    m_cancelled->store(true);
}

std::unique_ptr<ui::View> PropertiesDialog::createContent()
{
    auto layout = std::make_unique<ui::ColumnLayout>();
    layout->setSpacing(kLargeSpacing);

    struct Row
    {
        std::string label;
        std::string value;
        bool wrap = false;
        bool size = false;
    };
    std::vector<Row> rows = {
        {trNoop("Type:"), m_mimeType},
        {trNoop("Location:"), m_location, true},
        {trNoop("Size:"), m_size.empty() ? std::string(tr("Calculating...")) : m_size, false, true},
        {trNoop("Created:"), m_created},
        {trNoop("Modified:"), m_modified},
        {trNoop("Accessed:"), m_accessed},
    };

    // Hidden rows leave the grid, so the label column is measured over all.
    double labelWidth = 48;
    for (const Row& row : rows) {
        ui::Label probe(tr(row.label.c_str()));
        if (window())
            probe.setTextLayoutMode(window()->textLayoutMode());
        labelWidth = std::max(labelWidth, probe.sizeHint().width);
    }

    // The icon ends where the labels do and the name starts where the values do.
    auto* header = layout->addChild<ui::RowLayout>();
    header->setSpacing(kLargeSpacing);
    auto* iconBox = header->addChild<ui::View>();
    iconBox->setLayoutHints({.preferredWidth = labelWidth, .preferredHeight = 48});
    auto* icon = iconBox->addChild<ui::ThemeIcon>(m_iconName, 48);
    icon->setGeometry({std::ceil(labelWidth) - 48, 0, 48, 48});
    auto* name = header->addChild<ui::Label>(m_fileName);
    name->setElide(ui::Label::Elide::Middle);
    name->setLayoutHints({.fillWidth = true});

    auto* form = layout->addChild<ui::GridLayout>(2);
    form->setColumnSpacing(kLargeSpacing);
    form->setRowSpacing(kLargeSpacing);
    form->setLayoutHints({.fillWidth = true, .verticalAlignment = ui::Alignment::Start});
    for (const Row& row : rows) {
        // A row without a value is left out; location and size always show.
        const bool shown = !row.value.empty() || row.label == "Location:" || row.size;
        if (!shown)
            continue;
        auto* label = form->addChild<ui::Label>(tr(row.label.c_str()));
        label->setColor(ui::ColorId::DisabledText);
        label->setTextAlignment(ui::Label::TextAlignment::Right);
        label->setLayoutHints({.preferredWidth = labelWidth, .horizontalAlignment = ui::Alignment::End});
        auto* value = form->addChild<ui::Label>(row.value);
        if (row.wrap) {
            value->setWrap(true);
            value->setLayoutHints({.fillWidth = true});
        }
        if (row.size)
            m_sizeLabel = value;
    }
    return layout;
}

} // namespace fm
