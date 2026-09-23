#!/usr/bin/env python3
"""Compare animation frame/subframe progression with the original EXE routines.

Unicorn executes render/update arithmetic; graphical callees and the resource
lookup are stubbed. No executable bytes are distributed with the engine.
"""
import argparse
import ctypes as c
import hashlib
from pathlib import Path
import struct
import subprocess
import tempfile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_16
from unicorn.x86_const import *
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('executable',type=Path)
a=p.parse_args()
b=a.executable.read_bytes()
assert hashlib.sha256(b).hexdigest()=='26d6fc54a27e0bf17ea34a355aa9bf087c8f13dfb5d613d5d1d535190da9ab5b'
b=b[struct.unpack_from('<H',b,8)[0]*16:]
u=Uc(UC_ARCH_X86,UC_MODE_16);u.mem_map(0,0x100000);u.mem_write(0,b)
for addr in (0xe68c,0x10e6b,0x10c41):u.mem_write(addr,b'\xcb')
u.mem_write(0xc09b,b'\xb8\xfe\x00\xba\x00\x50\xcb')
actor=0x17c40+0x2e54

def call(address,args):
    u.reg_write(UC_X86_REG_CS,0);u.reg_write(UC_X86_REG_DS,0x17c4)
    u.reg_write(UC_X86_REG_SS,0x9000);u.reg_write(UC_X86_REG_SP,0xff00)
    u.mem_write(0x9ff00,struct.pack('<'+'H'*(len(args)+2),0xfff0,0,*args))
    u.emu_start(address,0xfff0,count=10000)
    assert u.reg_read(UC_X86_REG_IP)==0xfff0
    return u.reg_read(UC_X86_REG_AX)

root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as temp:
    temp=Path(temp)
    (temp/'test.cpp').write_text('''
#include "engines/sotc/animation.h"
extern "C" bool advance(unsigned control,unsigned mode,unsigned gate,unsigned count,unsigned *step,unsigned *sub) {
 return Sotc::advanceAnimation(control,mode,gate,count,*step,*sub);
}
''')
    subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-shared','-fPIC','-I',str(root),str(temp/'test.cpp'),'-o',str(temp/'test.so')],check=True)
    lib=c.CDLL(str(temp/'test.so'));lib.advance.restype=c.c_bool
    tested=0
    for mode in (0,2):
        for gate in (0,254,255):
            for control in (0,1,500,0x4000,0x4001,0x4002,0x4003,0x4010):
                for step in (0,3):
                    for sub in (0,1,2,15):
                        raw=bytearray(52)
                        for offset,value in ((6,mode),(14,step),(16,sub),(18,4)):
                            struct.pack_into('<H',raw,offset,value)
                        raw[20]=gate;u.mem_write(actor,bytes(raw))
                        u.mem_write(0x50100,struct.pack('<HHhh',0,control,0,0)*4)
                        if mode==2:
                            rendered=call(0xc0b4,[0,0x5000,0x100,0x5000,step,sub,0,0,4])
                            u.mem_write(actor+16,struct.pack('<H',rendered))
                        call(0x7afc,[0x2e54,0x17c4])
                        original=struct.unpack('<HH',u.mem_read(actor+14,4))
                        s,t=c.c_uint(step),c.c_uint(sub)
                        assert lib.advance(control,mode,gate,4,c.byref(s),c.byref(t))
                        assert (s.value,t.value)==original,(mode,gate,control,step,sub,(s.value,t.value),original)
                        tested+=1
    for control,mode,count,step in ((0x8000,2,4,0),(0,1,4,0),(0,2,0,0),(0,2,4,4)):
        s,t=c.c_uint(step),c.c_uint(7)
        assert not lib.advance(control,mode,255,count,c.byref(s),c.byref(t))
        assert (s.value,t.value)==(step,7)
    # Opcode 45 holds a selected step with a zero sequence-count field.
    for step in (0,1,3):
        raw=bytearray(52)
        struct.pack_into('<H',raw,6,2);struct.pack_into('<H',raw,14,step);raw[20]=step
        u.mem_write(actor,bytes(raw));call(0x7afc,[0x2e54,0x17c4])
        s,t=c.c_uint(step),c.c_uint(0)
        assert lib.advance(0,2,step,0,c.byref(s),c.byref(t))
        assert (s.value,t.value)==struct.unpack('<HH',u.mem_read(actor+14,4))
    print('Original render/update pairs matched:',tested)

# Original actor reset (90's non-player helper) and absolute position (7).
# Sequence lookup is stubbed above; its count lives one byte before the record.
u.mem_write(0x500ff,b'\x07')
for index in (1,5,10):
    address=actor+index*52
    u.mem_write(address,b'\xcc'*52)
    call(0x78c8,[index,65535])
    raw=bytes(u.mem_read(address,52))
    assert struct.unpack_from('<HH',raw,0)==(0,0)
    assert struct.unpack_from('<HHH',raw,4)==(0,1,1)
    assert raw[11]==255 and struct.unpack_from('<HHHH',raw,12)==(0,0,0,7)
    assert raw[20:23]==bytes([255,4,2])
    assert struct.unpack_from('<H',raw,26)[0]==10
    assert struct.unpack_from('<HH',raw,44)==(0,319) and raw[48:51]==bytes([0,199,1])
    call(0x87e3,[index,510,199])
    moved=bytes(u.mem_read(address,52))
    assert struct.unpack_from('<HH',moved,0)==(510,199)
    assert moved[4:]==raw[4:]
print('Original reset defaults and absolute actor positions matched: 3 actors')

# Opcode 1 clears mode before the original direction setter, then mode 1
# transitions after drawing. Confirm zero/unchanged direction and both bases.
for base in (0,4):
    for facing in range(1,5):
        for direction in range(5):
            raw=bytearray(52)
            struct.pack_into('<HHH',raw,4,base,0,facing)
            struct.pack_into('<HHHH',raw,12,9,3,2,4)
            u.mem_write(actor,bytes(raw))
            call(0x797b,[0x2e54,0x17c4,direction])
            changed=bool(direction and direction!=facing)
            result=bytes(u.mem_read(actor,52))
            assert struct.unpack_from('<HH',result,6)==(int(changed),direction if changed else facing)
            if changed:
                call(0x7afc,[0x2e54,0x17c4])
                result=bytes(u.mem_read(actor,52))
                assert struct.unpack_from('<H',result,6)[0]==0
                assert struct.unpack_from('<HHHH',result,12)==(base+direction-1,0,2,7)
                assert result[20]==255
print('Original facing and deferred sequence transitions matched: 40 cases')
