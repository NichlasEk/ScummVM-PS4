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
 std::cout.setf(std::ios::unitbuf);
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
 // Version 2 room saves remain readable after adding choice state in V3.
 auto &v2=files.files["v2.quick0"];v2.resize(400000);Snapshot two(v2.data(),v2.size(),false);
 two.identity(0x43544f53);two.identity(2);two.identity(1);two.identity(0);saved.sync(two);
 assert(scene.roomSnapshot(two,m));assert(m.snapshot(two,2));assert(scene.snapshot(two));v2.resize(two.size());
 unsigned v2Hash=Snapshot::hash(v2.data()+16,v2.size()-16);
 for(unsigned i=0;i<4;++i)v2[12+i]=v2Hash>>(i*8);
 assert(readQuickSave(files,"v2",m,scene,saved,pixels,status));assert(state(m,scene)==before);
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
  for(unsigned j=0;j<m.count();++j) assert(m.task(j)->result<kUnknownWait);
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
  if(m.choiceCount()) {
   assert(scene.room()==17 && m.choiceCount()==2 && m.choiceText(0)==22 && m.choiceText(1)==23);
   choices=true;Common::String text;assert(scene.choiceText(22,text) && !text.empty());
   assert(scene.choiceText(23,text) && !text.empty());
   m.highlightChoice(1);before=state(m,scene);assert(writeQuickSave(files,"choices",m,scene,saved));
   assert(!m.choose(2));assert(state(m,scene)==before);
   for(unsigned selected=0;selected<2;++selected) {
    assert(readQuickSave(files,"legacy",m,scene,saved,pixels,status));
    assert(readQuickSave(files,"choices",m,scene,saved,pixels,status));assert(state(m,scene)==before);
    assert(m.choiceSelection()==1 && m.choose(selected));assert(m.visit(5)==kAnimationWait);
    assert(!m.choiceCount() && m.task(5)->opcode==89);
    // Original relative branch at operand 18 -> 24; operand 22 -> 44.
    assert(m.task(5)->pc==(selected==0 ? 30U : 50U));
    assert(!scene.dialogueText().empty());
   }
   std::cout<<"Room 17 arrival: both original dialogue branches and active-choice save restore passed\n";break;
  }
  if(scene.room()==11 && !entered11) {
   entered11=true;before=state(m,scene);assert(writeQuickSave(files,"room11",m,scene,saved));
   assert(readQuickSave(files,"legacy",m,scene,saved,pixels,status));
   assert(readQuickSave(files,"room11",m,scene,saved,pixels,status));assert(scene.room()==11);assert(state(m,scene)==before);
   std::cout<<"Room 11 initialization and cross-room save restore passed\n";
  }
 }
 assert(entered && entered11 && entered17 && choices);
 // Continue one original dialogue path through the house's bank transition.
 assert(readQuickSave(files,"choices",m,scene,saved,pixels,status));
 bool interior=false;
 for(unsigned tick=0;tick<1600;++tick) {
  scene.finishDialogue();if(m.choiceCount()) assert(m.choose(0));
  assert(scene.movePlayer(m));m.tick();
  if(!scene.serviceRoom(m,status)) { std::cerr<<status<<"\n";break; }
  assert(scene.render(pixels,status,m));assert(scene.advance(m)==0);
  if(scene.bank()==1 && scene.room()==2 && !interior) {
   interior=true;before=state(m,scene);assert(writeQuickSave(files,"interior",m,scene,saved));
   assert(readQuickSave(files,"legacy",m,scene,saved,pixels,status));
   assert(readQuickSave(files,"interior",m,scene,saved,pixels,status));assert(state(m,scene)==before);
   std::cout<<"Entered house (bank 1 / room 2); cross-bank save restore passed\n";
  }
 }
 assert(interior);assert(scene.objectCount()==2);
 for(unsigned j=0;j<m.count();++j) assert(m.task(j)->result<kUnknownWait);
 before=state(m,scene);assert(writeQuickSave(files,"objects",m,scene,saved));
 assert(scene.removeObject(1));assert(scene.objectCount()==1);
 assert(readQuickSave(files,"objects",m,scene,saved,pixels,status));assert(state(m,scene)==before);assert(scene.objectCount()==2);
 std::cout<<"House arrival reaches idle with both object hotspots; hotspot save restore passed\n";
 assert(scene.destination(44,130,m));
 for(unsigned tick=0;tick<350;++tick) {
  scene.finishDialogue();if(m.choiceCount()) assert(m.choose(0));
  assert(scene.movePlayer(m));m.tick();
  assert(scene.serviceRoom(m,status));
  assert(scene.render(pixels,status,m));assert(scene.advance(m)==0);
 }
 assert(readQuickSave(files,"objects",m,scene,saved,pixels,status));
 ScriptActor player=*m.actor(0);player.x=68;player.y=135;assert(m.setActor(0,player));assert(scene.face(0,1));
 int lookX=0,lookY=0;assert(scene.lookedObject(lookX,lookY)>=0);assert(lookX==68 && lookY==94);
 assert(scene.face(0,3));assert(scene.lookedObject(lookX,lookY)<0);assert(scene.face(0,1));
 Common::String item;assert(scene.takeObject(item));assert(item=="Boleskine's diary");assert(m.inventoryState(1)==1);assert(scene.objectCount()==1);
 assert(!scene.takeObject(item));
 before=state(m,scene);assert(writeQuickSave(files,"pickup",m,scene,saved));
 assert(readQuickSave(files,"objects",m,scene,saved,pixels,status));assert(m.inventoryState(1)==0);
 assert(readQuickSave(files,"pickup",m,scene,saved,pixels,status));assert(state(m,scene)==before);
 for(unsigned tick=0;tick<300;++tick) {
  scene.finishDialogue();assert(scene.movePlayer(m));m.tick();
  assert(scene.serviceRoom(m,status));
  assert(scene.render(pixels,status,m));assert(scene.advance(m)==0);
 }
 assert(m.useObject(1));
 for(unsigned tick=0;tick<250;++tick) {
  scene.finishDialogue();assert(scene.movePlayer(m));m.tick();
  assert(scene.serviceRoom(m,status));
  assert(scene.render(pixels,status,m));assert(scene.advance(m)==0);
 }
 assert(m.inventoryState(1)==65535);
 player=*m.actor(0);player.x=40;player.y=160;assert(m.setActor(0,player));assert(scene.face(0,4));
 assert(scene.takeObject(item));assert(m.inventoryState(2)==1);
 for(unsigned phase=0;phase<2;++phase) {
  if(phase)assert(m.useObject(2));
  for(unsigned tick=0;tick<300;++tick){scene.finishDialogue();assert(scene.movePlayer(m));m.tick();assert(scene.serviceRoom(m,status));assert(scene.render(pixels,status,m));assert(scene.advance(m)==0);}
 }
 assert(m.inventoryState(2)==65535);assert(m.journalProgress()==2);
 assert(scene.destination(44,130,m));
 for(unsigned tick=0;tick<500;++tick) {
  scene.finishDialogue();assert(scene.movePlayer(m));m.tick();
  assert(scene.serviceRoom(m,status));
  assert(scene.render(pixels,status,m));assert(scene.advance(m)==0);
 }
 assert(scene.bank()==1 && scene.room()==1);
 for(unsigned i=0;i<m.count();++i)assert(m.task(i)->result<kUnknownWait);
 before=state(m,scene);assert(writeQuickSave(files,"hall",m,scene,saved));
 assert(readQuickSave(files,"objects",m,scene,saved,pixels,status));assert(scene.room()==2);
 assert(readQuickSave(files,"hall",m,scene,saved,pixels,status));assert(state(m,scene)==before);
 std::cout<<"Bedroom exit enters the hall and restores its save exactly\n";
 // Click the visible left door above the terrain: approach must trigger its original speech.
 assert(scene.destination(72,110,m));bool heardLocked=false;
 for(unsigned tick=0;tick<250;++tick){
  assert(scene.movePlayer(m));m.tick();assert(scene.serviceRoom(m,status));
  if(!scene.dialogueText().empty()){std::cout<<"Hall door says: "<<scene.dialogueText()<<"\n";heardLocked=true;scene.finishDialogue();}
  assert(scene.render(pixels,status,m));assert(scene.advance(m)==0);
 }
 assert(heardLocked);assert(scene.room()==1);
 assert(scene.exitDirection(160,198)==3);assert(scene.exitDirection(160,220)==3);
 assert(scene.exitDirection(20,198)==0);assert(scene.exitDirection(160,160)==0);
 assert(scene.destination(160,220,m));
 for(unsigned tick=0;tick<400;++tick){scene.finishDialogue();assert(scene.movePlayer(m));m.tick();assert(scene.serviceRoom(m,status));assert(scene.render(pixels,status,m));assert(scene.advance(m)==0);}
 assert(scene.bank()==1 && scene.room()==0);
 for(unsigned i=0;i<m.count();++i)assert(m.task(i)->result<kUnknownWait);
 std::cout<<"Hall door interaction and bottom-edge click enter room 0 without stops\n";
 // Inspect the foyer's bench using the same event source as the toolbar.
 assert(scene.destination(86,140,m));
 for(unsigned tick=0;tick<250;++tick){scene.finishDialogue();assert(scene.movePlayer(m));m.tick();if(!scene.serviceRoom(m,status)){std::cerr<<status<<std::endl;assert(false);}assert(scene.render(pixels,status,m));assert(scene.advance(m)==0);}
 assert(scene.examine());bool examined=false;
 for(unsigned tick=0;tick<100;++tick){assert(scene.movePlayer(m));m.tick();if(!scene.serviceRoom(m,status)){std::cerr<<status<<std::endl;assert(false);}
  if(!scene.dialogueText().empty()){std::cout<<"Foyer examine: "<<scene.dialogueText()<<"\n";examined=true;assert(!scene.examine());scene.finishDialogue();}
  assert(scene.render(pixels,status,m));assert(scene.advance(m)==0);
 }
 assert(examined);assert(!m.eventPending());
 // The path crosses the foyer door region and invokes its scripted return.
 assert(scene.examine());assert(scene.destination(160,180,m));assert(!m.eventPending());
 for(unsigned tick=0;tick<250;++tick){scene.finishDialogue();assert(scene.movePlayer(m));m.tick();if(!scene.serviceRoom(m,status)){std::cerr<<status<<std::endl;assert(false);}assert(scene.render(pixels,status,m));assert(scene.advance(m)==0);}
 assert(scene.bank()==1 && scene.room()==1);
 assert(scene.destination(160,220,m));
 for(unsigned tick=0;tick<400;++tick){scene.finishDialogue();assert(scene.movePlayer(m));m.tick();if(!scene.serviceRoom(m,status)){std::cerr<<status<<std::endl;assert(false);}assert(scene.render(pixels,status,m));assert(scene.advance(m)==0);}
 assert(scene.bank()==1 && scene.room()==0);
 assert(scene.destination(160,220,m));
 for(unsigned tick=0;tick<400;++tick){scene.finishDialogue();assert(scene.movePlayer(m));m.tick();if(!scene.serviceRoom(m,status)){std::cerr<<status<<std::endl;assert(false);}assert(scene.render(pixels,status,m));assert(scene.advance(m)==0);}
 std::cout<<"House departure bank="<<scene.bank()<<" room="<<scene.room()<<std::endl;
 assert(scene.bank()==0 && scene.room()==17);
 before=state(m,scene);assert(writeQuickSave(files,"outside",m,scene,saved));
 assert(readQuickSave(files,"hall",m,scene,saved,pixels,status));
 assert(readQuickSave(files,"outside",m,scene,saved,pixels,status));assert(state(m,scene)==before);
 for(unsigned i=0;i<m.count();++i)assert(m.task(i)->result<kUnknownWait);
 std::cout<<"Exit probe bank="<<scene.bank()<<" room="<<scene.room()<<" x="<<m.actor(0)->x<<" y="<<m.actor(0)->y<<"\n";
 std::cout<<"Object facing, pickup and inventory save restore passed\n";
 std::cout<<"Door probe ends in bank "<<scene.bank()<<" room "<<scene.room()<<"\n";

 std::cout<<"Save round trips: route, script waits, RNG, fresh load, dialogue; interrupted/corrupt saves rejected\n";
}
''')
 subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Wno-misleading-indentation','-g','-fsanitize=address,undefined','-I',str(t),'-I',str(root/'engines'),str(t/'test.cpp'),*[str(root/'engines/sotc'/f) for f in ['startup.cpp','save.cpp','pak.cpp']],'-o',str(t/'test')],check=True)
 subprocess.run([str(t/'test')],cwd=a.directory.resolve(),check=True)
