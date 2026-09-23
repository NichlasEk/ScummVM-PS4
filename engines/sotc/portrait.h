// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_PORTRAIT_H
#define SOTC_PORTRAIT_H
#include "va2.h"
namespace Sotc {
// Type-10 images use vertical runs. Each row has its own advancing X cursor.
// The packet selects a delta coding scheme; bit 1 advances without painting.
// Original decoder: 0x11736, dispatch 0x11636, handlers 0x1180c..0x12068.
inline bool va2Portrait(const uint8_t *data,uint32_t size,unsigned index,
                        uint8_t *pixels,uint8_t *mask,unsigned capacity,
                        unsigned &width,unsigned &height) {
 width=height=0; unsigned count=va2FrameCount(data,size);
 if (index>=count) return false;
 unsigned base=va2LE32(data+4),start=base+va2LE32(data+base+index*4);
 unsigned end=index+1<count ? base+va2LE32(data+base+(index+1)*4)-2 : va2LE32(data+8);
 if (end<start+3) return false;
 unsigned w=data[start]*16,h=data[start+1];
 if (!w || !h || w*h>capacity) return false;
 for (unsigned i=0;i<w*h;++i) pixels[i]=mask[i]=0;
 unsigned xs[256]={},row=0,p=start+2;
 // Schemes: independent +/-1, cumulative +/-1/2/3 bits, repeated triples,
 // packed short lengths, and one literal length.
 static const uint8_t scheme[4][16]={
  {0,2,2,2,0,3,3,3,9,9,9,0,2,2,11,4},
  {1,3,4,4,1,2,5,5,10,10,10,1,3,6,11,8},
  {4,4,8,5,5,5,7,4,4,5,7,6,6,8,11,7},
  {12,12,12,12,12,12,12,12,12,12,12,12,12,12,11,12}
 };
 while (p<end && data[p]) {
  unsigned flags=data[p++];
  if (p>=end) return false;
  unsigned tokens=data[p++]; if (!tokens) tokens=256;
  if (flags&128) { if (p>=end) return false; row=data[p++]; }
  if (p>=end) return false;
  uint8_t color=data[p++]; unsigned previous=1;
  auto run=[&](unsigned length) {
   if (row>=h || xs[row]>w || length>w-xs[row]) return false;
   for (unsigned x=0;x<length;++x) if (!(flags&2)) {
    unsigned at=row*w+xs[row]+x; pixels[at]=color; mask[at]=1;
   }
   xs[row++]+=length; return true;
  };
  if ((flags&60)==56) {
   if (!run(tokens&7)) return false;
   if ((tokens>>3)&7) {
    if (!run((tokens>>3)&7)) return false;
    if ((tokens>>6) && !run(tokens>>6)) return false;
   }
   continue;
  }
  for (unsigned t=0;t<tokens;++t) {
   if (p>=end) return false;
   unsigned token=data[p++],kind=scheme[token>>6][(flags&60)>>2];
   if (kind==12) { previous=token&63; if (!run(previous)) return false; continue; }
   if (kind==9 || kind==10) {
    unsigned lengths[3];
    for (unsigned i=0;i<3;++i) lengths[i]=(previous+(kind==9 ? 1 : -1)*int((token>>i)&1))&255;
    previous=lengths[2];
    for (unsigned repeat=0;repeat<=((token>>3)&7);++repeat)
     for (unsigned i=0;i<3;++i) if (!run(lengths[i])) return false;
    continue;
   }
   unsigned bits=kind<4 ? 1 : kind<7 ? 2 : 3;
   unsigned baseline=previous;
   for (unsigned i=0;i<6/bits;++i) {
    int delta=(token>>(i*bits))&((1<<bits)-1);
    if (kind==1 || kind==3 || kind==5 || kind==8) delta=-delta;
    if (kind==6) --delta;
    unsigned length=((kind<2 ? baseline : previous)+delta)&255;
    if (!run(length)) return false;
    previous=length;
   }
  }
 }
 if (p>=end || data[p]!=0 || p+1!=end) return false;
 width=w; height=h; return true;
}
}
#endif
