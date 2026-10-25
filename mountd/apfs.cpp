// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "apfs.h"

#include "decmpfs.h"

#include "base/i18n.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <sys/stat.h>

using cutefish::i18n::tr;

static constexpr uint32_t kTypeMask = 0xFFFF;
static constexpr uint32_t kContainerType = 0x1;
static constexpr uint32_t kTreeType = 0x2;
static constexpr uint32_t kTreeNodeType = 0x3;
static constexpr uint32_t kObjectMapType = 0xB;
static constexpr uint32_t kVolumeType = 0xD;

static constexpr uint16_t kRootNode = 0x1;
static constexpr uint16_t kLeafNode = 0x2;
static constexpr uint16_t kFixedNode = 0x4;
static constexpr uint16_t kNoHeaderNode = 0x10;

static constexpr uint32_t kPhysicalTree = 0x10;
static constexpr uint32_t kHashedTree = 0x80;
static constexpr uint32_t kNoHeaderTree = 0x100;
static constexpr size_t kTreeInfoSize = 40;
// Enough for any volume a block size allows.
static constexpr uint16_t kMaxLevel = 16;

static constexpr uint32_t kMapDeleted = 0x1;
static constexpr uint32_t kMapEncrypted = 0x4;
static constexpr uint32_t kMapNoHeader = 0x8;

static constexpr uint64_t kFusionContainer = 0x100;
static constexpr uint32_t kNonContiguousCheckpoints = 0x80000000;
static constexpr uint32_t kMaxCheckpointBlocks = 4096;

static constexpr uint64_t kUnencryptedVolume = 0x1;
static constexpr uint64_t kCaseInsensitive = 0x1;
static constexpr uint64_t kNormalizationInsensitive = 0x8;
// Case and normalization insensitive names, dataless snapshots, rolled keys: all that
// this reads. Incomplete restores and sealed system volumes are not.
static constexpr uint64_t kReadableVolumeFeatures = 0x1 | 0x2 | 0x4 | 0x8;

static constexpr uint64_t kIdMask = 0x0FFFFFFFFFFFFFFF;
static constexpr int kTypeShift = 60;
static constexpr uint8_t kInodeRecord = 3;
static constexpr uint8_t kAttributeRecord = 4;
static constexpr uint8_t kExtentRecord = 8;
static constexpr uint8_t kDirectoryRecord = 9;

static constexpr uint64_t kRootFolderId = 2;

static constexpr size_t kInodeSize = 92;
static constexpr uint8_t kDataStreamField = 8;
static constexpr uint32_t kCompressedFlag = 0x20;

static constexpr uint16_t kAttributeStream = 0x1;
static constexpr uint16_t kAttributeEmbedded = 0x2;

static constexpr uint64_t kExtentLengthMask = 0x00FFFFFFFFFFFFFF;
static constexpr size_t kMaxExtents = 1024 * 1024;
static constexpr size_t kMaxEntries = 1024 * 1024;
static constexpr uint64_t kMaxInline = 64 * 1024 * 1024;
static constexpr uint64_t kMaxLink = 64 * 1024;

static uint16_t le16(const uint8_t *p)
{
    return uint16_t(p[1] << 8 | p[0]);
}

static uint32_t le32(const uint8_t *p)
{
    return uint32_t(p[3]) << 24 | uint32_t(p[2]) << 16 | uint32_t(p[1]) << 8 | p[0];
}

static uint64_t le64(const uint8_t *p)
{
    return uint64_t(le32(p + 4)) << 32 | le32(p);
}

// Fletcher-64 over 32-bit words after the stored value.
static uint64_t checksum(const uint8_t *data, size_t size)
{
    uint64_t low = 0;
    uint64_t high = 0;
    for (size_t at = 8; at + 4 <= size; at += 4) {
        low = (low + le32(data + at)) % 0xFFFFFFFF;
        high = (high + low) % 0xFFFFFFFF;
    }
    const uint64_t first = 0xFFFFFFFF - (low + high) % 0xFFFFFFFF;
    const uint64_t second = 0xFFFFFFFF - (low + first) % 0xFFFFFFFF;
    return second << 32 | first;
}

static bool validObject(const std::vector<uint8_t> &bytes)
{
    return bytes.size() >= 32 && le64(bytes.data()) == checksum(bytes.data(), bytes.size());
}

static timespec unixTime(uint64_t nanoseconds)
{
    timespec time = {};
    time.tv_sec = time_t(nanoseconds / 1000000000);
    time.tv_nsec = long(nanoseconds % 1000000000);
    return time;
}

// Up to the terminating null, which the stored length counts.
static std::string text(const uint8_t *p, size_t length)
{
    const auto *begin = reinterpret_cast<const char *>(p);
    return std::string(begin, std::find(begin, begin + length, '\0'));
}

namespace {

struct MapKey
{
    uint64_t oid;
    uint64_t xid;
};

struct FileKey
{
    uint64_t id;
    uint8_t type;
};

}

static int compareMap(const uint8_t *key, size_t length, const void *target)
{
    if (length < 16)
        return -1;
    const MapKey &wanted = *static_cast<const MapKey *>(target);
    const uint64_t oid = le64(key);
    if (oid != wanted.oid)
        return oid < wanted.oid ? -1 : 1;
    const uint64_t xid = le64(key + 8);
    if (xid != wanted.xid)
        return xid < wanted.xid ? -1 : 1;
    return 0;
}

// Records of an object sort by id, then type; this compares only those.
static int compareFiles(const uint8_t *key, size_t length, const void *target)
{
    if (length < 8)
        return -1;
    const FileKey &wanted = *static_cast<const FileKey *>(target);
    const uint64_t header = le64(key);
    const uint64_t id = header & kIdMask;
    if (id != wanted.id)
        return id < wanted.id ? -1 : 1;
    const uint8_t type = uint8_t(header >> kTypeShift);
    if (type != wanted.type)
        return type < wanted.type ? -1 : 1;
    return 0;
}

class StreamFile : public DiskVolume::File
{
public:
    StreamFile(Apfs *volume, std::vector<Apfs::Extent> extents, uint64_t size)
        : m_volume(volume)
        , m_extents(std::move(extents))
        , m_size(size)
    {
    }

    int read(char *buffer, size_t size, uint64_t offset) override
    {
        if (offset >= m_size)
            return 0;
        size = size_t(std::min<uint64_t>(size, m_size - offset));
        return m_volume->readExtents(m_extents, offset, buffer, size) ? int(size) : -EIO;
    }

private:
    Apfs *m_volume;
    std::vector<Apfs::Extent> m_extents;
    uint64_t m_size;
};

Apfs::Apfs(DiskImage *disk, uint64_t offset, uint64_t size)
    : m_disk(disk)
    , m_offset(offset)
    , m_size(size)
{
}

Apfs::~Apfs() = default;

bool Apfs::open(std::string *message)
{
    const std::string damaged = tr("The volume in the disk image is damaged.");

    uint8_t start[64];
    if (m_size < 4096 || !m_disk->read(m_offset, start, sizeof(start)) || std::memcmp(start + 32, "NXSB", 4) != 0) {
        *message = damaged;
        return false;
    }
    m_blockSize = le32(start + 36);
    if (m_blockSize < 4096 || m_blockSize > 65536 || (m_blockSize & (m_blockSize - 1)) != 0
        || m_blockSize > m_size) {
        *message = damaged;
        return false;
    }

    std::vector<uint8_t> container(m_blockSize);
    if (!m_disk->read(m_offset, container.data(), container.size()) || !validObject(container)) {
        *message = damaged;
        return false;
    }
    m_totalBlocks = le64(container.data() + 40);
    if (m_totalBlocks == 0 || m_totalBlocks > m_size / m_blockSize) {
        *message = damaged;
        return false;
    }

    // Block zero copies the superblock of some checkpoint; the newest one valid is current.
    const uint32_t descriptorBlocks = le32(container.data() + 104);
    const uint64_t descriptorBase = le64(container.data() + 112);
    if (!(descriptorBlocks & kNonContiguousCheckpoints) && descriptorBlocks <= kMaxCheckpointBlocks
        && descriptorBase < m_totalBlocks && descriptorBlocks <= m_totalBlocks - descriptorBase) {
        std::vector<uint8_t> candidate(m_blockSize);
        for (uint32_t i = 0; i < descriptorBlocks; ++i) {
            if (!m_disk->read(m_offset + (descriptorBase + i) * m_blockSize, candidate.data(), candidate.size()))
                continue;
            if (validObject(candidate) && (le32(candidate.data() + 24) & kTypeMask) == kContainerType
                && std::memcmp(candidate.data() + 32, "NXSB", 4) == 0 && le32(candidate.data() + 36) == m_blockSize
                && le64(candidate.data() + 40) == m_totalBlocks
                && le64(candidate.data() + 16) > le64(container.data() + 16))
                container = candidate;
        }
    }
    m_xid = le64(container.data() + 16);

    if (le64(container.data() + 64) & kFusionContainer) {
        *message = tr("The disk image holds no volume that can be opened.");
        return false;
    }

    std::vector<uint8_t> map;
    const uint64_t mapAddress = le64(container.data() + 160);
    if (!readObject(mapAddress, mapAddress, kObjectMapType, &map)
        || !openTree(le64(map.data() + 48), true, nullptr, &m_containerMap)) {
        *message = damaged;
        return false;
    }

    bool encrypted = false;
    bool broken = false;
    const uint32_t volumes = std::min<uint32_t>(le32(container.data() + 180), 100);
    for (uint32_t i = 0; i < volumes; ++i) {
        const uint64_t oid = le64(container.data() + 184 + i * 8);
        if (oid == 0)
            continue;

        uint64_t address = 0;
        std::vector<uint8_t> volume;
        if (!resolve(m_containerMap, oid, &address) || !readObject(address, oid, kVolumeType, &volume)
            || std::memcmp(volume.data() + 32, "APSB", 4) != 0) {
            broken = true;
            continue;
        }

        if (!(le64(volume.data() + 264) & kUnencryptedVolume)) {
            encrypted = true;
            continue;
        }
        const uint64_t features = le64(volume.data() + 56);
        if (features & ~kReadableVolumeFeatures)
            continue;

        m_nodes.clear();
        m_directories.clear();
        m_hashedNames = features & (kCaseInsensitive | kNormalizationInsensitive);

        const uint64_t volumeMapAddress = le64(volume.data() + 128);
        if (!readObject(volumeMapAddress, volumeMapAddress, kObjectMapType, &map)
            || !openTree(le64(map.data() + 48), true, nullptr, &m_volumeMap)
            || !openTree(le64(volume.data() + 136), false, &m_volumeMap, &m_files)
            || inode(kRootFolderId, &m_root) < 0 || !m_root.folder) {
            broken = true;
            continue;
        }

        m_name = text(volume.data() + 704, 256);
        m_fileCount = le64(volume.data() + 184) + le64(volume.data() + 192) + le64(volume.data() + 200)
                + le64(volume.data() + 208);
        const uint64_t allocated = le64(volume.data() + 88);
        m_freeBlocks = allocated < m_totalBlocks ? m_totalBlocks - allocated : 0;
        return true;
    }

    if (broken)
        *message = damaged;
    else if (encrypted)
        *message = tr("Encrypted disk images cannot be opened.");
    else
        *message = tr("The disk image holds no volume that can be opened.");
    return false;
}

bool Apfs::readObject(uint64_t address, uint64_t oid, uint32_t type, std::vector<uint8_t> *bytes)
{
    if (address == 0 || address >= m_totalBlocks)
        return false;
    bytes->resize(m_blockSize);
    if (!m_disk->read(m_offset + address * m_blockSize, bytes->data(), bytes->size()) || !validObject(*bytes))
        return false;
    return le64(bytes->data() + 8) == oid && (type == 0 || (le32(bytes->data() + 24) & kTypeMask) == type);
}

const Apfs::Node *Apfs::cached(uint64_t address)
{
    if (const Node *found = m_nodes.find(address))
        return found;

    if (address == 0 || address >= m_totalBlocks)
        return nullptr;
    auto node = std::make_unique<Node>();
    node->bytes.resize(m_blockSize);
    if (!m_disk->read(m_offset + address * m_blockSize, node->bytes.data(), node->bytes.size())
        || !validObject(node->bytes))
        return nullptr;

    const uint8_t *bytes = node->bytes.data();
    const uint32_t type = le32(bytes + 24) & kTypeMask;
    const uint16_t flags = le16(bytes + 32);
    node->level = le16(bytes + 34);
    node->count = le32(bytes + 36);
    node->fixed = flags & kFixedNode;
    node->table = 56 + size_t(le16(bytes + 40));
    node->keys = node->table + le16(bytes + 42);
    node->valuesEnd = m_blockSize - ((flags & kRootNode) ? kTreeInfoSize : 0);

    const size_t entrySize = node->fixed ? 4 : 8;
    if ((type != kTreeType && type != kTreeNodeType) || (type == kTreeType) != bool(flags & kRootNode)
        || (flags & kNoHeaderNode) || bool(flags & kLeafNode) != (node->level == 0) || node->level >= kMaxLevel
        || node->keys > node->valuesEnd || uint64_t(node->count) * entrySize > node->keys - node->table)
        return nullptr;

    const Node *result = node.get();
    m_nodes.insert(address, std::move(node), m_blockSize);
    return result;
}

const Apfs::Node *Apfs::node(const Tree &tree, uint64_t id, uint64_t *address)
{
    *address = id;
    if (!tree.physical && !resolve(*tree.objectMap, id, address))
        return nullptr;

    const bool known = m_nodes.contains(*address);
    const Node *found = cached(*address);
    if (!found)
        return nullptr;
    // A node read the first time must be the object the parent names.
    if (!known && le64(found->bytes.data() + 8) != id) {
        m_nodes.remove(*address);
        return nullptr;
    }
    return found;
}

bool Apfs::openTree(uint64_t root, bool physical, const Tree *objectMap, Tree *tree)
{
    tree->root = root;
    tree->physical = physical;
    tree->objectMap = objectMap;

    uint64_t address = 0;
    const Node *top = node(*tree, root, &address);
    if (!top || (le32(top->bytes.data() + 24) & kTypeMask) != kTreeType)
        return false;

    const uint8_t *info = top->bytes.data() + m_blockSize - kTreeInfoSize;
    const uint32_t flags = le32(info);
    if (le32(info + 4) != m_blockSize || (flags & (kHashedTree | kNoHeaderTree))
        || bool(flags & kPhysicalTree) != physical)
        return false;

    tree->keySize = le32(info + 8);
    tree->valueSize = le32(info + 12);
    if (top->fixed
        && (tree->keySize == 0 || tree->valueSize == 0 || tree->keySize > m_blockSize || tree->valueSize > m_blockSize))
        return false;
    return true;
}

bool Apfs::resolve(const Tree &objectMap, uint64_t oid, uint64_t *address)
{
    const MapKey target {oid, m_xid};
    const uint8_t *key;
    const uint8_t *value;
    size_t keyLength;
    size_t valueLength;
    if (!seekLast(objectMap, compareMap, &target, &key, &keyLength, &value, &valueLength) || le64(key) != oid
        || valueLength < 16)
        return false;

    const uint32_t flags = le32(value);
    if (flags & (kMapDeleted | kMapEncrypted | kMapNoHeader))
        return false;
    *address = le64(value + 8);
    return *address != 0 && *address < m_totalBlocks;
}

bool Apfs::record(const Tree &tree, const Node &node, uint32_t index, const uint8_t **key, size_t *keyLength,
                  const uint8_t **value, size_t *valueLength)
{
    if (index >= node.count)
        return false;

    const uint8_t *bytes = node.bytes.data();
    size_t keyOffset;
    size_t valueOffset;
    if (node.fixed) {
        const uint8_t *entry = bytes + node.table + size_t(index) * 4;
        keyOffset = le16(entry);
        valueOffset = le16(entry + 2);
        *keyLength = tree.keySize;
        // Index records hold the child's id.
        *valueLength = node.level ? 8 : tree.valueSize;
    } else {
        const uint8_t *entry = bytes + node.table + size_t(index) * 8;
        keyOffset = le16(entry);
        *keyLength = le16(entry + 2);
        valueOffset = le16(entry + 4);
        *valueLength = le16(entry + 6);
    }

    if (node.keys + keyOffset + *keyLength > node.valuesEnd)
        return false;
    *key = bytes + node.keys + keyOffset;

    // A fixed-size record without a value.
    if (node.fixed && valueOffset == 0xFFFF) {
        *value = nullptr;
        *valueLength = 0;
        return true;
    }
    if (valueOffset < *valueLength || valueOffset > node.valuesEnd - node.keys)
        return false;
    *value = bytes + node.valuesEnd - valueOffset;
    return true;
}

bool Apfs::child(const Tree &tree, const Node &node, uint32_t index, uint64_t *id)
{
    const uint8_t *key;
    const uint8_t *value;
    size_t keyLength;
    size_t valueLength;
    if (!record(tree, node, index, &key, &keyLength, &value, &valueLength) || valueLength < 8)
        return false;
    *id = le64(value);
    return true;
}

bool Apfs::seekLast(const Tree &tree, Compare compare, const void *target, const uint8_t **key, size_t *keyLength,
                    const uint8_t **value, size_t *valueLength)
{
    uint64_t id = tree.root;
    int level = -1;
    while (true) {
        uint64_t address = 0;
        const Node *current = node(tree, id, &address);
        if (!current || (level >= 0 && current->level != level))
            return false;

        // The first record above target.
        uint32_t low = 0;
        uint32_t high = current->count;
        while (low < high) {
            const uint32_t middle = low + (high - low) / 2;
            if (!record(tree, *current, middle, key, keyLength, value, valueLength))
                return false;
            if (compare(*key, *keyLength, target) <= 0)
                low = middle + 1;
            else
                high = middle;
        }
        if (low == 0)
            return false;

        if (current->level == 0)
            return record(tree, *current, low - 1, key, keyLength, value, valueLength);
        if (!child(tree, *current, low - 1, &id))
            return false;
        level = current->level - 1;
    }
}

bool Apfs::seek(const Tree &tree, Compare compare, const void *target, Cursor *cursor)
{
    cursor->path.clear();
    cursor->leaves.clear();
    uint64_t id = tree.root;
    int level = -1;
    while (true) {
        uint64_t address = 0;
        const Node *current = node(tree, id, &address);
        if (!current || (level >= 0 && current->level != level))
            return false;

        // The first record not below target.
        uint32_t low = 0;
        uint32_t high = current->count;
        while (low < high) {
            const uint32_t middle = low + (high - low) / 2;
            const uint8_t *key;
            const uint8_t *value;
            size_t keyLength;
            size_t valueLength;
            if (!record(tree, *current, middle, &key, &keyLength, &value, &valueLength))
                return false;
            if (compare(key, keyLength, target) < 0)
                low = middle + 1;
            else
                high = middle;
        }

        if (current->level == 0) {
            cursor->leaves.insert(address);
            if (low < current->count) {
                cursor->path.emplace_back(address, low);
                return true;
            }
            if (current->count == 0)
                return false;
            cursor->path.emplace_back(address, current->count - 1);
            return next(tree, cursor);
        }

        // Records equal to target may begin in the child before the first not below it.
        const uint32_t index = low ? low - 1 : 0;
        cursor->path.emplace_back(address, index);
        if (!child(tree, *current, index, &id))
            return false;
        level = current->level - 1;
    }
}

bool Apfs::next(const Tree &tree, Cursor *cursor)
{
    std::vector<std::pair<uint64_t, uint32_t>> &path = cursor->path;
    if (path.empty())
        return false;

    const Node *leaf = cached(path.back().first);
    if (!leaf)
        return false;
    if (path.back().second + 1 < leaf->count) {
        ++path.back().second;
        return true;
    }

    size_t depth = path.size() - 1;
    while (depth > 0) {
        --depth;
        const Node *parent = cached(path[depth].first);
        if (!parent)
            return false;
        uint32_t &index = path[depth].second;
        if (index + 1 >= parent->count)
            continue;

        ++index;
        uint64_t id = 0;
        if (!child(tree, *parent, index, &id))
            return false;
        int level = parent->level - 1;
        path.resize(depth + 1);
        while (true) {
            uint64_t address = 0;
            const Node *current = node(tree, id, &address);
            if (!current || current->level != level || current->count == 0)
                return false;
            path.emplace_back(address, 0);
            if (level == 0)
                return cursor->leaves.insert(address).second;
            if (!child(tree, *current, 0, &id))
                return false;
            --level;
        }
    }
    return false;
}

bool Apfs::current(const Tree &tree, const Cursor &cursor, const uint8_t **key, size_t *keyLength,
                   const uint8_t **value, size_t *valueLength)
{
    if (cursor.path.empty())
        return false;
    const Node *leaf = cached(cursor.path.back().first);
    return leaf && record(tree, *leaf, cursor.path.back().second, key, keyLength, value, valueLength);
}

int Apfs::inode(uint64_t id, Item *item)
{
    const FileKey target {id, kInodeRecord};
    Cursor cursor;
    const uint8_t *key;
    const uint8_t *value;
    size_t keyLength;
    size_t valueLength;
    if (!seek(m_files, compareFiles, &target, &cursor)
        || !current(m_files, cursor, &key, &keyLength, &value, &valueLength)
        || compareFiles(key, keyLength, &target) != 0)
        return -EIO;
    if (valueLength < kInodeSize)
        return -EIO;

    *item = {};
    item->id = id;
    item->stream = le64(value + 8);
    item->modified = unixTime(le64(value + 24));
    item->changed = unixTime(le64(value + 32));
    item->accessed = unixTime(le64(value + 40));
    const int32_t links = int32_t(le32(value + 56));
    const uint32_t flags = le32(value + 68);
    item->mode = le16(value + 80);

    // Extended fields: a count, then each one's type, flags and size, then their data
    // in that order, each padded to eight bytes.
    if (valueLength >= kInodeSize + 4) {
        const size_t count = le16(value + kInodeSize);
        size_t data = kInodeSize + 4 + count * 4;
        for (size_t i = 0; i < count && data <= valueLength; ++i) {
            const uint8_t *field = value + kInodeSize + 4 + i * 4;
            const size_t size = le16(field + 2);
            if (size > valueLength - data)
                return -EIO;
            if (field[0] == kDataStreamField && size >= 8)
                item->streamSize = le64(value + data);
            data += (size + 7) & ~size_t(7);
        }
    }

    if ((item->mode & S_IFMT) == 0)
        item->mode |= S_IFREG;
    item->folder = S_ISDIR(item->mode);
    item->links = item->folder ? 1 : nlink_t(std::max(1, links));
    item->size = item->folder ? 0 : item->streamSize;

    if (S_ISLNK(item->mode)) {
        Attribute link;
        if (attribute(id, "com.apple.fs.symlink", &link) && !link.stream)
            item->size = text(link.data.data(), link.data.size()).size();
    } else if (!item->folder && (flags & kCompressedFlag)) {
        Attribute compression;
        std::vector<uint8_t> data;
        Decmpfs header;
        if (attribute(id, "com.apple.decmpfs", &compression) && readAttribute(compression, kMaxInline, &data)
            && parseDecmpfs(data, &header)) {
            item->compressed = true;
            item->size = header.size;
        }
    }
    return 0;
}

bool Apfs::attribute(uint64_t id, const std::string &name, Attribute *attribute)
{
    const FileKey target {id, kAttributeRecord};
    Cursor cursor;
    if (!seek(m_files, compareFiles, &target, &cursor))
        return false;

    while (true) {
        const uint8_t *key;
        const uint8_t *value;
        size_t keyLength;
        size_t valueLength;
        if (!current(m_files, cursor, &key, &keyLength, &value, &valueLength)
            || compareFiles(key, keyLength, &target) != 0 || keyLength < 10)
            return false;

        const size_t nameLength = le16(key + 8);
        if (nameLength <= keyLength - 10 && text(key + 10, nameLength) == name) {
            if (valueLength < 4)
                return false;
            const uint16_t flags = le16(value);
            const size_t length = le16(value + 2);
            if (length > valueLength - 4)
                return false;
            *attribute = {};
            if (flags & kAttributeEmbedded) {
                attribute->data.assign(value + 4, value + 4 + length);
                attribute->size = length;
                return true;
            }
            if ((flags & kAttributeStream) && length >= 16) {
                attribute->stream = true;
                attribute->streamId = le64(value + 4);
                attribute->size = le64(value + 12);
                return true;
            }
            return false;
        }

        if (!next(m_files, &cursor))
            return false;
    }
}

bool Apfs::readAttribute(const Attribute &attribute, uint64_t limit, std::vector<uint8_t> *value)
{
    if (attribute.size > limit)
        return false;
    if (!attribute.stream) {
        *value = attribute.data;
        return true;
    }

    std::vector<Extent> found;
    if (!extents(attribute.streamId, &found))
        return false;
    value->resize(size_t(attribute.size));
    return readExtents(found, 0, value->data(), value->size());
}

bool Apfs::extents(uint64_t stream, std::vector<Extent> *extents)
{
    extents->clear();
    const FileKey target {stream, kExtentRecord};
    Cursor cursor;
    if (!seek(m_files, compareFiles, &target, &cursor))
        return true;

    uint64_t end = 0;
    while (true) {
        const uint8_t *key;
        const uint8_t *value;
        size_t keyLength;
        size_t valueLength;
        if (!current(m_files, cursor, &key, &keyLength, &value, &valueLength))
            return false;
        if (compareFiles(key, keyLength, &target) != 0)
            return true;
        if (keyLength < 16 || valueLength < 16 || extents->size() >= kMaxExtents)
            return false;

        Extent extent;
        extent.logical = le64(key + 8);
        extent.length = le64(value) & kExtentLengthMask;
        extent.physical = le64(value + 8);
        const uint64_t blocks = extent.length / m_blockSize;
        if (extent.logical < end || extent.length == 0 || extent.length % m_blockSize != 0
            || (extent.physical && (extent.physical >= m_totalBlocks || blocks > m_totalBlocks - extent.physical)))
            return false;
        extents->push_back(extent);
        end = extent.logical + extent.length;

        if (!next(m_files, &cursor))
            return true;
    }
}

bool Apfs::readExtents(const std::vector<Extent> &extents, uint64_t offset, void *buffer, size_t size)
{
    auto *out = static_cast<uint8_t *>(buffer);
    // The first extent ending past offset.
    auto at = std::upper_bound(extents.begin(), extents.end(), offset, [](uint64_t wanted, const Extent &extent) {
        return wanted < extent.logical + extent.length;
    });

    while (size > 0) {
        size_t part;
        if (at == extents.end() || offset < at->logical) {
            part = at == extents.end() ? size : size_t(std::min<uint64_t>(size, at->logical - offset));
            std::memset(out, 0, part);
        } else {
            const uint64_t within = offset - at->logical;
            part = size_t(std::min<uint64_t>(size, at->length - within));
            if (at->physical == 0)
                std::memset(out, 0, part);
            else if (!m_disk->read(m_offset + at->physical * m_blockSize + within, out, part))
                return false;
            ++at;
        }
        out += part;
        offset += part;
        size -= part;
    }
    return true;
}

Apfs::Directory *Apfs::directory(uint64_t folderId, int *error)
{
    if (Directory *found = m_directories.find(folderId))
        return found;

    // Names and ids first: reading each inode moves the cursor's nodes out of the cache.
    std::vector<std::pair<std::string, uint64_t>> raw;
    const FileKey target {folderId, kDirectoryRecord};
    Cursor cursor;
    for (bool more = seek(m_files, compareFiles, &target, &cursor); more; more = next(m_files, &cursor)) {
        const uint8_t *key;
        const uint8_t *value;
        size_t keyLength;
        size_t valueLength;
        if (!current(m_files, cursor, &key, &keyLength, &value, &valueLength)) {
            *error = -EIO;
            return nullptr;
        }
        if (compareFiles(key, keyLength, &target) != 0)
            break;

        // Hashed keys keep the length in the low ten bits beside the name's hash.
        const size_t nameStart = m_hashedNames ? 12 : 10;
        if (keyLength < nameStart || valueLength < 8 || raw.size() >= kMaxEntries) {
            *error = -EIO;
            return nullptr;
        }
        const size_t nameLength = m_hashedNames ? (le32(key + 8) & 0x3FF) : le16(key + 8);
        if (nameLength > keyLength - nameStart) {
            *error = -EIO;
            return nullptr;
        }

        std::string name = text(key + nameStart, nameLength);
        std::replace(name.begin(), name.end(), '/', ':');
        if (name.empty() || name == "." || name == "..")
            continue;
        raw.emplace_back(std::move(name), le64(value));
    }

    auto created = std::make_unique<Directory>();
    for (auto &[name, id] : raw) {
        Item item;
        if (const int result = inode(id, &item); result < 0) {
            *error = result;
            return nullptr;
        }
        created->byName.emplace(name, created->entries.size());
        created->entries.emplace_back(std::move(name), item);
    }

    Directory *result = created.get();
    const size_t cost = result->entries.size() + 1;
    if (!m_directories.insert(folderId, std::move(created), cost)) {
        *error = -ENOMEM;
        return nullptr;
    }
    return result;
}

int Apfs::lookup(const std::string &path, Item *item)
{
    Item current = m_root;
    size_t at = 0;
    while (at < path.size()) {
        const size_t slash = path.find('/', at);
        const size_t end = slash == std::string::npos ? path.size() : slash;
        const std::string name = path.substr(at, end - at);
        at = end + 1;
        if (name.empty())
            continue;
        if (!current.folder)
            return -ENOTDIR;

        int error = 0;
        Directory *folder = directory(current.id, &error);
        if (!folder)
            return error;
        const auto found = folder->byName.find(name);
        if (found == folder->byName.end())
            return -ENOENT;
        current = folder->entries[found->second].second;
    }
    *item = current;
    return 0;
}

DiskVolume::Attributes Apfs::attributes(const Item &item)
{
    Attributes result;
    result.id = item.id;
    result.mode = item.mode;
    result.links = item.links;
    result.size = item.size;
    result.modified = item.modified;
    result.changed = item.changed;
    result.accessed = item.accessed;
    return result;
}

int Apfs::stat(const std::string &path, Attributes *attributes)
{
    Item item;
    if (const int error = lookup(path, &item); error < 0)
        return error;
    *attributes = Apfs::attributes(item);
    return 0;
}

int Apfs::list(const std::string &path, std::vector<std::pair<std::string, Attributes>> *entries)
{
    Item folder;
    if (const int error = lookup(path, &folder); error < 0)
        return error;
    if (!folder.folder)
        return -ENOTDIR;

    int error = 0;
    Directory *found = directory(folder.id, &error);
    if (!found)
        return error;
    entries->reserve(found->entries.size());
    for (const auto &[name, item] : found->entries)
        entries->emplace_back(name, attributes(item));
    return 0;
}

int Apfs::openItem(const Item &item, std::unique_ptr<File> *file)
{
    if (item.folder)
        return -EISDIR;

    if (!item.compressed) {
        std::vector<Extent> found;
        if (!extents(item.stream, &found))
            return -EIO;
        *file = std::make_unique<StreamFile>(this, std::move(found), item.streamSize);
        return 0;
    }

    Attribute compression;
    std::vector<uint8_t> value;
    Decmpfs header;
    if (!attribute(item.id, "com.apple.decmpfs", &compression) || !readAttribute(compression, kMaxInline, &value)
        || !parseDecmpfs(value, &header))
        return -EIO;

    Attribute resource;
    std::vector<Extent> resourceExtents;
    if (!attribute(item.id, "com.apple.ResourceFork", &resource))
        resource = {};
    else if (resource.stream && !extents(resource.streamId, &resourceExtents))
        return -EIO;

    const auto readResource = [this, resource, resourceExtents](uint64_t offset, void *buffer, size_t size) {
        if (offset > resource.size || size > resource.size - offset)
            return false;
        if (resource.stream)
            return readExtents(resourceExtents, offset, buffer, size);
        std::memcpy(buffer, resource.data.data() + offset, size);
        return true;
    };
    return openDecmpfs(header, resource.size, readResource, file);
}

int Apfs::openFile(const std::string &path, std::unique_ptr<File> *file)
{
    Item item;
    if (const int error = lookup(path, &item); error < 0)
        return error;
    return openItem(item, file);
}

int Apfs::readLink(const std::string &path, std::string *target)
{
    Item item;
    if (const int error = lookup(path, &item); error < 0)
        return error;
    if (!S_ISLNK(item.mode))
        return -EINVAL;

    Attribute link;
    std::vector<uint8_t> value;
    if (!attribute(item.id, "com.apple.fs.symlink", &link) || !readAttribute(link, kMaxLink, &value))
        return -EIO;
    *target = text(value.data(), value.size());
    return 0;
}
