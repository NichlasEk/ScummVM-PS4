// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PICE_SPRITE_H
#define PICE_SPRITE_H
#include "common/array.h"
#include "common/rect.h"
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
 bool load(const Common::Array<byte> &data,int32 archiveId=-1);
 int32 archiveId=-1;
 uint32 fingerprint=0;
 bool hasSequence(int index) const { return index>=0 && uint(index)<sequences.size() && !sequences[index].steps.empty(); }
 bool image(uint index, SpriteImage &out) const;
 bool paintFrame(uint frame,int x,int bottom,Common::Array<byte> &canvas,uint width,uint height) const;
 bool paintPart(uint frame,uint part,int x,int bottom,Common::Array<byte> &canvas,uint width,uint height) const;
 bool frameBounds(uint frame,Common::Rect &bounds) const;
 bool hitFrame(uint frame,int x,int bottom,int px,int py) const;
 Common::Array<SpriteFrame> frames;
 Common::Array<SpriteSequence> sequences;
 Common::Array<byte> palette;
private:
 bool bounds(uint frame,int x,int y,Common::Rect &out,bool &any,uint depth,uint &budget) const;
 bool hit(uint frame,int x,int bottom,int px,int py,uint depth,uint &budget) const;
 bool paintOne(const SpritePart &part,int x,int bottom,Common::Array<byte> &canvas,uint width,uint height,uint depth,uint &budget) const;
 bool paint(uint frame,int x,int bottom,Common::Array<byte> &canvas,uint width,uint height,uint depth,uint &budget) const;
 Common::Array<byte> _data;
 Common::Array<uint32> _images;
};
bool runSpriteTests();
}
#endif
