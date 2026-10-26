// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "core/lister.h"
#include "core/standardfolders.h"

#include "base/application_bundle.h"
#include "base/desktop_entry.h"
#include "base/file.h"

#include "cutefish/files/file_system_url.h"


namespace fm {

std::string DirectoryLister::localPathForUrl(const std::string& url)
{
    if (url.starts_with('/'))
        return cleanPath(url);
    const files::Url parsed(url);
    if (parsed.isLocalFile())
        return cleanPath(parsed.toLocalFile());
    if (parsed.scheme() == "trash") {
        try {
            return files::FileSystemUrl(parsed).localPath();
        } catch (...) {
            return {};
        }
    }
    return {};
}

FileItem DirectoryLister::snapshot(const files::FileItem& item)
{
    FileItem result;
    if (item.isNull())
        return result;
    result.url = item.url().toString();
    result.path = item.localPath();
    result.name = item.name();
    const std::string text = item.text();
    if (text != result.name)
        result.displayName = text;
    result.mimeType = item.mimetype();
    result.iconName = item.iconName();
    result.typeDescription = item.typeDescription();
    if (const files::Url target = item.targetUrl(); !target.isEmpty())
        result.targetUrl = target.toString();
    result.originalPath = item.originalPath();
    result.deletionTime = item.deletionTime().value_or(0);
    result.size = item.size();
    result.modified = item.time(files::FileItem::ModificationTime).value_or(0);
    result.accessed = item.time(files::FileItem::AccessTime).value_or(0);
    result.created = item.time(files::FileItem::CreationTime).value_or(0);
    result.isBundle = item.isDir() && result.mimeType == base::bundle::kType;
    result.isDir = item.isDir() && !result.isBundle;
    result.isFile = item.isFile();
    result.isLink = item.isLink();
    result.isHidden = item.isHidden();
    result.isWritable = item.isWritable();
    return result;
}

FileItem DirectoryLister::makeItem(const std::string& path)
{
    files::FileItem item(files::Url::fromLocalFile(path));
    item.prepareMetadata();
    return snapshot(item);
}

DirectoryLister::DirectoryLister()
{
    // A launcher shows under its own name and icon, an application bundle
    // under its folder's name without .app.
    m_lister.setItemPreparer([](files::FileItem& item) {
        if (item.isDir() && !base::bundle::bundlePath(item.localPath()).empty()) {
            if (const auto bundle = base::bundle::read(item.localPath(), {})) {
                item.setMimeType(base::bundle::kType);
                item.setDisplayName(item.name().substr(0, item.name().size() - 4));
                item.setIconName(bundle->icon.empty() ? "application-x-executable" : bundle->icon);
            }
            return;
        }
        if (!item.isDesktopFile() || !item.isFile())
            return;
        const auto contents = base::file::read(item.localPath());
        if (!contents)
            return;
        if (const auto entry = base::DesktopEntry::parse(item.name(), item.localPath(), *contents)) {
            item.setDisplayName(entry->name);
            if (!entry->icon.empty())
                item.setIconName(entry->icon);
        }
    });

    m_subscriptions.push_back(m_lister.started.connect([this] {
        started.emit();
    }));
    m_subscriptions.push_back(m_lister.completed.connect([this] {
        m_rootItem = snapshot(m_lister.rootItem());
        completed.emit();
    }));
    m_subscriptions.push_back(m_lister.canceled.connect([this] {
        canceled.emit();
    }));
    m_subscriptions.push_back(m_lister.error.connect([this](const std::string& message) {
        error.emit(message);
    }));
    m_subscriptions.push_back(m_lister.itemsChanged.connect([this] {
        m_rootItem = snapshot(m_lister.rootItem());
        itemsChanged.emit();
    }));
    m_subscriptions.push_back(m_lister.directoryChanged.connect([this] {
        directoryChanged.emit();
    }));
}

DirectoryLister::~DirectoryLister() = default;

void DirectoryLister::openUrl(const std::string& url)
{
    m_rootItem = {};
    m_lister.openUrl(files::Url(url));
}

void DirectoryLister::search(const std::string& rootUrl, const std::string& text)
{
    m_lister.search(files::Url(rootUrl), text);
}

void DirectoryLister::refresh()
{
    m_lister.updateDirectory(m_lister.url());
}

} // namespace fm
