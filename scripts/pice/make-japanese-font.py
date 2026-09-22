#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Create an external PJF1 Unicode bitmap font from a user-supplied Japanese TTF (Pillow required).
No font artwork is included in this script. Respect the source font's license.
"""
import argparse
from pathlib import Path
import struct
from PIL import Image, ImageDraw, ImageFont


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('font', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--latin-font', type=Path, help='Optional font for Latin text')
    args = parser.parse_args()
    font = ImageFont.truetype(str(args.font), 16)
    latin = ImageFont.truetype(str(args.latin_font), 16) if args.latin_font else font
    chars = set(range(32, 127))
    for first in range(0x81, 0xf0):
        for second in range(0x40, 0xfd):
            try:
                text = bytes((first, second)).decode('cp932')
                if len(text) == 1:
                    chars.add(ord(text))
            except UnicodeDecodeError:
                pass
    for code in range(0xa1, 0xe0):
        chars.add(ord(bytes((code,)).decode('cp932')))
    out = bytearray(b'PJF1' + struct.pack('<I', len(chars)))
    for code in sorted(chars):
        text = chr(code)
        selected = latin if code < 127 else font
        image = Image.new('1', (16, 16))
        # A fixed baseline keeps punctuation and full-width glyphs aligned.
        ImageDraw.Draw(image).text((0, 14), text, font=selected, fill=1, anchor='ls')
        advance = min(16, max(1, round(selected.getlength(text))))
        out += struct.pack('<IBB', code, advance, 0)
        for y in range(16):
            bits = sum(int(bool(image.getpixel((x, y)))) << (15-x) for x in range(16))
            out += struct.pack('>H', bits)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(out)
    print(f'{len(chars)} glyphs written to {args.output}')


if __name__ == '__main__':
    main()
