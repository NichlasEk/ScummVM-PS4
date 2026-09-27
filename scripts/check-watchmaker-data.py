#!/usr/bin/env python3
"""Compile the actual overlay record readers against a bounds-checking stream.
No game data is embedded. Optionally verify the locally owned English Data.wm.
"""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
src=root/'.tools/scummvm-vulkan-src/engines/watchmaker'
text=(src/'struct.h').read_text()
records=text[text.index('class SerializableAsset {'):text.index('struct SPlayerStand {')]
credits=text[text.index('struct SCreditsName '):text.index('struct STitoliCoda ')]
work=root/'build/watchmaker-data-test';work.mkdir(parents=True,exist_ok=True)
preamble=r'''
#include <cstdint>
#include <cstring>
#include <cassert>
#include <vector>
#include <fstream>
#include <iterator>
#include <cstdio>
#include "sysdef.h"
using uint8=uint8_t; using int8=int8_t; using uint16=uint16_t; using int16=int16_t;
using uint32=uint32_t; using int32=int32_t; using t3dF32=float;
#define T3D_NAMELEN 32
namespace Common {
class SeekableReadStream {
public:
 std::vector<uint8> bytes; size_t pos=0;
 explicit SeekableReadStream(size_t n):bytes(n,0){}
 void read(void *out,size_t n){assert(n<=bytes.size()-pos);memcpy(out,bytes.data()+pos,n);pos+=n;}
 void skip(size_t n){assert(n<=bytes.size()-pos);pos+=n;}
 uint8 readByte(){uint8 v;read(&v,1);return v;}
 uint16 readUint16LE(){unsigned a=readByte();return a|(unsigned(readByte())<<8);}
 int16 readSint16LE(){return readUint16LE();}
 uint32 readUint32LE(){uint32 a=readUint16LE();return a|(uint32(readUint16LE())<<16);}
 int32 readSint32LE(){return readUint32LE();}
 float readFloatLE(){uint32 v=readUint32LE();float out;memcpy(&out,&v,4);return out;}
};
}
'''
main=r'''
template<class T> void record(size_t length){Common::SeekableReadStream s(length);T t;t.loadFromStream(s);assert(s.pos==length);}
int main(int argc,char **argv){
 record<SRoom>(354);record<SObject>(548);record<SInvObject>(62);record<SSound>(572);
 record<SMusic>(360);record<SAnim>(2112);record<SDialog>(616);record<SItemCommand>(6);
 record<SDiary>(1096);record<SCreditsName>(68);record<SCreditsRole>(49);
 Common::SeekableReadStream o(548);o.bytes[28]=17;o.bytes[31]=0xee;o.bytes[32]=0x34;o.bytes[33]=0x12;o.bytes[34]=25;
 SObject object;object.loadFromStream(o);assert(object.room==17 && object.flags==0x1234 && object.pos==25);
 Common::SeekableReadStream d(616);d.bytes[2]=0xee;d.bytes[3]=0xdd;d.bytes[438]=0x53;d.bytes[439]=1;
 SDialog dialog;dialog.loadFromStream(d);assert(dialog.obj==0 && dialog.ItemIndex[11]==339);
 SerializableDynamicArray<SItemCommand> owned;owned=SerializableDynamicArray<SItemCommand>(5);
 Common::SeekableReadStream cmds(30);cmds.bytes[1]=0xee;cmds.bytes[2]=0x34;cmds.bytes[3]=0x12;
 owned.loadFromStream(cmds);assert(owned[0].param1==0x1234);owned=SerializableDynamicArray<SItemCommand>(2);
 if(argc>1){
  std::ifstream f(argv[1],std::ios::binary);assert(f);
  Common::SeekableReadStream s(0);s.bytes.assign(std::istreambuf_iterator<char>(f),{});
  assert(s.bytes.size()==6017156);
  for(int i=0;i<75;++i){SRoom r;r.loadFromStream(s);if(i==1)assert(!strcmp((char*)r.name,"r13"));}
  for(int i=0;i<1624;++i){SObject r;r.loadFromStream(s);}
  for(int i=0;i<86;++i){SInvObject r;r.loadFromStream(s);}
  for(int i=0;i<1120;++i){SSound r;r.loadFromStream(s);}
  for(int i=0;i<24;++i){SMusic r;r.loadFromStream(s);}
  for(int i=0;i<1360;++i){SAnim r;r.loadFromStream(s);}
  assert(s.pos==4443434);
  for(int i=0;i<114;++i){SDialog r;r.loadFromStream(s);if(i==99)assert(r.ItemIndex[11]==339);}
  for(int i=0;i<44;++i){SDiary r;r.loadFromStream(s);}
  for(int i=0;i<100;++i){SDlgMenu r;r.loadFromStream(s);}
  assert(s.pos==4562282);
  for(int i=0;i<341;++i){SDlgItem r;r.loadFromStream(s);if(i==339){assert(r.item[0][0].com==10 && r.item[0][0].param1==1);assert(r.item[0][1].com==3 && r.item[0][1].param1==201);}}
  assert(s.pos==5380682);puts("PASS owned English Data.wm: rooms, all data tables through intro commands");
 }
 puts("PASS production Win32 record readers, padding sentinels, serialized lengths and array ownership under ASan/UBSan");
}
'''
cpp=work/'check.cpp';cpp.write_text(preamble+records+credits+main)
subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(src),str(cpp),'-o',str(work/'check')],check=True)
game=root/'artifacts/thewatchmaker/game/Data.wm'
subprocess.run([str(work/'check')]+([str(game)] if game.exists() else []),check=True)
