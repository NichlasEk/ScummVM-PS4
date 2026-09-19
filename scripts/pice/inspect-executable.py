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


def inspect_fixups(data, image):
    """Describe the internal fixup variants used by these DOS executables.

    No relocated image is emitted. External, chained, additive and list
    encodings are rejected rather than interpreted speculatively.
    """
    le = image['le_offset']
    def u32(offset):
        if offset < 0 or offset+4 > len(data):
            raise ValueError('truncated fixup field')
        return struct.unpack_from('<I', data, offset)[0]
    table = le+u32(le+104)
    records = le+u32(le+108)
    limit = le+u32(le+112)
    pages = image['page_count']
    if table < le+196 or table+4*(pages+1) > records or records > limit or limit > len(data):
        raise ValueError('invalid fixup table bounds')
    offsets = [u32(table+4*i) for i in range(pages+1)]
    if offsets[0] or offsets[-1] > limit-records or any(a>b for a,b in zip(offsets,offsets[1:])):
        raise ValueError('invalid fixup page offsets')
    owners = {}
    objtab = le+u32(le+64)
    for obj in image['objects']:
        first = u32(objtab+(obj['object']-1)*24+12)-1
        for ordinal in range(len(obj['pages'])):
            if first+ordinal in owners:
                raise ValueError('overlapping object page maps')
            owners[first+ordinal] = (obj, ordinal)
    result = []
    widths = {2: 2, 6: 6, 7: 4}
    for page in range(pages):
        pos, end = records+offsets[page], records+offsets[page+1]
        while pos < end:
            if end-pos < 5:
                raise ValueError('truncated fixup record')
            source_type, flags, source, target_object = struct.unpack_from('<BBhB', data, pos)
            pos += 5
            if source_type not in widths or flags not in (0,16):
                raise ValueError('unsupported fixup encoding')
            target = 0
            if source_type != 2:
                width = 4 if flags & 16 else 2
                if end-pos < width:
                    raise ValueError('truncated fixup target')
                target = int.from_bytes(data[pos:pos+width], 'little')
                pos += width
            if page not in owners or not 1 <= target_object <= len(image['objects']):
                raise ValueError('invalid fixup object')
            obj, ordinal = owners[page]
            source_offset = ordinal*image['page_size']+source
            if source_offset < 0 or source_offset+widths[source_type] > obj['virtual_size']:
                raise ValueError(f'fixup outside object bounds: source={obj["object"]}:{source_offset:x}, target={target_object}:{target:x}')
            result.append({'source_object': obj['object'], 'source_offset': source_offset,
                           'source_type': source_type, 'target_object': target_object,
                           'target_offset': target,
                           'target_within_object': target < image['objects'][target_object-1]['virtual_size']})
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('--fixups', action='store_true', help='include internal relocation metadata')
    args = parser.parse_args()
    try:
        data = args.executable.read_bytes()
        result = inspect(data)
        if args.fixups:
            result['fixups'] = inspect_fixups(data, result)
        print(json.dumps(result, indent=2))
    except (ValueError, OSError) as error:
        parser.exit(1, f'{error}\n')


if __name__ == '__main__':
    main()
