#!/usr/bin/env python3
"""Compare zoom and palette arithmetic against the user's original DOS executable."""
import argparse
import ctypes as c
import hashlib
from pathlib import Path
import struct
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_16
from unicorn.x86_const import *
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('executable',type=Path)
a=p.parse_args()
b=a.executable.read_bytes()
assert hashlib.sha256(b).hexdigest()=='26d6fc54a27e0bf17ea34a355aa9bf087c8f13dfb5d613d5d1d535190da9ab5b'
b=b[struct.unpack_from('<H',b,8)[0]*16:]
u=Uc(UC_ARCH_X86,UC_MODE_16);u.mem_map(0,0x100000);u.mem_write(0,b);u.mem_write(0xdbb3,b'\xcb')
def call(addr,args):
    u.reg_write(UC_X86_REG_CS,(addr&0xf0000)>>4);u.reg_write(UC_X86_REG_DS,0x17c4)
    u.reg_write(UC_X86_REG_SS,0x9000);u.reg_write(UC_X86_REG_SP,0xff00)
    u.mem_write(0x9ff00,struct.pack('<'+'H'*(len(args)+2),0xfff0,0,*[v&65535 for v in args]))
    u.emu_start(addr,0xfff0,count=1000000)
    assert u.reg_read(UC_X86_REG_IP)==0xfff0,(hex(addr),hex(u.reg_read(UC_X86_REG_CS)),hex(u.reg_read(UC_X86_REG_IP)))
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as td:
    td=Path(td)
    (td/'test.cpp').write_text('''#include "engines/sotc/screeneffects.h"
extern "C" bool zoom(const uint8_t *s,uint8_t *d,unsigned m,int x,int y){return Sotc::zoomScreen(s,d,m,x,y);}
extern "C" bool project(const uint8_t *t,int *x,int *y,unsigned w,unsigned h){return Sotc::projectApproach(t,*x,*y,w,h);}
extern "C" void palette(const uint8_t *s,uint8_t *d,unsigned m,unsigned a){Sotc::paletteEffect(s,d,m,a);}
''')
    subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-shared','-fPIC','-I',str(root),str(td/'test.cpp'),'-o',str(td/'test.so')],check=True)
    lib=c.CDLL(str(td/'test.so'));lib.zoom.argtypes=[c.c_char_p,c.c_void_p,c.c_uint,c.c_int,c.c_int]
    lib.palette.argtypes=[c.c_char_p,c.c_void_p,c.c_uint,c.c_uint]
    src=bytes((x*7+y*13+(x*y)%113)&255 for y in range(200) for x in range(320))
    u.mem_write(0x50000,src)
    u.mem_write(0x17c40+0x59f0,struct.pack('<HHHH',0,0x5000,0,0x6000))
    u.mem_write(0x90000,struct.pack('<200H',*[y*320 for y in range(200)]))
    out=c.create_string_buffer(64000);tests=0
    for mode,addr in [(1,0x10360),(2,0x102e7),(3,0x10274)]:
        for x,y in [(0,0),(319,199),(160,100),(112,148),(-100,-100),(500,300)]:
            call(addr,[x,y]);assert lib.zoom(src,out,mode,x,y)
            assert out.raw==bytes(u.mem_read(0x60000,64000)),(mode,x,y)
            tests+=1
    print('Original zoom frames matched:',tests)
    src=bytes((i*13+i//9)&255 for i in range(768));u.mem_write(0x50000,src)
    out=c.create_string_buffer(768);tests=0
    for mode,addr in [(1,0x1073c),(2,0x10453),(3,0xd6f3)]:
        for amount in [0,1,7,15,16,63,128,255]:
            if mode==3:
                u.mem_write(0x17c40+0x5e76,struct.pack('<HH',0,0x5000));u.mem_write(0x17c40+0x5e5e,struct.pack('<HH',0,0x6000))
                call(addr,[amount])
            else: call(addr,[0,0x5000,0,0x6000,amount,256])
            lib.palette(src,out,mode,amount)
            assert out.raw==bytes(u.mem_read(0x60000,768)),(mode,amount)
            tests+=1
    print('Original palette transformations matched:',tests)

    lib.project.argtypes=[c.c_char_p,c.POINTER(c.c_int),c.POINTER(c.c_int),c.c_uint,c.c_uint]
    terrain=bytes(90+(i*17)%61 for i in range(320));u.mem_write(0x17c40+0x339e,terrain)
    tests=0
    for w,h in [(0,0),(4,2),(9,7),(20,12)]:
        for x,y in [(-20,20),(330,250),(160,1),(100,170),(5,199)]:
            u.mem_write(0x50000,struct.pack('<hh',x,y));call(0xcdcf,[0,0x5000,2,0x5000,w,h])
            xx,yy=c.c_int(x),c.c_int(y);assert lib.project(terrain,c.byref(xx),c.byref(yy),w,h)
            assert (xx.value,yy.value)==struct.unpack('<hh',u.mem_read(0x50000,4))
            tests+=1
    print('Original approach projections matched:',tests)
