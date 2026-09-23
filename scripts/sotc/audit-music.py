#!/usr/bin/env python3
"""Compare the ADLM player against register writes from the user's DOS driver.

Needs c++ and optional test-only unicorn (e.g. .tools/sotc-audit-env).
No original driver/music bytes are copied into the repository or package.
"""
import argparse
import ctypes as c
from pathlib import Path
import struct
import subprocess
import tempfile
import json
from unicorn import Uc, UC_ARCH_X86, UC_MODE_16, UC_HOOK_INSN
from unicorn.x86_const import *

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('music',type=Path)
p.add_argument('driver',type=Path)
p.add_argument('--ticks',type=int,default=60000)
a=p.parse_args()
root=Path(__file__).resolve().parents[2]
driver=a.driver.read_bytes()

def oracle(data):
    u=Uc(UC_ARCH_X86,UC_MODE_16);u.mem_map(0,0x100000)
    u.mem_write(0x10100,driver);u.mem_write(0x30000,data)
    output=[];selected=[0]
    def write(u,port,size,value,user):
        if port==0x388:selected[0]=value
        elif port==0x389:output.append((selected[0],value))
    u.hook_add(UC_HOOK_INSN,write,None,1,0,UC_X86_INS_OUT)
    u.hook_add(UC_HOOK_INSN,lambda *args:0,None,1,0,UC_X86_INS_IN)
    def call(address,**regs):
        for reg,value in [('CS',0x1000),('DS',0x1000),('SS',0x8000),('SP',0xffec),('EFLAGS',2)]:
            u.reg_write(globals()['UC_X86_REG_'+reg],value)
        u.mem_write(0x8ffec,struct.pack('<H',0xff00))
        for reg,value in regs.items():u.reg_write(globals()['UC_X86_REG_'+reg],value)
        u.emu_start(0x10000+address,0x1ff00,count=200000)
        assert u.reg_read(UC_X86_REG_IP)==0xff00
    call(0xbbe,CX=0x388)
    call(0x8fe)
    call(0xd9d,DX=0x3000,SI=0)
    call(0xbca,DX=0x80,SI=0,CX=0)
    output.clear()
    return call,output

with tempfile.TemporaryDirectory(prefix='sotc-music-') as t:
    t=Path(t)
    (t/'test.cpp').write_text('''
#include "engines/sotc/adlm.h"
#include <vector>
struct Output : Sotc::AdlmOutput {
 std::vector<unsigned> writes;
 void reg(unsigned a,unsigned v) override { writes.push_back(a); writes.push_back(v); }
} output;
Sotc::AdlmPlayer player(output);
extern "C" bool load(const uint8_t *d,unsigned n,const uint8_t *r,unsigned m) {
 bool ok=player.load(d,n,r,m); if(ok) player.start(); output.writes.clear(); return ok;
}
extern "C" bool tick() { output.writes.clear(); return player.tick(); }
extern "C" bool playing() {return player.playing();}
extern "C" unsigned count() {return output.writes.size();}
extern "C" unsigned get(unsigned i) {return output.writes.at(i);}
''')
    subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-shared','-fPIC','-I',str(root),str(t/'test.cpp'),'-o',str(t/'test.so')],check=True)
    lib=c.CDLL(str(t/'test.so'));lib.load.restype=lib.tick.restype=lib.playing.restype=c.c_bool
    lib.load.argtypes=[c.c_char_p,c.c_uint,c.c_char_p,c.c_uint]
    sample=next(a.music.glob('*.ADD')).read_bytes()
    for broken in (sample[:61], sample[:-1], sample[:8]+b'\xff'*4+sample[12:]):
        assert not lib.load(broken,len(broken),driver,len(driver))
    assert not lib.load(sample,len(sample),driver[:512],512)
    broken_driver=bytearray(driver)
    broken_driver[0x44b-256:0x44d-256]=b'\x00\x00'
    assert not lib.load(sample,len(sample),bytes(broken_driver),len(broken_driver))
    reports=[]
    for path in sorted(a.music.glob('*.ADD')):
        data=path.read_bytes()
        assert lib.load(data,len(data),driver,len(driver)),path
        call,expected=oracle(data)
        writes=0
        for tick in range(a.ticks):
            expected.clear();call(0xa4e)
            assert lib.tick()
            values=[lib.get(i) for i in range(lib.count())]
            actual=list(zip(values[::2],values[1::2]))
            assert actual==expected, (path.name,tick,actual,expected)
            writes+=len(actual)
            if not lib.playing():break
        reports.append(dict(file=path.name,ticks=tick+1,writes=writes,finished=not lib.playing()))
    print(json.dumps(reports,indent=2))
