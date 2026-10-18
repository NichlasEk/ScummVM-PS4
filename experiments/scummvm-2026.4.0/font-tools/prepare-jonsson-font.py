#!/usr/bin/env python3
"""Convert the user's embedded Sand font; never download game/font assets."""
import argparse, hashlib, subprocess, tempfile
from io import BytesIO
from fontTools.ttLib import TTFont
from pathlib import Path

REVISION = 'fca530f9ef388d7ff38fa6c7117feae5bb5411c6'
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('game', type=Path, help='Game root containing Data/Dialog.cst')
p.add_argument('--converter-source', required=True, type=Path, help='LibreShockwave source checkout')
a = p.parse_args()
archive = a.game / 'Data' / 'Dialog.cst'
if not archive.is_file():
    p.error('Expected Data/Dialog.cst under the game root')
root = a.converter_source.resolve()
revision = subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip()
if revision != REVISION:
    p.error('Use tested LibreShockwave revision ' + REVISION)
data = archive.read_bytes()
# The tested D7 archive uses little-endian XMED chunks (DEMX on disk).
# Require the PFR1 signature directly after that chunk's length, not an
# arbitrary substring match in compressed or textual data.
fonts = []
start = 0
while True:
    pos = data.find(b'DEMX', start)
    if pos < 0:
        break
    start = pos + 4
    if pos + 12 > len(data) or data[pos + 8:pos + 12] != b'PFR1':
        continue
    size = int.from_bytes(data[pos + 4:pos + 8], 'little')
    if size < 58 or size > 4 * 1024 * 1024 or pos + 8 + size > len(data):
        p.error('Invalid embedded font chunk; game files were not changed')
    fonts.append(data[pos + 8:pos + 8 + size])
if len(fonts) != 1 or b'Sand *' not in data:
    p.error('Expected exactly one embedded Sand PFR1 font; unsupported edition')
output = a.game / 'jonsson-dialogue.ttf'
with tempfile.TemporaryDirectory(prefix='jonsson-font-') as tmp:
    tmp = Path(tmp)
    source = root / 'cpp'
    binary = tmp / 'convert-pfr'
    subprocess.run(['c++', '-std=c++20', '-O2', '-I' + str(source / 'include'),
                    str(Path(__file__).with_name('convert-pfr.cpp')),
                    *[str(source / 'src' / 'font' / n) for n in
                      ('Pfr1Font.cpp', 'Pfr1TtfConverter.cpp', 'PfrBitReader.cpp')],
                    '-o', str(binary)], check=True)
    raw = tmp / 'Sand.pfr'
    result = tmp / 'Sand.ttf'
    raw.write_bytes(fonts[0])
    subprocess.run([str(binary), str(raw), str(result)], check=True)
    converted = result.read_bytes()
    if not converted.startswith(b'\x00\x01\x00\x00'):
        p.error('Converter did not produce TrueType data')
    # The pinned converter emits short loca entries but declares long ones.
    # Correct only the known layout, then rebuild table checksums and validate
    # actual outlines rather than accepting a font that merely opens.
    font = TTFont(BytesIO(converted), lazy=True, recalcTimestamp=False)
    count = font['maxp'].numGlyphs
    if font['head'].indexToLocFormat == 1 and font.reader.tables['loca'].length == 2 * (count + 1):
        font['head'].indexToLocFormat = 0
    cmap = font.getBestCmap()
    for char in 'ABCDEFGHIJKLMNOPQRSTUVWXYZÅÄÖåäö':
        glyph = cmap.get(ord(char))
        if not glyph or font['glyf'][glyph].numberOfContours == 0:
            p.error('Missing outline for ' + char + '; game files were not changed')
    fixed = BytesIO()
    font.save(fixed)
    converted = fixed.getvalue()
    if output.exists() and output.read_bytes() != converted:
        p.error('A different jonsson-dialogue.ttf already exists; move it aside first')
    output.write_bytes(converted)
print('Created:', output)
print('SHA-256:', hashlib.sha256(converted).hexdigest())
