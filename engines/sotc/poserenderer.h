// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_POSERENDERER_H
#define SOTC_POSERENDERER_H
#include "common/array.h"
#include "pose.h"
namespace Sotc {
class PoseRenderer {
 Common::Array<byte> _pixels, _mask, _working;
 Common::Array<byte> &_screen;
 unsigned _left, _top, _right, _bottom, _commands, _raster;
 bool dot(int x, int y, byte color) {
  if (!_raster) return false;
  --_raster;
  if (x >= 0 && x < 320 && y >= 0 && y < 200 && unsigned(x) >= _left && unsigned(x) <= _right &&
      unsigned(y) >= _top && unsigned(y) <= _bottom) _working[y * 320 + x] = color;
  return true;
 }
 bool line(int x, int y, int endX, int endY, byte color) {
  // Integer rasterization, including both endpoints; clip at pixel writes.
  // Normalize X as the original line routine does at 0x14d6d.
  if (x > endX) { int t = x; x = endX; endX = t; t = y; y = endY; endY = t; }
  int dx = endX - x, dy = endY >= y ? y - endY : endY - y;
  int sy = y < endY ? 1 : -1, error = dx + dy;
  for (;;) {
   if (!dot(x, y, color)) return false;
   if (x == endX && y == endY) return true;
   int twice = error * 2;
   if (twice >= dy) { error += dy; ++x; }
   if (twice <= dx) { error += dx; y += sy; }
  }
 }
 bool polygon(const int *xs, const int *ys, unsigned count, unsigned reference) {
  // Even-odd scan conversion. Original type 4 fills with the high byte and
  // closes an outline in the low byte unless that byte is 0xff (0x10efe).
  for (int y = 0; y < 200; ++y) {
   int intersections[255]; unsigned hits = 0;
   for (unsigned i = 0, j = count - 1; i < count; j = i++) {
    if (!_raster) return false;
    --_raster;
    if ((ys[i] <= y && ys[j] > y) || (ys[j] <= y && ys[i] > y)) {
     int64_t numerator = int64_t(y - ys[i]) * (xs[j] - xs[i]);
     int value = xs[i] + int(numerator / (ys[j] - ys[i]));
     unsigned k = hits++;
     while (k && intersections[k - 1] > value) { intersections[k] = intersections[k - 1]; --k; }
     intersections[k] = value;
    }
   }
   for (unsigned i = 0; i + 1 < hits; i += 2) {
    int left = intersections[i] < 0 ? 0 : intersections[i];
    int right = intersections[i + 1] > 319 ? 319 : intersections[i + 1];
    for (int x = left; x <= right; ++x) if (!dot(x, y, byte(reference >> 8))) return false;
   }
  }
  if ((reference & 255) != 255)
   for (unsigned i = 0; i < count; ++i) {
    unsigned j = (i + 1) % count;
    if (!line(xs[i], ys[i], xs[j], ys[j], byte(reference))) return false;
   }
  return true;
 }
 static int interpolate(int a, int b, unsigned fraction) {
  // Original 0x10eb9 uses signed multiplication and extracts bits 8..23.
  int delta = (b - a) & 65535; if (delta >= 32768) delta -= 65536;
  int product = delta * int(fraction);
  int scaled = product >= 0 ? product / 256 : -((-product + 255) / 256);
  int result = (a + scaled) & 65535; return result < 32768 ? result : result - 65536;
 }
 bool render(const Common::Array<byte> &data, unsigned pose, int x, int y, int onlyLayer, unsigned depth, int nextPose = -1, unsigned fraction = 0) {
  if (depth >= 16 || data.empty()) return false;
  const uint8_t *record; uint32_t length; unsigned layers;
  if (!va2Pose(data.data(), data.size(), pose, record, length, layers)) return false;
  const uint8_t *next = nullptr; unsigned nextLayers; uint32_t nextLength;
  if (nextPose >= 0 && (!va2Pose(data.data(), data.size(), unsigned(nextPose), next, nextLength, nextLayers) || nextLayers != layers || fraction > 256)) return false;
  if (onlyLayer >= 0 && unsigned(onlyLayer) >= layers) return false;
  for (unsigned i = 0; i < layers; ++i) {
   unsigned type = record[0], points = record[1], ref = va2LE16(record + 2);
   const uint8_t *xy = record + 4; record += 4 + points * 4;
   const uint8_t *nextXY = nullptr;
   if (next) {
    // Verified equal-topology vector morph. Other merge cases stay unsupported.
    if ((type != 4 && type != 6) || next[0] != type || next[1] != points) return false;
    if (layers - i < (layers * fraction) / 256) ref = va2LE16(next + 2);
    nextXY = next + 4; next += 4 + points * 4;
   }
   if (onlyLayer >= 0 && i != unsigned(onlyLayer)) continue;
   if (!_commands) return false;
   --_commands;
   int px = x + va2Signed16(xy), py = y + va2Signed16(xy + 2);
   if (type == 0 && points == 1) {
    if (!render(data, ref, px, py, -1, depth + 1)) return false;
   } else if (type == 1 && points == 1) {
    if (ref & 0x7000) return false;
    unsigned width, height;
    if (!va2Frame(data.data(), data.size(), ref & 0xfff, _pixels.data(), _mask.data(), _pixels.size(), width, height)) return false;
    for (unsigned row = 0; row < height; ++row) for (unsigned col = 0; col < width; ++col) {
     unsigned source = row * width + ((ref & 0x8000) ? width - 1 - col : col);
     if (_mask[source] && !dot(px + int(col), py - int(height) + 1 + int(row), _pixels[source])) return false;
    }
   } else if ((type == 6 && points >= 2) || (type == 4 && points >= 3)) {
    int xs[255], ys[255];
    for (unsigned j = 0; j < points; ++j) {
     int vx = va2Signed16(xy + j * 4), vy = va2Signed16(xy + j * 4 + 2);
     if (nextXY) { vx = interpolate(vx, va2Signed16(nextXY + j * 4), fraction); vy = interpolate(vy, va2Signed16(nextXY + j * 4 + 2), fraction); }
     xs[j] = x + vx; ys[j] = y + vy;
    }
    if (type == 4) { if (!polygon(xs, ys, points, ref)) return false; continue; }
    // 0x10f82 passes the header's high byte as the polyline colour.
    for (unsigned j = 1; j < points; ++j) {
     if (!line(xs[j - 1], ys[j - 1], xs[j], ys[j], byte(ref >> 8))) return false;
    }
   } else return false;
  }
  return true;
 }
public:
 PoseRenderer(Common::Array<byte> &screen) : _screen(screen) { _pixels.resize(640 * 256); _mask.resize(640 * 256); }
 bool pose(const Common::Array<byte> &data, unsigned pose, int x, int y, unsigned clipX1 = 0, unsigned clipY1 = 0, unsigned clipX2 = 319, unsigned clipY2 = 199, int onlyLayer = -1, int nextPose = -1, unsigned fraction = 0) {
  if (_screen.size() != 64000) return false;
  _left = clipX1; _top = clipY1; _right = clipX2; _bottom = clipY2;
  _commands = 4096; _raster = 2000000; _working = _screen;
  if (!render(data, pose, x, y, onlyLayer, 0, nextPose, fraction)) return false;
  _screen = _working; return true; // Unsupported commands never leave partial poses.
 }
};
}
#endif
