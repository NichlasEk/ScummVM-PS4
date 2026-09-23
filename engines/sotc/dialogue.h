// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_DIALOGUE_H
#define SOTC_DIALOGUE_H
#include "cc4.h"
namespace Sotc {
// Original 0x7386 / 0x72c1: nested LE32 tables, then subtract a rolling
// 0x54 multiplier relative to the start of the bank's string payload.
// Keep embedded zeroes: they are line breaks, not the end of the record.
inline bool dialogueRecord(Cc4View file, unsigned bank, unsigned index,
                           uint8_t *out, unsigned capacity, unsigned &size) {
 size=0; Cc4View block;
 if (!out || !cc4Entry(file,4,bank,block) || block.size<4) return false;
 unsigned first=cc4Word(block.data,4);
 if (first<4 || first>block.size || first%4 || index>=first/4) return false;
 unsigned start=cc4Word(block.data+index*4,4);
 unsigned end=index+1<first/4 ? cc4Word(block.data+(index+1)*4,4) : block.size;
 // Some language banks have stale tail offsets. Validate the requested pair,
 // as the original file loader does, without rejecting unrelated good text.
 if (start<first || end<start || end>block.size || end-start<2 || end-start>capacity) return false;
 unsigned offset=start-first;
 for (unsigned i=0;i<end-start;++i) out[i]=uint8_t(block.data[start+i]-84*(offset+i+1));
 size=end-start; return true;
}
}
#endif
