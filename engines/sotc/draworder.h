// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_DRAWORDER_H
#define SOTC_DRAWORDER_H
#include <stdint.h>
namespace Sotc {
// Original 0x71b5: unsigned byte depth, inserted after existing equal depths.
struct DrawOrder {
 struct Entry { unsigned depth, id; } entries[256];
 unsigned count;
 DrawOrder() : count(0) {}
 bool append(unsigned depth, unsigned id) {
  if (count==256) return false;
  entries[count++]={depth & 255,id}; return true;
 }
 bool insert(unsigned depth, unsigned id) {
  if (count==256) return false;
  depth &= 255;
  unsigned p=0;
  while (p<count && entries[p].depth<=depth) ++p;
  for (unsigned i=count;i>p;--i) entries[i]=entries[i-1];
  entries[p]={depth,id}; ++count; return true;
 }
};
}
#endif
