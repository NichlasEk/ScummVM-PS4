#!/usr/bin/env python3
"""Validate the shared DOS start-state reader and saved task rebasing."""
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
#include "engines/sotc/startstate.h"
extern "C" int read(const uint8_t *p, unsigned n, unsigned address) {
 Sotc::StartState s;
 if (!s.load(Sotc::Cc4View(p,n))) return -1;
 if (!s.range(address,2)) return -2;
 return s.word(address);
}
extern "C" int task(const uint8_t *p, unsigned n, const uint8_t *b, unsigned size, unsigned i) {
 Sotc::StartState s; uint32_t pc;
 if (!s.load(Sotc::Cc4View(p,n)) || !s.taskPC(Sotc::Cc4View(b,size),i,pc)) return -1;
 return pc;
}
'''
with tempfile.TemporaryDirectory(prefix='sotc-init-') as temp:
    src = Path(temp) / 'test.cpp'
    libpath = Path(temp) / 'test.so'
    src.write_text(wrapper)
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror', '-shared',
                    '-fPIC', '-I', str(root), str(src), '-o', str(libpath)], check=True)
    lib = c.CDLL(str(libpath))
    lib.read.argtypes = [c.c_char_p, c.c_uint, c.c_uint]
    lib.task.argtypes = [c.c_char_p, c.c_uint, c.c_char_p, c.c_uint, c.c_uint]
    data = (args.directory / 'INIT.SAV').read_bytes()
    def read(addr, source=data):
        return lib.read(source, len(source), addr)
    assert read(0x2df2) >= 0, 'Unsupported INIT.SAV layout'
    for malformed in [b'', data[:63], data[:-1], data + b'\0', b'BAD\0' + data[4:]]:
        assert read(0x2df2, malformed) == -1
    assert read(0x2ded) == -2 and read(0x3d2b) == -2
    bank, room, background, tasks = (read(a) for a in (0x2df2, 0x2df8, 0x2e14, 0x2e02))
    cc4 = (args.directory / f'R{bank:02}.CC4').read_bytes()
    blocks = struct.unpack_from('<I', cc4)[0] // 4
    assert room < blocks
    start = struct.unpack_from('<I', cc4, room * 4)[0]
    end = struct.unpack_from('<I', cc4, (room + 1) * 4)[0] if room + 1 < blocks else len(cc4)
    block = cc4[start:end]
    assert struct.unpack_from('<H', block)[0] // 2 == tasks
    decoded = bytes(value ^ ((i * 5 + 5) & 255) for i, value in enumerate(data[64:]))
    result = []
    for i in range(tasks):
        record = 0x3110 - 0x2dee + i * 22
        expected = struct.unpack_from('<H', decoded, record + 4)[0] - struct.unpack_from('<H', decoded, record)[0]
        pc = lib.task(data, len(data), block, len(block), i)
        assert pc == expected and pc >= 0
        result.append(dict(task=i, pc=pc, actor=decoded[record + 8], flags=decoded[record + 9]))
    assert lib.task(data, len(data), block, len(block), tasks) == -1
    assert lib.task(data, len(data), block[:1], 1, 0) == -1
    # Mutate saved IP below its saved base; XOR encoding is reversible.
    damaged = bytearray(decoded)
    record = 0x3110 - 0x2dee
    struct.pack_into('<H', damaged, record + 4, 0)
    bad = data[:64] + bytes(v ^ ((i * 5 + 5) & 255) for i, v in enumerate(damaged))
    assert lib.task(bad, len(bad), block, len(block), 0) == -1
    print(json.dumps(dict(bank=bank, block=room, background=background,
                          protagonist_x=read(0x2e54), protagonist_y=read(0x2e56),
                          draw_entries=read(0x34de) & 255, tasks=result), indent=2))
