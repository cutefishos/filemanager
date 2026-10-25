// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "hfsplus.h"

#include "decmpfs.h"

#include "base/i18n.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <sys/stat.h>
#include <unordered_set>
#include <unicode/normalizer2.h>
#include <unicode/unistr.h>

using cutefish::i18n::tr;

static constexpr uint32_t kRootParentId = 1;
static constexpr uint32_t kRootFolderId = 2;
static constexpr uint32_t kExtentsFileId = 3;
static constexpr uint32_t kCatalogFileId = 4;
static constexpr uint32_t kAttributesFileId = 8;

static constexpr int16_t kFolderRecord = 1;
static constexpr int16_t kFileRecord = 2;

static constexpr uint8_t kDataFork = 0x00;
static constexpr uint8_t kResourceFork = 0xFF;

static constexpr int8_t kIndexNode = 0;
static constexpr int8_t kLeafNode = -1;

static constexpr uint32_t kVariableIndexKeys = 0x4;
static constexpr uint8_t kCompressedFlag = 0x20;

// Seconds from 1904-01-01 to 1970-01-01.
static constexpr int64_t kEpochOffset = 2082844800;

static constexpr uint64_t kMaxInline = 64 * 1024 * 1024;

static constexpr const char16_t *kFileLinksFolder = u"\0\0\0\0HFS+ Private Data";
static constexpr size_t kFileLinksFolderLength = 21;
static constexpr const char16_t *kFolderLinksFolder = u".HFS+ Private Directory Data\r";

static uint16_t be16(const uint8_t *p)
{
    return uint16_t(p[0] << 8 | p[1]);
}

static uint32_t be32(const uint8_t *p)
{
    return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
}

static uint64_t be64(const uint8_t *p)
{
    return uint64_t(be32(p)) << 32 | be32(p + 4);
}

static constexpr uint32_t fourCc(const char (&code)[5])
{
    return uint32_t(uint8_t(code[0])) << 24 | uint32_t(uint8_t(code[1])) << 16 | uint32_t(uint8_t(code[2])) << 8
            | uint8_t(code[3]);
}

static time_t unixTime(uint32_t hfsTime)
{
    return time_t(int64_t(hfsTime) - kEpochOffset);
}

static HfsPlus::Fork parseFork(const uint8_t *p)
{
    HfsPlus::Fork fork;
    fork.logicalSize = be64(p);
    fork.totalBlocks = be32(p + 12);
    for (int i = 0; i < 8; ++i) {
        const HfsPlus::Extent extent {be32(p + 16 + i * 8), be32(p + 20 + i * 8)};
        if (extent.count)
            fork.extents.push_back(extent);
    }
    return fork;
}

static std::u16string unicodeName(const uint8_t *p, size_t length)
{
    std::u16string name;
    name.reserve(length);
    for (size_t i = 0; i < length; ++i)
        name.push_back(char16_t(be16(p + i * 2)));
    return name;
}

// The catalog keeps the separator the user typed; ':' takes its place in a path. Names
// are stored decomposed and read composed, as everything else here writes them; the
// characters the volume's decomposition leaves alone, such as U+2126, stay as they are.
static std::string pathName(std::u16string name)
{
    std::replace(name.begin(), name.end(), u'/', u':');
    const icu::UnicodeString text(false, name.data(), int32_t(name.size()));

    UErrorCode status = U_ZERO_ERROR;
    const icu::Normalizer2 *nfc = icu::Normalizer2::getNFCInstance(status);
    icu::UnicodeString composed;
    int32_t segment = 0;
    const auto compose = [&](int32_t end) {
        icu::UnicodeString part(text, segment, end - segment);
        if (nfc && U_SUCCESS(status))
            part = nfc->normalize(part, status);
        composed += part;
    };
    for (int32_t at = 0; nfc && U_SUCCESS(status) && at < text.length();) {
        const UChar32 c = text.char32At(at);
        const int32_t next = text.moveIndex32(at, 1);
        if (!nfc->isNormalized(icu::UnicodeString(c), status)) {
            compose(at);
            composed += c;
            segment = next;
        }
        at = next;
    }
    compose(text.length());
    if (U_FAILURE(status))
        composed = text;

    std::string result;
    composed.toUTF8String(result);
    return result;
}

namespace {

struct ExtentKey
{
    uint32_t fileId;
    uint8_t forkType;
    uint32_t start;
};

}

static int compareCatalog(const uint8_t *key, size_t length, const void *target)
{
    if (length < 8)
        return -1;
    const uint32_t parent = be32(key + 2);
    const uint32_t wanted = *static_cast<const uint32_t *>(target);
    if (parent != wanted)
        return parent < wanted ? -1 : 1;
    return be16(key + 6) == 0 ? 0 : 1;
}

static int compareExtents(const uint8_t *key, size_t length, const void *target)
{
    if (length < 12)
        return -1;
    const ExtentKey &wanted = *static_cast<const ExtentKey *>(target);
    const uint32_t fileId = be32(key + 4);
    if (fileId != wanted.fileId)
        return fileId < wanted.fileId ? -1 : 1;
    if (key[2] != wanted.forkType)
        return key[2] < wanted.forkType ? -1 : 1;
    const uint32_t start = be32(key + 8);
    if (start != wanted.start)
        return start < wanted.start ? -1 : 1;
    return 0;
}

static int compareAttributes(const uint8_t *key, size_t length, const void *target)
{
    if (length < 14)
        return -1;
    const uint32_t fileId = be32(key + 4);
    const uint32_t wanted = *static_cast<const uint32_t *>(target);
    if (fileId != wanted)
        return fileId < wanted ? -1 : 1;
    return be16(key + 12) == 0 && be32(key + 8) == 0 ? 0 : 1;
}

class PlainFile : public HfsPlus::File
{
public:
    PlainFile(HfsPlus *volume, uint32_t fileId, HfsPlus::Fork fork)
        : m_volume(volume)
        , m_fileId(fileId)
        , m_fork(std::move(fork))
    {
    }

    int read(char *buffer, size_t size, uint64_t offset) override
    {
        if (offset >= m_fork.logicalSize)
            return 0;
        size = size_t(std::min<uint64_t>(size, m_fork.logicalSize - offset));
        return m_volume->readFork(m_fileId, kDataFork, m_fork, offset, buffer, size) ? int(size) : -EIO;
    }

private:
    HfsPlus *m_volume;
    uint32_t m_fileId;
    HfsPlus::Fork m_fork;
};

HfsPlus::HfsPlus(DiskImage *disk, uint64_t offset, uint64_t size)
    : m_disk(disk)
    , m_offset(offset)
    , m_size(size)
{
}

HfsPlus::~HfsPlus() = default;

bool HfsPlus::open(std::string *message)
{
    const std::string damaged = tr("The volume in the disk image is damaged.");

    uint8_t header[512];
    if (!m_disk->read(m_offset + 1024, header, sizeof(header))) {
        *message = damaged;
        return false;
    }

    const uint16_t signature = be16(header);
    if (signature != 0x482B && signature != 0x4858) {
        *message = damaged;
        return false;
    }

    m_fileCount = uint64_t(be32(header + 32)) + be32(header + 36);
    m_blockSize = be32(header + 40);
    m_totalBlocks = be32(header + 44);
    m_freeBlocks = be32(header + 48);
    if (m_blockSize < 512 || m_blockSize > 1024 * 1024 || (m_blockSize & (m_blockSize - 1)) != 0
        || m_totalBlocks * m_blockSize > m_size || m_freeBlocks > m_totalBlocks) {
        *message = damaged;
        return false;
    }

    // The extents tree first: the others may continue in it.
    if (!openTree(&m_extents, kExtentsFileId, parseFork(header + 192))
        || !openTree(&m_catalog, kCatalogFileId, parseFork(header + 272))) {
        *message = damaged;
        return false;
    }
    const Fork attributes = parseFork(header + 352);
    m_hasAttributes = attributes.totalBlocks > 0 && openTree(&m_attributes, kAttributesFileId, attributes);

    std::vector<std::pair<std::string, Item>> top;
    if (listRaw(kRootParentId, &top) < 0) {
        *message = damaged;
        return false;
    }
    for (const auto &[name, item] : top) {
        if (item.folder && item.id == kRootFolderId) {
            m_root = item;
            m_name = name;
        }
    }
    if (m_root.id != kRootFolderId) {
        *message = damaged;
        return false;
    }

    const std::string fileLinks = pathName(std::u16string(kFileLinksFolder, kFileLinksFolderLength));
    const std::string folderLinks = pathName(kFolderLinksFolder);
    std::vector<std::pair<std::string, Item>> root;
    if (listRaw(kRootFolderId, &root) < 0) {
        *message = damaged;
        return false;
    }
    for (const auto &[name, item] : root) {
        if (item.folder && name == fileLinks)
            m_fileLinks = item.id;
        else if (item.folder && name == folderLinks)
            m_folderLinks = item.id;
    }
    return true;
}

bool HfsPlus::openTree(Tree *tree, uint32_t fileId, const Fork &fork)
{
    std::vector<Extent> extents;
    if (!resolve(fileId, kDataFork, fork, &extents))
        return false;

    tree->fileId = fileId;
    tree->fork = fork;
    tree->fork.extents = extents;
    tree->fork.totalBlocks = 0;
    for (const Extent &extent : extents)
        tree->fork.totalBlocks += extent.count;

    uint8_t header[512];
    if (fork.logicalSize < sizeof(header) || !readFork(fileId, kDataFork, tree->fork, 0, header, sizeof(header)))
        return false;
    if (int8_t(header[8]) != 1)
        return false;

    tree->root = be32(header + 16);
    tree->nodeSize = be16(header + 32);
    tree->maxKeyLength = be16(header + 34);
    tree->variableIndexKeys = be32(header + 52) & kVariableIndexKeys;
    const uint32_t nodeSize = tree->nodeSize;
    return nodeSize >= 512 && nodeSize <= 32768 && (nodeSize & (nodeSize - 1)) == 0;
}

bool HfsPlus::resolve(uint32_t fileId, uint8_t forkType, const Fork &fork, std::vector<Extent> *extents)
{
    *extents = fork.extents;
    uint64_t blocks = 0;
    for (const Extent &extent : *extents)
        blocks += extent.count;

    while (blocks < fork.totalBlocks) {
        // The extents tree lists its own blocks in its header only.
        if (fileId == kExtentsFileId || m_extents.root == 0)
            return false;

        const ExtentKey key {fileId, forkType, uint32_t(blocks)};
        Position position;
        const uint8_t *recordKey;
        const uint8_t *data;
        size_t keyLength;
        size_t dataLength;
        if (!seek(m_extents, compareExtents, &key, &position)
            || !record(m_extents, position, &recordKey, &keyLength, &data, &dataLength)
            || compareExtents(recordKey, keyLength, &key) != 0 || dataLength < 64)
            return false;

        const uint64_t before = blocks;
        for (int i = 0; i < 8; ++i) {
            const Extent extent {be32(data + i * 8), be32(data + 4 + i * 8)};
            if (!extent.count)
                break;
            extents->push_back(extent);
            blocks += extent.count;
        }
        if (blocks == before)
            return false;
    }
    return true;
}

bool HfsPlus::readFork(uint32_t fileId, uint8_t forkType, const Fork &fork, uint64_t offset, void *buffer,
                       size_t size)
{
    if (offset > fork.logicalSize || size > fork.logicalSize - offset)
        return false;

    std::vector<Extent> resolved;
    const std::vector<Extent> *extents = &fork.extents;
    uint64_t blocks = 0;
    for (const Extent &extent : fork.extents)
        blocks += extent.count;
    if (blocks < fork.totalBlocks) {
        if (!resolve(fileId, forkType, fork, &resolved))
            return false;
        extents = &resolved;
    }

    auto *out = static_cast<uint8_t *>(buffer);
    uint64_t extentStart = 0;
    for (const Extent &extent : *extents) {
        if (size == 0)
            break;
        const uint64_t extentBytes = uint64_t(extent.count) * m_blockSize;
        if (offset < extentStart + extentBytes) {
            const uint64_t within = offset - extentStart;
            const size_t part = size_t(std::min<uint64_t>(size, extentBytes - within));
            const uint64_t at = uint64_t(extent.start) * m_blockSize + within;
            if (at + part > m_totalBlocks * m_blockSize || !m_disk->read(m_offset + at, out, part))
                return false;
            out += part;
            offset += part;
            size -= part;
        }
        extentStart += extentBytes;
    }
    return size == 0;
}

const std::vector<uint8_t> *HfsPlus::node(const Tree &tree, uint32_t index)
{
    const uint64_t key = uint64_t(tree.fileId) << 32 | index;
    if (const std::vector<uint8_t> *cached = m_nodes.find(key))
        return cached;

    auto data = std::make_unique<std::vector<uint8_t>>(tree.nodeSize);
    if (!readFork(tree.fileId, kDataFork, tree.fork, uint64_t(index) * tree.nodeSize, data->data(), data->size()))
        return nullptr;

    // Record offsets run back from the node's end, one more than the records, rising.
    const uint8_t *bytes = data->data();
    const uint16_t count = be16(bytes + 10);
    if (size_t(count + 1) * 2 + 14 > tree.nodeSize)
        return nullptr;
    uint16_t previous = 14;
    for (uint16_t i = 0; i <= count; ++i) {
        const uint16_t offset = be16(bytes + tree.nodeSize - 2 * (i + 1));
        if (offset < previous || offset > tree.nodeSize - 2 * (count + 1))
            return nullptr;
        previous = offset;
    }

    std::vector<uint8_t> *result = data.get();
    m_nodes.insert(key, std::move(data), tree.nodeSize);
    return result;
}

bool HfsPlus::record(const Tree &tree, const Position &position, const uint8_t **key, size_t *keyLength,
                     const uint8_t **data, size_t *dataLength)
{
    const std::vector<uint8_t> *bytes = node(tree, position.node);
    if (!bytes || position.record >= be16(bytes->data() + 10))
        return false;

    const uint8_t *base = bytes->data();
    const uint16_t begin = be16(base + tree.nodeSize - 2 * (position.record + 1));
    const uint16_t end = be16(base + tree.nodeSize - 2 * (position.record + 2));
    if (end - begin < 2)
        return false;

    const size_t length = be16(base + begin);
    size_t dataStart = 2 + length;
    if (int8_t(base[8]) == kIndexNode && !tree.variableIndexKeys)
        dataStart = 2 + tree.maxKeyLength;
    dataStart += dataStart & 1;
    if (2 + length > size_t(end - begin) || dataStart > size_t(end - begin))
        return false;

    *key = base + begin;
    *keyLength = 2 + length;
    *data = base + begin + dataStart;
    *dataLength = size_t(end - begin) - dataStart;
    return true;
}

bool HfsPlus::seek(const Tree &tree, Compare compare, const void *target, Position *position)
{
    uint32_t index = tree.root;
    if (index == 0)
        return false;

    for (int depth = 0; depth < 16; ++depth) {
        const std::vector<uint8_t> *bytes = node(tree, index);
        if (!bytes)
            return false;
        const int8_t kind = int8_t((*bytes)[8]);
        const uint16_t count = be16(bytes->data() + 10);

        if (kind == kIndexNode) {
            uint32_t child = 0;
            bool found = false;
            for (uint16_t i = 0; i < count; ++i) {
                const uint8_t *key;
                const uint8_t *data;
                size_t keyLength;
                size_t dataLength;
                if (!record(tree, {index, i}, &key, &keyLength, &data, &dataLength) || dataLength < 4)
                    return false;
                if (found && compare(key, keyLength, target) > 0)
                    break;
                child = be32(data);
                found = true;
            }
            if (!found)
                return false;
            index = child;
            continue;
        }

        if (kind != kLeafNode)
            return false;

        for (uint16_t i = 0; i < count; ++i) {
            const uint8_t *key;
            const uint8_t *data;
            size_t keyLength;
            size_t dataLength;
            if (!record(tree, {index, i}, &key, &keyLength, &data, &dataLength))
                return false;
            if (compare(key, keyLength, target) >= 0) {
                *position = {index, i};
                return true;
            }
        }
        *position = {index, uint16_t(count ? count - 1 : 0)};
        return count && next(tree, position);
    }
    return false;
}

bool HfsPlus::next(const Tree &tree, Position *position)
{
    const std::vector<uint8_t> *bytes = node(tree, position->node);
    if (!bytes)
        return false;
    if (position->record + 1u < be16(bytes->data() + 10)) {
        ++position->record;
        return true;
    }

    // Empty leaves are passed over, a bounded number of them.
    for (int skipped = 0; skipped < 64; ++skipped) {
        const uint32_t following = be32(bytes->data());
        if (following == 0 || following == position->node)
            return false;
        bytes = node(tree, following);
        if (!bytes || int8_t((*bytes)[8]) != kLeafNode)
            return false;
        *position = {following, 0};
        if (be16(bytes->data() + 10) > 0)
            return true;
    }
    return false;
}

bool HfsPlus::attribute(uint32_t fileId, const std::u16string &name, std::vector<uint8_t> *value)
{
    if (!m_hasAttributes)
        return false;

    Position position;
    if (!seek(m_attributes, compareAttributes, &fileId, &position))
        return false;

    std::unordered_set<uint32_t> visited {position.node};
    while (true) {
        const uint8_t *key;
        const uint8_t *data;
        size_t keyLength;
        size_t dataLength;
        if (!record(m_attributes, position, &key, &keyLength, &data, &dataLength) || keyLength < 14
            || be32(key + 4) != fileId)
            return false;

        const size_t nameLength = be16(key + 12);
        if (14 + nameLength * 2 <= keyLength && unicodeName(key + 14, nameLength) == name) {
            if (dataLength < 16)
                return false;
            const uint32_t type = be32(data);
            if (type == 0x10) {
                const uint32_t size = be32(data + 12);
                if (size > dataLength - 16)
                    return false;
                value->assign(data + 16, data + 16 + size);
                return true;
            }
            if (type == 0x20 && dataLength >= 88) {
                const Fork fork = parseFork(data + 8);
                if (fork.logicalSize > kMaxInline)
                    return false;
                value->resize(size_t(fork.logicalSize));
                return readFork(fileId, kDataFork, fork, 0, value->data(), value->size());
            }
            return false;
        }

        // A damaged tree's leaves may lead back to one already walked.
        const uint32_t previous = position.node;
        if (!next(m_attributes, &position) || (position.node != previous && !visited.insert(position.node).second))
            return false;
    }
}

int HfsPlus::finish(Item *item, const uint8_t *data, size_t length)
{
    const int16_t type = int16_t(be16(data));
    if ((type == kFolderRecord && length < 88) || (type == kFileRecord && length < 248))
        return -EIO;

    item->folder = type == kFolderRecord;
    item->id = be32(data + 8);
    item->created = unixTime(be32(data + 12));
    item->modified = unixTime(be32(data + 16));
    item->changed = unixTime(be32(data + 20));
    item->accessed = unixTime(be32(data + 24));
    item->special = be32(data + 44);
    item->mode = be16(data + 42);
    if ((item->mode & S_IFMT) == 0)
        item->mode = item->folder ? S_IFDIR | 0755 : S_IFREG | 0644;

    if (item->folder)
        return 0;

    item->fileType = be32(data + 48);
    item->creator = be32(data + 52);
    item->data = parseFork(data + 88);
    item->resource = parseFork(data + 168);
    item->size = item->data.logicalSize;

    if (data[41] & kCompressedFlag) {
        std::vector<uint8_t> value;
        Decmpfs header;
        if (attribute(item->id, u"com.apple.decmpfs", &value) && parseDecmpfs(value, &header)) {
            item->compressed = true;
            item->size = header.size;
        }
    }
    return 0;
}

int HfsPlus::listRaw(uint32_t folderId, std::vector<std::pair<std::string, Item>> *entries)
{
    Position position;
    if (!seek(m_catalog, compareCatalog, &folderId, &position))
        return 0;

    std::unordered_set<uint32_t> visited {position.node};
    while (true) {
        const uint8_t *key;
        const uint8_t *data;
        size_t keyLength;
        size_t dataLength;
        if (!record(m_catalog, position, &key, &keyLength, &data, &dataLength) || keyLength < 8)
            return -EIO;
        if (be32(key + 2) != folderId)
            return 0;

        const int16_t type = dataLength >= 2 ? int16_t(be16(data)) : 0;
        if (type == kFolderRecord || type == kFileRecord) {
            const size_t nameLength = be16(key + 6);
            if (8 + nameLength * 2 > keyLength)
                return -EIO;
            Item item;
            if (const int error = finish(&item, data, dataLength); error < 0)
                return error;
            entries->emplace_back(pathName(unicodeName(key + 8, nameLength)), std::move(item));
        }

        const uint32_t previous = position.node;
        if (!next(m_catalog, &position))
            return 0;
        if (position.node != previous && !visited.insert(position.node).second)
            return -EIO;
    }
}

HfsPlus::Directory *HfsPlus::directory(uint32_t folderId, int *error)
{
    if (Directory *cached = m_directories.find(folderId))
        return cached;

    std::vector<std::pair<std::string, Item>> raw;
    if (const int result = listRaw(folderId, &raw); result < 0) {
        *error = result;
        return nullptr;
    }

    const bool privateFolder = folderId == m_fileLinks || folderId == m_folderLinks;
    auto created = std::make_unique<Directory>();
    for (auto &[name, item] : raw) {
        if (item.folder && (item.id == m_fileLinks || item.id == m_folderLinks))
            continue;

        // A hard link names the file it shares in a private folder.
        if (!privateFolder && !item.folder) {
            Item target;
            if (item.fileType == fourCc("hlnk") && item.creator == fourCc("hfs+") && m_fileLinks
                && child(m_fileLinks, "iNode" + std::to_string(item.special), &target) == 0 && !target.folder) {
                target.links = std::max<nlink_t>(1, target.special);
                item = target;
            } else if (item.fileType == fourCc("fdrp") && item.creator == fourCc("MACS") && m_folderLinks
                       && child(m_folderLinks, "dir_" + std::to_string(item.special), &target) == 0
                       && target.folder) {
                item = target;
            }
        }

        created->byName.emplace(name, created->entries.size());
        created->entries.emplace_back(name, std::move(item));
    }

    Directory *result = created.get();
    const size_t cost = result->entries.size() + 1;
    if (!m_directories.insert(folderId, std::move(created), cost)) {
        *error = -ENOMEM;
        return nullptr;
    }
    return result;
}

int HfsPlus::child(uint32_t folderId, const std::string &name, Item *item)
{
    int error = 0;
    Directory *folder = directory(folderId, &error);
    if (!folder)
        return error;
    const auto found = folder->byName.find(name);
    if (found == folder->byName.end())
        return -ENOENT;
    *item = folder->entries[found->second].second;
    return 0;
}

int HfsPlus::lookup(const std::string &path, Item *item)
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
        if (const int error = child(current.id, name, &current); error < 0)
            return error;
    }
    *item = current;
    return 0;
}

int HfsPlus::list(const Item &folder, std::vector<std::pair<std::string, Item>> *entries)
{
    if (!folder.folder)
        return -ENOTDIR;
    int error = 0;
    Directory *found = directory(folder.id, &error);
    if (!found)
        return error;
    *entries = found->entries;
    return 0;
}

int HfsPlus::openFile(const Item &item, std::unique_ptr<File> *file)
{
    if (item.folder)
        return -EISDIR;
    if (!item.compressed) {
        *file = std::make_unique<PlainFile>(this, item.id, item.data);
        return 0;
    }

    std::vector<uint8_t> value;
    Decmpfs header;
    if (!attribute(item.id, u"com.apple.decmpfs", &value) || !parseDecmpfs(value, &header))
        return -EIO;

    const uint32_t id = item.id;
    const Fork resource = item.resource;
    const auto readResource = [this, id, resource](uint64_t offset, void *buffer, size_t size) {
        return readFork(id, kResourceFork, resource, offset, buffer, size);
    };
    return openDecmpfs(header, resource.logicalSize, readResource, file);
}

int HfsPlus::readLink(const Item &item, std::string *target)
{
    if (!S_ISLNK(item.mode))
        return -EINVAL;

    std::unique_ptr<File> file;
    if (const int error = openFile(item, &file); error < 0)
        return error;
    if (item.size > 64 * 1024)
        return -EIO;
    target->resize(size_t(item.size));
    const int got = file->read(target->data(), target->size(), 0);
    if (got < 0)
        return got;
    target->resize(size_t(got));
    return 0;
}

static DiskVolume::Attributes attributes(const HfsPlus::Item &item)
{
    DiskVolume::Attributes result;
    result.id = item.id;
    result.mode = item.mode;
    result.links = item.links;
    result.size = item.folder ? 0 : item.size;
    result.modified.tv_sec = item.modified;
    result.changed.tv_sec = item.changed;
    result.accessed.tv_sec = item.accessed;
    return result;
}

int HfsPlus::stat(const std::string &path, Attributes *attributes)
{
    Item item;
    if (const int error = lookup(path, &item); error < 0)
        return error;
    *attributes = ::attributes(item);
    return 0;
}

int HfsPlus::list(const std::string &path, std::vector<std::pair<std::string, Attributes>> *entries)
{
    Item folder;
    if (const int error = lookup(path, &folder); error < 0)
        return error;

    std::vector<std::pair<std::string, Item>> children;
    if (const int error = list(folder, &children); error < 0)
        return error;

    entries->reserve(children.size());
    for (const auto &[name, item] : children)
        entries->emplace_back(name, ::attributes(item));
    return 0;
}

int HfsPlus::openFile(const std::string &path, std::unique_ptr<File> *file)
{
    Item item;
    if (const int error = lookup(path, &item); error < 0)
        return error;
    return openFile(item, file);
}

int HfsPlus::readLink(const std::string &path, std::string *target)
{
    Item item;
    if (const int error = lookup(path, &item); error < 0)
        return error;
    return readLink(item, target);
}
