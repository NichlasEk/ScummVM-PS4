// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PICE_SPRITE_H
#define PICE_SPRITE_H
#include "common/array.h"
namespace Pice {
struct SpriteImage {
 uint16 width = 0, height = 0;
 Common::Array<byte> pixels, mask;
};
// Transparent spans and literals; palette index zero can itself be opaque.
bool decodeSprite(const byte *data, uint32 size, SpriteImage &out);
struct SpritePart { uint16 image; int16 x, y; byte flagsX, flagsY; };
struct SpriteFrame { Common::Array<SpritePart> parts; };
struct SequenceStep { uint16 frame, ticks; int16 x, y; };
struct SpriteSequence { Common::Array<SequenceStep> steps; };
class SpriteResource {
public:
 bool load(const Common::Array<byte> &data);
 bool hasSequence(int index) const { return index>=0 && uint(index)<sequences.size() && !sequences[index].steps.empty(); }
 bool image(uint index, SpriteImage &out) const;
 bool paintFrame(uint frame,int x,int bottom,Common::Array<byte> &canvas,uint width,uint height) const;
 Common::Array<SpriteFrame> frames;
 Common::Array<SpriteSequence> sequences;
 Common::Array<byte> palette;
private:
 bool paint(uint frame,int x,int bottom,Common::Array<byte> &canvas,uint width,uint height,uint depth,uint &budget) const;
 Common::Array<byte> _data;
 Common::Array<uint32> _images;
};
bool runSpriteTests();
}
#endif
