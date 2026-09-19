#!/usr/bin/env python3
"""Independent original sprite-format oracle; writes metadata/hashes, no assets."""
import argparse
import collections
import importlib.util
import json
from pathlib import Path
import struct
import zlib

spec = importlib.util.spec_from_file_location('kro_audit', Path(__file__).with_name('audit-resources.py'))
kro = importlib.util.module_from_spec(spec)
spec.loader.exec_module(kro)


def decode(data):
    if len(data) < 4:
        raise ValueError('short bitmap')
    w, h = struct.unpack_from('<HH', data)
    if not 0 < w <= 4096 or not 0 < h <= 4096:
        raise ValueError('dimensions')
    pixels, mask = bytearray(w*h), bytearray(w*h)
    pos = 4
    for y in range(h):
        if pos >= len(data):
            raise ValueError('short row')
        segments = data[pos]
        pos += 1
        x = 0
        for _ in range(segments):
            if pos+3 > len(data):
                raise ValueError('short span')
            skip, groups, remainder = data[pos:pos+3]
            pos += 3
            x += skip
            count = groups*4 + remainder
            if remainder > 3 or x+count > w or pos+count > len(data):
                raise ValueError('span bounds')
            pixels[y*w+x:y*w+x+count] = data[pos:pos+count]
            mask[y*w+x:y*w+x+count] = b'\xff'*count
            pos += count
            x += count
        if pos >= len(data) or x+data[pos] != w:
            raise ValueError('row width')
        pos += 1
    if data[pos:] not in (b'', b'\0\0'):
        raise ValueError('bitmap tail')
    return {'width': w, 'height': h, 'pixel_fnv': kro.fnv(pixels), 'mask_fnv': kro.fnv(mask)}


def inspect(path):
    index = kro.inspect(path)
    raw = path.read_bytes()
    counts = collections.Counter()
    examples, unsupported, failures = [], [], []
    for entry in index['entries']:
        blob = raw[entry['offset']:entry['offset']+entry['packed']]
        if entry['flags'] == 4:
            blob = zlib.decompress(blob, -15)  # Already size-validated by the KRO audit.
        if len(blob) < 24 or struct.unpack_from('<I', blob)[0] != 24:
            continue
        a, b, c, reserved, p, end = struct.unpack_from('<6I', blob)
        if reserved or not 24 <= b <= c <= p <= end == len(blob) or end-p != 768:
            unsupported.append(entry['id'])
            continue
        try:
            first = struct.unpack_from('<I', blob, b)[0]
            if first < 6 or (first-2) % 4 or first > c-b:
                raise ValueError('bitmap offset table')
            n = (first-2)//4
            offsets = list(struct.unpack_from('<' + str(n)+'I', blob, b)) + [c-b]
            if offsets[0] != first or any(x >= y for x,y in zip(offsets, offsets[1:])):
                raise ValueError('bitmap offsets')
            for i, (start, stop) in enumerate(zip(offsets, offsets[1:])):
                result = decode(blob[b+start:b+stop])
                counts['bitmaps'] += 1
                if entry['id'] == 0 and i == 0:
                    examples.append({'resource': 0, 'bitmap': 0, **result, 'palette_fnv': kro.fnv(blob[p:end])})
            counts['resources'] += 1
        except (ValueError, struct.error) as error:
            failures.append({'resource': entry['id'], 'error': str(error)})
    return {'archive': path.name, 'counts': dict(counts), 'oracles': examples,
            'unsupported_header_variants': unsupported, 'failures': failures}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = inspect(args.archive)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))
    if result['failures']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
