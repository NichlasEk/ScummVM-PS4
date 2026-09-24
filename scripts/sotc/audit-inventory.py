#!/usr/bin/env python3
"""Exercise the original inventory frame, icons and selection using externally supplied game resources."""
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
#include "sotc/inventory.h"
#include "sotc/explode.h"
extern "C" bool unpack(const uint8_t*p,unsigned n,uint8_t*o,unsigned m,unsigned f){return Sotc::explode(p,n,o,m,f);}
extern "C" bool test(const byte*i,unsigned ni,const byte*f,unsigned nf,const byte*t,unsigned nt,byte*out){
 Common::Array<byte> frame(i,i+ni),font(f,f+nf),objects(t,t+nt),screen(64000,0);
 Sotc::Inventory menu;if(!menu.load(frame,objects,font))return false;
 Common::Array<unsigned> ids={0,10,42,70};
 Common::Array<Common::String> names={"Notebook","Set of plates","Magnifying glass","Rope ladder"};
 if(!menu.render(screen,ids,names,2))return false;
 for(unsigned j=0;j<64000;++j)out[j]=screen[j];
 if(menu.itemAt(100,88,2,4)!=2 || menu.itemAt(72,88,2,4)!=-1 || menu.itemAt(100,120,2,4)!=-1)return false;
 ids.resize(21,0);names.resize(21,"Notebook");
 for(unsigned selected=0;selected<21;++selected)if(!menu.render(screen,ids,names,selected))return false;
 if(menu.itemAt(100,63,20,21)!=20 || menu.itemAt(100,75,20,21)!=-1)return false;
 ids.clear();names.clear();return menu.render(screen,ids,names,0);
}
''')
 subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-shared','-fPIC','-I',str(t),'-I',str(root/'engines'),str(t/'test.cpp'),'-o',str(t/'test.so')],check=True)
 lib=c.CDLL(str(t/'test.so'));lib.test.restype=lib.unpack.restype=c.c_bool
 pak=a.directory/'RES.PAK';raw=pak.read_bytes();entries=inspect(pak)['entries']
 def load(i):
  e=entries[i];b=raw[e['payload']:e['payload']+e['packed']]
  if e['method']==0:return b
  out=c.create_string_buffer(e['unpacked']);assert lib.unpack(b,len(b),out,len(out),e['flags']);return out.raw
 icons,font,text=load(3),load(0),load(4);out=c.create_string_buffer(64000)
 assert lib.test(icons,len(icons),font,len(font),text,len(text),out)
 from PIL import Image
 image=Image.frombytes('P',(320,200),out.raw);image.putpalette(load(5));image.resize((960,600)).save(str(root/'artifacts/sotc/inventory-preview.png'))
 print('Original inventory frame/icons/font, row hit tests, empty inventory and page boundaries passed')
