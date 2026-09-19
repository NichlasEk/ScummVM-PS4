#!/usr/bin/env python3
"""Read-only format audit/oracle. Reports hashes, never exports game assets."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib


def fnv(data):
    value = 2166136261
    for c in data:
        value = ((value ^ c) * 16777619) & 0xffffffff
    return f'{value:08x}'


def inspect(path):
    data = path.read_bytes()
    if len(data) < 8 or data[:4] != b'Burp':
        raise ValueError(f'{path}: bad header')
    count = struct.unpack_from('<I', data, 4)[0]
    end = 8 + count * 20
    if not 0 < count <= 65536 or end > len(data):
        raise ValueError(f'{path}: invalid directory')
    report = {'file': path.name, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(), 'entries': []}
    for i in range(count):
        size, packed, reserved, offset, flag = struct.unpack_from('<5I', data, 8 + 20*i)
        if reserved or flag not in (0, 4) or offset < end or offset + packed > len(data) or max(size, packed) > 64*1024*1024:
            raise ValueError(f'{path}:{i}: invalid entry')
        payload = data[offset:offset+packed]
        if flag == 4:
            decoder = zlib.decompressobj(-15)
            payload = decoder.decompress(payload, size+1)
            if not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
                raise ValueError(f'{path}:{i}: invalid deflate stream')
        if len(payload) != size:
            raise ValueError(f'{path}:{i}: length mismatch')
        entry = {'id': i, 'size': size, 'packed': packed, 'offset': offset, 'flags': flag,
                 'sha256': hashlib.sha256(payload).hexdigest()}
        if len(payload) == 768:
            entry['kind'] = 'palette-candidate'
        elif payload[:4] == b'RIFF':
            entry['kind'] = 'riff'
        elif len(payload) >= 4:
            w, h = struct.unpack_from('<HH', payload)
            if 0 < w <= 4096 and 0 < h <= 4096 and len(payload) == 4+w*h:
                entry.update(kind='indexed-image', width=w, height=h)
        if (path.name.upper(), i) in [('KSVGA.KRO',815),('KSVGA.KRO',816),('E_KLANG.KRO',0)]:
            entry['fnv'] = fnv(payload)
        report['entries'].append(entry)
        end = offset + packed
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game_directory', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    reports = [inspect(p) for p in sorted(args.game_directory.iterdir()) if p.suffix.lower() == '.kro']
    if not reports:
        parser.error('no KRO files in this directory')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(reports, indent=2)+'\n')
    for r in reports:
        print(f"{r['file']}: {len(r['entries'])} resources verified")
        for e in r['entries']:
            if 'fnv' in e:
                print(f"  oracle resource {e['id']}: {e['fnv']}")


if __name__ == '__main__':
    main()
