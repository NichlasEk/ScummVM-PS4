// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_NAR_H
#define SOTC_NAR_H
#include "cc4.h"
namespace Sotc {
// Original 0xf118 uses the first nonzero LE32 offset as the table size.
// Zero offsets denote missing recordings (not empty adjacent entries).
inline bool narEntry(Cc4View table,uint32_t fileSize,unsigned index,unsigned &start,unsigned &end) {
 start=end=0;
 if (!table.data || !table.size || table.size%4 || index>=table.size/4) return false;
 unsigned first=0,previous=0;
 for (unsigned i=0;i<table.size/4;++i) {
  unsigned p=cc4Word(table.data+i*4,4);
  if (!p) continue;
  if (!first) first=p;
  if (p<table.size || p>=fileSize || (previous && p<=previous)) return false;
  previous=p;
 }
 if (first!=table.size) return false;
 start=cc4Word(table.data+index*4,4);
 if (!start) return true;
 end=fileSize;
 for (unsigned i=index+1;i<table.size/4;++i) {
  unsigned p=cc4Word(table.data+i*4,4);
  if (p) {end=p;break;}
 }
 return true;
}
// NAR contains concatenated VOCs. Byte zero is repurposed as a continuation
// marker; it is not the 'C' expected by an ordinary VOC loader. 0xf243 reads
// the 32-byte header, then streams the unsigned PCM payload in DOS-size chunks.
struct NarChunk { unsigned offset,size,rate,next; };
inline bool narChunk(Cc4View record,unsigned position,NarChunk &out) {
 if (!record.data || position>record.size || record.size-position<33) return false;
 const uint8_t *p=record.data+position;
 const char signature[]="reative Voice File\x1a";
 for (unsigned i=0;i<19;++i) if (p[i+1]!=uint8_t(signature[i])) return false;
 if (cc4Word(p+20,2)!=26 || p[26]!=1 || p[31]!=0) return false;
 unsigned version=cc4Word(p+22,2);
 if (cc4Word(p+24,2)!=((~version+0x1234)&65535)) return false;
 unsigned length=cc4Word(p+27,3);
 if (length<3 || length>record.size-position-31 || p[30]==255 || p[30]==0) return false;
 unsigned next=position+31+length;
 if (record.data[next-1]!=0) return false;
 out={position+32,length-2,1000000U/(256-p[30]),next};
 return true;
}
}
#endif
