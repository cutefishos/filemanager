// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "decmpfs.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <lzfse.h>
#include <zlib.h>

static constexpr uint64_t kMaxInline = 64 * 1024 * 1024;
static constexpr uint64_t kCompressedBlock = 64 * 1024;

static uint32_t be32(const uint8_t *p)
{
    return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
}

static uint32_t le32(const uint8_t *p)
{
    return uint32_t(p[3]) << 24 | uint32_t(p[2]) << 16 | uint32_t(p[1]) << 8 | p[0];
}

static uint64_t le64(const uint8_t *p)
{
    return uint64_t(le32(p + 4)) << 32 | le32(p);
}

static bool decodeLzvn(const uint8_t *in, size_t inSize, uint8_t *out, size_t outSize, std::vector<uint8_t> *scratch)
{
    // lzfse decodes LZVN inside its own block framing.
    std::vector<uint8_t> framed(12 + inSize + 4);
    std::memcpy(framed.data(), "bvxn", 4);
    for (int i = 0; i < 4; ++i) {
        framed[4 + i] = uint8_t(outSize >> (8 * i));
        framed[8 + i] = uint8_t(inSize >> (8 * i));
    }
    std::memcpy(framed.data() + 12, in, inSize);
    std::memcpy(framed.data() + 12 + inSize, "bvx$", 4);

    if (scratch->empty())
        scratch->resize(lzfse_decode_scratch_size());
    return lzfse_decode_buffer(out, outSize, framed.data(), framed.size(), scratch->data()) == outSize;
}

// One compressed block or inline value of a decmpfs type, stored or encoded.
static bool decodeBlock(uint32_t type, const uint8_t *in, size_t inSize, uint8_t *out, size_t outSize,
                        std::vector<uint8_t> *scratch)
{
    if (outSize == 0)
        return true;
    if (inSize == 0)
        return false;

    const auto stored = [&] {
        if (inSize - 1 < outSize)
            return false;
        std::memcpy(out, in + 1, outSize);
        return true;
    };

    switch (type) {
    case 1:
        if (inSize < outSize)
            return false;
        std::memcpy(out, in, outSize);
        return true;
    case 3:
    case 4: {
        if ((in[0] & 0x0F) == 0x0F)
            return stored();
        uLongf length = outSize;
        return ::uncompress(out, &length, in, uLong(inSize)) == Z_OK && length == outSize;
    }
    case 7:
    case 8:
        if (in[0] == 0x06)
            return stored();
        return decodeLzvn(in, inSize, out, outSize, scratch);
    case 11:
    case 12:
        if (in[0] == 0xFF && inSize == outSize + 1)
            return stored();
        if (scratch->empty())
            scratch->resize(lzfse_decode_scratch_size());
        return lzfse_decode_buffer(out, outSize, in, inSize, scratch->data()) == outSize;
    }
    return false;
}

static bool inlineCompression(uint32_t type)
{
    return type == 1 || type == 3 || type == 7 || type == 11;
}

static bool resourceCompression(uint32_t type)
{
    return type == 4 || type == 8 || type == 12;
}

class InlineFile : public DiskVolume::File
{
public:
    explicit InlineFile(std::vector<uint8_t> data)
        : m_data(std::move(data))
    {
    }

    int read(char *buffer, size_t size, uint64_t offset) override
    {
        if (offset >= m_data.size())
            return 0;
        size = size_t(std::min<uint64_t>(size, m_data.size() - offset));
        std::memcpy(buffer, m_data.data() + offset, size);
        return int(size);
    }

private:
    std::vector<uint8_t> m_data;
};

// Compressed blocks of 64 KiB in the resource fork, decoded one at a time.
class ResourceFile : public DiskVolume::File
{
public:
    struct Block
    {
        uint64_t offset = 0;
        uint64_t size = 0;
    };

    ResourceFile(ReadResource readResource, uint32_t type, uint64_t size, std::vector<Block> blocks)
        : m_readResource(std::move(readResource))
        , m_type(type)
        , m_size(size)
        , m_blocks(std::move(blocks))
    {
    }

    int read(char *buffer, size_t size, uint64_t offset) override
    {
        if (offset >= m_size)
            return 0;
        size = size_t(std::min<uint64_t>(size, m_size - offset));

        size_t done = 0;
        while (done < size) {
            const uint64_t index = (offset + done) / kCompressedBlock;
            if (!load(index))
                return -EIO;
            const uint64_t within = (offset + done) - index * kCompressedBlock;
            const size_t part = size_t(std::min<uint64_t>(size - done, m_decoded.size() - within));
            std::memcpy(buffer + done, m_decoded.data() + within, part);
            done += part;
        }
        return int(done);
    }

private:
    bool load(uint64_t index)
    {
        if (index == m_decodedIndex)
            return true;
        if (index >= m_blocks.size())
            return false;

        const Block &block = m_blocks[index];
        std::vector<uint8_t> input(block.size);
        if (block.size > kCompressedBlock * 2 || !m_readResource(block.offset, input.data(), input.size()))
            return false;

        const uint64_t length = std::min(kCompressedBlock, m_size - index * kCompressedBlock);
        m_decoded.resize(length);
        if (!decodeBlock(m_type, input.data(), input.size(), m_decoded.data(), length, &m_scratch)) {
            m_decodedIndex = UINT64_MAX;
            return false;
        }
        m_decodedIndex = index;
        return true;
    }

    ReadResource m_readResource;
    uint32_t m_type;
    uint64_t m_size;
    std::vector<Block> m_blocks;
    std::vector<uint8_t> m_decoded;
    uint64_t m_decodedIndex = UINT64_MAX;
    std::vector<uint8_t> m_scratch;
};

bool parseDecmpfs(const std::vector<uint8_t> &value, Decmpfs *header)
{
    if (value.size() < 16 || std::memcmp(value.data(), "fpmc", 4) != 0)
        return false;
    header->type = le32(value.data() + 4);
    header->size = le64(value.data() + 8);
    header->inlineData.assign(value.begin() + 16, value.end());
    return true;
}

int openDecmpfs(const Decmpfs &header, uint64_t resourceSize, ReadResource readResource,
                std::unique_ptr<DiskVolume::File> *file)
{
    if (inlineCompression(header.type)) {
        if (header.size > kMaxInline)
            return -EIO;
        std::vector<uint8_t> decoded(size_t(header.size));
        std::vector<uint8_t> scratch;
        if (!decodeBlock(header.type, header.inlineData.data(), header.inlineData.size(), decoded.data(),
                         decoded.size(), &scratch))
            return -EIO;
        *file = std::make_unique<InlineFile>(std::move(decoded));
        return 0;
    }

    if (!resourceCompression(header.type))
        return -EOPNOTSUPP;

    const uint64_t expected = (header.size + kCompressedBlock - 1) / kCompressedBlock;
    // The table alone takes four bytes a block.
    if (expected > resourceSize / 4)
        return -EIO;
    std::vector<ResourceFile::Block> blocks;

    if (header.type == 4) {
        // A resource map holding one resource: a block table, then the blocks.
        uint8_t start[16];
        if (!readResource(0, start, sizeof(start)))
            return -EIO;
        const uint64_t base = uint64_t(be32(start)) + 4;
        uint8_t count[4];
        if (!readResource(base, count, sizeof(count)) || le32(count) != expected)
            return -EIO;
        std::vector<uint8_t> table(size_t(expected) * 8);
        if (!readResource(base + 4, table.data(), table.size()))
            return -EIO;
        for (uint64_t i = 0; i < expected; ++i)
            blocks.push_back({base + le32(table.data() + i * 8), le32(table.data() + i * 8 + 4)});
    } else {
        // Offsets of every block and of the end, from the start of the fork.
        std::vector<uint8_t> table(size_t(expected + 1) * 4);
        if (!readResource(0, table.data(), table.size()) || le32(table.data()) != table.size())
            return -EIO;
        for (uint64_t i = 0; i < expected; ++i) {
            const uint32_t begin = le32(table.data() + i * 4);
            const uint32_t end = le32(table.data() + (i + 1) * 4);
            if (end < begin)
                return -EIO;
            blocks.push_back({begin, end - begin});
        }
    }

    *file = std::make_unique<ResourceFile>(std::move(readResource), header.type, header.size, std::move(blocks));
    return 0;
}
