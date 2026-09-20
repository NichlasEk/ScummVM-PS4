// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/movie.h"
#include "common/endian.h"
namespace Pice {
namespace {
struct Reader {
 Reader(const Common::Array<byte> &bytes,uint start,uint end) : data(bytes),pos(start),limit(end) {}
 const Common::Array<byte> &data;
 uint pos,limit;
 uint32 mask=0;
 uint bits=0;
 bool low=false;
 byte pair=0;
 bool byteValue(byte &value) { if(pos>=limit) return false; value=data[pos++]; return true; }
 bool nibble(uint &value) {
  if(!low && !byteValue(pair)) return false;
  value=low ? pair&15 : pair>>4; low=!low; return true;
 }
 bool bit(bool &value) {
  if(!bits) { if(limit-pos<4) return false; mask=READ_LE_UINT32(&data[pos]); pos+=4; bits=32; }
  value=(mask&0x80000000U)!=0; mask<<=1; --bits; return true;
 }
};
}
bool decodeMovieFrame(const Common::Array<byte> &data,Common::Array<byte> &pixels) {
 // ICE640 0x22ff5/0x2300c expand control + literal streams. 0x230e5
 // then applies run lengths and (flag 2) unchanged-pixel spans.
 if(data.size()<13 || (data[0]!=0 && data[0]!=2 && data[0]!=4 && data[0]!=6) || pixels.empty()) return false;
 uint32 offset=READ_LE_UINT32(&data[1]),control=READ_LE_UINT32(&data[5]),literal=READ_LE_UINT32(&data[9]);
 uint64 total=uint64(control)+literal;
 if(offset>pixels.size() || !control || total>4*1024*1024) return false;
 Common::Array<byte> expanded;
 Reader source{data,13,data.size()};
 // Original 0x22ff5: bit 4 selects LZ expansion; otherwise the two
 // streams are stored verbatim (SBLOOD mixes raw and compressed frames).
 if(!(data[0]&4)) {
  if(total!=data.size()-13) return false;
  expanded.resize(total);
  for(uint i=0;i<total;++i) expanded[i]=data[13+i];
  source.pos=data.size();
 }
 uint32 flags=0;
 while(expanded.size()<total) {
  // Stage one uses a sentinel bit, unlike stage two's full 32-bit words.
  bool copy=(flags&0x80000000U)!=0; flags<<=1;
  if(!flags) {
   if(source.limit-source.pos<4) return false;
   flags=READ_LE_UINT32(&data[source.pos]); source.pos+=4;
   if(!(flags&0x7fffffffU)) return false;
   copy=(flags&0x80000000U)!=0; flags<<=1;
  }
  byte distance=0; uint count;
  if(!copy && (!source.byteValue(distance) || !distance || distance>expanded.size())) return false;
  if(!source.nibble(count)) return false;
  count+=copy ? 1 : 2;
  if(count>total-expanded.size()) return false;
  for(uint i=0;i<count;++i) {
   byte value;
   if(copy) { if(!source.byteValue(value)) return false; }
   else value=expanded[expanded.size()-distance];
   expanded.push_back(value);
  }
 }
 if(source.pos!=data.size()) return false;
 Common::Array<byte> result=pixels;
 Reader commands{expanded,0,control};
 uint input=control,output=offset;
 while(input<expanded.size()) {
  bool bit; uint count=1;
  if(!commands.bit(bit)) return false;
  if(bit) {
   if(!commands.bit(bit)) return false;
   if(!bit) {
    if(!commands.nibble(count)) return false;
    if(count) count+=4;
    else { byte extra; if(!commands.byteValue(extra)) return false; count=extra+20; }
   } else {
    if(!commands.bit(bit)) return false;
    if(!bit) count=2;
    else {
     if(!commands.bit(bit)) return false;
     if(!bit) count=3;
     else {
      count=4;
      if(data[0]&2) {
       if(!commands.bit(bit)) return false;
       if(bit) {
        byte skip; if(!commands.byteValue(skip) || skip>result.size()-output) return false;
        output+=skip; continue;
       }
      }
     }
    }
   }
  }
  if(count>result.size()-output) return false;
  byte value=expanded[input++];
  for(uint i=0;i<count;++i) result[output++]=value;
 }
 pixels=result; return true;
}
bool MovieResource::load(const Common::Array<byte> &data) {
 _data.clear(); _chunks.clear(); pixels.clear(); palette.clear(); audio.clear(); _next=frame=0; failed=false;
 if(data.size()<32 || data.size()>64*1024*1024 || READ_LE_UINT32(&data[0])!=0xaa07) return false;
 uint header=READ_LE_UINT16(&data[4]);
 width=READ_LE_UINT16(&data[28]); height=READ_LE_UINT16(&data[30]); divisor=READ_LE_UINT16(&data[12]);
 // Verified 8-bit mono PCM variants, including the 640x480 SBLOOD clip.
 if(!((width==320 && height==200) || (width==640 && height==480)) || !divisor || READ_LE_UINT16(&data[14])!=22050 || READ_LE_UINT16(&data[26])!=8 || header<32 || header>data.size()) return false;
 Common::Array<Chunk> chunks; Common::Array<byte> pcm;
 uint pos=header; bool ended=false;
 while(pos<data.size()) {
  if(data.size()-pos<6 || ended) return false;
  Chunk c{READ_LE_UINT16(&data[pos]),pos+6,READ_LE_UINT32(&data[pos+2])};
  if(c.size>data.size()-c.offset) return false;
  if(c.type==6 || c.type==7) {
   for(uint i=0;i<c.size;++i) pcm.push_back(data[c.offset+i]);
  } else if(c.type==112) { if(c.size!=768) return false; }
  else if(c.type==111) { if(c.size<13) return false; }
  else if(c.type==5) { if(c.size!=2) return false; }
  else if(c.type==4) ended=true;
  else return false;
  chunks.push_back(c); pos=c.offset+c.size;
 }
 if(!ended || pcm.empty()) return false;
 _data=data; _chunks=chunks; audio=pcm; pixels.resize(width*height,0); palette.resize(768,0); return true;
}
bool MovieResource::nextFrame() {
 while(_next<_chunks.size()) {
  const Chunk &c=_chunks[_next++];
  if(c.type==4) return false;
  if(c.type==112) for(uint i=0;i<768;++i) palette[i]=_data[c.offset+i];
  if(c.type==111) {
   Common::Array<byte> bytes; bytes.resize(c.size);
   for(uint i=0;i<c.size;++i) bytes[i]=_data[c.offset+i];
   if(!decodeMovieFrame(bytes,pixels)) { failed=true; return false; }
  }
  if(c.type==5) { ++frame; return true; }
 }
 return false;
}
bool runMovieTests() {
 // One stage-one literal run containing one control word and one pixel.
 Common::Array<byte> data; data.resize(23,0); data[0]=4;
 WRITE_LE_UINT32(&data[5],4); WRITE_LE_UINT32(&data[9],1);
 WRITE_LE_UINT32(&data[13],0x80000001); data[17]=0x40; data[22]=42;
 Common::Array<byte> pixels; pixels.resize(8,9);
 if(!decodeMovieFrame(data,pixels) || pixels[0]!=42 || pixels[1]!=9) return false;
 for(uint n=0;n<data.size();++n) {
  Common::Array<byte> shortData=data; shortData.resize(n); Common::Array<byte> before=pixels;
  if(decodeMovieFrame(shortData,pixels) || pixels!=before) return false;
 }
 Common::Array<byte> bad=data; WRITE_LE_UINT32(&bad[1],8);
 if(decodeMovieFrame(bad,pixels)) return false;
 bad=data; WRITE_LE_UINT32(&bad[5],0xffffffff);
 if(decodeMovieFrame(bad,pixels)) return false;
 bad=data; bad[0]=7; if(decodeMovieFrame(bad,pixels)) return false;
 // Delta skip preserves the previous image; a run cannot cross its end.
 Common::Array<byte> delta; delta.resize(24,0); delta[0]=6;
 WRITE_LE_UINT32(&delta[5],5); WRITE_LE_UINT32(&delta[9],1);
 WRITE_LE_UINT32(&delta[13],0x80000001); delta[17]=0x50;
 WRITE_LE_UINT32(&delta[18],0xf8000000); delta[22]=3; delta[23]=77;
 pixels.clear(); pixels.resize(8,9);
 if(!decodeMovieFrame(delta,pixels) || pixels[0]!=9 || pixels[2]!=9 || pixels[3]!=77 || pixels[4]!=9) return false;
 delta[0]=4; WRITE_LE_UINT32(&delta[18],0x80000000); delta[22]=0x10;
 if(!decodeMovieFrame(delta,pixels) || pixels[4]!=77 || pixels[5]!=9) return false;
 delta[22]=0xf0; Common::Array<byte> unchanged=pixels;
 if(decodeMovieFrame(delta,pixels) || pixels!=unchanged) return false;
 // Raw control/literal streams have identical stage-two semantics.
 Common::Array<byte> raw; raw.resize(18,0);
 WRITE_LE_UINT32(&raw[5],4); WRITE_LE_UINT32(&raw[9],1); raw[17]=42;
 for(uint mode=0;mode<=2;mode+=2) {
  raw[0]=mode; pixels.clear(); pixels.resize(8,9);
  if(!decodeMovieFrame(raw,pixels) || pixels[0]!=42 || pixels[1]!=9) return false;
  Common::Array<byte> truncated=raw; truncated.resize(17);
  if(decodeMovieFrame(truncated,pixels)) return false;
 }
 // A delta frame with no literal pixels leaves the image unchanged.
 raw[0]=2; raw.resize(17); WRITE_LE_UINT32(&raw[9],0);
 unchanged=pixels;
 if(!decodeMovieFrame(raw,pixels) || pixels!=unchanged) return false;
 // Minimal complete container, plus corrupt chunk lengths and all prefixes.
 Common::Array<byte> container; container.resize(32,0);
 WRITE_LE_UINT32(&container[0],0xaa07); WRITE_LE_UINT16(&container[4],32);
 WRITE_LE_UINT16(&container[12],8285); WRITE_LE_UINT16(&container[14],22050);
 WRITE_LE_UINT16(&container[26],8); WRITE_LE_UINT16(&container[28],320); WRITE_LE_UINT16(&container[30],200);
 const uint types[]={7,112,111,5,4}, sizes[]={1,768,23,2,0};
 for(uint c=0;c<5;++c) {
  uint at=container.size(); container.resize(at+6+sizes[c],0);
  WRITE_LE_UINT16(&container[at],types[c]); WRITE_LE_UINT32(&container[at+2],sizes[c]);
  if(types[c]==111) for(uint j=0;j<data.size();++j) container[at+6+j]=data[j];
 }
 MovieResource movie;
 if(!movie.load(container) || !movie.nextFrame() || movie.frame!=1 || movie.pixels[0]!=42 || movie.nextFrame() || movie.failed) return false;
 for(uint n=0;n<container.size();++n) {
  Common::Array<byte> partial=container; partial.resize(n);
  if(movie.load(partial)) return false;
 }
 WRITE_LE_UINT16(&container[28],640); WRITE_LE_UINT16(&container[30],480);
 if(!movie.load(container) || movie.pixels.size()!=640*480 || !movie.nextFrame() || movie.pixels[0]!=42 || movie.nextFrame() || movie.failed) return false;
 WRITE_LE_UINT16(&container[28],0xffff);
 if(movie.load(container)) return false;
 WRITE_LE_UINT16(&container[28],640);
 WRITE_LE_UINT32(&container[34],0xffffffff);
 if(movie.load(container)) return false;
 return true;
}
}
