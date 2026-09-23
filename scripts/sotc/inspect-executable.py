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
            ('Variable-length pose commands', 0x10c41, 0x10cc9),
            ('Vector interpolation renderer', 0x10e6b, 0x10efe),
            ('Polygon fill and optional outline', 0x10efe, 0x10f3e),
            ('Polyline drawing', 0x10f82, 0x10fad),
            ('Task timer wait', 0xb008, 0xb030),
            ('Task animation wait', 0xaf5a, 0xaf9b),
            ('Footprint versus script rectangle', 0xa904, 0xa9d0),
            ('Region wait commands', 0xa9d0, 0xaa6e),
            ('Inclusive rectangle intersection', 0x134ac, 0x134ec),
            ('Timer command and actor binding', 0xaef4, 0xaf28),
            ('New game chooses INIT.SAV', 0x238e, 0x23a3),
            ('Save header and state load', 0x2508, 0x25f3),
            ('Saved task IP rebasing', 0xe12c, 0xe1a7),
            ('Save XOR transform', 0xe26a, 0xe29f),
            ('State payload restore', 0xe4b5, 0xe598),
            ('Saved actor rendering', 0xc2db, 0xc3f8),
            ('Interleaved actors and scenery', 0xc724, 0xc843),
            ('Room script table and initialization', 0x8e8d, 0x8fc4),
            ('Script operands and expression readers', 0x8ff9, 0x9216),
            ('Random range helper', 0x7051, 0x706d),
            ('Random opcode and variable 33', 0x9667, 0x9688),
            ('Pending event conditionals', 0xa713, 0xa768),
            ('Event footprint rectangle reader', 0xa3fd, 0xa4f1),
            ('Borland random seed and generator', 0x17b4e, 0x17b87),
            ('Conditional branch', 0x93ad, 0x93f8),
            ('Background pair selection', 0x9b79, 0x9b90),
            ('Byte coordinate list', 0x9c62, 0x9cc0),
            ('Task control and relative jump', 0x9f9f, 0xa060),
            ('Opcode dispatch', 0xb2ec, 0xb314),
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
