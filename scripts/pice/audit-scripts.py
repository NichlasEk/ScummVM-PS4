#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Audit English EU SM script entry tables; emit metadata, never script bytes."""
import argparse
import json
import struct
import zlib
from pathlib import Path


def inspect(path):
    archive = path.read_bytes()
    if archive[:4] != b'Burp' or len(archive) < 8:
        raise ValueError('invalid KRO header')
    count, = struct.unpack_from('<I', archive, 4)
    if count != 1338 or 8+count*20 > len(archive):
        raise ValueError('requires English EU KSVGA layout')
    banks, placeholders = [], []
    # SM02..SM12 group boundaries recovered from ICE640 object 1, 0xd60.
    for resource in range(1138, 1306):
        size, packed, reserved, offset, flag = struct.unpack_from('<5I', archive, 8+resource*20)
        if reserved or flag not in (0, 4) or size > 131072 or offset+packed > len(archive):
            raise ValueError(f'invalid resource {resource}')
        data = archive[offset:offset+packed]
        if flag == 4:
            decoder = zlib.decompressobj(-15)
            data = decoder.decompress(data, size+1)
            if not decoder.eof or decoder.unconsumed_tail or decoder.unused_data:
                raise ValueError('invalid deflate stream')
        if len(data) != size:
            raise ValueError('resource size mismatch')
        if data == b'dummy':
            placeholders.append(resource)
            continue
        if len(data) < 4:
            raise ValueError('short script')
        entries, = struct.unpack_from('<H', data)
        if not 0 < entries <= 50 or entries*2 > len(data):
            raise ValueError('invalid script count')
        offsets = [word*2 for word in struct.unpack_from('<'+'H'*entries, data)]
        if any(o < entries*2 or o+2 > len(data) for o in offsets) or offsets != sorted(offsets):
            raise ValueError('invalid entry offset')
        fnv = 2166136261
        for value in data:
            fnv = ((fnv ^ value)*16777619) & 0xffffffff
        banks.append({'resource': resource, 'entries': entries, 'offsets': offsets,
                      'entry_opcodes': [struct.unpack_from('<H', data, o)[0] for o in offsets],
                      'fnv': f'{fnv:08x}'})
    return {'banks': banks, 'entries': sum(b['entries'] for b in banks), 'placeholders': placeholders}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(inspect(args.archive), indent=2))
    except (OSError, ValueError) as error:
        parser.exit(1, str(error)+'\n')
