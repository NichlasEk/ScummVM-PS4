#!/usr/bin/env python3
"""Exercise the shared SOTC scheduler with fixtures and the supplied INIT.SAV."""
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
with tempfile.TemporaryDirectory(prefix='sotc-vm-') as temp:
    temp = Path(temp)
    (temp / 'test.cpp').write_text('''
#include "engines/sotc/scriptmachine.h"
static Sotc::ScriptMachine vm;
extern "C" bool load(const uint8_t *s,unsigned n,const uint8_t *b,unsigned m) {
 Sotc::StartState state;
 return state.load(Sotc::Cc4View(s,n)) && vm.load(state,Sotc::Cc4View(b,m));
}
extern "C" int visit(unsigned i) {return vm.visit(i);}
extern "C" void tick() {vm.tick();}
extern "C" bool actor(unsigned i,int x,int y,unsigned step,unsigned count) {
 Sotc::ScriptActor a={x,y,4,2,step,count}; return vm.setActor(i,a);
}
extern "C" bool info(unsigned i,unsigned *out) {
 const Sotc::ScriptTask *t=vm.task(i); if(!t) return false;
 out[0]=t->pc; out[1]=t->flags; out[2]=t->timer; out[3]=t->actor;
 out[4]=t->opcode; out[5]=t->result; out[6]=t->executed; return true;
}
''')
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
                    '-I', str(root), str(temp / 'test.cpp'), '-o', str(temp / 'test.so')], check=True)
    lib = c.CDLL(str(temp / 'test.so'))
    lib.load.restype = lib.actor.restype = lib.info.restype = c.c_bool
    lib.load.argtypes = [c.c_char_p, c.c_uint, c.c_char_p, c.c_uint]
    def info(i=1):
        out = (c.c_uint * 7)()
        assert lib.info(i, out)
        return list(out)
    def fixture(script, flags=0, rectangle=(0, 0, 0, 0), extra=None):
        scripts = [b'\x02', bytes(script)] + ([] if extra is None else [bytes(extra)])
        offsets, pos = [], len(scripts) * 2
        for data in scripts:
            offsets.append(pos)
            pos += len(data)
        block = struct.pack('<' + 'H' * len(scripts), *offsets) + b''.join(scripts)
        state = bytearray(3902)
        def word(address, value):
            struct.pack_into('<H', state, address - 0x2dee, value)
        word(0x2e02, len(scripts))
        for i in range(len(scripts)):
            state[0x3119 - 0x2dee + i * 22] = flags if i == 1 else 32
        for i, v in enumerate(rectangle):
            word(0x3110 + 22 + 14 + i * 2, v)
        save = b'NED\0' + bytes(60) + bytes(v ^ ((i + 1) * 5 & 255) for i, v in enumerate(state))
        assert lib.load(save, len(save), block, len(block))
        return save, block
    fixture([9, 2, 21, 3, 2])
    assert lib.visit(1) == 2 and info()[:3] == [2, 2, 2]
    assert lib.visit(1) == 2 and info()[2] == 1
    assert lib.visit(1) == 1 and info()[0] == 4 and info()[3] == 3
    fixture([40, 55, 154, 63, 199, 2])
    assert lib.actor(0, 220, 151, 0, 1)
    assert lib.visit(1) == 4 and info()[0] == 0
    assert lib.actor(0, 106, 154, 0, 1)  # Footprint edge touches region edge.
    assert lib.visit(1) == 1 and info()[0] == 5
    fixture([39, 2], rectangle=(110, 154, 126, 199))
    lib.actor(0, 110, 154, 0, 1)
    assert lib.visit(1) == 4
    lib.actor(0, 200, 154, 0, 1)
    assert lib.visit(1) == 1
    for compare, value, expected_actor in [(0, 0, 0), (0, 1, 5), (2, 65535, 0), (4, 65535, 5), (6, 1, 5), (7, 1, 0)]:
        fixture([10, 232, 3, compare, 13, value & 255, value >> 8, 3, 0, 2, 21, 5, 2])
        assert lib.visit(1) == 1 and info()[3] == expected_actor
    fixture([3, 255, 255])
    assert lib.visit(1) == 8 and info()[0] == 0 and info()[6] == 128
    fixture([255])
    assert lib.visit(1) == 6 and info()[0] == 0 and info()[6] == 0
    fixture([21, 255])
    assert lib.visit(1) == 7 and info()[0] == 0
    fixture([2], flags=4)
    lib.actor(0, 0, 0, 0, 2)
    assert lib.visit(1) == 3
    lib.actor(0, 0, 0, 1, 2)
    assert lib.visit(1) == 1
    fixture([27, 2, 2], extra=[21, 4, 2])
    lib.tick()
    assert info(0)[6] == 0 and info(2)[3] == 4 and info(2)[5] == 1
    fixture([2], flags=8)
    assert lib.visit(1) == 5 and info()[0] == 0
    assert lib.visit(17) == 7
    assert not lib.actor(11, 0, 0, 0, 1)

    save = (args.directory / 'INIT.SAV').read_bytes()
    state = bytes(v ^ ((i + 1) * 5 & 255) for i, v in enumerate(save[64:]))
    bank, room = (struct.unpack_from('<H', state, a - 0x2dee)[0] for a in (0x2df2, 0x2df8))
    cc4 = (args.directory / f'R{bank:02}.CC4').read_bytes()
    start, end = struct.unpack_from('<II', cc4, room * 4)
    block = cc4[start:end]
    assert lib.load(save, len(save), block, len(block))
    lib.tick()
    result = [dict(zip(('pc', 'flags', 'timer', 'actor', 'opcode', 'result', 'executed'), info(i)), task=i) for i in range(15)]
    assert result[1]['result'] == 4 and result[1]['pc'] == 14
    assert [(r['task'], r['opcode']) for r in result if r['result'] == 6] == [(5, 66), (8, 79), (12, 99), (14, 79)]
    assert sum(r['result'] == 1 for r in result) == 6
    print(json.dumps(result, indent=2))
