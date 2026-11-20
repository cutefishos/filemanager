// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "core/dsstore.h"

#include "base/file.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <set>
#include <utility>
#include <vector>

namespace fm {

namespace fs = std::filesystem;

// The file is a buddy allocator's blocks: a root block naming the others and
// a table of contents, where "DSDB" is a B-tree of records sorted by file
// name, each a file name, a four-character code and a typed value.
static constexpr uint32_t kMaximumBlocks = 1u << 20;
static constexpr int kMaximumDepth = 16;

static uint32_t be32(std::string_view data, size_t offset)
{
    if (offset + 4 > data.size())
        return 0;
    const auto* p = reinterpret_cast<const unsigned char*>(data.data() + offset);
    return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | uint32_t(p[3]);
}

static uint16_t be16(std::string_view data, size_t offset)
{
    if (offset + 2 > data.size())
        return 0;
    const auto* p = reinterpret_cast<const unsigned char*>(data.data() + offset);
    return uint16_t(p[0] << 8 | p[1]);
}

static uint64_t beN(std::string_view data, size_t offset, size_t size)
{
    uint64_t value = 0;
    for (size_t i = 0; i < size && offset + i < data.size(); ++i)
        value = value << 8 | uint8_t(data[offset + i]);
    return value;
}

static void appendUtf8(std::string& out, uint32_t c)
{
    if (c < 0x80) {
        out += char(c);
    } else if (c < 0x800) {
        out += char(0xc0 | c >> 6);
        out += char(0x80 | (c & 0x3f));
    } else if (c < 0x10000) {
        out += char(0xe0 | c >> 12);
        out += char(0x80 | (c >> 6 & 0x3f));
        out += char(0x80 | (c & 0x3f));
    } else {
        out += char(0xf0 | c >> 18);
        out += char(0x80 | (c >> 12 & 0x3f));
        out += char(0x80 | (c >> 6 & 0x3f));
        out += char(0x80 | (c & 0x3f));
    }
}

static std::string utf16BigEndian(std::string_view data)
{
    std::string out;
    for (size_t i = 0; i + 1 < data.size(); i += 2) {
        uint32_t c = be16(data, i);
        if (c >= 0xd800 && c < 0xdc00 && i + 3 < data.size()) {
            const uint32_t low = be16(data, i + 2);
            if (low >= 0xdc00 && low < 0xe000) {
                c = 0x10000 + ((c - 0xd800) << 10) + (low - 0xdc00);
                i += 2;
            }
        }
        appendUtf8(out, c);
    }
    return out;
}

namespace {

// What the layout needs of a binary property list: one dictionary of
// numbers, strings and data.
struct PlistValue
{
    enum class Type { None, Boolean, Number, Data, String, Dictionary };

    Type type = Type::None;
    double number = 0;
    std::string bytes;
    std::vector<std::pair<std::string, PlistValue>> members;

    const PlistValue* find(std::string_view key) const
    {
        for (const auto& [name, value] : members) {
            if (name == key)
                return &value;
        }
        return nullptr;
    }

    double numberOf(std::string_view key, double fallback) const
    {
        const PlistValue* value = find(key);
        return value && (value->type == Type::Number || value->type == Type::Boolean) ? value->number : fallback;
    }
};

class BinaryPlist
{
public:
    explicit BinaryPlist(std::string_view data)
        : m_data(data)
    {
        if (data.size() < 40 || data.substr(0, 8) != "bplist00")
            return;
        const size_t trailer = data.size() - 32;
        m_offsetSize = uint8_t(data[trailer + 6]);
        m_refSize = uint8_t(data[trailer + 7]);
        m_count = beN(data, trailer + 8, 8);
        m_top = beN(data, trailer + 16, 8);
        m_offsets = beN(data, trailer + 24, 8);
        m_valid = m_offsetSize >= 1 && m_offsetSize <= 8 && m_refSize >= 1 && m_refSize <= 8 && m_top < m_count
                && m_offsets < trailer && m_count <= (trailer - m_offsets) / m_offsetSize;
    }

    std::optional<PlistValue> root() const
    {
        if (!m_valid)
            return std::nullopt;
        return object(m_top, 0);
    }

private:
    // The length after a marker, and where the value starts.
    bool length(size_t offset, size_t* count, size_t* start) const
    {
        const uint8_t marker = uint8_t(m_data[offset]);
        *start = offset + 1;
        *count = marker & 0x0f;
        if (*count != 0x0f)
            return true;
        if (*start >= m_data.size())
            return false;
        const uint8_t intMarker = uint8_t(m_data[*start]);
        if ((intMarker & 0xf0) != 0x10)
            return false;
        const size_t size = size_t(1) << (intMarker & 0x0f);
        if (size > 8)
            return false;
        *count = size_t(beN(m_data, *start + 1, size));
        *start += 1 + size;
        return *start <= m_data.size();
    }

    PlistValue object(uint64_t index, int depth) const
    {
        PlistValue value;
        if (index >= m_count || depth > 8)
            return value;
        const size_t offset = size_t(beN(m_data, size_t(m_offsets + index * m_offsetSize), m_offsetSize));
        if (offset >= m_data.size() - 32)
            return value;
        const uint8_t marker = uint8_t(m_data[offset]);
        size_t count = 0;
        size_t start = 0;
        switch (marker >> 4) {
        case 0x0:
            if (marker == 0x08 || marker == 0x09) {
                value.type = PlistValue::Type::Boolean;
                value.number = marker == 0x09;
            }
            break;
        case 0x1: {
            const size_t size = size_t(1) << (marker & 0x0f);
            if (size <= 8) {
                value.type = PlistValue::Type::Number;
                value.number = double(int64_t(beN(m_data, offset + 1, size)));
            }
            break;
        }
        case 0x2: {
            const size_t size = size_t(1) << (marker & 0x0f);
            const uint64_t bits = beN(m_data, offset + 1, size);
            value.type = PlistValue::Type::Number;
            if (size == 4) {
                float number;
                const uint32_t narrow = uint32_t(bits);
                std::memcpy(&number, &narrow, 4);
                value.number = number;
            } else if (size == 8) {
                std::memcpy(&value.number, &bits, 8);
            } else {
                value.type = PlistValue::Type::None;
            }
            break;
        }
        case 0x4:
            if (length(offset, &count, &start) && start + count <= m_data.size()) {
                value.type = PlistValue::Type::Data;
                value.bytes = std::string(m_data.substr(start, count));
            }
            break;
        case 0x5:
            if (length(offset, &count, &start) && start + count <= m_data.size()) {
                value.type = PlistValue::Type::String;
                value.bytes = std::string(m_data.substr(start, count));
            }
            break;
        case 0x6:
            if (length(offset, &count, &start) && start + count * 2 <= m_data.size()) {
                value.type = PlistValue::Type::String;
                value.bytes = utf16BigEndian(m_data.substr(start, count * 2));
            }
            break;
        case 0xd:
            if (length(offset, &count, &start) && start + count * 2 * m_refSize <= m_data.size()) {
                value.type = PlistValue::Type::Dictionary;
                for (size_t i = 0; i < count; ++i) {
                    const uint64_t key = beN(m_data, start + i * m_refSize, m_refSize);
                    const uint64_t member = beN(m_data, start + (count + i) * m_refSize, m_refSize);
                    PlistValue name = object(key, depth + 1);
                    if (name.type == PlistValue::Type::String)
                        value.members.emplace_back(std::move(name.bytes), object(member, depth + 1));
                }
            }
            break;
        default:
            break;
        }
        return value;
    }

    std::string_view m_data;
    bool m_valid = false;
    uint8_t m_offsetSize = 0;
    uint8_t m_refSize = 0;
    uint64_t m_count = 0;
    uint64_t m_top = 0;
    uint64_t m_offsets = 0;
};

struct Record
{
    std::string name;
    std::string code;
    std::string type;
    std::string_view blob;
    uint32_t number = 0;
};

class Store
{
public:
    explicit Store(std::string_view data)
        : m_data(data)
    {
    }

    bool read(std::vector<Record>* records)
    {
        if (be32(m_data, 0) != 1 || m_data.substr(4, 4) != "Bud1")
            return false;
        const size_t root = size_t(be32(m_data, 8)) + 4;
        const uint32_t count = be32(m_data, root);
        if (count == 0 || count > kMaximumBlocks)
            return false;
        for (uint32_t i = 0; i < count; ++i)
            m_addresses.push_back(be32(m_data, root + 8 + size_t(i) * 4));
        size_t offset = root + 8 + size_t((count + 255) / 256 * 256) * 4;
        const uint32_t entries = be32(m_data, offset);
        offset += 4;
        std::optional<uint32_t> master;
        for (uint32_t i = 0; i < entries && offset < m_data.size(); ++i) {
            const size_t length = uint8_t(m_data[offset]);
            const std::string_view name = m_data.substr(offset + 1, length);
            const uint32_t value = be32(m_data, offset + 1 + length);
            offset += 1 + length + 4;
            if (name == "DSDB")
                master = value;
        }
        std::string_view header;
        if (!master || !block(*master, &header) || header.size() < 20)
            return false;
        return node(be32(header, 0), 0, records);
    }

private:
    bool block(uint32_t index, std::string_view* out) const
    {
        if (index >= m_addresses.size())
            return false;
        const uint32_t address = m_addresses[index];
        const size_t offset = size_t(address & ~0x1fu) + 4;
        const size_t size = size_t(1) << (address & 0x1f);
        if (offset >= m_data.size())
            return false;
        *out = m_data.substr(offset, std::min(size, m_data.size() - offset));
        return true;
    }

    bool node(uint32_t index, int depth, std::vector<Record>* records)
    {
        std::string_view data;
        if (depth > kMaximumDepth || !m_visited.insert(index).second || !block(index, &data))
            return false;
        const uint32_t next = be32(data, 0);
        const uint32_t count = be32(data, 4);
        size_t offset = 8;
        for (uint32_t i = 0; i < count; ++i) {
            if (next != 0) {
                if (!node(be32(data, offset), depth + 1, records))
                    return false;
                offset += 4;
            }
            Record record;
            if (!this->record(data, &offset, &record))
                return false;
            records->push_back(std::move(record));
        }
        return next == 0 || node(next, depth + 1, records);
    }

    static bool record(std::string_view data, size_t* offset, Record* record)
    {
        const size_t length = be32(data, *offset);
        size_t at = *offset + 4;
        if (at + length * 2 + 8 > data.size())
            return false;
        record->name = utf16BigEndian(data.substr(at, length * 2));
        at += length * 2;
        record->code = std::string(data.substr(at, 4));
        record->type = std::string(data.substr(at + 4, 4));
        at += 8;
        size_t size = 0;
        if (record->type == "long" || record->type == "shor" || record->type == "type") {
            size = 4;
            record->number = be32(data, at);
        } else if (record->type == "bool") {
            size = 1;
            record->number = at < data.size() ? uint8_t(data[at]) : 0;
        } else if (record->type == "comp" || record->type == "dutc") {
            size = 8;
        } else if (record->type == "blob") {
            const size_t bytes = be32(data, at);
            if (at + 4 + bytes > data.size())
                return false;
            record->blob = data.substr(at + 4, bytes);
            size = 4 + bytes;
        } else if (record->type == "ustr") {
            size = 4 + size_t(be32(data, at)) * 2;
        } else {
            return false;
        }
        if (at + size > data.size())
            return false;
        *offset = at + size;
        return true;
    }

    std::string_view m_data;
    std::vector<uint32_t> m_addresses;
    std::set<uint32_t> m_visited;
};

// What an alias record of version 2 names: the target's path from its
// volume's root (tag 18), else its file name.
struct AliasTarget
{
    std::string path;
    std::string name;
};

} // namespace

static AliasTarget aliasTarget(std::string_view alias)
{
    AliasTarget target;
    if (alias.size() < 150 || be16(alias, 6) != 2)
        return target;
    const size_t nameLength = std::min<size_t>(uint8_t(alias[50]), 63);
    target.name = std::string(alias.substr(51, nameLength));
    size_t offset = 150;
    while (offset + 4 <= alias.size()) {
        const int16_t tag = int16_t(be16(alias, offset));
        const size_t length = be16(alias, offset + 2);
        if (tag == -1 || offset + 4 + length > alias.size())
            break;
        const std::string_view value = alias.substr(offset + 4, length);
        if (tag == 18)
            target.path = std::string(value);
        else if (tag == 14 && length >= 2)
            target.name = utf16BigEndian(value.substr(2, size_t(be16(value, 0)) * 2));
        offset += 4 + length + (length & 1);
    }
    return target;
}

static bool isImageName(const std::string& name)
{
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    for (const char* suffix : {".png", ".tif", ".tiff", ".jpg", ".jpeg", ".gif", ".bmp", ".heic"}) {
        if (lower.ends_with(suffix))
            return true;
    }
    return false;
}

static bool isFile(const std::string& path)
{
    std::error_code error;
    return fs::is_regular_file(path, error);
}

// The alias's target in the folder, which is its volume's root on a disk
// image; else the folder's only picture in .background, where images keep it.
static std::string findPicture(const std::string& folder, std::string_view alias)
{
    const AliasTarget target = aliasTarget(alias);
    if (!target.path.empty() && target.path.find("..") == std::string::npos) {
        const std::string path = folder + (target.path.starts_with('/') ? "" : "/") + target.path;
        if (isFile(path))
            return path;
    }
    if (!target.name.empty() && target.name.find('/') == std::string::npos) {
        for (const std::string& path : {folder + "/.background/" + target.name, folder + "/" + target.name}) {
            if (isFile(path))
                return path;
        }
    }
    std::vector<std::string> pictures;
    std::error_code error;
    for (auto it = fs::directory_iterator(folder + "/.background", error); it != fs::directory_iterator();
         it.increment(error)) {
        if (error)
            break;
        if (isImageName(it->path().filename().string()) && it->is_regular_file(error))
            pictures.push_back(it->path().string());
    }
    std::sort(pictures.begin(), pictures.end());
    return pictures.empty() ? std::string() : pictures.front();
}

// "{{x, y}, {width, height}}".
static ui::SizeF boundsSize(const std::string& bounds)
{
    std::vector<double> numbers;
    const char* p = bounds.c_str();
    while (*p) {
        if ((*p >= '0' && *p <= '9') || *p == '-' || *p == '.') {
            char* end = nullptr;
            numbers.push_back(std::strtod(p, &end));
            p = end;
        } else {
            ++p;
        }
    }
    if (numbers.size() != 4 || numbers[2] <= 0 || numbers[3] <= 0)
        return {};
    return {numbers[2], numbers[3]};
}

static ui::Color colorOf(double red, double green, double blue)
{
    const auto channel = [](double value) { return uint8_t(std::clamp(std::lround(value * 255), 0L, 255L)); };
    return {channel(red), channel(green), channel(blue), 255};
}

std::optional<FolderLayout> parseFolderLayout(std::string_view data, const std::string& folder)
{
    std::vector<Record> records;
    if (!Store(data).read(&records))
        return std::nullopt;

    FolderLayout layout;
    std::string pictureAlias;
    bool picture = false;
    for (const Record& record : records) {
        if (record.code == "Iloc" && record.blob.size() >= 8 && record.name != ".") {
            layout.positions[record.name] = {double(int32_t(be32(record.blob, 0))),
                                             double(int32_t(be32(record.blob, 4)))};
            continue;
        }
        if (record.name != ".")
            continue;
        if (record.code == "bwsp") {
            if (const std::optional<PlistValue> settings = BinaryPlist(record.blob).root()) {
                if (const PlistValue* bounds = settings->find("WindowBounds");
                    bounds && bounds->type == PlistValue::Type::String) {
                    layout.windowSize = boundsSize(bounds->bytes);
                }
            }
        } else if (record.code == "fwi0" && record.blob.size() >= 8 && layout.windowSize.width <= 0) {
            const double top = int16_t(be16(record.blob, 0));
            const double left = int16_t(be16(record.blob, 2));
            const double bottom = int16_t(be16(record.blob, 4));
            const double right = int16_t(be16(record.blob, 6));
            if (right > left && bottom > top)
                layout.windowSize = {right - left, bottom - top};
        } else if (record.code == "icvp") {
            const std::optional<PlistValue> options = BinaryPlist(record.blob).root();
            if (!options)
                continue;
            layout.iconSize = options->numberOf("iconSize", layout.iconSize);
            const int type = int(options->numberOf("backgroundType", 0));
            if (type == 1) {
                layout.background = FolderLayout::Background::Color;
                layout.color = colorOf(options->numberOf("backgroundColorRed", 1),
                                       options->numberOf("backgroundColorGreen", 1),
                                       options->numberOf("backgroundColorBlue", 1));
            } else if (type == 2) {
                picture = true;
                if (const PlistValue* alias = options->find("backgroundImageAlias");
                    alias && alias->type == PlistValue::Type::Data) {
                    pictureAlias = alias->bytes;
                }
            }
        } else if (record.code == "icvo" && record.blob.size() >= 6 && layout.iconSize <= 0
                   && record.blob.substr(0, 4) == "icv4") {
            layout.iconSize = be16(record.blob, 4);
        } else if (record.code == "BKGD" && record.blob.size() >= 10
                   && layout.background == FolderLayout::Background::Default) {
            const std::string_view kind = record.blob.substr(0, 4);
            if (kind == "ClrB") {
                layout.background = FolderLayout::Background::Color;
                layout.color = colorOf(be16(record.blob, 4) / 65535.0, be16(record.blob, 6) / 65535.0,
                                       be16(record.blob, 8) / 65535.0);
            } else if (kind == "PctB") {
                picture = true;
            }
        } else if (record.code == "pict" && pictureAlias.empty()) {
            pictureAlias = std::string(record.blob);
        }
    }
    if (layout.positions.empty())
        return std::nullopt;
    if (picture) {
        layout.picture = findPicture(folder, pictureAlias);
        if (!layout.picture.empty())
            layout.background = FolderLayout::Background::Picture;
    }
    return layout;
}

std::optional<FolderLayout> readFolderLayout(const std::string& folder)
{
    const std::optional<std::string> data = base::file::read(folder + "/.DS_Store");
    if (!data)
        return std::nullopt;
    return parseFolderLayout(*data, folder);
}

} // namespace fm
