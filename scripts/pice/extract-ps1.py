#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Extract a single-track MODE2/2352 PICE disc, preserving STR/XA raw sectors.
The input is a user-owned BIN file, not a ZIP or a cooked ISO. No assets shipped.
"""
import argparse
import json
from pathlib import Path, PurePosixPath
import struct


def extract(source, output):
    if output.exists() and any(output.iterdir()):
        raise ValueError('output directory must be empty')
    size = source.stat().st_size
    if not size or size % 2352:
        raise ValueError('expected whole 2352-byte sectors')
    rows, visited = [], set()
    with source.open('rb') as disc:
        def sector(lba):
            if lba < 0 or (lba+1)*2352 > size:
                raise ValueError('extent outside disc')
            disc.seek(lba*2352)
            block = disc.read(2352)
            if block[:12] != b'\0'+b'\xff'*10+b'\0' or block[15] != 2 or block[16:20] != block[20:24]:
                raise ValueError('expected a raw Mode 2 sector with matching subheaders')
            return block

        def read_data(lba, length):
            data = bytearray()
            for i in range((length+2047)//2048):
                block = sector(lba+i)
                if block[18] & 0x20:
                    raise ValueError('Form 2 media must retain raw sectors')
                data += block[24:2072]
            return bytes(data[:length])

        def directory(lba, length, prefix='', depth=0):
            if depth > 8 or length > 1024*1024 or (lba, length) in visited:
                raise ValueError('invalid/recursive directory')
            visited.add((lba, length))
            data, pos = read_data(lba, length), 0
            while pos < len(data):
                count = data[pos]
                if not count:
                    pos = (pos//2048+1)*2048
                    continue
                row = data[pos:pos+count]
                pos += count
                if len(row) != count or count < 34 or 33+row[32] > count:
                    raise ValueError('short directory record')
                name = row[33:33+row[32]]
                if name in (b'\0', b'\1'):
                    continue
                name = name.decode('ascii').split(';')[0]
                if not name or name in ('.', '..') or '/' in name or '\\' in name:
                    raise ValueError('unsafe ISO filename')
                at, n = struct.unpack_from('<I', row, 2)[0], struct.unpack_from('<I', row, 10)[0]
                if (at+(n+2047)//2048)*2352 > size or row[25] & 0x80:
                    raise ValueError('unsupported file extent')
                path = prefix+name
                if row[25] & 2:
                    directory(at, n, path+'/', depth+1)
                else:
                    rows.append(dict(path=path, lba=at, logical_bytes=n))

        pvd = read_data(16, 2048)
        if pvd[:7] != b'\1CD001\1':
            raise ValueError('no primary ISO9660 volume at sector 16')
        root = pvd[156:]
        directory(struct.unpack_from('<I', root, 2)[0], struct.unpack_from('<I', root, 10)[0])
        if len({r['path'] for r in rows}) != len(rows):
            raise ValueError('duplicate file paths')
        output.mkdir(parents=True, exist_ok=True)
        for row in rows:
            path = output.joinpath(*PurePosixPath(row['path']).parts)
            path.parent.mkdir(parents=True, exist_ok=True)
            raw = path.suffix.upper() in ('.STR', '.XA')
            with path.open('xb') as target:
                for i in range((row['logical_bytes']+2047)//2048):
                    block = sector(row['lba']+i)
                    if raw:
                        target.write(block)
                    else:
                        if block[18] & 0x20:
                            raise ValueError('unexpected Form 2 non-media file')
                        target.write(block[24:24+min(2048, row['logical_bytes']-i*2048)])
            row['raw_sectors'] = raw
            row['extracted_bytes'] = path.stat().st_size
        (output/'extraction.json').write_text(json.dumps(rows, indent=2)+'\n')
    print(f'{len(rows)} files extracted; STR/XA sectors preserved')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bin', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    extract(args.bin, args.output)
