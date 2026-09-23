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
extern std::vector<TestResource> resources[2];
class Pak {
 bool rooms=false;
public:
 std::vector<Entry> entries;
 bool open(const Common::String &name) {rooms=name=="D05.PAK"; for(auto &r:resources[rooms])entries.push_back(r.entry);return true;}
 bool load(unsigned i,std::vector<byte> &out) const {if(i>=resources[rooms].size())return false;out=resources[rooms][i].data;return !out.empty();}
};
class SoundEffects {public: bool effect(unsigned) {return true;} bool repeatedEffect(unsigned i,unsigned n) {return i==34 && n==3;}};
}
''')
 (t/'test.cpp').write_text(r'''
#include "sotc/openingscene.h"
#include "sotc/opening.h"
#include "sotc/explode.h"
namespace Sotc {std::vector<TestResource> resources[2];}
extern "C" bool unpack(const uint8_t*p,unsigned n,uint8_t*out,unsigned size,unsigned flags) {return Sotc::explode(p,n,out,size,flags);}
extern "C" void resource(unsigned bank,unsigned id,const uint8_t*p,unsigned n,const char *name) {
 auto &r=Sotc::resources[bank];if(r.size()<=id)r.resize(id+1);r[id].data.assign(p,p+n);r[id].entry.name=name;
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
 for bank,name in enumerate(['A05.PAK','D05.PAK']):
  path=a.directory/name;raw=path.read_bytes()
  for e in pak.inspect(path)['entries']:
   data=raw[e['payload']:e['payload']+e['packed']]
   if e['method']==1:
    dst=c.create_string_buffer(e['unpacked']);assert lib.unpack(data,len(data),dst,len(dst),e['flags']);data=dst.raw
   else:assert e['method']==0
   lib.resource(bank,e['index'],data,len(data),e['name'].encode())
 script=(a.directory/'R05.CC4').read_bytes();changes=c.c_uint()
 elapsed=lib.run(script,len(script),c.byref(changes))
 assert elapsed>=0,(elapsed,changes.value)
 assert elapsed>100000 and changes.value>350,(elapsed,changes.value)
 first=(elapsed,changes.value)
 assert lib.run(script,len(script),c.byref(changes))==first[0]
 assert changes.value==first[1]  # Deterministic scheduler and seeded random task.
 print(f'Animated opening reached supported boundary in {elapsed} ms, {changes.value} changed frames')
 # Missing actor data must stop visibly; it must not silently continue the script.
 lib.resource(0,17,b'',0,b'missing credits')
 assert lib.run(script,len(script),c.byref(changes))==-4
