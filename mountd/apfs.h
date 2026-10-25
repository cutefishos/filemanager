// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "diskimage.h"
#include "diskvolume.h"

#include "base/lru_cache.h"

#include <cstdint>
#include <memory>
#include <string>
#include <sys/types.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// The first unencrypted volume of an APFS container, read only, as of the container's
// latest checkpoint. Hard links share an id, and compressed files read as their contents.
class Apfs : public DiskVolume
{
public:
    // Of a file's data or an attribute's: logical and length in bytes, physical a block
    // number or 0 for a hole.
    struct Extent
    {
        uint64_t logical = 0;
        uint64_t length = 0;
        uint64_t physical = 0;
    };

    Apfs(DiskImage *disk, uint64_t offset, uint64_t size);
    ~Apfs() override;

    Apfs(const Apfs &) = delete;
    Apfs &operator=(const Apfs &) = delete;

    bool open(std::string *message) override;

    const std::string &name() const override
    {
        return m_name;
    }

    uint32_t blockSize() const override
    {
        return m_blockSize;
    }
    uint64_t totalBlocks() const override
    {
        return m_totalBlocks;
    }
    uint64_t freeBlocks() const override
    {
        return m_freeBlocks;
    }
    uint64_t fileCount() const override
    {
        return m_fileCount;
    }

    int stat(const std::string &path, Attributes *attributes) override;
    int list(const std::string &path, std::vector<std::pair<std::string, Attributes>> *entries) override;
    int openFile(const std::string &path, std::unique_ptr<File> *file) override;
    int readLink(const std::string &path, std::string *target) override;

    // Bytes of a stream laid out in extents, sorted and apart; zeros where no extent
    // reaches. False on a damaged volume.
    bool readExtents(const std::vector<Extent> &extents, uint64_t offset, void *buffer, size_t size);

private:
    struct Item
    {
        uint64_t id = 0;
        // Names the extents of the file's data.
        uint64_t stream = 0;
        bool folder = false;
        mode_t mode = 0;
        nlink_t links = 1;
        // As the reader sees it, uncompressed.
        uint64_t size = 0;
        uint64_t streamSize = 0;
        timespec modified = {};
        timespec changed = {};
        timespec accessed = {};
        bool compressed = false;
    };

    // An extended attribute: inline, or a stream of its own.
    struct Attribute
    {
        bool stream = false;
        std::vector<uint8_t> data;
        uint64_t streamId = 0;
        uint64_t size = 0;
    };

    struct Node
    {
        std::vector<uint8_t> bytes;
        uint16_t level = 0;
        uint32_t count = 0;
        bool fixed = false;
        size_t table = 0;
        size_t keys = 0;
        size_t valuesEnd = 0;
    };

    struct Tree
    {
        // A physical address, or an id the object map resolves.
        uint64_t root = 0;
        bool physical = true;
        const Tree *objectMap = nullptr;
        // Of fixed-size records.
        uint32_t keySize = 0;
        uint32_t valueSize = 0;
    };

    struct Cursor
    {
        // From the root to a leaf: node address and record index.
        std::vector<std::pair<uint64_t, uint32_t>> path;
        // A damaged tree's index may lead back to a leaf already walked.
        std::unordered_set<uint64_t> leaves;
    };

    // compare: a record key against the one searched for, as <0, 0 or >0.
    using Compare = int (*)(const uint8_t *key, size_t length, const void *target);

    struct Directory
    {
        std::vector<std::pair<std::string, Item>> entries;
        std::unordered_map<std::string, size_t> byName;
    };

    // The block at address, checked to hold object oid of type (any type for 0).
    bool readObject(uint64_t address, uint64_t oid, uint32_t type, std::vector<uint8_t> *bytes);
    bool openTree(uint64_t root, bool physical, const Tree *objectMap, Tree *tree);
    bool resolve(const Tree &objectMap, uint64_t oid, uint64_t *address);
    const Node *node(const Tree &tree, uint64_t id, uint64_t *address);
    const Node *cached(uint64_t address);
    bool record(const Tree &tree, const Node &node, uint32_t index, const uint8_t **key, size_t *keyLength,
                const uint8_t **value, size_t *valueLength);
    bool child(const Tree &tree, const Node &node, uint32_t index, uint64_t *id);
    // The last record not above target.
    bool seekLast(const Tree &tree, Compare compare, const void *target, const uint8_t **key, size_t *keyLength,
                  const uint8_t **value, size_t *valueLength);
    // The first record not below target, or false at the end of the tree.
    bool seek(const Tree &tree, Compare compare, const void *target, Cursor *cursor);
    bool next(const Tree &tree, Cursor *cursor);
    bool current(const Tree &tree, const Cursor &cursor, const uint8_t **key, size_t *keyLength,
                 const uint8_t **value, size_t *valueLength);

    int inode(uint64_t id, Item *item);
    bool attribute(uint64_t id, const std::string &name, Attribute *attribute);
    bool readAttribute(const Attribute &attribute, uint64_t limit, std::vector<uint8_t> *value);
    bool extents(uint64_t stream, std::vector<Extent> *extents);
    Directory *directory(uint64_t folderId, int *error);
    int lookup(const std::string &path, Item *item);
    static Attributes attributes(const Item &item);
    int openItem(const Item &item, std::unique_ptr<File> *file);

    DiskImage *m_disk;
    uint64_t m_offset;
    uint64_t m_size;
    uint32_t m_blockSize = 0;
    uint64_t m_totalBlocks = 0;
    uint64_t m_freeBlocks = 0;
    uint64_t m_fileCount = 0;
    uint64_t m_xid = 0;
    bool m_hashedNames = false;
    std::string m_name;
    Item m_root;
    Tree m_containerMap;
    Tree m_volumeMap;
    Tree m_files;
    cutefish::base::LruCache<uint64_t, Node> m_nodes {8 * 1024 * 1024};
    cutefish::base::LruCache<uint64_t, Directory> m_directories {256 * 1024};
};
