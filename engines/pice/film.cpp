// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/film.h"
#include "common/endian.h"
#include "common/memstream.h"
namespace Pice {
bool readFilmIndex(Common::SeekableReadStream &s,FilmIndex &out) {
 out=FilmIndex(); byte h[64];
 if(!s.seek(0) || s.read(h,64)!=64 || memcmp(h,"FILM",4) || memcmp(h+8,"1.09",4) ||
    memcmp(h+16,"FDSC",4) || READ_BE_UINT32(h+20)!=32 || memcmp(h+24,"cvid",4) ||
    memcmp(h+48,"STAB",4)) return false;
 FilmIndex f; f.height=READ_BE_UINT32(h+28); f.width=READ_BE_UINT32(h+32);
 f.channels=h[37]; f.bits=h[38]; f.rate=READ_BE_UINT16(h+40); f.clock=READ_BE_UINT32(h+56);
 uint32 count=READ_BE_UINT32(h+60),base=READ_BE_UINT32(h+4);
 // This adapter deliberately accepts only the PCM variant verified on PICE.
 if(!f.width || f.width>640 || !f.height || f.height>480 || (f.width%4) || (f.height%4) ||
    h[36]!=24 || h[39]!=0 || f.channels!=1 || f.bits!=8 || f.rate<8000 || f.rate>48000 ||
    !f.clock || f.clock>1000000 || !count || count>100000 ||
    uint64(count)*16+16!=READ_BE_UINT32(h+52) || uint64(count)*16+64!=base || base>uint64(s.size())) return false;
 uint64 previousEnd=base; uint32 lastTime=0;
 for(uint i=0;i<count;++i) {
  byte row[16]; if(s.read(row,16)!=16) return false;
  FilmSample a; uint64 offset=uint64(base)+READ_BE_UINT32(row);
  a.size=READ_BE_UINT32(row+4); a.audio=READ_BE_UINT32(row+8)==0xffffffff;
  a.time=READ_BE_UINT32(row+8)&0x7fffffff; a.duration=READ_BE_UINT32(row+12);
  if(!a.size || a.size>4*1024*1024 || offset<previousEnd || offset+a.size>uint64(s.size()) || offset>0xffffffff) return false;
  a.offset=offset; previousEnd=offset+a.size;
  if(a.audio) {
   if(uint64(f.audioBytes)+a.size>64*1024*1024) return false;
   f.audioBytes+=a.size;
  } else {
   if((f.videoCount && a.time<lastTime) || !a.duration || (uint64(a.time)+a.duration)*1000/f.clock>3600000) return false;
   lastTime=a.time; ++f.videoCount;
   f.endMs=MAX<uint32>(f.endMs,(uint64(a.time)+a.duration)*1000/f.clock);
  }
  f.samples.push_back(a);
 }
 if(!f.videoCount || s.err()) return false;
 f.endMs=MAX<uint32>(f.endMs,uint64(f.audioBytes)*1000/f.rate);
 out=f; return true;
}
bool readFilmSample(Common::SeekableReadStream &s,const FilmSample &a,Common::Array<byte> &data) {
 data.clear();
 if(!a.size || a.size>4*1024*1024 || uint64(a.offset)+a.size>uint64(s.size()) || !s.seek(a.offset)) return false;
 data.resize(a.size); return s.read(data.data(),a.size)==a.size;
}
bool validateFilmFrame(const Common::Array<byte> &d,const FilmIndex &f,uint &strips) {
 if(d.size()<12 || READ_BE_UINT16(d.data()+4)!=f.width || READ_BE_UINT16(d.data()+6)!=f.height) return false;
 uint count=READ_BE_UINT16(d.data()+8),length=(uint(d[1])<<16)|READ_BE_UINT16(d.data()+2);
 if(!length || !count || count>f.height/4 || (strips && count!=strips)) return false;
 // Cinepak's Saturn packet has either a two-byte prefix or FE00 + four bytes.
 uint pos=10;
 if(length!=d.size()) { if(READ_BE_UINT16(d.data()+10)==0xfe00) pos=16; else if(d.size()%length) pos=12; }
 uint height=0;
 for(uint i=0;i<count;++i) {
  if(pos+12>d.size()) return false;
  uint size=READ_BE_UINT16(d.data()+pos+2);
  if(size<12 || size>d.size()-pos) return false;
  uint stripHeight=READ_BE_UINT16(d.data()+pos+8);
  if(!stripHeight || stripHeight%4 || height+stripHeight>f.height) return false;
  height+=stripHeight;
  uint end=pos+size; pos+=12;
  while(pos<end) {
   if(end-pos<4) return false;
   uint chunk=(uint(d[pos+1])<<16)|READ_BE_UINT16(d.data()+pos+2);
   if(chunk<4 || chunk>end-pos) return false;
   pos+=chunk;
  }
 }
 if(height!=f.height) return false;
 strips=count; return true;
}
bool runFilmTests() {
 Common::Array<byte> d; d.resize(81,0);
 memcpy(d.data(),"FILM",4); WRITE_BE_UINT32(d.data()+4,80); memcpy(d.data()+8,"1.09",4);
 memcpy(d.data()+16,"FDSC",4); WRITE_BE_UINT32(d.data()+20,32); memcpy(d.data()+24,"cvid",4);
 WRITE_BE_UINT32(d.data()+28,176); WRITE_BE_UINT32(d.data()+32,320);
 d[36]=24; d[37]=1; d[38]=8; WRITE_BE_UINT16(d.data()+40,22254);
 memcpy(d.data()+48,"STAB",4); WRITE_BE_UINT32(d.data()+52,32); WRITE_BE_UINT32(d.data()+56,30);
 WRITE_BE_UINT32(d.data()+60,1); WRITE_BE_UINT32(d.data()+68,1); WRITE_BE_UINT32(d.data()+76,3);
 auto parse=[&]() { Common::MemoryReadStream s(d.data(),d.size()); FilmIndex f; return readFilmIndex(s,f); };
 if(!parse()) return false;
 // Reject truncation, overflowing extents, no timebase and unsupported ADX.
 d.pop_back(); if(parse()) return false; d.push_back(0);
 WRITE_BE_UINT32(d.data()+64,0xfffffff0); if(parse()) return false; WRITE_BE_UINT32(d.data()+64,0);
 WRITE_BE_UINT32(d.data()+56,0); if(parse()) return false; WRITE_BE_UINT32(d.data()+56,30);
 d[39]=2; if(parse()) return false; d[39]=0;
 WRITE_BE_UINT32(d.data()+72,0x80000000); if(!parse()) return false; // keyframe flag is not a timestamp bit
 // A minimal structural frame; validate it before handing bytes to Cinepak.
 FilmIndex f; f.width=320; f.height=176;
 Common::Array<byte> frame; frame.resize(22,0);
 frame[3]=22; WRITE_BE_UINT16(frame.data()+4,320); WRITE_BE_UINT16(frame.data()+6,176);
 WRITE_BE_UINT16(frame.data()+8,1); WRITE_BE_UINT16(frame.data()+12,12);
 WRITE_BE_UINT16(frame.data()+18,176);
 uint strips=0;
 if(!validateFilmFrame(frame,f,strips) || strips!=1) return false;
 WRITE_BE_UINT16(frame.data()+18,180); if(validateFilmFrame(frame,f,strips)) return false;
 WRITE_BE_UINT16(frame.data()+18,176); frame[3]=0; if(validateFilmFrame(frame,f,strips)) return false;
 return true;
}
}
