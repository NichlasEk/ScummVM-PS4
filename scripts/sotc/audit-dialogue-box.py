#!/usr/bin/env python3
"""Exercise the original dialogue box and three-line pagination using externally supplied game resources."""
import argparse, ctypes as c, runpy, subprocess, tempfile
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('directory',type=Path)
a=p.parse_args();root=Path(__file__).resolve().parents[2]
inspect=runpy.run_path(str(Path(__file__).with_name('inspect-pak.py')))['inspect']
with tempfile.TemporaryDirectory(prefix='sotc-dialogue-') as folder:
 t=Path(folder);(t/'common').mkdir()
 (t/'common/array.h').write_text('#include <vector>\n#include <stdint.h>\ntypedef uint8_t byte;\nnamespace Common {template<class T> using Array=std::vector<T>;}\n')
 (t/'common/str.h').write_text('#include <string>\nnamespace Common {using String=std::string;}\n')
 (t/'test.cpp').write_text(r'''
#include "sotc/dialoguebox.h"
#include "sotc/explode.h"
extern "C" bool unpack(const uint8_t*p,unsigned n,uint8_t*o,unsigned m,unsigned f){return Sotc::explode(p,n,o,m,f);}
extern "C" bool test(const uint8_t*i,unsigned ni,const uint8_t*f,unsigned nf,const uint8_t*t,unsigned nt,uint8_t*out){
 Common::Array<byte> bubble(i,i+ni),font(f,f+nf),screen(64000,119);
 Sotc::DialogueBox box;if(!box.load(font,bubble))return false;
 uint8_t decoded[16384];unsigned n;
 if(!Sotc::dialogueRecord(Sotc::Cc4View(t,nt),3,240,decoded,sizeof(decoded),n))return false;
 Common::String text;for(unsigned j=1;j+1<n;++j)text+=decoded[j] ? char(decoded[j]) : '\n';
 box.text(text);if(box.count()!=11 || !box.more(0) || box.next(0)!=3)return false;
 if(!box.render(screen,0,160,160,true))return false;
 for(unsigned j=0;j<64000;++j)out[j]=screen[j];
 for(unsigned j=0;j<160*320;++j)if(screen[j]!=119)return false;
 screen.assign(64000,119);if(!box.render(screen,3,160,160,true))return false;
 bool changed=false;for(unsigned j=0;j<64000;++j)changed|=out[j]!=screen[j];if(!changed)return false;
 for(unsigned page=0;page<box.count();page=box.next(page))if(!box.render(screen,page,319,199,false))return false;
 if(box.more(9) || box.render(screen,box.count(),0,0,true))return false;
 box.text("");if(box.count() || box.more(0))return false;
 box.text(Common::String(500,'W'));if(box.count()<2)return false;
 for(unsigned page=0;page<box.count();page=box.next(page))if(!box.render(screen,page,-500,-500,false))return false;
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
 icons,font,text=load(1),load(0),(a.directory/'E.CC4').read_bytes();out=c.create_string_buffer(64000)
 assert lib.test(icons,len(icons),font,len(font),text,len(text),out)
 from PIL import Image
 image=Image.frombytes('P',(320,200),out.raw);image.putpalette(load(5));image.resize((960,600)).save(str(root/'artifacts/sotc/dialogue-box-preview.png'))
 print('Original Jugg dialogue: 11 authored lines, four pages, original font/bubble, bounds and long-line wrapping passed')
