// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "diskimagebackend.h"

#include "apfs.h"
#include "hfsplus.h"

#include "base/i18n.h"

#include <climits>
#include <fcntl.h>
#include <unistd.h>

using cutefish::i18n::tr;

DiskImageBackend::DiskImageBackend(const std::string &path)
    : m_path(path)
{
}

DiskImageBackend::~DiskImageBackend() = default;

ConnectResult DiskImageBackend::connect(const Credentials &)
{
    ConnectResult result;
    if (!m_image.open(m_path, &result.message))
        return result;

    for (const DiskImage::Volume &volume : m_image.volumes()) {
        std::unique_ptr<DiskVolume> opened;
        if (volume.format == DiskImage::Format::Apfs)
            opened = std::make_unique<Apfs>(&m_image, volume.offset, volume.size);
        else
            opened = std::make_unique<HfsPlus>(&m_image, volume.offset, volume.size);
        if (!opened->open(&result.message))
            return result;
        m_volume = std::move(opened);
        result.status = ConnectResult::Connected;
        return result;
    }

    result.message = tr("The disk image holds no volume that can be opened.");
    return result;
}

std::string DiskImageBackend::volumeName() const
{
    return m_volume ? m_volume->name() : std::string();
}

void DiskImageBackend::fillStat(const DiskVolume::Attributes &attributes, struct stat *st) const
{
    const bool folder = S_ISDIR(attributes.mode);
    *st = {};
    st->st_ino = attributes.id;
    st->st_mode = attributes.mode;
    st->st_nlink = folder ? 1 : attributes.links;
    // Owners on the disk are accounts of the machine that made it.
    st->st_uid = ::getuid();
    st->st_gid = ::getgid();
    st->st_size = folder ? 0 : off_t(attributes.size);
    st->st_blksize = 4096;
    st->st_blocks = blkcnt_t((attributes.size + 511) / 512);
    st->st_mtim = attributes.modified;
    st->st_ctim = attributes.changed;
    st->st_atim = attributes.accessed;
}

int DiskImageBackend::stat(const std::string &path, struct stat *st)
{
    DiskVolume::Attributes attributes;
    if (const int error = m_volume->stat(path, &attributes); error < 0)
        return error;
    fillStat(attributes, st);
    return 0;
}

int DiskImageBackend::list(const std::string &path, std::vector<DirectoryEntry> *entries)
{
    std::vector<std::pair<std::string, DiskVolume::Attributes>> children;
    if (const int error = m_volume->list(path, &children); error < 0)
        return error;

    entries->reserve(children.size());
    for (const auto &[name, attributes] : children) {
        DirectoryEntry entry;
        entry.name = name;
        fillStat(attributes, &entry.st);
        entries->push_back(std::move(entry));
    }
    return 0;
}

int DiskImageBackend::open(const std::string &path, int flags, mode_t, uint64_t *handle)
{
    if ((flags & O_ACCMODE) != O_RDONLY || (flags & (O_CREAT | O_TRUNC)))
        return -EROFS;

    std::unique_ptr<DiskVolume::File> file;
    if (const int error = m_volume->openFile(path, &file); error < 0)
        return error;

    *handle = m_nextHandle++;
    m_files.emplace(*handle, std::move(file));
    return 0;
}

int DiskImageBackend::read(uint64_t handle, char *buffer, size_t size, off_t offset)
{
    const auto found = m_files.find(handle);
    if (found == m_files.end())
        return -EBADF;
    if (offset < 0)
        return -EINVAL;
    return found->second->read(buffer, std::min<size_t>(size, INT_MAX), uint64_t(offset));
}

int DiskImageBackend::write(uint64_t, const char *, size_t, off_t)
{
    return -EROFS;
}

int DiskImageBackend::release(uint64_t handle)
{
    m_files.erase(handle);
    return 0;
}

int DiskImageBackend::truncate(const std::string &, off_t, const uint64_t *)
{
    return -EROFS;
}

int DiskImageBackend::remove(const std::string &)
{
    return -EROFS;
}

int DiskImageBackend::makeDirectory(const std::string &, mode_t)
{
    return -EROFS;
}

int DiskImageBackend::removeDirectory(const std::string &)
{
    return -EROFS;
}

int DiskImageBackend::rename(const std::string &, const std::string &, unsigned)
{
    return -EROFS;
}

int DiskImageBackend::setMode(const std::string &, mode_t)
{
    return -EROFS;
}

int DiskImageBackend::setTimes(const std::string &, const struct timespec[2])
{
    return -EROFS;
}

int DiskImageBackend::readLink(const std::string &path, std::string *target)
{
    return m_volume->readLink(path, target);
}

int DiskImageBackend::makeLink(const std::string &, const std::string &)
{
    return -EROFS;
}

int DiskImageBackend::statfs(const std::string &, struct statvfs *st)
{
    *st = {};
    st->f_bsize = m_volume->blockSize();
    st->f_frsize = m_volume->blockSize();
    st->f_blocks = m_volume->totalBlocks();
    st->f_bfree = m_volume->freeBlocks();
    st->f_files = m_volume->fileCount();
    st->f_flag = ST_RDONLY;
    st->f_namemax = 255;
    return 0;
}
