// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/resource-names.h"
#include "common/endian.h"
namespace Pice {
int chapterRoomBase(int chapter) { return chapter==2 ? 1140 : chapter==3 ? 1182 : chapter==6 ? 1236 : chapter==8 ? 1253 : chapter==9 ? 1264 : chapter==10 ? 1273 : chapter==11 ? 1288 : chapter==12 ? 1294 : -1; }
int chapterRoomCount(int chapter) { return chapter==2 ? 40 : chapter==3 ? 52 : chapter==6 ? 15 : chapter==8 ? 9 : chapter==9 ? 7 : chapter==10 ? 13 : chapter==11 ? 4 : chapter==12 ? 12 : 0; }
int chapterForBank(uint bank) {
 for(int chapter : {2,3,6,8,9,10,11,12}) if(bank>=uint(chapterRoomBase(chapter)) && bank<uint(chapterRoomBase(chapter)+chapterRoomCount(chapter))) return chapter;
 return -1;
}

bool matchesResourceGroup(const Common::String &name,const Common::String &group) {
 return name.equalsIgnoreCase(group) || name.equalsIgnoreCase(group+".PAK");
}
bool resolveChapterGraphics(const Common::String &name,int local,uint32 &resource) {
 // English CD graphics group boundaries from its original resource directory.
 struct Group { const char *name; uint base,count; };
 static const Group groups[]={{"A02",0,179},{"A03",179,201},{"B02",815,67},{"B03",882,119},{"A06",380,79},{"B06",1001,38},{"A08",459,85},{"B08",1039,18},{"A09",544,65},{"B09",1057,19},{"A10",609,79},{"B10",1076,23},{"A11",688,36},{"B11",1099,9},{"A12",724,91},{"B12",1108,30}};
 for(uint i=0;i<16;++i) if(matchesResourceGroup(name,groups[i].name)) {
  if(local<0 || uint(local)>=groups[i].count) return false;
  resource=groups[i].base+local; return true;
 }
 return false;
}
bool ResourceNames::load(const Common::Array<byte> &encoded, uint32 archiveCount) {
 _entries.clear(); _archiveCount = 0;
 if (!archiveCount || encoded.size() < 9 || encoded.size() > 1024*1024) return false;
 uint32 length = READ_LE_UINT32(&encoded[0]);
 if (length != encoded.size()-5 || length < 4 || encoded.back()) return false;
 Common::Array<byte> decoded; decoded.resize(length);
 byte previous = 0x5a;
 for (uint32 i=0; i<length; ++i) previous = decoded[i] = encoded[i+4] ^ previous;
 uint32 count = READ_LE_UINT32(&decoded[0]);
 if (!count || count > 65536 || uint64(count)*12+4 != length) return false;
 Common::Array<ResourceName> parsed;
 for (uint i=0; i<count; ++i) {
  const byte *record = &decoded[4+i*12];
  ResourceName entry;
  bool padding = false;
  for (uint j=0; j<8; ++j) {
   byte c=record[j];
   if (!c) { padding=true; continue; }
   if (padding || !((c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_')) return false;
   entry.name += char(c);
  }
  entry.base = READ_LE_UINT32(record+8);
  if (entry.name.empty() || entry.base >= archiveCount || (i && entry.base <= parsed.back().base)) return false;
  for (uint j=0; j<parsed.size(); ++j) if (parsed[j].name == entry.name) return false;
  parsed.push_back(entry);
 }
 _entries = parsed; _archiveCount = archiveCount;
 return true;
}
bool ResourceNames::resolve(const Common::String &name, uint32 local, uint32 &resource) const {
 resource = 0;
 for (uint i=0; i<_entries.size(); ++i) {
  if (!_entries[i].name.equalsIgnoreCase(name)) continue;
  uint32 end = i+1 < _entries.size() ? _entries[i+1].base : _archiveCount;
  // Unlike the original unchecked addition, do not let a local index cross
  // into the next named group or wrap around the archive index.
  if (local >= end-_entries[i].base) return false;
  resource = _entries[i].base+local;
  return true;
 }
 return false;
}
static Common::Array<byte> encodeFixture(const Common::Array<byte> &plain) {
 Common::Array<byte> result; result.resize(plain.size()+5,0);
 WRITE_LE_UINT32(&result[0],plain.size());
 byte previous=0x5a;
 for (uint i=0;i<plain.size();++i) { result[i+4]=plain[i]^previous; previous=plain[i]; }
 return result;
}
bool runResourceNameTests() {
 uint32 id=0;
 if(chapterRoomBase(12)!=1294 || chapterRoomCount(12)!=12 || chapterForBank(1293)!=-1 ||
    chapterForBank(1294)!=12 || chapterForBank(1305)!=12 || chapterForBank(1306)!=-1 ||
    !resolveChapterGraphics("A12.Pak",90,id) || id!=814 || resolveChapterGraphics("A12",91,id) ||
    !resolveChapterGraphics("B12",29,id) || id!=1137 || resolveChapterGraphics("B12",30,id)) return false;
 if(chapterRoomBase(11)!=1288 || chapterRoomCount(11)!=4 || chapterForBank(1287)!=-1 ||
    chapterForBank(1288)!=11 || chapterForBank(1291)!=11 || chapterForBank(1292)!=-1 ||
    !resolveChapterGraphics("A11.Pak",35,id) || id!=723 || resolveChapterGraphics("A11",36,id) ||
    !resolveChapterGraphics("B11",8,id) || id!=1107 || resolveChapterGraphics("B11",9,id)) return false;
 if(chapterRoomBase(10)!=1273 || chapterRoomCount(10)!=13 || chapterForBank(1272)!=-1 ||
    chapterForBank(1273)!=10 || chapterForBank(1285)!=10 || chapterForBank(1286)!=-1 ||
    !resolveChapterGraphics("A10.Pak",78,id) || id!=687 || resolveChapterGraphics("A10",79,id) ||
    !resolveChapterGraphics("B10",22,id) || id!=1098 || resolveChapterGraphics("B10",23,id)) return false;
 if(chapterRoomBase(9)!=1264 || chapterRoomCount(9)!=7 || chapterForBank(1263)!=-1 ||
    chapterForBank(1264)!=9 || chapterForBank(1270)!=9 || chapterForBank(1271)!=-1 ||
    !resolveChapterGraphics("A09.Pak",64,id) || id!=608 || resolveChapterGraphics("A09",65,id) ||
    !resolveChapterGraphics("B09",18,id) || id!=1075 || resolveChapterGraphics("B09",19,id)) return false;
 if(chapterRoomBase(8)!=1253 || chapterRoomCount(8)!=9 || chapterForBank(1252)!=-1 ||
    chapterForBank(1253)!=8 || chapterForBank(1261)!=8 || chapterForBank(1262)!=-1 ||
    !resolveChapterGraphics("A08.Pak",84,id) || id!=543 || resolveChapterGraphics("A08",85,id) ||
    !resolveChapterGraphics("B08",17,id) || id!=1056 || resolveChapterGraphics("B08",18,id)) return false;
 if(chapterRoomBase(6)!=1236 || chapterRoomCount(6)!=15 || chapterForBank(1235)!=-1 ||
    chapterForBank(1236)!=6 || chapterForBank(1250)!=6 || chapterForBank(1251)!=-1 ||
    chapterRoomBase(4)!=-1 || chapterRoomCount(4)!=0 || chapterForBank(1181)!=-1 ||
    chapterForBank(1182)!=3 || chapterForBank(1233)!=3 || chapterForBank(1234)!=-1 ||
    !resolveChapterGraphics("A06.Pak",78,id) || id!=458 || resolveChapterGraphics("A06",79,id) ||
    !resolveChapterGraphics("B06",37,id) || id!=1038 || resolveChapterGraphics("B06",38,id)) return false;
 if(!resolveChapterGraphics("A03.Pak",0,id) || id!=179 || !resolveChapterGraphics("A03",200,id) || id!=379 ||
    resolveChapterGraphics("A03",201,id) || resolveChapterGraphics("A02",179,id) || resolveChapterGraphics("A03",-1,id) ||
    !resolveChapterGraphics("B03",44,id) || id!=926 || resolveChapterGraphics("A04",0,id)) return false;
 if(!matchesResourceGroup("A02","A02") || !matchesResourceGroup("a02.Pak","A02") ||
    matchesResourceGroup("A03.PAK","A02") || matchesResourceGroup("A02.PAK.extra","A02") ||
    matchesResourceGroup("dir/A02.PAK","A02")) return false;
 Common::Array<byte> plain; plain.resize(28,0);
 WRITE_LE_UINT32(&plain[0],2);
 memcpy(&plain[4],"ROOM_A",6); WRITE_LE_UINT32(&plain[12],3);
 memcpy(&plain[16],"ROOM_B",6); WRITE_LE_UINT32(&plain[24],8);
 Common::Array<byte> valid=encodeFixture(plain);
 ResourceNames names; uint32 resource;
 if (!names.load(valid,12) || names.count()!=2 || !names.resolve("room_a",4,resource) || resource!=7 ||
     names.resolve("ROOM_A",5,resource) || !names.resolve("ROOM_B",3,resource) || resource!=11 ||
     names.resolve("ROOM_B",4,resource) || names.resolve("ROOM_B",0xffffffff,resource) ||
     names.resolve("UNKNOWN",0,resource)) return false;
 for (uint n=0;n<valid.size();++n) {
  Common::Array<byte> cut=valid; cut.resize(n);
  if (!names.load(valid,12) || names.load(cut,12) || names.count()) return false;
 }
 Common::Array<byte> bad=valid; bad.back()=1;
 if (names.load(bad,12) || names.load(valid,8)) return false;
 bad=plain; WRITE_LE_UINT32(&bad[24],2);
 if (names.load(encodeFixture(bad),12)) return false;
 bad=plain; memcpy(&bad[16],&bad[4],8);
 if (names.load(encodeFixture(bad),12)) return false;
 bad=plain; bad[11]='X';
 if (names.load(encodeFixture(bad),12)) return false;
 bad=plain; WRITE_LE_UINT32(&bad[0],0xffffffff);
 return !names.load(encodeFixture(bad),12) && names.load(valid,12);
}
}
