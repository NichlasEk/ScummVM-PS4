#!/usr/bin/env python3
"""Generate small PNGs and pixel oracles without using libpng or game data."""
import pathlib
import struct
import sys
import zlib

out = pathlib.Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
width, height = 19, 13  # Odd dimensions exercise packed rows and short Adam7 passes.


def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))


def paeth(a, b, c):
    p = a + b - c
    return min((a, b, c), key=lambda x: abs(p - x))


def pack(samples, depth):
    if depth == 16:
        return b''.join(struct.pack('>H', s) for s in samples)
    if depth == 8:
        return bytes(samples)
    result = bytearray((len(samples) * depth + 7) // 8)
    for i, value in enumerate(samples):
        result[i * depth // 8] |= value << (8 - depth - i * depth % 8)
    return bytes(result)


def make_png(depth, color, interlace):
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[color]
    max_sample = (1 << depth) - 1
    palette = [(i * 37 % 256, i * 19 % 256, i * 73 % 256) for i in range(1 << min(depth, 8))]
    alpha = [(i * 51) % 256 for i in range(len(palette))]
    pixels, rgba = [], bytearray()
    for y in range(height):
        row = []
        for x in range(width):
            sample = tuple((x * 7919 + y * 3947 + c * 11003) & max_sample for c in range(channels))
            row.append(sample)
            scaled = tuple(s >> 8 if depth == 16 else s * 255 // max_sample for s in sample)
            if color == 3:
                rgba.extend((*palette[sample[0]], alpha[sample[0]]))
            elif color in (0, 4):
                rgba.extend((scaled[0], scaled[0], scaled[0], scaled[1] if color == 4 else (0 if sample[0] == 0 else 255)))
            else:
                rgba.extend((*scaled[:3], scaled[3] if color == 6 else (0 if sample == (0, 0, 0) else 255)))
        pixels.append(row)
    raw = bytearray()
    passes = [(0, 0, 1, 1)] if not interlace else [(0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8), (2, 0, 4, 4), (0, 2, 2, 4), (1, 0, 2, 2), (0, 1, 1, 2)]
    bpp = max(1, (channels * depth + 7) // 8)
    for x0, y0, dx, dy in passes:
        previous = None
        for row_index, y in enumerate(range(y0, height, dy)):
            row = pack([s for x in range(x0, width, dx) for s in pixels[y][x]], depth)
            previous = previous or bytes(len(row))
            mode = row_index % 5
            raw.append(mode)
            for i, value in enumerate(row):
                a, b, c = (row[i - bpp] if i >= bpp else 0), previous[i], (previous[i - bpp] if i >= bpp else 0)
                predictor = (0, a, b, (a + b) // 2, paeth(a, b, c))[mode]
                raw.append((value - predictor) & 255)
            previous = row
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, depth, color, 0, 0, interlace))
    if color == 3:
        png += chunk(b'PLTE', bytes(v for entry in palette for v in entry)) + chunk(b'tRNS', bytes(alpha))
    elif color == 0:
        png += chunk(b'tRNS', struct.pack('>H', 0))
    elif color == 2:
        png += chunk(b'tRNS', struct.pack('>HHH', 0, 0, 0))
    data = zlib.compress(raw)
    # Split compressed stream across chunks, including an empty IDAT.
    png += chunk(b'IDAT', data[:7]) + chunk(b'IDAT', b'') + chunk(b'IDAT', data[7:]) + chunk(b'IEND', b'')
    return png, rgba


names, expected = [], []
for color, depths in [(0, [1, 2, 4, 8, 16]), (2, [8, 16]), (3, [1, 2, 4, 8]), (4, [8, 16]), (6, [8, 16])]:
    for depth in depths:
        for interlace in (0, 1):
            name = f'c{color}-d{depth}-i{interlace}.png'
            png, rgba = make_png(depth, color, interlace)
            (out / name).write_bytes(png)
            names.append(name)
            hash_value = 2166136261
            for value in rgba:
                hash_value = ((hash_value ^ value) * 16777619) & 0xffffffff
            expected.append(f'{name} {width}x{height} RGBA fnv1a={hash_value:08x}')
valid, _ = make_png(8, 6, 0)
bad = bytearray(valid)
bad[29] ^= 1  # IHDR CRC
(out / 'bad-crc.png').write_bytes(bad)
(out / 'truncated.png').write_bytes(valid[:-18])
(out / 'invalid.png').write_bytes(b'not a PNG')
(out / 'files.txt').write_text('\n'.join(names) + '\n')
expected += [name + ' rejected' for name in ('bad-crc.png', 'truncated.png', 'invalid.png')]
(out.parent / 'expected.txt').write_text('\n'.join(expected + ['PASS']) + '\n')
