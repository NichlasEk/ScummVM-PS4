// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_SEQUENCE_H
#define SOTC_SEQUENCE_H
#include "va2.h"
namespace Sotc {
inline unsigned va2LE16(const uint8_t *p) { return unsigned(p[0]) | (unsigned(p[1]) << 8); }
inline int va2Signed16(const uint8_t *p) { unsigned v = va2LE16(p); return v < 32768 ? int(v) : int(v) - 65536; }
inline unsigned va2Records(const uint8_t *data, uint32_t size, unsigned section) {
 if (size < 16 || section > 2 || va2LE32(data) != 16) return 0;
 uint32_t start = va2LE32(data + section * 4), end = va2LE32(data + section * 4 + 4);
 if (start < 16 || start >= end || end > size || end - start < 6) return 0;
 uint32_t first = va2LE32(data + start);
 if (first < 6 || first % 4 != 2 || first >= end - start) return 0;
 unsigned count = (first - 2) / 4;
 uint32_t previous = first;
 for (unsigned i = 0; i < count; ++i) {
  uint32_t offset = va2LE32(data + start + i * 4);
  if (offset < previous || offset >= end - start) return 0;
  previous = offset;
 }
 return count;
}
inline bool va2Record(const uint8_t *data, uint32_t size, unsigned section, unsigned index,
                      const uint8_t *&record, unsigned &count) {
 unsigned records = va2Records(data, size, section);
 if (index >= records) return false;
 uint32_t base = va2LE32(data + section * 4);
 uint32_t start = base + va2LE32(data + base + index * 4);
 uint32_t end = index + 1 < records ? base + va2LE32(data + base + (index + 1) * 4) : va2LE32(data + section * 4 + 4);
 if (end - start < 2) return false;
 count = (unsigned(data[start]) << 8) | data[start + 1];
 uint32_t length = 2 + count * 8;
 // Additional payloads in composite records need another interpreter.
 if (end - start != length && !(end - start == length + 2 && data[end - 2] == 0 && data[end - 1] == 0)) return false;
 record = data + start + 2; return true;
}
struct SequenceStep { unsigned pose, duration; int dx, dy; };
inline unsigned va2SequenceLength(const uint8_t *data, uint32_t size, unsigned index) {
 const uint8_t *r; unsigned count;
 return va2Record(data, size, 2, index, r, count) ? count : 0;
}
inline bool va2Step(const uint8_t *data, uint32_t size, unsigned sequence, unsigned index, SequenceStep &step) {
 const uint8_t *r; unsigned count;
 if (!va2Record(data, size, 2, sequence, r, count) || index >= count) return false;
 r += index * 8; step.pose = va2LE16(r); step.duration = va2LE16(r + 2);
 step.dx = va2Signed16(r + 4); step.dy = va2Signed16(r + 6);
 return step.pose < va2Records(data, size, 0);
}
struct PoseLayer { unsigned frame; bool mirror; int x, y; };
inline bool va2Layer(const uint8_t *data, uint32_t size, unsigned pose, unsigned index, PoseLayer &layer) {
 const uint8_t *r; unsigned count;
 if (!va2Record(data, size, 0, pose, r, count) || index >= count) return false;
 r += index * 8;
 // Only the observed reference-to-frame mode, with unit scale, is supported.
 if (r[0] != 1 || r[1] != 1) return false;
 unsigned ref = va2LE16(r + 2); layer.frame = ref & 0x7fff; layer.mirror = (ref & 0x8000) != 0;
 layer.x = va2Signed16(r + 4); layer.y = va2Signed16(r + 6);
 return layer.frame < va2FrameCount(data, size);
}
}
#endif
