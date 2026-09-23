#!/usr/bin/env python3
"""Audit real room directories and decode instruction prefixes with engine code.

This is NOT VM execution/coverage: stops at branches, task stop and unknowns.
JSON output contains metadata only; supplied commercial resources stay external.
"""
import argparse
import ctypes as c
import json
from pathlib import Path
import struct
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('directory', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
wrapper = r'''
#include "engines/sotc/script.h"
extern "C" unsigned count(const uint8_t *p, unsigned n, unsigned w) {
 return Sotc::cc4Count(Sotc::Cc4View(p,n),w);
}
extern "C" int entry(const uint8_t *p, unsigned n, unsigned w, unsigned i, unsigned *out) {
 Sotc::Cc4View v;
 if (!Sotc::cc4Entry(Sotc::Cc4View(p,n),w,i,v)) return 0;
 out[0] = v.data-p; out[1] = v.size; return 1;
}
extern "C" int decode(const uint8_t *p, unsigned n, unsigned pc, int *out) {
 Sotc::ScriptInstruction op;
 int status=Sotc::decodeScript(Sotc::Cc4View(p,n),pc,op);
 out[0]=op.opcode; out[1]=op.next; out[2]=op.target;
 out[3]=op.argument; out[4]=op.variable; out[5]=op.value;
 return status;
}
'''
with tempfile.TemporaryDirectory(prefix='sotc-cc4-') as temp:
    source = Path(temp) / 'audit.cpp'
    library = Path(temp) / 'audit.so'
    source.write_text(wrapper)
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror', '-shared',
                    '-fPIC', '-I', str(root), str(source), '-o', str(library)], check=True)
    lib = c.CDLL(str(library))
    lib.count.argtypes = [c.c_char_p, c.c_uint, c.c_uint]
    lib.count.restype = c.c_uint
    lib.entry.argtypes = [c.c_char_p, c.c_uint, c.c_uint, c.c_uint, c.POINTER(c.c_uint)]
    lib.decode.argtypes = [c.c_char_p, c.c_uint, c.c_uint, c.POINTER(c.c_int)]

    def entries(data, width):
        count = lib.count(data, len(data), width)
        assert count, 'invalid directory'
        for index in range(count):
            out = (c.c_uint * 2)()
            assert lib.entry(data, len(data), width, index, out)
            yield data[out[0]:out[0] + out[1]]

    def decode(data, pc=0):
        out = (c.c_int * 6)()
        status = lib.decode(data, len(data), pc, out)
        return status, list(out)

    # Hostile tables: truncated/unaligned header, decreasing offsets, EOF overflow.
    for bad in [b'', b'\x04', struct.pack('<I', 5), struct.pack('<II', 8, 7),
                struct.pack('<II', 8, 100)]:
        assert lib.count(bad, len(bad), 4) == 0
    fixture = struct.pack('<II', 8, 10) + b'abcd'
    assert list(entries(fixture, 4)) == [b'ab', b'cd']
    out = (c.c_uint * 2)()
    assert not lib.entry(fixture, len(fixture), 4, 2, out)
    assert lib.count(fixture, len(fixture), 3) == 0
    # Relative operand base: jump at 0, displacement -1 points back to opcode.
    assert decode(b'\x03\xff\xff')[1][2] == 0
    assert decode(b'\x03\xfe\xff')[0] == 3
    assert decode(b'\x03\x02\x00')[0] == 3  # Exactly EOF is not executable.
    assert decode(b'\x03\0')[0] == 2
    assert decode(b'\xff')[0] == 1
    assert decode(b'\x02')[1][1] == 0  # Task stop leaves IP unchanged.
    condition = bytes([10, 34, 0, 0, 13, 5, 0, 0xf9, 0xff])
    status, op = decode(condition)
    assert status == 0 and op[1:3] == [9, 0] and op[4:6] == [34, 5]
    assert decode(bytes([10, 34, 0, 0, 12, 255, 250, 255]))[1][5] == 255
    assert decode(bytes([10, 34, 0, 0, 11, 234, 3, 249, 255]))[1][5] == 1002
    assert decode(bytes([10, 34, 0, 8, 13, 5, 0, 249, 255]))[0] == 3
    assert decode(bytes([10, 34, 0, 0, 99, 5, 0, 249, 255]))[0] == 1
    for size in range(1, len(condition)):
        assert decode(condition[:size])[0] == 2
    assert decode(bytes([22, 2, 1, 3, 4, 5]))[1][1] == 6
    assert decode(bytes([22, 2, 1, 3, 4]))[0] == 2
    assert decode(b'\x3a\x07')[1][3] == 7
    assert decode(b'\x3a')[0] == 2

    report = []
    total_blocks = total_scripts = total_instructions = 0
    paths = sorted(args.directory.glob('R[0-9][0-9].CC4'))
    if not paths:
        parser.error('No room CC4 banks found')
    for path in paths:
        blocks = list(entries(path.read_bytes(), 4))
        scripts_count = 0
        prefixes = []
        for block_index, block in enumerate(blocks):
            for script_index, script in enumerate(entries(block, 2)):
                scripts_count += 1
                pc = 0
                decoded = 0
                while pc < len(script):
                    status, op = decode(script, pc)
                    if status == 0:
                        decoded += 1
                    if status or op[0] in (2, 3, 10, 80):
                        break
                    assert op[1] > pc
                    pc = op[1]
                else:
                    status, op = 2, [-1]
                total_instructions += decoded
                prefixes.append(dict(block=block_index, script=script_index,
                                     bytes=len(script), decoded=decoded, stop_pc=pc,
                                     stop_opcode=op[0], status=status))
        total_blocks += len(blocks)
        total_scripts += scripts_count
        report.append(dict(bank=path.name, blocks=len(blocks), scripts=scripts_count,
                           prefixes=prefixes))
    print(json.dumps(dict(blocks=total_blocks, scripts=total_scripts,
                          decoded_prefix_instructions=total_instructions,
                          banks=report), indent=2))
