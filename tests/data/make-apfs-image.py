#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause
#
# Makes the APFS containers the disk image tests read: apfs.img.gz ('hashed'), a
# case-insensitive volume whose directory records carry name hashes, and
# apfs-plain.img.gz ('plain'), the same files under plain names. The whole container is laid out here, following Apple's
# published APFS reference; 7-Zip reads the result as a check on this script.
#
# What it holds beyond plain files: a superblock of an older checkpoint after the
# current one, object maps with older versions of every node, trees of several levels
# (nodes are kept small on purpose), a file in single-block extents with a hole and a
# hole, hard links, symlinks, and decmpfs files inline and with resource forks.

import ctypes
import gzip
import struct
import sys
import unicodedata
import uuid
import zlib

BLOCK = 4096
TOTAL_BLOCKS = 2048
XID = 40
NOW = 1767225600 * 1000000000
VOLUME_NAME = 'Fixture Disk'
VOLUME_OID = 1026

# Records per node, small so the trees grow several levels.
LEAF_RECORDS = 12
INDEX_RECORDS = 6
MAP_LEAF_RECORDS = 40

S_IFDIR = 0o040000
S_IFREG = 0o100000
S_IFLNK = 0o120000


def pattern(label, size):
    """Compressible bytes that still differ from block to block."""
    out = bytearray()
    for block in range(0, size, BLOCK):
        line = f'{label} block {block // BLOCK:06d}\n'.encode()
        out += (line * (BLOCK // len(line) + 1))[:BLOCK]
    return bytes(out[:size])


def fletcher64(block):
    low = high = 0
    for (word,) in struct.iter_unpack('<I', block[8:]):
        low = (low + word) % 0xFFFFFFFF
        high = (high + low) % 0xFFFFFFFF
    first = 0xFFFFFFFF - (low + high) % 0xFFFFFFFF
    second = 0xFFFFFFFF - (low + first) % 0xFFFFFFFF
    return second << 32 | first


def seal(block):
    struct.pack_into('<Q', block, 0, fletcher64(bytes(block)))
    return bytes(block)


def object_header(block, oid, xid, kind, subtype=0):
    struct.pack_into('<QQQII', block, 0, 0, oid, xid, kind, subtype)


CRC32C = []
for n in range(256):
    c = n
    for _ in range(8):
        c = (c >> 1) ^ 0x82F63B78 if c & 1 else c >> 1
    CRC32C.append(c)


def name_hash(name):
    folded = unicodedata.normalize('NFD', name.casefold())
    crc = 0xFFFFFFFF
    for byte in folded.encode('utf-32-le'):
        crc = CRC32C[(crc ^ byte) & 0xFF] ^ (crc >> 8)
    return crc & 0x3FFFFF


def lzfse_library():
    library = ctypes.CDLL('liblzfse.so')
    library.lzfse_encode_buffer.restype = ctypes.c_size_t
    library.lzfse_encode_buffer.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_char_p, ctypes.c_size_t,
                                            ctypes.c_void_p]
    return library


def lzfse_encode(data):
    out = ctypes.create_string_buffer(len(data) + 4096)
    written = LZFSE.lzfse_encode_buffer(out, len(out), data, len(data), None)
    assert written > 0
    return out.raw[:written]


def lzvn_encode(data):
    # The encoder takes LZVN for small inputs; decmpfs stores its payload without the framing.
    framed = lzfse_encode(data)
    assert framed[:4] == b'bvxn' and framed[-4:] == b'bvx$', framed[:4]
    return framed[12:-4]


def decmpfs(kind, size, payload=b''):
    return b'fpmc' + struct.pack('<IQ', kind, size) + payload


def blocks_table(blocks):
    offset = (len(blocks) + 1) * 4
    table = b''
    for block in blocks:
        table += struct.pack('<I', offset)
        offset += len(block)
    return table + struct.pack('<I', offset) + b''.join(blocks)


def zlib_resource(blocks):
    """A resource map holding one 'cmpf' resource, as decmpfs type 4 stores it."""
    body = struct.pack('<I', len(blocks))
    offset = 4 + 8 * len(blocks)
    entries = b''
    for block in blocks:
        entries += struct.pack('<II', offset, len(block))
        offset += len(block)
    body += entries + b''.join(blocks)
    data = struct.pack('>I', len(body)) + body
    header = struct.pack('>IIII', 256, 256 + len(data), len(data), 50)
    return header + bytes(256 - 16) + data + bytes(50)


def chunks(data, size=65536):
    return [data[i:i + size] for i in range(0, len(data), size)]


class Image:
    def __init__(self):
        self.blocks = {}
        self.next_block = 32

    def allocate(self, count=1):
        start = self.next_block
        self.next_block += count
        assert self.next_block <= TOTAL_BLOCKS
        return start

    def put(self, number, data):
        assert len(data) <= BLOCK
        self.blocks[number] = bytes(data) + bytes(BLOCK - len(data))

    def bytes(self):
        out = bytearray(TOTAL_BLOCKS * BLOCK)
        for number, data in self.blocks.items():
            out[number * BLOCK:(number + 1) * BLOCK] = data
        return bytes(out)


def pad8(data):
    return data + bytes(-len(data) % 8)


def node(records, level, root, fixed):
    """A B-tree node; records are (key, value) pairs, value None for no value."""
    block = bytearray(BLOCK)
    flags = (1 if root else 0) | (2 if level == 0 else 0) | (4 if fixed else 0)
    entry = 4 if fixed else 8
    table = len(records) * entry
    keys = 56 + table
    values_end = BLOCK - (40 if root else 0)

    key_at = 0
    value_at = 0
    for i, (key, value) in enumerate(records):
        stored_key = key if fixed else pad8(key)
        block[keys + key_at:keys + key_at + len(key)] = key
        if value is None:
            voff = 0xFFFF
        else:
            value_at += len(value) if fixed else len(pad8(value))
            voff = value_at
            block[values_end - voff:values_end - voff + len(value)] = value
        if fixed:
            struct.pack_into('<HH', block, 56 + i * 4, key_at, voff)
        else:
            struct.pack_into('<HHHH', block, 56 + i * 8, key_at, len(key), voff, len(value))
        key_at += len(stored_key)
    free = values_end - value_at - (keys + key_at)
    assert free >= 0, 'node overflow'
    struct.pack_into('<HHI', block, 32, flags, level, len(records))
    struct.pack_into('<HHHHHHHH', block, 40, 0, table, key_at, free, 0xFFFF, 0, 0xFFFF, 0)
    return block


def pack(records, count, fixed):
    """Records in groups of up to count that fit a node, leaving room for the tree's info."""
    room = BLOCK - 56 - 40
    groups = [[]]
    used = 0
    for key, value in records:
        size = len(key) + len(value) + 4 if fixed else len(pad8(key)) + len(pad8(value)) + 8
        if groups[-1] and (len(groups[-1]) == count or used + size > room):
            groups.append([])
            used = 0
        groups[-1].append((key, value))
        used += size
    return groups


def build_tree(image, records, fixed, physical, leaf_size, index_size, subtype, key_size, value_size,
               allocate_virtual=None):
    """Writes a tree bottom up; returns the root's id (address if physical) and the virtual nodes."""
    virtual_nodes = []
    total_nodes = 0

    def tree_info():
        flags = 0x10 if physical else 0
        longest_key = max(len(k) for k, _ in records)
        longest_value = max(len(v) for _, v in records)
        return struct.pack('<IIIIIIQQ', flags, BLOCK, key_size, value_size, longest_key, longest_value,
                           len(records), total_nodes)

    level = 0
    current = records
    while True:
        groups = pack(current, leaf_size if level == 0 else index_size, fixed)
        is_root = len(groups) == 1
        parents = []
        for group in groups:
            total_nodes += 1
            if physical:
                oid = image.allocate()
                address = oid
            else:
                oid, address = allocate_virtual()
                virtual_nodes.append((oid, address))
            kind = (0x40000000 if physical else 0) | (2 if is_root else 3)
            block = node(group, level, is_root, fixed)
            object_header(block, oid, XID, kind, subtype)
            parents.append((group[0][0], struct.pack('<Q', oid), address, block, is_root))
        for _, _, address, block, is_root in parents:
            if is_root:
                block[BLOCK - 40:] = tree_info()
            image.put(address, seal(block))
        if is_root:
            return struct.unpack('<Q', parents[0][1])[0], virtual_nodes
        current = [(key, child) for key, child, _, _, _ in parents]
        level += 1


def map_key(oid, xid):
    return struct.pack('<QQ', oid, xid)


def map_value(address):
    return struct.pack('<IIQ', 0, BLOCK, address)


def write_object_map(image, entries):
    """entries: (oid, xid, address); returns the object map's address."""
    records = [(map_key(oid, xid), map_value(address)) for oid, xid, address in sorted(entries)]
    tree, _ = build_tree(image, records, True, True, MAP_LEAF_RECORDS, INDEX_RECORDS, 0xB, 16, 16)
    address = image.allocate()
    block = bytearray(BLOCK)
    object_header(block, address, XID, 0x4000000B)
    struct.pack_into('<IIIIQQQQQ', block, 32, 0, 0, 0x40000002, 0x40000002, tree, 0, 0, 0, 0)
    image.put(address, seal(block))
    return address


class Volume:
    def __init__(self, image, hashed):
        self.image = image
        self.hashed = hashed
        self.records = []
        self.next_id = 16
        self.children = {2: 0}
        self.counts = {'files': 0, 'folders': 1, 'links': 0}
        self.inodes = {}

    def new_id(self):
        self.next_id += 1
        return self.next_id - 1

    def key(self, oid, kind, rest=b''):
        return struct.pack('<Q', oid | kind << 60) + rest

    def write_data(self, data, fragmented=False):
        """Extents (logical, length, physical block) holding data."""
        extents = []
        blocks = (len(data) + BLOCK - 1) // BLOCK
        if fragmented:
            for i in range(blocks):
                address = self.image.allocate(2)
                self.image.put(address, data[i * BLOCK:(i + 1) * BLOCK])
                extents.append((i * BLOCK, BLOCK, address))
        elif blocks:
            start = self.image.allocate(blocks)
            for i in range(blocks):
                self.image.put(start + i, data[i * BLOCK:(i + 1) * BLOCK])
            extents.append((0, blocks * BLOCK, start))
        return extents

    def add_stream(self, stream, extents, counted=True):
        # Only a file's own stream has a reference count; an attribute's has none.
        if counted:
            self.records.append((self.key(stream, 6), struct.pack('<I', 1)))
        for logical, length, physical in extents:
            self.records.append((self.key(stream, 8, struct.pack('<Q', logical)),
                                 struct.pack('<QQQ', length, physical, 0)))

    def add_attribute(self, oid, name, value, system=False, stream=False):
        encoded = name.encode() + b'\0'
        key = self.key(oid, 4, struct.pack('<H', len(encoded)) + encoded)
        owned = 0x4 if system else 0
        if not stream and len(value) <= 3804:
            self.records.append((key, struct.pack('<HH', 0x2 | owned, len(value)) + value))
            return
        stream = self.new_id()
        extents = self.write_data(value)
        self.add_stream(stream, extents, counted=False)
        dstream = struct.pack('<QQQQQ', len(value), len(extents) and extents[0][1], 0, len(value), 0)
        self.records.append((key, struct.pack('<HH', 0x1 | owned, 48) + struct.pack('<Q', stream) + dstream))

    def add_entry(self, parent, name, oid, kind, sibling=None):
        encoded = name.encode() + b'\0'
        if self.hashed:
            rest = struct.pack('<I', name_hash(name) << 10 | len(encoded)) + encoded
        else:
            rest = struct.pack('<H', len(encoded)) + encoded
        value = struct.pack('<QQH', oid, NOW, kind)
        if sibling is not None:
            value += struct.pack('<HH', 1, 8) + struct.pack('<BBH', 1, 0, 8) + struct.pack('<Q', sibling)
        self.records.append((self.key(parent, 9, rest), value))
        self.children[parent] = self.children.get(parent, 0) + 1

    def add_inode(self, oid, parent, name, mode, links=1, size=None, flags=0, bsd_flags=0, uncompressed=0):
        fields = [(4, 0x2, name.encode() + b'\0')]
        if size is not None:
            fields.append((8, 0x20, struct.pack('<QQQQQ', size, -(-size // BLOCK) * BLOCK, 0, size, 0)))
        xfields = struct.pack('<HH', len(fields), sum(len(pad8(d)) for _, _, d in fields))
        xfields += b''.join(struct.pack('<BBH', t, f, len(d)) for t, f, d in fields)
        xfields += b''.join(pad8(d) for _, _, d in fields)
        self.inodes[oid] = (parent, mode, links, flags, bsd_flags, uncompressed, xfields)

    def finish_inodes(self):
        for oid, (parent, mode, links, flags, bsd_flags, uncompressed, xfields) in self.inodes.items():
            count = self.children.get(oid, 0) if mode & 0o170000 == S_IFDIR else links
            value = struct.pack('<QQQQQQQiIIIIIHHQ', parent, oid, NOW, NOW + 1000000000, NOW + 2000000000,
                                NOW + 3000000000, flags, count, 0, 0, bsd_flags, 501, 20, mode, 0, uncompressed)
            self.records.append((self.key(oid, 3), value + xfields))

    def file(self, parent, name, data, mode=0o644, fragmented=False):
        oid = self.new_id()
        extents = self.write_data(data, fragmented)
        if data:
            self.add_stream(oid, extents)
        self.add_inode(oid, parent, name, S_IFREG | mode, size=len(data) if data else None)
        self.add_entry(parent, name, oid, 8)
        self.counts['files'] += 1
        return oid

    def folder(self, parent, name):
        oid = self.new_id()
        self.add_inode(oid, parent, name, S_IFDIR | 0o755, flags=0x2)
        self.add_entry(parent, name, oid, 4)
        self.counts['folders'] += 1
        return oid

    def symlink(self, parent, name, target):
        oid = self.new_id()
        self.add_inode(oid, parent, name, S_IFLNK | 0o755)
        self.add_attribute(oid, 'com.apple.fs.symlink', target.encode() + b'\0', system=True)
        self.add_entry(parent, name, oid, 10)
        self.counts['links'] += 1

    def compressed(self, parent, name, header, resource, size):
        oid = self.new_id()
        # Has a resource fork or none, and the uncompressed size.
        flags = (0x4000 if resource else 0x8000) | 0x40000
        self.add_inode(oid, parent, name, S_IFREG | 0o644, flags=flags, bsd_flags=0x20, uncompressed=size)
        self.add_attribute(oid, 'com.apple.decmpfs', header, system=True)
        if resource is not None:
            self.add_attribute(oid, 'com.apple.ResourceFork', resource, stream=True)
        self.add_entry(parent, name, oid, 8)
        self.counts['files'] += 1

    def hard_link(self, first_parent, first_name, second_parent, second_name, data):
        oid = self.new_id()
        self.add_stream(oid, self.write_data(data))
        self.add_inode(oid, first_parent, first_name, S_IFREG | 0o644, links=2, size=len(data))
        for parent, name in ((first_parent, first_name), (second_parent, second_name)):
            sibling = self.new_id()
            encoded = name.encode() + b'\0'
            self.records.append((self.key(oid, 5, struct.pack('<Q', sibling)),
                                 struct.pack('<QH', parent, len(encoded)) + encoded))
            self.records.append((self.key(sibling, 12), struct.pack('<Q', oid)))
            self.add_entry(parent, name, oid, 8, sibling)
        self.counts['files'] += 1

    def sparse(self, parent, name):
        """Block 0, a hole of three blocks, then blocks 4 and 5 of which the file ends in 5."""
        oid = self.new_id()
        data = pattern('sparse', 6 * BLOCK)
        first = self.image.allocate()
        self.image.put(first, data[:BLOCK])
        last = self.image.allocate(2)
        self.image.put(last, data[4 * BLOCK:5 * BLOCK])
        self.image.put(last + 1, data[5 * BLOCK:])
        self.add_stream(oid, [(0, BLOCK, first), (BLOCK, 3 * BLOCK, 0), (4 * BLOCK, 2 * BLOCK, last)])
        size = 5 * BLOCK + 100
        self.add_inode(oid, parent, name, S_IFREG | 0o644, size=size, flags=0x200)
        self.add_entry(parent, name, oid, 8)
        self.counts['files'] += 1

    def sort_key(self, record):
        key = record[0]
        header = struct.unpack_from('<Q', key)[0]
        oid, kind = header & ((1 << 60) - 1), header >> 60
        rest = key[8:]
        if kind == 9 and self.hashed:
            (length_and_hash,) = struct.unpack_from('<I', rest)
            return (oid, kind, length_and_hash >> 10, rest[4:])
        if kind in (4, 9):
            return (oid, kind, 0, rest[2:])
        if kind in (5, 8):
            return (oid, kind, struct.unpack('<Q', rest)[0], b'')
        return (oid, kind, 0, rest)


def populate(volume):
    root = 2
    volume.add_inode(2, 1, 'root', S_IFDIR | 0o755, flags=0x2)
    volume.add_inode(3, 1, 'private-dir', S_IFDIR | 0o755, flags=0x2)
    volume.add_entry(1, 'root', 2, 4)
    volume.add_entry(1, 'private-dir', 3, 4)

    volume.file(root, 'hello.txt', b'Hello, disk image.\n', mode=0o640)
    volume.file(root, 'empty', b'')
    folder = volume.folder(root, 'Folder')
    nested = volume.folder(folder, 'nested')
    volume.file(nested, 'deep.txt', pattern('deep', 5000))
    volume.file(root, 'Café ünïcode.txt', b'composed\n')
    volume.file(root, 'Café decomposed.txt', b'decomposed\n')
    volume.file(root, 'a:b.txt', b'colon\n')
    volume.symlink(root, 'link', 'hello.txt')
    volume.symlink(root, 'Applications', '/Applications')
    volume.hard_link(root, 'original.bin', folder, 'hardlink.bin', pattern('linked', 9000))
    volume.file(root, 'fragmented.bin', pattern('fragmented', 40 * BLOCK), fragmented=True)
    volume.sparse(root, 'sparse.bin')

    filler = volume.folder(root, 'filler')
    for count in range(1, 451):
        volume.file(filler, f'{count:04d}', pattern(f'filler {count}', BLOCK))

    sizes = {
        'zlib-inline.txt': 3000,
        'stored-inline.txt': 700,
        'zlib-resource.txt': 200000,
        'lzvn-inline.txt': 2000,
        'lzvn-resource.txt': 3000,
        'lzfse-resource.txt': 150000,
    }
    for name, size in sizes.items():
        data = pattern(name, size)
        header, resource = {
            'zlib-inline.txt': lambda d: (decmpfs(3, len(d), zlib.compress(d)), None),
            'stored-inline.txt': lambda d: (decmpfs(3, len(d), b'\xff' + d), None),
            'zlib-resource.txt': lambda d: (decmpfs(4, len(d)), zlib_resource([zlib.compress(c) for c in chunks(d)])),
            'lzvn-inline.txt': lambda d: (decmpfs(7, len(d), lzvn_encode(d)), None),
            'lzvn-resource.txt': lambda d: (decmpfs(8, len(d)), blocks_table([lzvn_encode(d)])),
            'lzfse-resource.txt': lambda d: (decmpfs(12, len(d)),
                                             blocks_table([lzfse_encode(c) for c in chunks(d)])),
        }[name](data)
        volume.compressed(root, name, header, resource, size)


def make(hashed):
    image = Image()
    volume = Volume(image, hashed)
    populate(volume)
    volume.finish_inodes()
    records = sorted(volume.records, key=volume.sort_key)

    # Every node of the tree has an older version in the map, pointing at an empty block.
    stale = image.allocate()
    mapped = []
    next_oid = [2000]

    def allocate_virtual():
        oid = next_oid[0]
        next_oid[0] += 1
        address = image.allocate()
        mapped.append((oid, XID, address))
        mapped.append((oid, XID - 3, stale))
        return oid, address

    root_tree, _ = build_tree(image, records, False, False, LEAF_RECORDS, INDEX_RECORDS, 0xE, 0, 0,
                              allocate_virtual)
    volume_map = write_object_map(image, mapped)

    superblock = image.allocate()
    block = bytearray(BLOCK)
    object_header(block, VOLUME_OID, XID, 0xD)
    incompatible = 0x1 if hashed else 0x0
    struct.pack_into('<4sIQQQQQQQ', block, 32, b'APSB', 0, 0x2, 0, incompatible, NOW, 0, 0,
                     image.next_block)
    struct.pack_into('<III', block, 116, 0x2, 0x40000002, 0x40000002)
    struct.pack_into('<QQQQQQQQQQQQQQ', block, 128, volume_map, root_tree, 0, 0, 0, 0, volume.next_id,
                     volume.counts['files'], volume.counts['folders'], volume.counts['links'], 0, 0,
                     image.next_block, 0)
    block[240:256] = uuid.UUID('6f1e2d3c-4b5a-4968-8776-a5b4c3d2e1f0').bytes
    struct.pack_into('<QQ', block, 256, NOW, 0x1)
    block[272:272 + 32] = b'make-apfs-image.py'.ljust(32, b'\0')
    block[704:704 + len(VOLUME_NAME)] = VOLUME_NAME.encode()
    image.put(superblock, seal(block))

    container_map = write_object_map(image, [(VOLUME_OID, XID, superblock)])

    def container(xid):
        block = bytearray(BLOCK)
        object_header(block, 1, xid, 0x80000001)
        struct.pack_into('<4sIQQQQ', block, 32, b'NXSB', BLOCK, TOTAL_BLOCKS, 0, 0, 0x2)
        block[72:88] = uuid.UUID('0a1b2c3d-4e5f-4061-8273-94a5b6c7d8e9').bytes
        struct.pack_into('<QQIIQQIIIIII', block, 88, 3000, xid + 1, 8, 8, 1, 9, 4, 0, 0, 2, 0, 0)
        struct.pack_into('<QQQII', block, 152, 0, container_map, 0, 0, 1)
        struct.pack_into('<Q', block, 184, VOLUME_OID)
        return seal(block)

    def checkpoint_map(xid, address):
        block = bytearray(BLOCK)
        object_header(block, address, xid, 0x4000000C)
        struct.pack_into('<II', block, 32, 1, 0)
        return seal(block)

    # The current checkpoint first, an older one after it.
    image.put(0, container(XID))
    image.put(1, container(XID))
    image.put(2, checkpoint_map(XID, 2))
    image.put(3, container(XID - 3))
    image.put(4, checkpoint_map(XID - 3, 4))
    return image.bytes()


LZFSE = lzfse_library()


def main():
    if len(sys.argv) != 3 or sys.argv[2] not in ('hashed', 'plain'):
        sys.exit(f'usage: {sys.argv[0]} <output.img.gz> hashed|plain')
    data = make(sys.argv[2] == 'hashed')
    with gzip.GzipFile(sys.argv[1], 'wb', mtime=0) as out:
        out.write(data)


if __name__ == '__main__':
    main()
