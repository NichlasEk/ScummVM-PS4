// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/dialogue.h"
#include "common/endian.h"
namespace Pice {
bool decodeDialogue(const Common::Array<byte> &data,const Common::String &key,DialogueLine &line) {
 line=DialogueLine();
 if (data.size()<21 || data.size()>1024*1024 || key.empty() || key.size()>7) return false;
 // Original object 2 0x1d08a/0x1d0f2: (first offset - 10) / 11.
 uint32 first=READ_LE_UINT32(&data[7]);
 if (first<21 || first>=data.size() || (first-10)%11) return false;
 uint32 count=(first-10)/11, directoryEnd=count*11;
 if (count>4096 || data[directoryEnd+8]!=0) return false;
 DialogueLine decoded;
 bool terminated=false;
 for (uint i=0;i<8;++i) {
  byte c=data[directoryEnd+i];
  if (!c) terminated=true;
  else { if (terminated || c<33 || c>126) return false; decoded.voiceGroup+=char(c); }
 }
 if (decoded.voiceGroup.empty()) return false;
 uint32 selected=0, end=data.size(), previous=0;
 for (uint32 i=0;i<count;++i) {
  uint32 record=i*11, offset=READ_LE_UINT32(&data[record+7]);
  if (offset<first || offset>=data.size() || data.size()-offset<5 || (i && offset<=previous)) return false;
  previous=offset;
  Common::String name;
  bool terminated=false;
  for (uint j=0;j<7;++j) {
   byte c=data[record+j];
   if (!c) terminated=true;
   else { if (terminated || c<33 || c>126) return false; name+=char(c); }
  }
  if (selected && end==data.size() && offset>selected) end=offset-1;
  if (name==key) { if (selected) return false; selected=offset; }
 }
 if (!selected || selected+4>=end) return false;
 // 0x1d0f2 returns entry+2. 0x4499 checks entry[-1] for voice;
 // 0x44b9 reads the signed voice index at entry. Page offsets are relative
 // to entry+2 (0x4555); the first offset is also twice the page count.
 if (data[selected-1]) decoded.voice=int16(READ_LE_UINT16(&data[selected]));
 uint32 base=selected+2, tableBytes=READ_LE_UINT16(&data[base]);
 if (!tableBytes || (tableBytes&1) || tableBytes>128 || base+tableBytes>=end) return false;
 for (uint page=0;page<tableBytes/2;++page) {
  uint32 start=base+READ_LE_UINT16(&data[base+page*2]);
  uint32 limit=page+1<tableBytes/2 ? base+READ_LE_UINT16(&data[base+page*2+2]) : end;
  if (start<base+tableBytes || start>=limit || limit>end) return false;
  uint lines=data[start++];
  if (!lines || lines>32) return false;
  Common::Array<Common::String> text;
  for (uint i=0;i<lines;++i) {
   Common::String value;
   while (start<limit && data[start]) {
    if (value.size()>=512) return false;
    value+=char(data[start++]);
   }
   if (start>=limit) return false;
   ++start; text.push_back(value);
  }
  decoded.pages.push_back(text);
 }
 line=decoded; return true;
}
bool runDialogueTests() {
 // Original-format synthetic fixture, no game text/audio embedded.
 Common::Array<byte> data; data.resize(32,0);
 memcpy(&data[0],"TEST",4); WRITE_LE_UINT32(&data[7],21);
 memcpy(&data[11],"E_M00R00",8); data[20]=1;
 WRITE_LE_UINT16(&data[21],7); WRITE_LE_UINT16(&data[23],2);
 data[25]=2; memcpy(&data[26],"Hi\0OK",5);
 DialogueLine line;
 if (!decodeDialogue(data,"TEST",line) || line.voiceGroup!="E_M00R00" || line.voice!=7 ||
     line.pages.size()!=1 || line.pages[0].size()!=2 || line.pages[0][0]!="Hi" || line.pages[0][1]!="OK") return false;
 if (decodeDialogue(data,"MISSING",line) || !line.pages.empty()) return false;
 Common::Array<byte> bad=data; bad[31]='x'; if (decodeDialogue(bad,"TEST",line)) return false;
 bad=data; WRITE_LE_UINT32(&bad[7],0xffffffff); if (decodeDialogue(bad,"TEST",line)) return false;
 bad=data; bad.resize(23); if (decodeDialogue(bad,"TEST",line)) return false;
 bad=data; WRITE_LE_UINT16(&bad[23],3); if (decodeDialogue(bad,"TEST",line)) return false;
 data[20]=0;
 if (!decodeDialogue(data,"TEST",line) || line.voice!=-1) return false;
 // Two pages, with an independent relative offset to each page.
 data.resize(34,0); data[20]=1;
 WRITE_LE_UINT16(&data[23],4); WRITE_LE_UINT16(&data[25],8);
 data[27]=1; memcpy(&data[28],"Hi\0",3); data[31]=1; data[32]='!';
 if (!decodeDialogue(data,"TEST",line) || line.pages.size()!=2 || line.pages[1][0]!="!") return false;
 WRITE_LE_UINT16(&data[25],4); if (decodeDialogue(data,"TEST",line)) return false;
 return true;
}
}
