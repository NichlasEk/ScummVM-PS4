#!/usr/bin/env python3
"""Exercise the actual Jönsson service branch across many short script calls."""
from pathlib import Path
import subprocess, sys, tempfile
s=(Path(sys.argv[1])/'engines/director/lingo/lingo.cpp').read_text()
a=s.index('\t\tif (ps4Jonsson) {',s.index('bool Lingo::execute('))
b=s.index(' else if (localCounter > 0',a)
branch=s[a:b]
code=r'''
#include <cassert>
#include <cstdint>
using uint32=uint32_t;
struct System {uint32 now=0;uint32 getMillis(){return now;}void delayMillis(int){}} system_,*g_system=&system_;
struct Score {void updateWidgets(bool){}} score;
struct Movie {Score* getScore(){return &score;}} movie;
struct VM {int polls=0,draws=0;void processSysEvents(){++polls;}Movie* getCurrentMovie(){return &movie;}void draw(){++draws;}} vm;
struct Lingo {
 VM *_vm=&vm;uint32 _lastPS4InputService=0;bool enabled=true;
 bool isPS4Jonsson(){return enabled;}
 void tick(unsigned localCounter){
 const bool ps4Jonsson = isPS4Jonsson();
''' + branch + r'''
 }
};
int main(){
 Lingo l;
 // New execute calls reset localCounter, but must not reset the shared clock.
 for(int i=0;i<1000;++i){system_.now=100+i/10;l.tick(0);l.tick(100);}
 assert(vm.polls==7 && vm.draws==7);
 // Disabled titles do not enter this path.
 l.enabled=false;system_.now=300;l.tick(0);assert(vm.polls==7);
 // Preserve input during long scripts; mid-batch instructions do no work.
 l.enabled=true;l.tick(99);assert(vm.polls==7);l.tick(100);assert(vm.polls==8);
 // Unsigned millisecond clock rollover.
 l._lastPS4InputService=0xfffffff8;system_.now=8;l.tick(0);assert(vm.polls==9);
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(code)
 subprocess.run(['c++','-std=c++17',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Director input service: short/long scripts, disabled title and timer rollover passed')
