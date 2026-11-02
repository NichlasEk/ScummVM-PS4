#!/usr/bin/env python3
"""Exercise the actual Jönsson service branch across many short script calls."""
from pathlib import Path
import subprocess, sys, tempfile
s=(Path(sys.argv[1])/'engines/director/lingo/lingo.cpp').read_text()
a=s.index('\t\tif (ps4Jonsson) {',s.index('bool Lingo::execute('))
b=s.index(' else if (localCounter > 0',a)
branch=s[a:b]
gate=next(line.strip() for line in s.splitlines() if "const bool ps4Jonsson1 =" in line)
code=r'''
#include <cassert>
#include <cstdint>
#include <string>
namespace Common {using String=std::string;}
using uint32=uint32_t;
struct System {uint32 now=0;uint32 getMillis(){return now;}void delayMillis(int){}} system_,*g_system=&system_;
struct Score {void updateWidgets(bool){}} score;
struct Movie {bool _videoPlayback=false;Score* getScore(){return &score;}} movie;
struct VM {std::string gameId="jonssonligan2";const char* getGameId(){return gameId.c_str();}int polls=0,draws=0;uint32 drawDelay=0;void processSysEvents(){++polls;}Movie* getCurrentMovie(){return &movie;}void draw(){++draws;system_.now+=drawDelay;}} vm;
struct Lingo {
 VM *_vm=&vm;uint32 _lastPS4InputService=0;bool enabled=true;bool mjolner=false;
 bool isPS4Jonsson(){return enabled;}
 void tick(unsigned localCounter){
 const bool ps4Jonsson = isPS4Jonsson();
 _vm->gameId = mjolner ? "jonssonligan1" : "jonssonligan2";
''' + gate + r'''
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
 // A blocking present must not make the very next short behavior present again.
 vm.drawDelay=80;system_.now=1000;l.tick(0);assert(vm.polls==10);
 for(int i=0;i<100;++i)l.tick(0);
 assert(vm.polls==10);
 system_.now+=15;l.tick(100);assert(vm.polls==10);
 system_.now+=1;l.tick(100);assert(vm.polls==11);
 // The same rule holds when a blocking present crosses clock rollover.
 l._lastPS4InputService=0xffffff00;system_.now=0xfffffff0;l.tick(0);
 assert(vm.polls==12);l.tick(0);assert(vm.polls==12);
 // Mjölner still polls input in scene scripts without presenting stale Stage data.
 l.mjolner=true;vm.drawDelay=0;system_.now+=16;
 int oldDraws=vm.draws;l.tick(0);assert(vm.polls==13 && vm.draws==oldDraws);
 // Videos retain mid-script updates; the second game's cursor path is unchanged.
 movie._videoPlayback=true;system_.now+=16;l.tick(0);assert(vm.draws==oldDraws+1);
 movie._videoPlayback=false;l.mjolner=false;system_.now+=16;l.tick(0);assert(vm.draws==oldDraws+2);
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(code)
 subprocess.run(['c++','-std=c++17',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Director input service: short/long scripts, disabled title, blocking present and timer rollover passed')
