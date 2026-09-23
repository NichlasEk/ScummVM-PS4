#!/usr/bin/env python3
"""Check nested text records against the original DOS decryption routine."""
import argparse,ctypes as c,hashlib,struct,subprocess,tempfile
from pathlib import Path
from unicorn import Uc,UC_ARCH_X86,UC_MODE_16
from unicorn.x86_const import *
p=argparse.ArgumentParser(description=__doc__);p.add_argument('executable',type=Path);p.add_argument('directory',type=Path)
a=p.parse_args();exe=a.executable.read_bytes()
assert hashlib.sha256(exe).hexdigest()=='26d6fc54a27e0bf17ea34a355aa9bf087c8f13dfb5d613d5d1d535190da9ab5b'
code=exe[struct.unpack_from('<H',exe,8)[0]*16:]
u=Uc(UC_ARCH_X86,UC_MODE_16);u.mem_map(0,0x100000);u.mem_write(0,code)
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as td:
 td=Path(td);(td/'test.cpp').write_text('''#include "engines/sotc/dialogue.h"
extern "C" bool decode(const uint8_t *p,unsigned n,unsigned b,unsigned i,uint8_t *out,unsigned *size) {return Sotc::dialogueRecord(Sotc::Cc4View(p,n),b,i,out,16384,*size);}
''')
 subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-shared','-fPIC','-I',str(root),str(td/'test.cpp'),'-o',str(td/'test.so')],check=True)
 lib=c.CDLL(str(td/'test.so'));lib.decode.restype=c.c_bool
 out=c.create_string_buffer(16384);size=c.c_uint()
 for raw in (b'',b'\x04\0\0\0',struct.pack('<3I',4,0xffffffff,0)):
  assert not lib.decode(raw,len(raw),0,0,out,c.byref(size))
 tested=0; rejected=0
 for name in ('E','D','S','I'):
  data=(a.directory/(name+'.CC4')).read_bytes();banks=struct.unpack_from('<I',data)[0]//4
  for bank in range(banks):
   base=struct.unpack_from('<I',data,bank*4)[0];end=struct.unpack_from('<I',data,(bank+1)*4)[0] if bank+1<banks else len(data)
   first=struct.unpack_from('<I',data,base)[0];count=first//4
   for index in range(count):
    start=base+struct.unpack_from('<I',data,base+index*4)[0]
    stop=base+struct.unpack_from('<I',data,base+(index+1)*4)[0] if index+1<count else end
    if start<base+first or stop> end or stop-start<2:
     rejected+=1; continue # unused placeholders or stale translated tail offsets
    out=c.create_string_buffer(16384);size=c.c_uint()
    assert lib.decode(data,len(data),bank,index,out,c.byref(size)),(name,bank,index)
    offset=start-base-first;raw=data[start:stop]
    u.mem_write(0x50000,raw)
    u.reg_write(UC_X86_REG_CS,0x68f);u.reg_write(UC_X86_REG_DS,0x17c4)
    u.reg_write(UC_X86_REG_SS,0x9000);u.reg_write(UC_X86_REG_SP,0xff00)
    u.mem_write(0x9ff00,struct.pack('<8H',0xfff0,0,0,0x5000,len(raw),0,offset&65535,offset>>16))
    u.emu_start(0x72c1,0xfff0,count=500000)
    assert u.reg_read(UC_X86_REG_IP)==0xfff0
    assert out.raw[:size.value]==bytes(u.mem_read(0x50000,len(raw))),(name,bank,index)
    tested+=1
 print('Original dialogue records matched:',tested, 'Unused/stale entries excluded:',rejected)
