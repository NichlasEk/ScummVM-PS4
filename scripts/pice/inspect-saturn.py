#!/usr/bin/env python3
"""Read-only Saturn resource inspection; optional locally generated menu atlas.

Input is a directory containing intact extracted KVGA/J_KLANG/KSOUND KRO,
OUT.BIN and MOVIE files. Raw XA sectors are not extracted or converted here.
The JSON contains metadata only; game data must remain outside the repository.
"""
import argparse
import importlib.util
import json
from pathlib import Path
import struct

spec = importlib.util.spec_from_file_location('kro', Path(__file__).with_name('audit-resources.py'))
kro = importlib.util.module_from_spec(spec)
spec.loader.exec_module(kro)


def menu_records(data):
    if len(data) < 8:
        raise ValueError('short OUT.BIN')
    count = struct.unpack_from('>I', data)[0]
    if not 1 <= count <= 4096 or 4 + count*4 > len(data):
        raise ValueError('invalid menu directory')
    offsets = list(struct.unpack_from('>' + str(count) + 'I', data, 4)) + [len(data)]
    if offsets[0] != 4 + count*4 or any(a >= b for a, b in zip(offsets, offsets[1:])):
        raise ValueError('invalid menu offsets')
    textures = []
    for i, (start, end) in enumerate(zip(offsets, offsets[1:])):
        block = data[start:end]
        if len(block) < 4:
            continue
        w, h = struct.unpack_from('>HH', block)
        if 0 < w <= 640 and 0 < h <= 480 and w*h % 2 == 0 and len(block) == 4 + w*h//2:
            textures.append({'id': i, 'offset': start, 'bytes': len(block), 'width': w, 'height': h})
    return count, textures


def movie_info(path):
    data = path.read_bytes()
    if len(data) < 64 or data[:4] != b'FILM' or data[16:20] != b'FDSC' or data[48:52] != b'STAB':
        raise ValueError(f'{path.name}: unsupported FILM header')
    header = struct.unpack_from('>I', data, 4)[0]
    table_size, timebase, count = struct.unpack_from('>III', data, 52)
    if not timebase or table_size != 16 + count*16 or header != 48 + table_size or header > len(data):
        raise ValueError(f'{path.name}: invalid sample directory')
    audio, video = 0, 0
    for i in range(count):
        offset, size, time, duration = struct.unpack_from('>4I', data, 64+i*16)
        if offset + size > len(data) - header:
            raise ValueError(f'{path.name}: sample outside file')
        if time == 0xffffffff:
            audio += 1
        else:
            video += 1
    height, width = struct.unpack_from('>II', data, 28)
    return {'file': path.name, 'codec': data[24:28].decode('ascii'), 'width': width,
            'height': height, 'timebase': timebase, 'video_packets': video, 'audio_packets': audio}


def atlas(data, records, output):
    # This is a glyph inspection sheet, not the Saturn menu layout/palette.
    from PIL import Image, ImageDraw
    if not records:
        raise ValueError('no menu textures')
    cell_w = max(r['width']*2 for r in records) + 32
    cell_h = max(r['height']*2 for r in records) + 24
    sheet = Image.new('RGB', (cell_w*3, ((len(records)+2)//3)*cell_h), (25, 25, 25))
    draw = ImageDraw.Draw(sheet)
    for i, r in enumerate(records):
        pixels = []
        for byte in data[r['offset']+4:r['offset']+r['bytes']]:
            pixels.extend([(byte >> 4)*17, (byte & 15)*17])
        tile = Image.new('L', (r['width'], r['height']))
        tile.putdata(pixels)
        tile = tile.resize((r['width']*2, r['height']*2), Image.Resampling.NEAREST)
        x, y = (i % 3)*cell_w, (i//3)*cell_h
        draw.text((x, y), str(r['id']), fill='yellow')
        sheet.paste(tile, (x+22, y+12))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--menu-atlas', type=Path, help='Optional local PNG; requires Pillow')
    args = parser.parse_args()
    archives = [kro.inspect(args.directory/name) for name in ('KVGA.KRO', 'J_KLANG.KRO', 'KSOUND.KRO')]
    data = (args.directory/'OUT.BIN').read_bytes()
    count, textures = menu_records(data)
    movies = [movie_info(p) for p in sorted((args.directory/'MOVIE').glob('*.CPK'))]
    report = {'archives': archives, 'menu_records': count, 'menu_textures': textures,
              'movies': movies, 'runtime_supported': False}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2)+'\n')
    if args.menu_atlas:
        atlas(data, textures, args.menu_atlas)
    print(f"KRO entries: {sum(len(a['entries']) for a in archives)}; "
          f"menu textures: {len(textures)}/{count}; FILM directories: {len(movies)}")
    print('This is a resource audit, not a playable Saturn engine target.')


if __name__ == '__main__':
    main()
