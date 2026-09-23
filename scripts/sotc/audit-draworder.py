#!/usr/bin/env python3
"""Compare portable draw insertion against original 16-bit code (user EXE)."""
import ctypes as c
import hashlib
import random
import struct
import subprocess
import sys
import tempfile
from pathlib import Path
from unicorn import Uc, UC_ARCH_X86, UC_MODE_16
from unicorn.x86_const import *
b=Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(b).hexdigest()=='26d6fc54a27e0bf17ea34a355aa9bf087c8f13dfb5d613d5d1d535190da9ab5b'
b=b[struct.unpack_from('<H',b,8)[0]*16:]
u=Uc(UC_ARCH_X86,UC_MODE_16);u.mem_map(0,0x100000);u.mem_write(0,b)
ds=0x17c40
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as t:
    t=Path(t)
    (t/'test.cpp').write_text('''
#include "engines/sotc/draworder.h"
extern "C" void build(const unsigned *depths,unsigned n,unsigned *out) {
 Sotc::DrawOrder d;
 d.append(199,16); d.append(177,16); d.append(113,16);
 for(unsigned i=0;i<n;++i) d.insert(depths[i],i);
 for(unsigned i=0;i<n+3;++i) {out[i*2]=d.entries[i].depth;out[i*2+1]=d.entries[i].id;}
}
''')
    subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-shared','-fPIC','-I',str(root),str(t/'test.cpp'),'-o',str(t/'test.so')],check=True)
    lib=c.CDLL(str(t/'test.so'));rng=random.Random(4)
    cases=[[0]*11,[255]*11,list(range(11)),list(range(10,-1,-1)),[65535,256,511,0,255]]
    cases += [[rng.randrange(65536) for _ in range(20)] for _ in range(100)]
    for depths in cases:
        u.mem_write(ds+0x34de,b'\x03')
        u.mem_write(ds+0x34e0,bytes([199,16,177,16,113,16]))
        for i,y in enumerate(depths):
            u.reg_write(UC_X86_REG_CS,0);u.reg_write(UC_X86_REG_DS,0x17c4)
            u.reg_write(UC_X86_REG_SS,0x9000);u.reg_write(UC_X86_REG_SP,0xff00)
            u.mem_write(0x9ff00,struct.pack('<HHHH',0xfff0,0,y,i))
            u.emu_start(0x71b5,0xfff0,count=10000)
            assert u.reg_read(UC_X86_REG_IP)==0xfff0
        n=len(depths);out=(c.c_uint*((n+3)*2))()
        lib.build((c.c_uint*n)(*depths),n,out)
        assert list(out)==list(u.mem_read(ds+0x34e0,(n+3)*2))
    print('Original draw-order insertion matched:',len(cases),'cases')
