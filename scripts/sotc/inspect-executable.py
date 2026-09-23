#!/usr/bin/env python3
"""Inspect known routines in the supplied European DOS executable (read only).

Offsets describe this specific release; output belongs in ignored artifacts/.
Requires GNU objdump. No executable bytes are embedded in the engine.
"""
import argparse
import hashlib
from pathlib import Path
import struct
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('executable', type=Path)
args = parser.parse_args()
data = args.executable.read_bytes()
if len(data) < 28 or data[:2] != b'MZ':
    parser.error('Expected a DOS MZ executable')
if hashlib.sha256(data).hexdigest() != '26d6fc54a27e0bf17ea34a355aa9bf087c8f13dfb5d613d5d1d535190da9ab5b':
    parser.error('Unknown executable revision; routine offsets must be mapped first')
header = struct.unpack_from('<H', data, 8)[0] * 16
if header < 28 or header >= len(data):
    parser.error('Invalid MZ header extent')
print('SHA256:', hashlib.sha256(data).hexdigest(), flush=True)
print('Load image starts at file offset:', hex(header), flush=True)
# Anchors discovered by tracing callers of the VA2 section/record accessor.
routines = [('Sequence preview and inclusive movement', 0x1641, 0x1715),
            ('VA2 section/record lookup', 0x6e4c, 0x6ec3),
            ('Sequence renderer and control-word dispatch', 0xc0b4, 0xc25d),
            ('Frame orientation XOR and bottom anchor', 0x149e6, 0x14a44),
            ('PIT divisor setup (not yet linked to animation cadence)', 0x136cf, 0x136e9)]
with tempfile.TemporaryDirectory(prefix='sotc-exe-') as temp:
    code = Path(temp) / 'load-image.bin'
    code.write_bytes(data[header:])
    for name, start, end in routines:
        if end > len(data) - header:
            parser.error('Executable too short for known routine offsets')
        print('\n' + name, flush=True)
        subprocess.run(['objdump', '-D', '-b', 'binary', '-mi8086', '-Mintel',
                        '--start-address=' + hex(start), '--stop-address=' + hex(end), str(code)], check=True)
