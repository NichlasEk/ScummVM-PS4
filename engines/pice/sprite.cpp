// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/sprite.h"
#include "common/endian.h"
namespace Pice {
bool decodeSprite(const byte *data, uint32 size, SpriteImage &out) {
 out = SpriteImage();
 if (!data || size < 4) return false;
 uint16 w = READ_LE_UINT16(data), h = READ_LE_UINT16(data + 2);
 if (!w || !h || w > 4096 || h > 4096) return false;
 SpriteImage decoded;
 decoded.width = w; decoded.height = h;
 decoded.pixels.resize(uint32(w) * h, 0); decoded.mask.resize(uint32(w) * h, 0);
 uint32 pos = 4;
 for (uint y = 0; y < h; ++y) {
  if (pos >= size) return false;
  uint segments = data[pos++], x = 0;
  for (uint s = 0; s < segments; ++s) {
   if (size - pos < 3) return false;
   x += data[pos++];
   uint count = data[pos++] * 4;
   byte remainder = data[pos++];
   if (remainder > 3) return false;
   count += remainder;
   if (x > w || count > w - x || count > size - pos) return false;
   for (uint j = 0; j < count; ++j) {
    decoded.pixels[y*w+x] = data[pos++]; decoded.mask[y*w+x++] = 255;
   }
  }
  if (pos >= size || x + data[pos++] != w) return false;
 }
 // Observed bitmap records have either no padding or a zero uint16 tail.
 if (pos != size && !(size - pos == 2 && data[pos] == 0 && data[pos+1] == 0)) return false;
 out = decoded;
 return true;
}
static bool table(const Common::Array<byte> &data, uint32 start, uint32 end,
                  uint extra, Common::Array<uint32> &out) {
 out.clear();
 if (end > data.size() || start > end || end - start < 4) return false;
 uint32 first = READ_LE_UINT32(&data[start]);
 if (first < 4 + extra || first > end - start || (first - extra) % 4) return false;
 uint count = (first - extra) / 4;
 if (count > 65536) return false;
 for (uint i = 0; i < count; ++i) {
  uint32 offset = READ_LE_UINT32(&data[start + i*4]);
  if (offset < first || offset >= end - start || (i && offset <= out.back() - start)) return false;
  out.push_back(start + offset);
 }
 if (extra && READ_LE_UINT16(&data[start + count*4]) != 0) return false;
 out.push_back(end);
 return true;
}
bool SpriteResource::load(const Common::Array<byte> &data) {
 frames.clear(); sequences.clear(); palette.clear(); _data.clear(); _images.clear();
 if (data.size() < 24 || READ_LE_UINT32(&data[0]) != 24) return false;
 uint32 b = READ_LE_UINT32(&data[4]), c = READ_LE_UINT32(&data[8]);
 uint32 p = READ_LE_UINT32(&data[16]), end = READ_LE_UINT32(&data[20]);
 // This first implementation accepts the fully populated, palette-bearing variant.
 if (READ_LE_UINT32(&data[12]) || b < 24 || c < b || p < c || end < p ||
     end != data.size() || end - p != 768) return false;
 Common::Array<uint32> frameOffsets, imageOffsets, sequenceOffsets;
 if (!table(data, 24, b, 0, frameOffsets) || !table(data, b, c, 2, imageOffsets) ||
     !table(data, c, p, 0, sequenceOffsets)) return false;
 SpriteResource parsed;
 for (uint i = 0; i + 1 < frameOffsets.size(); ++i) {
  uint32 start = frameOffsets[i], length = frameOffsets[i+1] - start;
  if (length < 2 || data[start] || length != 2U + 8U*data[start+1]) return false;
  SpriteFrame frame;
  for (uint j = 0; j < data[start+1]; ++j) {
   const byte *r = &data[start + 2 + j*8];
   SpritePart part;
   part.flagsX = r[0]; part.flagsY = r[1]; part.image = READ_LE_UINT16(r+2);
   part.x = int16(READ_LE_UINT16(r+4)); part.y = int16(READ_LE_UINT16(r+6));
   if (part.image >= imageOffsets.size()-1) return false;
   frame.parts.push_back(part);
  }
  parsed.frames.push_back(frame);
 }
 for (uint i = 0; i + 1 < sequenceOffsets.size(); ++i) {
  uint32 start = sequenceOffsets[i], length = sequenceOffsets[i+1] - start;
  if (length < 2 || data[start] || length != 2U + 8U*data[start+1]) return false;
  SpriteSequence seq;
  for (uint j = 0; j < data[start+1]; ++j) {
   const byte *r = &data[start + 2 + j*8];
   SequenceStep step;
   step.frame = READ_LE_UINT16(r); step.ticks = READ_LE_UINT16(r+2);
   step.x = int16(READ_LE_UINT16(r+4)); step.y = int16(READ_LE_UINT16(r+6));
   if (step.frame >= parsed.frames.size()) return false;
   seq.steps.push_back(step);
  }
  parsed.sequences.push_back(seq);
 }
 parsed._data = data; parsed._images = imageOffsets;
 parsed.palette.resize(768); memcpy(&parsed.palette[0], &data[p], 768);
 *this = parsed;
 return true;
}
bool SpriteResource::image(uint index, SpriteImage &out) const {
 out = SpriteImage();
 return index + uint64(1) < _images.size() &&
  decodeSprite(&_data[_images[index]], _images[index+1] - _images[index], out);
}
bool runSpriteTests() {
 // Two rows: a transparent pixel, two opaque pixels (including palette 0),
 // then a fully transparent row. This fixture is original synthetic data.
 const byte valid[] = {3,0,2,0, 1,1,0,2,0,7,0, 0,3};
 SpriteImage image;
 if (!decodeSprite(valid, sizeof(valid), image) || image.width != 3 || image.height != 2 ||
     image.mask[0] || image.mask[1] != 255 || image.pixels[1] != 0 || image.pixels[2] != 7 || image.mask[3]) return false;
 for (uint n = 0; n < sizeof(valid); ++n)
  if (decodeSprite(valid, n, image) || image.width) return false;
 byte bad[sizeof(valid)]; memcpy(bad, valid, sizeof(valid)); bad[7] = 4;
 if (decodeSprite(bad, sizeof(bad), image)) return false;
 memcpy(bad, valid, sizeof(valid)); bad[5] = 3;
 if (decodeSprite(bad, sizeof(bad), image)) return false;
 memcpy(bad, valid, sizeof(valid)); bad[12] = 2;
 if (decodeSprite(bad, sizeof(bad), image)) return false;
 return !decodeSprite(nullptr, 0, image);
}
}
