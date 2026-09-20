// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/dialogue.h"
#include "common/endian.h"
namespace Pice {
Common::U32String displayText(const Common::String &text) {
 return text.decode(Common::kDos850);
}
static bool findEntry(const Common::Array<byte> &data,const Common::String &key,uint32 &selected,uint32 &end,Common::String &voiceGroup) {
 selected=0; end=data.size(); voiceGroup.clear();
 if (data.size()<21 || data.size()>1024*1024 || key.empty() || key.size()>7) return false;
 // Original object 2 0x1d08a/0x1d0f2: (first offset - 10) / 11.
 uint32 first=READ_LE_UINT32(&data[7]);
 if (first<21 || first>=data.size() || (first-10)%11) return false;
 uint32 count=(first-10)/11, directoryEnd=count*11;
 if (count>4096 || data[directoryEnd+8]!=0) return false;
 bool terminated=false;
 for (uint i=0;i<8;++i) {
  byte c=data[directoryEnd+i];
  if (!c) terminated=true;
  else { if (terminated || c<33 || c>126) return false; voiceGroup+=char(c); }
 }
 if (voiceGroup.empty()) return false;
 uint32 previous=0;
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
 return !selected || selected+4<end;
}
bool decodeDialogue(const Common::Array<byte> &data,const Common::String &key,DialogueLine &line) {
 line=DialogueLine(); DialogueLine decoded; uint32 selected,end;
 if(!findEntry(data,key,selected,end,decoded.voiceGroup) || !selected) return false;
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
Common::String objectDialogueKey(const Common::String &base,int16 context,int16 item,bool genericItem) {
 // ICE640 object 2 0xdef: ACT/LOOK, otherwise O%02d then OBJ.
 if(base.empty() || base.size()>3) return Common::String();
 if(context==-6) return base+"ACT";
 if(context==-1) return base+"LOOK";
 if(genericItem) return base+"OBJ";
 if(item<0 || item>99) return Common::String();
 return Common::String::format("%sO%02d",base.c_str(),item);
}
bool selectObjectDialogueKey(const Common::Array<byte> &data,const Common::String &base,int16 context,int16 item,Common::String &key) {
 key=objectDialogueKey(base,context,item); uint32 offset,end; Common::String group;
 if(key.empty() || !findEntry(data,key,offset,end,group)) return false;
 if(!offset && context!=-6 && context!=-1) {
  key=objectDialogueKey(base,context,item,true);
  if(!findEntry(data,key,offset,end,group)) return false;
 }
 if(!offset) key.clear();
 return true;
}
bool genericDialogueKey(const Common::Array<byte> &data,int16 context,uint32 random,Common::String &key) {
 key.clear(); const char *suffix=context==-6 ? "ACT" : context==-1 ? "LOOK" : "OBJ";
 Common::String maximum=Common::String("MAX")+suffix,group; uint32 offset,end;
 if(!findEntry(data,maximum,offset,end,group)) return false;
 uint count=1; // Original handler defaults a missing MAX field to 1.
 if(offset) {
  DialogueLine line;
  if(!decodeDialogue(data,maximum,line) || line.pages.size()!=1 || line.pages[0].size()!=1) return false;
  const Common::String &number=line.pages[0][0];
  if(number.empty() || number.size()>3) return false;
  count=0;
  for(uint i=0;i<number.size();++i) { if(number[i]<'0' || number[i]>'9') return false; count=count*10+number[i]-'0'; }
  if(!count || count>100) return false;
 }
 key=Common::String::format("D%s%02u",suffix,random%count);
 DialogueLine line;
 return decodeDialogue(data,key,line);
}
static Common::Array<byte> testText(const char *const *keys,const char *const *values,uint count) {
 Common::Array<byte> data; data.resize(count*11+9,0);
 memcpy(&data[count*11],"E_M00R00",8);
 for(uint i=0;i<count;++i) {
  memcpy(&data[i*11],keys[i],strlen(keys[i]));
  data.push_back(0); uint offset=data.size();
  WRITE_LE_UINT32(&data[i*11+7],offset);
  data.resize(offset+5,0); WRITE_LE_UINT16(&data[offset+2],2); data[offset+4]=1;
  for(const char *p=values[i];*p;++p) data.push_back(*p);
  data.push_back(0);
 }
 return data;
}
bool runDialogueTests() {
 const Common::U32String nordic=displayText("\x8f\x8e\x99\x86\x84\x94\x92\x9d\x91\x9b");
 const uint32 expected[]={0xc5,0xc4,0xd6,0xe5,0xe4,0xf6,0xc6,0xd8,0xe6,0xf8};
 if(nordic.size()!=10 || displayText("ASCII 123!")!=Common::U32String("ASCII 123!")) return false;
 for(uint i=0;i<10;++i) if(nordic[i]!=expected[i]) return false;

 if(objectDialogueKey("D09",-6,-1)!="D09ACT" || objectDialogueKey("D09",-1,-1)!="D09LOOK" ||
    objectDialogueKey("D09",15,15)!="D09O15" || objectDialogueKey("D09",15,15,true)!="D09OBJ" ||
    !objectDialogueKey("TOOLONG",-1,-1).empty() || !objectDialogueKey("D09",15,-1).empty()) return false;
 const char *keys[]={"D09O15","D09OBJ","DACT00","DACT01","DLOOK00","DOBJ00","MAXACT","MAXLOOK","MAXOBJ"};
 const char *values[]={"item","object","a","b","look","obj","2","1","1"};
 Common::Array<byte> fixture=testText(keys,values,9); Common::String key;
 if(!selectObjectDialogueKey(fixture,"D09",15,15,key) || key!="D09O15" ||
    !selectObjectDialogueKey(fixture,"D09",16,16,key) || key!="D09OBJ" ||
    !selectObjectDialogueKey(fixture,"D08",16,16,key) || !key.empty()) return false;
 for(uint r=0;r<6;++r) {
  if(!genericDialogueKey(fixture,-6,r,key) || key!=(r%2 ? "DACT01" : "DACT00") ||
     !genericDialogueKey(fixture,-1,r,key) || key!="DLOOK00" ||
     !genericDialogueKey(fixture,15,r,key) || key!="DOBJ00") return false;
 }
 const char *onlyKey[]={"DACT00"},*onlyValue[]={"default"};
 if(!genericDialogueKey(testText(onlyKey,onlyValue,1),-6,7,key) || key!="DACT00") return false;
 const char *invalidCounts[]={"0","-1","101","abc","2x"};
 for(uint i=0;i<5;++i) { values[6]=invalidCounts[i]; if(genericDialogueKey(testText(keys,values,9),-6,0,key)) return false; }
 values[6]="3"; if(genericDialogueKey(testText(keys,values,9),-6,2,key)) return false;
 // Corruption is not an absent entry and must not trigger fallback.
 WRITE_LE_UINT32(&fixture[7],0xffffffff);
 if(selectObjectDialogueKey(fixture,"D09",15,15,key) || genericDialogueKey(fixture,-6,0,key)) return false;
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
