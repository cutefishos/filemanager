// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "testutil.h"

#include "core/dsstore.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

using fm::FolderLayout;
using fmtest::TemporaryDir;
using fmtest::writeFile;

static void put32(std::string& out, uint32_t value)
{
    for (int shift = 24; shift >= 0; shift -= 8)
        out += char(value >> shift);
}

static void put16(std::string& out, uint16_t value)
{
    out += char(value >> 8);
    out += char(value);
}

static void putAt(std::string& out, size_t offset, const std::string& bytes)
{
    if (out.size() < offset + bytes.size())
        out.resize(offset + bytes.size());
    out.replace(offset, bytes.size(), bytes);
}

static std::string utf16(const std::string& ascii)
{
    std::string out;
    for (const char c : ascii)
        put16(out, uint16_t(uint8_t(c)));
    return out;
}

// A binary property list of one dictionary whose values are integers,
// strings or data, as the platform's file manager writes its view options.
struct PlistMember
{
    std::string key;
    char kind;
    int64_t number = 0;
    std::string bytes;
};

static std::string binaryPlist(const std::vector<PlistMember>& members)
{
    std::vector<std::string> objects;
    std::string dictionary;
    dictionary += char(0xd0 | members.size());
    for (size_t i = 0; i < members.size(); ++i)
        dictionary += char(1 + i);
    for (size_t i = 0; i < members.size(); ++i)
        dictionary += char(1 + members.size() + i);
    objects.push_back(dictionary);
    const auto counted = [](uint8_t marker, const std::string& bytes) {
        std::string object;
        if (bytes.size() < 15) {
            object += char(marker | bytes.size());
        } else {
            object += char(marker | 0x0f);
            object += char(0x11);
            put16(object, uint16_t(bytes.size()));
        }
        return object + bytes;
    };
    for (const PlistMember& member : members)
        objects.push_back(counted(0x50, member.key));
    for (const PlistMember& member : members) {
        std::string object;
        if (member.kind == 'b') {
            object += char(member.number ? 0x09 : 0x08);
        } else if (member.kind == 'i') {
            object += char(0x13);
            for (int shift = 56; shift >= 0; shift -= 8)
                object += char(member.number >> shift);
        } else if (member.kind == 'r') {
            object += char(0x23);
            double value = double(member.number) / 1000;
            uint64_t bits;
            std::memcpy(&bits, &value, 8);
            for (int shift = 56; shift >= 0; shift -= 8)
                object += char(bits >> shift);
        } else {
            object = counted(member.kind == 's' ? 0x50 : 0x40, member.bytes);
        }
        objects.push_back(object);
    }

    std::string out = "bplist00";
    std::vector<uint16_t> offsets;
    for (const std::string& object : objects) {
        offsets.push_back(uint16_t(out.size()));
        out += object;
    }
    const size_t table = out.size();
    for (const uint16_t offset : offsets)
        put16(out, offset);
    out += std::string(6, '\0');
    out += char(2);
    out += char(1);
    for (const uint64_t value : {uint64_t(objects.size()), uint64_t(0), uint64_t(table)}) {
        for (int shift = 56; shift >= 0; shift -= 8)
            out += char(value >> shift);
    }
    return out;
}

// An alias record of version 2 to a file, with its path from the volume's
// root.
static std::string alias(const std::string& name, const std::string& path)
{
    std::string out(150, '\0');
    out.replace(6, 2, std::string("\x00\x02", 2));
    out[50] = char(name.size());
    out.replace(51, name.size(), name);
    put16(out, 18);
    put16(out, uint16_t(path.size()));
    out += path;
    if (path.size() & 1)
        out += '\0';
    put16(out, 0xffff);
    put16(out, 0);
    return out;
}

static std::string record(const std::string& name, const std::string& code, const std::string& type,
                          const std::string& value)
{
    std::string out;
    put32(out, uint32_t(name.size()));
    out += utf16(name);
    out += code;
    out += type;
    if (type == "blob")
        put32(out, uint32_t(value.size()));
    out += value;
    return out;
}

static std::string location(uint32_t x, uint32_t y)
{
    std::string out;
    put32(out, x);
    put32(out, y);
    out += std::string("\xff\xff\xff\xff\xff\xff\x00\x00", 8);
    return out;
}

// The allocator's file: a root block, the B-tree's header and one leaf.
static std::string store(const std::vector<std::string>& records)
{
    std::string file;
    put32(file, 1);
    file += "Bud1";
    put32(file, 0x800);
    put32(file, 0x800);
    put32(file, 0x800);
    file += std::string(16, '\0');

    std::string root;
    put32(root, 3);
    put32(root, 0);
    put32(root, 0x800 | 11);
    put32(root, 0x1000 | 5);
    put32(root, 0x2000 | 12);
    root += std::string(253 * 4, '\0');
    put32(root, 1);
    root += char(4);
    root += "DSDB";
    put32(root, 1);
    putAt(file, 4 + 0x800, root);

    std::string header;
    for (const uint32_t value : {2u, 0u, uint32_t(records.size()), 1u, 0x1000u})
        put32(header, value);
    putAt(file, 4 + 0x1000, header);

    std::string leaf;
    put32(leaf, 0);
    put32(leaf, uint32_t(records.size()));
    for (const std::string& entry : records)
        leaf += entry;
    putAt(file, 4 + 0x2000, leaf);
    file.resize(4 + 0x3000);
    return file;
}

TEST(DsStoreTest, ReadsTheWindowTheIconsAndThePicture)
{
    TemporaryDir volume;
    ASSERT_TRUE(volume.isValid());
    std::filesystem::create_directories(volume.filePath(".background"));
    ASSERT_TRUE(writeFile(volume.filePath(".background/Install.png"), "png"));
    ASSERT_TRUE(writeFile(volume.filePath(".background/Other.png"), "png"));

    const std::string data = store({
            record(".", "bwsp", "blob", binaryPlist({{"WindowBounds", 's', 0, "{{200, 120}, {640, 400}}"}})),
            record(".", "icvp", "blob",
                   binaryPlist({{"iconSize", 'r', 128000},
                                {"textSize", 'r', 16000},
                                {"labelOnBottom", 'b', 0},
                                {"backgroundType", 'i', 2},
                                {"backgroundImageAlias", 'd', 0, alias("Other.png", "/.background/Other.png")}})),
            record(".", "vSrn", "long", std::string("\0\0\0\1", 4)),
            record("Applications", "Iloc", "blob", location(480, 190)),
            record("Editor.app", "Iloc", "blob", location(160, 190)),
    });
    const std::optional<FolderLayout> layout = fm::parseFolderLayout(data, volume.path());
    ASSERT_TRUE(layout);
    EXPECT_EQ(layout->windowSize.width, 640);
    EXPECT_EQ(layout->windowSize.height, 400);
    EXPECT_EQ(layout->iconSize, 128);
    EXPECT_EQ(layout->textSize, 16);
    EXPECT_FALSE(layout->labelOnBottom);
    EXPECT_EQ(layout->background, FolderLayout::Background::Picture);
    EXPECT_EQ(layout->picture, volume.filePath(".background/Other.png"));
    ASSERT_EQ(layout->positions.size(), 2u);
    EXPECT_EQ(layout->positions.at("Editor.app").x, 160);
    EXPECT_EQ(layout->positions.at("Applications").y, 190);
}

TEST(DsStoreTest, FindsThePictureInItsFolderAndReadsColours)
{
    TemporaryDir volume;
    ASSERT_TRUE(volume.isValid());
    std::filesystem::create_directories(volume.filePath(".background"));
    ASSERT_TRUE(writeFile(volume.filePath(".background/Drag.tiff"), "tiff"));

    // An alias whose path is gone: the folder's only picture.
    const std::string picture = store({
            record(".", "icvp", "blob",
                   binaryPlist({{"backgroundType", 'i', 2},
                                {"backgroundImageAlias", 'd', 0, alias("Gone.png", "/Gone.png")}})),
            record("Editor.app", "Iloc", "blob", location(100, 100)),
    });
    const std::optional<FolderLayout> found = fm::parseFolderLayout(picture, volume.path());
    ASSERT_TRUE(found);
    EXPECT_EQ(found->picture, volume.filePath(".background/Drag.tiff"));
    EXPECT_EQ(found->iconSize, 0);
    EXPECT_EQ(found->textSize, 0);
    EXPECT_TRUE(found->labelOnBottom);
    EXPECT_EQ(found->windowSize.width, 0);

    const std::string colour = store({
            record(".", "icvp", "blob",
                   binaryPlist({{"backgroundType", 'i', 1},
                                {"backgroundColorRed", 'r', 1000},
                                {"backgroundColorGreen", 'r', 500},
                                {"backgroundColorBlue", 'r', 0}})),
            record("Editor.app", "Iloc", "blob", location(100, 100)),
    });
    const std::optional<FolderLayout> coloured = fm::parseFolderLayout(colour, volume.path());
    ASSERT_TRUE(coloured);
    EXPECT_EQ(coloured->background, FolderLayout::Background::Color);
    EXPECT_EQ(coloured->color.r, 255);
    EXPECT_EQ(coloured->color.g, 128);
    EXPECT_EQ(coloured->color.b, 0);
}

TEST(DsStoreTest, RefusesWhatIsNoStoreOrPlacesNothing)
{
    EXPECT_FALSE(fm::parseFolderLayout("", "/"));
    EXPECT_FALSE(fm::parseFolderLayout(std::string(64, 'x'), "/"));
    EXPECT_FALSE(fm::parseFolderLayout(store({record(".", "vSrn", "long", std::string("\0\0\0\1", 4))}), "/"));
    // A truncated file.
    const std::string whole = store({record("Editor.app", "Iloc", "blob", location(1, 2))});
    EXPECT_FALSE(fm::parseFolderLayout(whole.substr(0, 0x900), "/"));
}
