#!/usr/bin/env python3
"""Exercise real scene/VM save code with original assets; stub only filesystem API."""
import argparse
from pathlib import Path
import subprocess
import tempfile
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('directory',type=Path)
a=p.parse_args()
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='sotc-save-') as tmp:
 t=Path(tmp); (t/'common').mkdir()
 (t/'common/array.h').write_text('''#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
using byte=uint8_t;using uint32=uint32_t;using int64=int64_t;using uint=unsigned;
namespace Common {template<class T> using Array=std::vector<T>;}
''')
 (t/'common/str.h').write_text('''#pragma once
#include <string>
#include <cstdarg>
#include <cstdio>
namespace Common {class String:public std::string {public: using std::string::string; String(const std::string &s):std::string(s) {} static String format(const char *f,...) {char b[1024];va_list a;va_start(a,f);vsnprintf(b,sizeof(b),f,a);va_end(a);return b;}};using Path=String;}
''')
 (t/'common/file.h').write_text('''#pragma once
#include "common/array.h"
#include "common/str.h"
#include <fstream>
namespace Common {class File {std::ifstream f;int64 n=0;public:bool open(const Path &p) {f.open(p.c_str(),std::ios::binary);if(!f)return false;f.seekg(0,std::ios::end);n=f.tellg();f.seekg(0);return true;}int64 size(){return n;}unsigned read(void *p,unsigned n){f.read((char*)p,n);return f.gcount();}};}
''')
 (t/'common/endian.h').write_text('''#pragma once
inline unsigned READ_LE_UINT16(const byte*p){return p[0]|unsigned(p[1])<<8;}
inline unsigned READ_LE_UINT32(const byte*p){return READ_LE_UINT16(p)|READ_LE_UINT16(p+2)<<16;}
''')
 for name in ['debug','textconsole']:
  (t/'common'/f'{name}.h').write_text('#pragma once\ninline void '+('debug' if name=='debug' else 'warning')+'(const char*,...) {}\n')
 (t/'common/savefile.h').write_text('''#pragma once
#include "common/array.h"
#include "common/str.h"
#include <map>
namespace Common {
class InSaveFile {Array<byte> d;public:InSaveFile(const Array<byte>&a):d(a){} int64 size(){return d.size();}unsigned read(void*p,unsigned n){n=n<d.size()?n:d.size();memcpy(p,d.data(),n);return n;}bool err(){return false;}};
class OutSaveFile {Array<byte> &d;bool broken;public:OutSaveFile(Array<byte>&a,bool b):d(a),broken(b){d.clear();}unsigned write(const void*p,unsigned n){if(broken)n/=2;d.assign((const byte*)p,(const byte*)p+n);return n;}void finalize(){}bool err(){return broken;}};
class SaveFileManager {public:std::map<String,Array<byte>> files;bool fail=false;InSaveFile *openForLoading(const String &s){auto i=files.find(s);return i==files.end()?nullptr:new InSaveFile(i->second);}OutSaveFile *openForSaving(const String&s){return new OutSaveFile(files[s],fail);}};
}
''')
 (t/'test.cpp').write_text(r'''
#include "sotc/save.h"
#include <cassert>
#include <iostream>
using namespace Sotc;
struct Sound:ScriptSound {bool speech(unsigned,unsigned)override{return true;}bool music(unsigned)override{return true;}bool effect(unsigned)override{return true;}} sound;
static std::vector<byte> state(ScriptMachine &m,StartupScene &scene) {
 std::vector<byte> data(400000);Snapshot s(data.data(),data.size(),false);
 assert(scene.roomSnapshot(s,m));assert(m.snapshot(s));assert(scene.snapshot(s));data.resize(s.size());return data;
}
int main() {
 ScriptMachine m; StartupScene scene; Common::Array<byte> pixels;Common::String status;
 m.setSound(&sound);m.setAnimation(&scene);assert(scene.load(pixels,status,m));
 Common::SaveFileManager files;SaveState saved;saved.ticks=47;
 // Active scripted waits, RNG and moving route must survive, not just position.
 m.tick();m.seedRandom(9123);m.randomRange(17);m.setEventPending(true);
 assert(scene.destination(225,176,m));
 auto before=state(m,scene);assert(writeQuickSave(files,"test",m,scene,saved));
 for(unsigned i=0;i<3;++i){scene.movePlayer(m);scene.advance(m);}
 assert(state(m,scene)!=before);
 assert(readQuickSave(files,"test",m,scene,saved,pixels,status));assert(state(m,scene)==before);assert(saved.ticks==47);
 // Restore independently into a fresh session, then execute both states forward.
 ScriptMachine other;StartupScene otherScene;other.setSound(&sound);other.setAnimation(&otherScene);
 assert(readQuickSave(files,"test",other,otherScene,saved,pixels,status));assert(state(other,otherScene)==before);
 for(unsigned i=0;i<3;++i){assert(scene.movePlayer(m)==otherScene.movePlayer(other));assert(scene.advance(m)==otherScene.advance(other));m.tick();other.tick();assert(state(m,scene)==state(other,otherScene));}
 // Portrait dialogue resources and pre-talk animation must survive a round trip.
 scene.cancelRoute();scene.walk(0,m);bool pending=false;scene.dialoguePending(pending);
 if(pending){scene.finishDialogue();scene.dialoguePending(pending);}
 assert(scene.dialogue(1,0,255,256));before=state(m,scene);
 assert(writeQuickSave(files,"test",m,scene,saved));scene.finishDialogue();scene.dialoguePending(pending);
 assert(readQuickSave(files,"test",m,scene,saved,pixels,status));assert(state(m,scene)==before);
 // A partial overwrite retains the previous complete generation.
 files.fail=true;assert(!writeQuickSave(files,"test",m,scene,saved));files.fail=false;
 scene.finishDialogue();assert(readQuickSave(files,"test",m,scene,saved,pixels,status));assert(state(m,scene)==before);
 // Corrupt both copies: fail without modifying the live scene.
 for(auto &f:files.files) if(f.second.size()>20)f.second[20]^=1;
 assert(!readQuickSave(files,"test",m,scene,saved,pixels,status));assert(state(m,scene)==before);
 // Structurally invalid data with a recomputed checksum is also rejected transactionally.
 assert(writeQuickSave(files,"bad",m,scene,saved));
 for(auto &f:files.files) if(f.first.find("bad")==0) {
  auto &d=f.second;d[20]=d[21]=d[22]=d[23]=255;
  unsigned h=Snapshot::hash(d.data()+16,d.size()-16);for(unsigned i=0;i<4;++i)d[12+i]=h>>(8*i);
 }
 assert(!readQuickSave(files,"bad",m,scene,saved,pixels,status));assert(state(m,scene)==before);
 // Legacy V1 payloads have no room prefix; retain their INIT-based decoding.
 assert(scene.load(pixels,status,m));
 auto &legacy=files.files["legacy.quick0"];legacy.resize(400000);
 Snapshot old(legacy.data(),legacy.size(),false);
 old.identity(0x43544f53);old.identity(1);old.identity(1);old.identity(0);saved.sync(old);
 assert(m.snapshot(old,1));assert(scene.snapshot(old));legacy.resize(old.size());
 unsigned legacyHash=Snapshot::hash(legacy.data()+16,legacy.size()-16);
 for(unsigned i=0;i<4;++i)legacy[12+i]=legacyHash>>(i*8);
 before=state(m,scene);assert(readQuickSave(files,"legacy",m,scene,saved,pixels,status));assert(state(m,scene)==before);
 // Execute the original initial conversation and transition, advancing dialogue
 // as a player would. No task PCs or global story flags are patched.
 assert(scene.load(pixels,status,m));
 m.tick();assert(scene.destination(112,176,m));
 bool entered=false,entered11=false,entered17=false,choices=false;
 for(unsigned tick=0;tick<1600;++tick) {
  scene.finishDialogue();m.setEventPending(true);
  assert(scene.movePlayer(m));m.tick();
  if(!scene.serviceRoom(m,status)) { std::cerr<<status<<"\n";break; }
  assert(scene.render(pixels,status,m));assert(scene.advance(m)==0);
  for(unsigned j=0;j<m.count();++j) if(m.task(j)->result>=kUnknownWait) {
   assert(scene.room()==17 && j==5 && m.task(j)->pc==10 && m.task(j)->opcode==53);
   choices=true;

  }
  if(scene.room()==21 && !entered) {
   entered=true;before=state(m,scene);assert(writeQuickSave(files,"room21",m,scene,saved));
   assert(readQuickSave(files,"legacy",m,scene,saved,pixels,status));assert(scene.room()==0);
   assert(readQuickSave(files,"room21",m,scene,saved,pixels,status));assert(scene.room()==21);assert(state(m,scene)==before);
   std::cout<<"Entered room 21; cross-room save restore passed\n";
  }
  if(scene.room()==17 && !entered17) {
   entered17=true;before=state(m,scene);assert(writeQuickSave(files,"room17",m,scene,saved));
   assert(readQuickSave(files,"legacy",m,scene,saved,pixels,status));
   assert(readQuickSave(files,"room17",m,scene,saved,pixels,status));assert(scene.room()==17);assert(state(m,scene)==before);
   assert(m.actor(0)->x==160 && m.actor(0)->y==190); // Original default entrance, not the prior door region.
  }
  if(choices) { std::cout<<"Room 17 arrival and NPC destinations passed; next blocker is dialogue choices (53)\n";break; }
  if(scene.room()==11 && !entered11) {
   entered11=true;before=state(m,scene);assert(writeQuickSave(files,"room11",m,scene,saved));
   assert(readQuickSave(files,"legacy",m,scene,saved,pixels,status));
   assert(readQuickSave(files,"room11",m,scene,saved,pixels,status));assert(scene.room()==11);assert(state(m,scene)==before);
   std::cout<<"Room 11 initialization and cross-room save restore passed\n";
  }
 }
 assert(entered && entered11 && entered17 && choices);
 std::cout<<"Save round trips: route, script waits, RNG, fresh load, dialogue; interrupted/corrupt saves rejected\n";
}
''')
 subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Wno-misleading-indentation','-g','-fsanitize=address,undefined','-I',str(t),'-I',str(root/'engines'),str(t/'test.cpp'),*[str(root/'engines/sotc'/f) for f in ['startup.cpp','save.cpp','pak.cpp']],'-o',str(t/'test')],check=True)
 subprocess.run([str(t/'test')],cwd=a.directory.resolve(),check=True)
