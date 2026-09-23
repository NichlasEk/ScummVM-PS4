#!/usr/bin/env python3
"""Compare walk-delta adjustment with the user's original DOS executable."""
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
#include "engines/sotc/walk.h"
extern "C" void delta(unsigned d,int s,int x,int y,bool same,int *dx,int *dy) {
 Sotc::walkDelta(d,s,x,y,same,*dx,*dy);
}
''')
 subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-shared','-fPIC','-I',str(root),str(t/'test.cpp'),'-o',str(t/'test.so')],check=True)
 lib=c.CDLL(str(t/'test.so'));rng=random.Random(7)
 cases=[(d,s,0,0,True,2,y) for d in range(1,5) for s in (-128,-1,0,1,127) for y in (-32768,-2,-1,0,1,2,32767)]
 cases += [(rng.randrange(1,5),rng.randrange(-128,128),rng.randrange(-32768,32768),rng.randrange(-32768,32768),bool(i%2),rng.randrange(-32768,32768),rng.randrange(-32768,32768)) for i in range(400)]
 for d,s,x,y,same,dx,dy in cases:
  u.mem_write(ds+0x2e2a,bytes([s&255]));u.mem_write(ds+0x2e24,struct.pack('<hh',x,y))
  u.mem_write(ds+0x2df8,struct.pack('<HH',0,0 if same else 1))
  u.mem_write(0x50100,struct.pack('<hh',dx,dy))
  u.reg_write(UC_X86_REG_CS,0);u.reg_write(UC_X86_REG_DS,0x17c4)
  u.reg_write(UC_X86_REG_SS,0x9000);u.reg_write(UC_X86_REG_SP,0xff00)
  u.mem_write(0x9ff00,struct.pack('<7H',0xfff0,0,d,0x100,0x5000,0x102,0x5000))
  u.emu_start(0x8242,0xfff0,count=10000)
  assert u.reg_read(UC_X86_REG_IP)==0xfff0
  a,z=c.c_int(dx),c.c_int(dy);lib.delta(d,s,x,y,same,c.byref(a),c.byref(z))
  assert (a.value,z.value)==struct.unpack('<hh',u.mem_read(0x50100,4))
 print('Original walk-delta adjustment matched:',len(cases),'cases')
