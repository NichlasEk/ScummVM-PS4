#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Inspect embedded DOS/4G LE objects without exporting commercial code/data.

Reports file/object offsets for follow-up reverse engineering. This is not a
loader: fixups are not applied, and offsets must not be treated as relocated
runtime addresses. Format reference: Open Watcom bld/watcom/h/exeflat.h.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path


def inspect(data):
    def u32(offset):
        if offset < 0 or offset + 4 > len(data):
            raise ValueError('truncated field')
        return struct.unpack_from('<I', data, offset)[0]

    # DOS extenders may prepend another MZ/BW program. Resolve e_lfanew
    # relative to each candidate MZ rather than assuming the outer header.
    candidates = []
    start = 0
    while True:
        mz = data.find(b'MZ', start)
        if mz < 0:
            break
        start = mz + 2
        if mz + 64 > len(data):
            continue
        le = mz + u32(mz + 60)
        if data[le:le + 4] == b'LE\0\0':
            candidates.append((mz, le))
    if len(candidates) != 1:
        raise ValueError(f'expected one embedded little-endian LE executable, found {len(candidates)}')
    mz, le = candidates[0]
    if le + 196 > len(data):
        raise ValueError('truncated LE header')
    pages, entry_object, entry_offset = (u32(le + n) for n in (20, 24, 28))
    page_size, last_size = u32(le + 40), u32(le + 44)
    objects_at, object_count = le + u32(le + 64), u32(le + 68)
    map_at, page_at = le + u32(le + 72), mz + u32(le + 128)
    if not pages or not page_size or not 0 < last_size <= page_size:
        raise ValueError('unsupported page geometry')
    if not 0 < object_count <= 65536 or objects_at + 24 * object_count > len(data):
        raise ValueError('invalid object table')
    if map_at + 4 * pages > len(data):
        raise ValueError('truncated page map')
    if page_at + (pages - 1) * page_size + last_size > len(data):
        raise ValueError('truncated page data')
    objects = []
    entry_file = None
    for index in range(object_count):
        size, base, flags, first, count, reserved = struct.unpack_from('<6I', data, objects_at + index * 24)
        if reserved or (count and (first < 1 or first + count - 1 > pages)):
            raise ValueError('invalid object page range')
        mappings = []
        for ordinal in range(count):
            record = map_at + (first - 1 + ordinal) * 4
            # LE's 24-bit physical page index is big endian, unlike its header.
            physical = int.from_bytes(data[record:record + 3], 'big')
            kind = data[record + 3]
            if kind != 0 or not 1 <= physical <= pages:
                raise ValueError('unsupported nonresident/iterated page')
            file_offset = page_at + (physical - 1) * page_size
            length = last_size if physical == pages else page_size
            mappings.append({'object_offset': ordinal * page_size, 'file_offset': file_offset, 'stored_size': length})
            if index + 1 == entry_object and ordinal * page_size <= entry_offset < ordinal * page_size + length:
                entry_file = file_offset + entry_offset - ordinal * page_size
        objects.append({'object': index + 1, 'virtual_size': size, 'preferred_base': base,
                        'flags': flags, 'executable': bool(flags & 4), 'default_32bit': bool(flags & 0x2000),
                        'pages': mappings})
    if not 1 <= entry_object <= object_count or entry_offset >= objects[entry_object - 1]['virtual_size'] or entry_file is None:
        raise ValueError('entry point is not backed by a stored object page')
    return {'sha256': hashlib.sha256(data).hexdigest(), 'size': len(data),
            'embedded_mz_offset': mz, 'le_offset': le, 'page_data_offset': page_at,
            'page_size': page_size, 'page_count': pages,
            'entry': {'object': entry_object, 'object_offset': entry_offset, 'file_offset': entry_file},
            'fixups_applied': False, 'objects': objects}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(inspect(args.executable.read_bytes()), indent=2))
    except (ValueError, OSError) as error:
        parser.exit(1, f'{error}\n')


if __name__ == '__main__':
    main()
