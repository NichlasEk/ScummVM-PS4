#!/usr/bin/env python3
"""Compare type-10 frame pixels against the user-supplied DOS blitter."""
import argparse,ctypes as c,hashlib,struct,subprocess,tempfile
from pathlib import Path
from unicorn import Uc,UC_ARCH_X86,UC_MODE_16
from unicorn.x86_const import *
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('executable',type=Path)
p.add_argument('unpacked',type=Path)
a=p.parse_args();exe=a.executable.read_bytes()
assert hashlib.sha256(exe).hexdigest()=='26d6fc54a27e0bf17ea34a355aa9bf087c8f13dfb5d613d5d1d535190da9ab5b'
code=exe[struct.unpack_from('<H',exe,8)[0]*16:]
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as td:
 td=Path(td)
 (td/'test.cpp').write_text('''#include "engines/sotc/portrait.h"
extern "C" bool decode(const uint8_t *p,unsigned n,unsigned i,uint8_t *out,uint8_t *mask,unsigned *w,unsigned *h) {return Sotc::va2Portrait(p,n,i,out,mask,65536,*w,*h);}
''')
 subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-shared','-fPIC','-I',str(root),str(td/'test.cpp'),'-o',str(td/'test.so')],check=True)
 lib=c.CDLL(str(td/'test.so'));lib.decode.restype=c.c_bool
 frames=0
 for path in sorted(a.unpacked.glob('*.VA2')):
  data=path.read_bytes();base=struct.unpack_from('<I',data,4)[0]
  # Only resources whose poses contain type 10; other images use another codec.
  pb=struct.unpack_from('<I',data,0)[0];n=(struct.unpack_from('<I',data,pb)[0]-2)//4
  refs=set()
  for i in range(n):
   q=pb+struct.unpack_from('<I',data,pb+i*4)[0];layers=data[q+1];q+=2
   for _ in range(layers):
    kind,points,ref=struct.unpack_from('<BBH',data,q)
    if kind==10:refs.add(ref)
    q+=4+points*4
  for index in sorted(refs):
   out=c.create_string_buffer(65536);mask=c.create_string_buffer(65536);w,h=c.c_uint(),c.c_uint()
   assert lib.decode(data,len(data),index,out,mask,c.byref(w),c.byref(h)),(path.name,index,'decode')
   u=Uc(UC_ARCH_X86,UC_MODE_16);u.mem_map(0,0x100000);u.mem_write(0,code)
   start=base+struct.unpack_from('<I',data,base+index*4)[0]
   u.mem_write(0x50000,data[start+2:])
   u.mem_write(0x60000,bytes([253])*65536)
   u.mem_write(0x17c40+0x59f0,struct.pack('<HH',0,0x6000))
   u.mem_write(0x90000,struct.pack('<200H',*(i*320 for i in range(200))))
   u.reg_write(UC_X86_REG_CS,0x110b);u.reg_write(UC_X86_REG_DS,0x17c4)
   u.reg_write(UC_X86_REG_SS,0x9000);u.reg_write(UC_X86_REG_SP,0xff00)
   u.mem_write(0x9ff00,struct.pack('<6H',0xfff0,0,0,0,0,0x5000))
   u.emu_start(0x11736,0xfff0,count=2000000)
   assert u.reg_read(UC_X86_REG_IP)==0xfff0
   original=bytes(u.mem_read(0x60000,64000))
   expected=bytearray([253])*64000
   for y in range(h.value):
    for x in range(w.value):
     k=y*w.value+x
     if mask.raw[k]:expected[y*320+x]=out.raw[k]
   assert original==expected,(path.name,index,'pixels')
   frames+=1
 print('Original type-10 frames matched:',frames)
