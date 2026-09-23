#!/usr/bin/env python3
"""Compare collision predicates with routines in the user's original EXE."""
import ctypes as c
import hashlib,random,struct,subprocess,sys,tempfile
from pathlib import Path
from unicorn import Uc,UC_ARCH_X86,UC_MODE_16
from unicorn.x86_const import *
b=Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(b).hexdigest()=='26d6fc54a27e0bf17ea34a355aa9bf087c8f13dfb5d613d5d1d535190da9ab5b'
b=b[struct.unpack_from('<H',b,8)[0]*16:]
u=Uc(UC_ARCH_X86,UC_MODE_16);u.mem_map(0,0x100000);u.mem_write(0,b);ds=0x17c40
root=Path(__file__).resolve().parents[2]
def call(addr,args):
 u.reg_write(UC_X86_REG_CS,0);u.reg_write(UC_X86_REG_DS,0x17c4)
 u.reg_write(UC_X86_REG_SS,0x9000);u.reg_write(UC_X86_REG_SP,0xff00)
 u.mem_write(0x9ff00,struct.pack('<'+'H'*(len(args)+2),0xfff0,0,*args))
 u.emu_start(addr,0xfff0,count=20000)
 assert u.reg_read(UC_X86_REG_IP)==0xfff0
 return u.reg_read(UC_X86_REG_AX)
with tempfile.TemporaryDirectory() as t:
 t=Path(t)
 (t/'test.cpp').write_text('''
#include "engines/sotc/collision.h"
extern "C" unsigned terrain(const unsigned char *p,const int *r,unsigned d) {
 return Sotc::terrainCollision(p,{r[0],r[1],r[2],r[3]},d);
}
extern "C" unsigned obstacles(const unsigned char *p,unsigned n,const int *r) {
 return Sotc::obstacleCollision(p,n,{r[0],r[1],r[2],r[3]});
}
extern "C" unsigned actors(const unsigned char *p,unsigned self,const int *r) {
 Sotc::CollisionActor a[11];
 for(unsigned i=0;i<11;++i) {
  const unsigned char *q=p+i*52;
  int x=Sotc::walkWord(q[0]|q[1]<<8),y=Sotc::walkWord(q[2]|q[3]<<8);
  a[i]={Sotc::footprint(x,y,q[21],q[22]),bool(q[26]|q[27]),q[23]==8};
 }
 return Sotc::actorCollision(a,self,{r[0],r[1],r[2],r[3]});
}
''')
 subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-shared','-fPIC','-I',str(root),str(t/'test.cpp'),'-o',str(t/'test.so')],check=True)
 lib=c.CDLL(str(t/'test.so'));rng=random.Random(9)
 hits=[set(),set(),set()]
 for n in range(1200):
  x=rng.randrange(320);y=rng.randrange(220);w=rng.randrange(16);h=rng.randrange(10)
  r=[x-w,y-h,x+w,y];rc=(c.c_int*4)(*r);u.mem_write(0x50100,struct.pack('<4h',*r));d=n%4+1
  terrain=bytes(rng.randrange(130,180) for _ in range(320));u.mem_write(ds+0x339e,terrain)
  actual=call(0xcf9d,[0x100,0x5000,d]);assert lib.terrain(terrain,rc,d)==actual;hits[0].add(actual)
  rects=bytes(rng.randrange(160) for _ in range(32));u.mem_write(ds+0x3542,b'\x08');u.mem_write(ds+0x3544,rects)
  actual=call(0xd116,[0x100,0x5000]);assert lib.obstacles(rects,8,rc)==actual;hits[1].add(actual)
  actors=bytearray(11*52)
  for i in range(11):
   a=i*52;struct.pack_into('<hh',actors,a,rng.randrange(320),rng.randrange(200))
   actors[a+21]=rng.randrange(30);actors[a+22]=rng.randrange(30)
   actors[a+23]=rng.choice([0,0,8]);actors[a+26]=rng.randrange(2)
  u.mem_write(ds+0x2e54,bytes(actors));self=n%11
  actual=call(0xd053,[self,0x100,0x5000]);assert lib.actors(bytes(actors),self,rc)==actual;hits[2].add(actual)
 assert all(len(s)>2 for s in hits)
 print('Original collision predicates matched: 3600 cases; result coverage', [len(s) for s in hits])
