// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <ctime>
#include <memory>
#include <string>
#include <sys/types.h>
#include <utility>
#include <vector>

// A file system in a disk image, read only. Paths are absolute within the volume.
class DiskVolume
{
public:
    struct Attributes
    {
        uint64_t id = 0;
        mode_t mode = 0;
        nlink_t links = 1;
        // As the reader sees it, uncompressed.
        uint64_t size = 0;
        timespec modified = {};
        timespec changed = {};
        timespec accessed = {};
    };

    // Reads a file's contents.
    class File
    {
    public:
        virtual ~File() = default;
        // A byte count, or a negative errno.
        virtual int read(char *buffer, size_t size, uint64_t offset) = 0;
    };

    virtual ~DiskVolume() = default;

    // False with a message the user can read.
    virtual bool open(std::string *message) = 0;

    virtual const std::string &name() const = 0;
    virtual uint32_t blockSize() const = 0;
    virtual uint64_t totalBlocks() const = 0;
    virtual uint64_t freeBlocks() const = 0;
    virtual uint64_t fileCount() const = 0;

    // 0, or a negative errno.
    virtual int stat(const std::string &path, Attributes *attributes) = 0;
    virtual int list(const std::string &path, std::vector<std::pair<std::string, Attributes>> *entries) = 0;
    virtual int openFile(const std::string &path, std::unique_ptr<File> *file) = 0;
    virtual int readLink(const std::string &path, std::string *target) = 0;
};
