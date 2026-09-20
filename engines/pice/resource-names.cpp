// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/resource-names.h"
#include "common/endian.h"
namespace Pice {
bool matchesResourceGroup(const Common::String &name,const Common::String &group) {
 return name.equalsIgnoreCase(group) || name.equalsIgnoreCase(group+".PAK");
}
bool resolveChapterGraphics(const Common::String &name,int local,uint32 &resource) {
 // English CD graphics group boundaries from its original resource directory.
 struct Group { const char *name; uint base,count; };
 static const Group groups[]={{"A02",0,179},{"A03",179,201},{"B02",815,67},{"B03",882,119}};
 for(uint i=0;i<4;++i) if(matchesResourceGroup(name,groups[i].name)) {
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
