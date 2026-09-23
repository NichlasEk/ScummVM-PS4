// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_POSE_H
#define SOTC_POSE_H
#include "sequence.h"
namespace Sotc {
struct PoseCommand {
 unsigned type, points, reference;
 const uint8_t *coordinates;
};
// Original 0x10c41: each command has four header bytes, then N signed XY pairs.
// The old fixed-eight-byte accessor remains useful for simple frame-only poses.
inline bool va2Pose(const uint8_t *data, uint32_t size, unsigned pose,
                    const uint8_t *&commands, uint32_t &length, unsigned &count) {
 commands = nullptr; length = count = 0;
 unsigned records = va2Records(data, size, 0);
 if (pose >= records) return false;
 uint32_t base = va2LE32(data), start = base + va2LE32(data + base + pose * 4);
 uint32_t end = pose + 1 < records ? base + va2LE32(data + base + (pose + 1) * 4) : va2LE32(data + 4);
 if (pose + 1 < records) { if (end - start < 2) return false; end -= 2; }
 if (end - start < 2 || data[start] != 0) return false; // Pose effects not yet supported.
 count = data[start + 1];
 uint32_t p = start + 2;
 for (unsigned i = 0; i < count; ++i) {
  if (end - p < 4) return false;
  unsigned points = data[p + 1];
  if (!points || points > (end - p - 4) / 4) return false;
  p += 4 + points * 4;
 }
 if (p != end) return false;
 commands = data + start + 2; length = end - start - 2; return true;
}
inline bool va2PoseCommand(const uint8_t *data, uint32_t size, unsigned pose, unsigned index, PoseCommand &out) {
 const uint8_t *p; uint32_t length; unsigned count;
 if (!va2Pose(data, size, pose, p, length, count) || index >= count) return false;
 for (unsigned i = 0; i < index; ++i) p += 4 + p[1] * 4;
 out.type = p[0]; out.points = p[1]; out.reference = va2LE16(p + 2); out.coordinates = p + 4;
 return true;
}
}
#endif
