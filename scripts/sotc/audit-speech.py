#!/usr/bin/env python3
"""Validate the runtime NAR reader against supplied recordings, without bundling them."""
import argparse,ctypes as c,struct,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('directory',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as td:
 td=Path(td);(td/'test.cpp').write_text('''#include "engines/sotc/nar.h"
extern "C" bool entry(const uint8_t *p,unsigned n,unsigned f,unsigned i,unsigned *s,unsigned *e) {return Sotc::narEntry(Sotc::Cc4View(p,n),f,i,*s,*e);}
extern "C" bool chunk(const uint8_t *p,unsigned n,unsigned pos,unsigned *out) {Sotc::NarChunk v; if (!Sotc::narChunk(Sotc::Cc4View(p,n),pos,v)) return false;out[0]=v.offset;out[1]=v.size;out[2]=v.rate;out[3]=v.next;return true;}
''')
 subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-shared','-fPIC','-I',str(root),str(td/'test.cpp'),'-o',str(td/'test.so')],check=True)
 lib=c.CDLL(str(td/'test.so'));lib.entry.restype=lib.chunk.restype=c.c_bool
 records=parts=missing=0; rates=set();example=None
 for path in sorted(a.directory.glob('*.NAR')):
  data=path.read_bytes();first=next(struct.unpack_from('<I',data,i)[0] for i in range(0,65536,4) if struct.unpack_from('<I',data,i)[0]);table=data[:first]
  offsets=struct.unpack('<'+'I'*(first//4),table)
  for i,start in enumerate(offsets):
   s,e=c.c_uint(),c.c_uint()
   assert lib.entry(table,len(table),len(data),i,c.byref(s),c.byref(e)),(path.name,i)
   assert s.value==start
   if not start:missing+=1;continue
   end=next((x for x in offsets[i+1:] if x),len(data));assert e.value==end
   record=data[start:end];pos=0;record_rates=set()
   while pos<len(record):
    out=(c.c_uint*4)();assert lib.chunk(record,len(record),pos,out),(path.name,i,pos)
    length=int.from_bytes(record[pos+27:pos+30],'little')
    assert list(out)==[pos+32,length-2,1000000//(256-record[pos+30]),pos+length+31]
    assert record[out[0]:out[0]+out[1]]==record[pos+32:pos+30+length]
    rates.add(out[2]);record_rates.add(out[2]);pos=out[3];parts+=1
   assert len(record_rates)==1 and pos==len(record)
   records+=1;example=record
 assert example
 out=(c.c_uint*4)()
 for length in range(33):assert not lib.chunk(example[:length],length,0,out)
 for offset,value in ((1,0),(20,0),(26,2),(31,1),(27,255),(28,255),(29,255)):
  bad=bytearray(example);bad[offset]=value
  assert not lib.chunk(bytes(bad),len(bad),0,out),(offset,value)
 assert not lib.chunk(example,len(example),len(example)+1,out)
 print('NAR recordings:',records,'VOC parts:',parts,'missing indices:',missing,'rates:',sorted(rates))
