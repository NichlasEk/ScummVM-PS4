// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_CC4_H
#define SOTC_CC4_H
#include <stdint.h>
namespace Sotc {
// Non-owning views. The caller keeps the original buffer alive.
struct Cc4View {
 const uint8_t *data;
 uint32_t size;
 Cc4View(const uint8_t *p = nullptr, uint32_t n = 0) : data(p), size(n) {}
};
inline uint32_t cc4Word(const uint8_t *p, unsigned width) {
 uint32_t n = 0;
 for (unsigned i = 0; i < width; ++i) n |= uint32_t(p[i]) << (i * 8);
 return n;
}
// Outer CC4 offsets are LE32; room-local script offsets are LE16.
// Equal offsets are permitted (empty/aliased entries); never dereference them.
inline unsigned cc4Count(Cc4View v, unsigned width) {
 if (!v.data || (width != 2 && width != 4) || v.size < width) return 0;
 uint32_t first = cc4Word(v.data, width);
 if (first < width || first > v.size || first % width) return 0;
 uint32_t previous = first;
 for (uint32_t p = 0; p < first; p += width) {
  uint32_t offset = cc4Word(v.data + p, width);
  if (offset < previous || offset > v.size) return 0;
  previous = offset;
 }
 return first / width;
}
inline bool cc4Entry(Cc4View v, unsigned width, unsigned index, Cc4View &out) {
 out = Cc4View();
 unsigned count = cc4Count(v, width);
 if (index >= count) return false;
 uint32_t start = cc4Word(v.data + index * width, width);
 uint32_t end = index + 1 < count ? cc4Word(v.data + (index + 1) * width, width) : v.size;
 out = Cc4View(v.data + start, end - start);
 return true;
}
}
#endif
