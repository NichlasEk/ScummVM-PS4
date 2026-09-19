#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Independent oracle for the English CD language resource-name index."""
import argparse
import hashlib
import json
import struct
from pathlib import Path


def inspect(data, archive_count):
    if len(data) < 9 or len(data) > 1024*1024 or data[-1] != 0:
        raise ValueError('unsupported index envelope')
    length, = struct.unpack_from('<I', data)
    if length != len(data)-5:
        raise ValueError('invalid declared length')
    decoded = bytearray()
    previous = 0x5a
    for value in data[4:-1]:
        previous ^= value
        decoded.append(previous)
    count, = struct.unpack_from('<I', decoded)
    if not 0 < count <= 65536 or len(decoded) != 4+count*12:
        raise ValueError('invalid name table size')
    entries = []
    for raw_name, base in struct.iter_unpack('<8sI', decoded[4:]):
        name = raw_name.rstrip(b'\0')
        if not name or any(c not in b'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_' for c in name):
            raise ValueError('invalid name/padding')
        if base >= archive_count or (entries and base <= entries[-1]['base']):
            raise ValueError('invalid archive base')
        entries.append({'name': name.decode('ascii'), 'base': base})
    if len({entry['name'] for entry in entries}) != count:
        raise ValueError('duplicate names')
    fnv = 2166136261
    for value in data:
        fnv = ((fnv ^ value)*16777619) & 0xffffffff
    return {'groups': count, 'encoded_fnv': f'{fnv:08x}',
            'sha256': hashlib.sha256(data).hexdigest(), 'entries': entries}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('index', type=Path)
    parser.add_argument('--archive-count', type=int, required=True)
    args = parser.parse_args()
    try:
        print(json.dumps(inspect(args.index.read_bytes(), args.archive_count), indent=2))
    except (OSError, ValueError) as error:
        parser.exit(1, str(error)+'\n')


if __name__ == '__main__':
    main()
