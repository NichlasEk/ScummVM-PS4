#!/usr/bin/env python3
"""Prepare original TWP SPIR-V and reflected uniform payloads for native draws."""
import json
from pathlib import Path
import struct
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
output = Path(sys.argv[1])
output.mkdir(parents=True, exist_ok=True)
source = root / 'artifacts/vulkan/twp-shaders'
subprocess.run([sys.executable, str(root / 'scripts/prepare-twp-shaders.py')], check=True)
manifest = json.loads((source / 'manifest.json').read_text())['shaders']
names = ['vsrc', 'fragmentSrc', 'bwShader', 'ghostShader', 'sepiaShader', 'fadeShader']
header = ['/* Generated from original TWP shaders; see manifest source hashes. */\n#include <stdint.h>\n']
for name in names:
    raw = (source / (name + '.spv')).read_bytes()
    words = struct.unpack('<' + 'I' * (len(raw) // 4), raw)
    header.append('static const uint32_t twp_' + name + '[] = {\n' + ','.join(hex(w) for w in words) + '\n};\n')
(output / 'shaders.h').write_text(''.join(header))

payload = bytearray(2816)

def pack(shader, base, values):
    block = next(d for d in manifest[shader]['descriptors'] if d['kind'] == 'Struct')
    assert block['block_size'] <= 256
    assert set(values) == {m['name'] for m in block['members']}
    for member in block['members']:
        value = values[member['name']]
        value = value if isinstance(value, list) else [value]
        offset = base + member['Offset']
        if 'count' in member:
            assert len(value) == member['count']
            for index, item in enumerate(value):
                struct.pack_into('<f', payload, offset + index * member['ArrayStride'], item)
        else:
            fmt = 'i' if member['type'] == 'int' else 'f'
            assert len(value) * 4 == member['size']
            struct.pack_into('<' + fmt * len(value), payload, offset, *value)

pack('vsrc', 0, {'u_transform': [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1]})
for row in range(2):
    pack('ghostShader', 256 * (row * 5 + 3), {
        'iGlobalTime': .7, 'iFade': .5 if row == 0 else .8, 'wobbleIntensity': 0,
        'shadows': [.1, .02, -.03], 'midtones': [-.02, .06, .01], 'highlights': [.04, -.01, .08]})
    pack('sepiaShader', 256 * (row * 5 + 4), {
        'sepiaFlicker': .9 if row == 0 else .7, 'RandomValue': [.8, .7, .6, .5, .4], 'TimeLapse': 1.2})
    pack('fadeShader', 256 * (row * 5 + 5), {
        'u_timer': .3, 'u_fade': .25 if row == 0 else .75, 'u_fadeToSep': row, 'u_movement': 0})
(output / 'twp-data.h').write_text('/* Generated using reflected std140 offsets. */\nstatic const unsigned char twp_uniforms[] = {\n' + ','.join(str(v) for v in payload) + '\n};\n')
