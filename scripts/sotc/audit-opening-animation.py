#!/usr/bin/env python3
"""Run the actual animated-opening player with external assets and stubbed audio/I/O."""
import ctypes as c
import importlib.util
from pathlib import Path
import argparse
import subprocess
import tempfile
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('directory',type=Path)
a=p.parse_args()
root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('pak',Path(__file__).with_name('inspect-pak.py'))
pak=importlib.util.module_from_spec(spec);spec.loader.exec_module(pak)
with tempfile.TemporaryDirectory(prefix='sotc-opening-animation-') as tmp:
 t=Path(tmp);(t/'common').mkdir()
 (t/'common/array.h').write_text('#pragma once\n#include <vector>\n#include <cstdint>\nusing byte=uint8_t; using uint32=uint32_t; namespace Common {template<class T> using Array=std::vector<T>;}\n')
 for name in ['str.h','debug.h','textconsole.h']:(t/'common'/name).write_text('#pragma once\n')
 (t/'stub.h').write_text(r'''
#include "common/array.h"
#include <string>
#include <cstring>
#include <cstdio>
#include <algorithm>
#define MIN std::min
#define SOTC_PAK_H
#define SOTC_SOUND_H
namespace Common {using String=std::string;}
inline void debug(const char *,...) {}
inline void warning(const char *message,unsigned pc,const char *reason) {fprintf(stderr,message,pc,reason);fputc('\n',stderr);}
namespace Sotc {
struct Entry {Common::String name;};
struct TestResource {std::vector<byte> data;Entry entry;};
extern std::vector<TestResource> resources[5];
class Pak {
 unsigned rooms=0;
public:
 std::vector<Entry> entries;
 bool open(const Common::String &name) {rooms=name=="A09.PAK" ? 3 : name=="D09.PAK" ? 4 : name=="RES.PAK" ? 2 : name=="D05.PAK" ? 1 : 0; entries.clear(); for(auto &r:resources[rooms])entries.push_back(r.entry);return true;}
 bool load(unsigned i,std::vector<byte> &out) const {if(i>=resources[rooms].size())return false;out=resources[rooms][i].data;return !out.empty();}
};
class SoundEffects {public: unsigned bank=999,text=999; bool speaking=false; bool music(unsigned) {return true;} bool speech(unsigned b,unsigned t) {bank=b;text=t;speaking=true;return true;} void stopSpeech() {speaking=false;} bool speechActive() const {return speaking;} bool effect(unsigned) {return true;} bool repeatedEffect(unsigned i,unsigned n) {return i==34 && n==3;}};
}
''')
 (t/'test.cpp').write_text(r'''
#include "sotc/openingscene.h"
#include "sotc/opening.h"
#include "sotc/explode.h"
namespace Sotc {std::vector<TestResource> resources[5];}
extern "C" bool unpack(const uint8_t*p,unsigned n,uint8_t*out,unsigned size,unsigned flags) {return Sotc::explode(p,n,out,size,flags);}
extern "C" void resource(unsigned bank,unsigned id,const uint8_t*p,unsigned n,const char *name) {
 auto &r=Sotc::resources[bank];if(r.size()<=id)r.resize(id+1);r[id].data.assign(p,p+n);r[id].entry.name=name;
}
extern "C" int cd(const uint8_t*p,unsigned n,const uint8_t*t,unsigned tn) {
 Sotc::SoundEffects sound;Sotc::OpeningScene scene(sound);
 if(!scene.startCd(Sotc::Cc4View(p,n),Sotc::Cc4View(t,tn)))return -1;
 unsigned lines=0,mask=0,visited=0,last=999; const unsigned blocks[]={0,1,2,3,5,6,7,6,8}; const unsigned expected[]={0,1,3,4,5,6,12};
 for(unsigned ms=0;ms<600000;ms+=16) {
  scene.advance(16);mask|=1U<<scene.cdBlock();
  if(scene.cdBlock()!=last) {
   last=scene.cdBlock();
   if(visited>=9 || last!=blocks[visited++])return -7;
  }
  if(!scene.dialogueText().empty()) {
   if(lines>=7 || sound.bank!=5 || sound.text!=expected[lines++])return -2;
   const auto text=scene.dialogueText();
   for(unsigned i=0;i<200;++i)scene.advance(16);
   if(scene.dialogueText()!=text || scene.done())return -5;
   sound.speaking=false;scene.advance(16);
   if(!scene.dialogueText().empty())return -6;
  }
  if(scene.done())return scene.failed() || lines!=7 || mask!=495 || visited!=9 ? -3 : int(ms);
 }
 return -4;
}
extern "C" int story(const uint8_t*p,unsigned n,const uint8_t*t,unsigned tn) {
 Sotc::SoundEffects sound;Sotc::OpeningScene scene(sound);
 if(!scene.startStory(Sotc::Cc4View(p,n),Sotc::Cc4View(t,tn)))return -1;
 unsigned spoke=0;
 for(unsigned ms=0;ms<120000;ms+=16) {
  scene.advance(16);
  if(!scene.dialogueText().empty()) {
   if(sound.bank!=4 || sound.text!=spoke)return -6;
   if(!spoke && (scene.textX()!=150 || scene.textY()!=10 || scene.textColor()!=147))return -2;
   const auto text=scene.dialogueText();
   for(unsigned i=0;i<1000;++i)scene.advance(16);
   if(scene.done() || scene.dialogueText()!=text)return -3;
   ++spoke;scene.finishDialogue();
  }
  if(scene.done())return scene.failed() || spoke!=10 ? -4 : int(ms);
 }
 return -5;
}
extern "C" int run(const uint8_t*p,unsigned n,unsigned *changes) {
 Sotc::OpeningPlan plan={};if(!plan.load(Sotc::Cc4View(p,n)))return -1;
 Sotc::SoundEffects sound;Sotc::OpeningScene scene(sound);
 if(!scene.start(Sotc::Cc4View(p,n),plan.continuation,Sotc::resources[1][10].data))return -2;
 std::vector<byte> previous=scene.pixels(); *changes=0;
 for(unsigned ms=0;ms<300000;ms+=16) {
  scene.advance(16);
  if(scene.pixels()!=previous) {++*changes;previous=scene.pixels();}
  if(scene.brightness()>255 || scene.pixels().size()!=64000)return -3;
  if(scene.done())return scene.failed() ? -4 : int(ms);
 }
 return -5;
}
''')
 subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-O2','-shared','-fPIC','-include',str(t/'stub.h'),'-I',str(t),'-I',str(root/'engines'),str(t/'test.cpp'),str(root/'engines/sotc/openingscene.cpp'),'-o',str(t/'test.so')],check=True)
 lib=c.CDLL(str(t/'test.so'))
 lib.unpack.argtypes=[c.c_char_p,c.c_uint,c.c_void_p,c.c_uint,c.c_uint];lib.unpack.restype=c.c_bool
 lib.resource.argtypes=[c.c_uint,c.c_uint,c.c_char_p,c.c_uint,c.c_char_p]
 lib.run.argtypes=[c.c_char_p,c.c_uint,c.POINTER(c.c_uint)];lib.run.restype=c.c_int
 loaded={}
 for bank,name in enumerate(['A05.PAK','D05.PAK','RES.PAK','A09.PAK','D09.PAK']):
  path=a.directory/name;raw=path.read_bytes()
  for e in pak.inspect(path)['entries']:
   data=raw[e['payload']:e['payload']+e['packed']]
   if e['method']==1:
    dst=c.create_string_buffer(e['unpacked']);assert lib.unpack(data,len(data),dst,len(dst),e['flags']);data=dst.raw
   else:assert e['method']==0
   loaded[bank,e['index']]=(data,e['name'].encode())
   lib.resource(bank,e['index'],data,len(data),e['name'].encode())
 script=(a.directory/'R05.CC4').read_bytes();changes=c.c_uint()
 elapsed=lib.run(script,len(script),c.byref(changes))
 assert elapsed>=0,(elapsed,changes.value)
 assert elapsed>100000 and changes.value>350,(elapsed,changes.value)
 first=(elapsed,changes.value)
 assert lib.run(script,len(script),c.byref(changes))==first[0]
 assert changes.value==first[1]  # Deterministic scheduler and seeded random task.
 print(f'Animated opening reached supported boundary in {elapsed} ms, {changes.value} changed frames')
 text=(a.directory/'E.CC4').read_bytes()
 lib.story.argtypes=[c.c_char_p,c.c_uint,c.c_char_p,c.c_uint];lib.story.restype=c.c_int
 story_ms=lib.story(script,len(script),text,len(text))
 assert story_ms>=0,story_ms
 print(f'Story office completed in {story_ms} ms; dialogue held until confirmation')
 assert lib.story(script,len(script),text,len(text))==story_ms
 lib.resource(2,2,b'',0,b'missing player')
 assert lib.story(script,len(script),text,len(text))==-1
 data,name=loaded[2,2];lib.resource(2,2,data,len(data),name)
 lib.resource(0,16,b'',0,b'missing portrait')
 assert lib.story(script,len(script),text,len(text))==-4
 data,name=loaded[0,16];lib.resource(0,16,data,len(data),name)
 cdscript=(a.directory/'R09.CC4').read_bytes()
 lib.cd.argtypes=[c.c_char_p,c.c_uint,c.c_char_p,c.c_uint];lib.cd.restype=c.c_int
 cd_ms=lib.cd(cdscript,len(cdscript),text,len(text))
 assert cd_ms>=0,cd_ms
 broken=bytearray(cdscript);block=int.from_bytes(broken[:4],'little');init=int.from_bytes(broken[block:block+2],'little');broken[block+init]=254
 assert lib.cd(bytes(broken),len(broken),text,len(text))==-1
 assert lib.cd(cdscript[:4],4,text,len(text))==-1
 print(f'CD opening completed in {cd_ms} ms, seven speech records and all eight scene IDs verified')
 # Missing actor data must stop visibly; it must not silently continue the script.
 lib.resource(0,17,b'',0,b'missing credits')
 assert lib.run(script,len(script),c.byref(changes))==-4
