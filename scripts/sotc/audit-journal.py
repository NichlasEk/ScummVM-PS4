#!/usr/bin/env python3
"""Exercise the real journal renderer using externally supplied game resources."""
import argparse, ctypes as c, runpy, subprocess, tempfile
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('directory',type=Path)
a=p.parse_args();root=Path(__file__).resolve().parents[2]
inspect=runpy.run_path(str(Path(__file__).with_name('inspect-pak.py')))['inspect']
with tempfile.TemporaryDirectory(prefix='sotc-journal-') as folder:
 t=Path(folder);(t/'common').mkdir()
 (t/'common/array.h').write_text('#include <vector>\n#include <stdint.h>\ntypedef uint8_t byte;\nnamespace Common {template<class T> using Array=std::vector<T>;}\n')
 (t/'test.cpp').write_text('''
#include "sotc/journal.h"
#include "sotc/explode.h"
extern "C" bool unpack(const uint8_t*p,unsigned n,uint8_t*o,unsigned m,unsigned f){return Sotc::explode(p,n,o,m,f);}
extern "C" bool test(const uint8_t*i,unsigned ni,const uint8_t*f,unsigned nf,const uint8_t*t,unsigned nt,uint8_t*out){
 Common::Array<byte> icons(i,i+ni),font(f,f+nf),text(t,t+nt),screen(64000,0);
 Sotc::Journal j;if(!j.load(icons,font,text) || !j.open(1) || j.page()!=1 || j.turn(1))return false;
 if(!j.render(screen))return false;
 for(unsigned x=0;x<64000;++x)out[x]=screen[x];
 if(!j.turn(-1) || j.page()!=0 || j.turn(-1) || !j.render(screen))return false;
 if(!j.turn(1) || !j.render(screen))return false;
 for(unsigned x=0;x<64000;++x)if(out[x]!=screen[x])return false;
 if(j.open(65535) || !j.open(0) || j.turn(1))return false;
 Sotc::JournalFont pf;if(!pf.load(font) || pf.width("Just arrived")!=50)return false;
 font.resize(7);if(pf.load(font))return false;
 return true;
}
''')
 subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-shared','-fPIC','-I',str(t),'-I',str(root/'engines'),str(t/'test.cpp'),'-o',str(t/'test.so')],check=True)
 lib=c.CDLL(str(t/'test.so'));lib.test.restype=lib.unpack.restype=c.c_bool
 pak=a.directory/'RES.PAK';raw=pak.read_bytes();entries=inspect(pak)['entries']
 def load(i):
  e=entries[i];b=raw[e['payload']:e['payload']+e['packed']]
  if e['method']==0:return b
  out=c.create_string_buffer(e['unpacked']);assert lib.unpack(b,len(b),out,len(out),e['flags']);return out.raw
 icons,font,text=load(3),load(0),(a.directory/'E.CC4').read_bytes();out=c.create_string_buffer(64000)
 assert lib.test(icons,len(icons),font,len(font),text,len(text),out)
 # Diary text and page numbers must actually be drawn with original colors.
 assert out.raw.count(bytes([64]))>100 and out.raw.count(bytes([58]))>10
 print('Journal: original resources, font, current-progress limit, both page directions and stable redraw passed')
