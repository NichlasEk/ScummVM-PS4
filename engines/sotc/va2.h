// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_VA2_H
#define SOTC_VA2_H
#include <stdint.h>
namespace Sotc {
inline uint32_t va2LE32(const uint8_t *p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); }
// Section 1 contains frame offsets relative to its start, followed by a
// per-frame orientation prefix. Each offset points past that prefix to the
// dimensions. Other sections contain sequence/pose records.
inline unsigned va2FrameCount(const uint8_t *data, uint32_t size) {
 if (size < 20 || va2LE32(data) != 16) return 0;
 uint32_t start = va2LE32(data + 4), end = va2LE32(data + 8);
 if (start < 16 || end > size || start >= end || end - start < 6) return 0;
 uint32_t first = va2LE32(data + start);
 if (first < 6 || first % 4 != 2 || first >= end - start) return 0;
 unsigned count = (first - 2) / 4;
 uint32_t previous = first;
 for (unsigned i = 0; i < count; ++i) {
  uint32_t offset = va2LE32(data + start + 4 * i);
  if (offset < previous || offset >= end - start) return 0;
  previous = offset;
 }
 return count;
}
// Output includes an explicit mask: palette index zero may be an opaque pixel.
// False also covers the still-unknown compressed-frame variant. Callers must
// discard partial output on failure, never present it as a decoded frame.
inline bool va2Frame(const uint8_t *data, uint32_t size, unsigned index,
                     uint8_t *pixels, uint8_t *mask, uint32_t capacity,
                     unsigned &width, unsigned &height) {
 width = height = 0;
 unsigned count = va2FrameCount(data, size);
 if (index >= count) return false;
 uint32_t base = va2LE32(data + 4);
 uint32_t start = base + va2LE32(data + base + index * 4);
 uint32_t end = index + 1 < count ? base + va2LE32(data + base + (index + 1) * 4) : va2LE32(data + 8);
 if (end - start < 2) return false;
 unsigned orientation = unsigned(data[start - 2]) | (unsigned(data[start - 1]) << 8);
 if (orientation & ~0x8000U) return false;
 unsigned w = data[start] * 16, h = data[start + 1];
 // Explicit empty frames are used by dummy/hidden objects.
 if (!w && !h && end - start == 2) return true;
 if (!w || !h || w * h > capacity) return false;
 for (unsigned i = 0; i < w * h; ++i) pixels[i] = mask[i] = 0;
 uint32_t pos = start + 2;
 for (unsigned y = 0; y < h; ++y) {
  if (pos >= end) return false;
  unsigned runs = data[pos++], x = 0;
  for (unsigned r = 0; r < runs; ++r) {
   if (end - pos < 3) return false;
   x += data[pos++]; unsigned quads = data[pos++], rest = data[pos++];
   unsigned length = quads * 4 + rest;
   if (rest > 3 || x > w || length > w - x || length > end - pos) return false;
   for (unsigned k = 0; k < length; ++k) { pixels[y * w + x] = data[pos++]; mask[y * w + x++] = 1; }
  }
  if (pos >= end || x + data[pos++] != w) return false;
 }
 if (pos != end && !(end - pos == 2 && data[pos] == 0 && (data[pos + 1] == 0 || data[pos + 1] == 128))) return false;
 // Original blitter XORs the pose flag with this per-frame prefix word
 // (load-image 0x14a31). Return canonical orientation; pose mirroring follows.
 if (orientation & 0x8000) for (unsigned y = 0; y < h; ++y) for (unsigned x = 0; x < w / 2; ++x) {
  unsigned a = y * w + x, b = y * w + w - 1 - x;
  uint8_t tmp = pixels[a]; pixels[a] = pixels[b]; pixels[b] = tmp;
  tmp = mask[a]; mask[a] = mask[b]; mask[b] = tmp;
 }
 width = w; height = h; return true;
}
}
#endif
