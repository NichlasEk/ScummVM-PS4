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
bool SpriteResource::load(const Common::Array<byte> &data,int32 sourceId) {
 archiveId=sourceId; fingerprint=2166136261U;
 for(uint i=0;i<data.size();++i) fingerprint=(fingerprint^data[i])*16777619U;
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
 SpriteResource parsed; parsed.archiveId=sourceId; parsed.fingerprint=fingerprint;
 for (uint i = 0; i + 1 < frameOffsets.size(); ++i) {
  uint32 start = frameOffsets[i], length = frameOffsets[i+1] - start;
  if (length < 2 || data[start] || length != 2U + 8U*data[start+1]) return false;
  SpriteFrame frame;
  for (uint j = 0; j < data[start+1]; ++j) {
   const byte *r = &data[start + 2 + j*8];
   SpritePart part;
   part.flagsX = r[0]; part.flagsY = r[1]; part.image = READ_LE_UINT16(r+2);
   part.x = int16(READ_LE_UINT16(r+4)); part.y = int16(READ_LE_UINT16(r+6));
   if (part.flagsX>1 || part.image >= (part.flagsX==0 ? frameOffsets.size()-1 : imageOffsets.size()-1)) return false;
   frame.parts.push_back(part);
  }
  parsed.frames.push_back(frame);
 }
 for (uint i = 0; i + 1 < sequenceOffsets.size(); ++i) {
  uint32 start = sequenceOffsets[i], length = sequenceOffsets[i+1] - start;
  if (length < 2 || data[start] || length != 2U + 8U*data[start+1]) return false;
  // Some original resources contain unused empty sequence slots. Preserve
  // their indices; callers must reject selecting them via hasSequence().
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
bool SpriteResource::frameBounds(uint frame,Common::Rect &out) const {
 uint budget=4096; bool any=false; out=Common::Rect();
 return bounds(frame,0,0,out,any,0,budget) && any;
}
bool SpriteResource::bounds(uint frame,int x,int y,Common::Rect &out,bool &any,uint depth,uint &budget) const {
 if(frame>=frames.size() || depth>=16 || !budget) return false;
 --budget;
 for(uint i=0;i<frames[frame].parts.size();++i) {
  const SpritePart &p=frames[frame].parts[i];
  if(p.flagsY!=1) return false;
  int px=x+p.x,py=y+p.y;
  if(p.flagsX==0) { if(!bounds(p.image,px,py,out,any,depth+1,budget)) return false; }
  else if(p.flagsX==1) {
   SpriteImage bitmap; if(!image(p.image,bitmap) || px < -16384 || py < -16384 || px+bitmap.width>16384 || py>16384) return false;
   Common::Rect rect(px,py-bitmap.height,px+bitmap.width,py);
   if(any) out.extend(rect); else { out=rect; any=true; }
  } else return false;
 }
 return true;
}
bool SpriteResource::hitFrame(uint frame,int x,int bottom,int px,int py) const {
 uint budget=4096; return hit(frame,x,bottom,px,py,0,budget);
}
bool SpriteResource::hit(uint frame,int x,int bottom,int px,int py,uint depth,uint &budget) const {
 if(frame>=frames.size() || depth>=16 || !budget) return false;
 --budget;
 for(uint i=0;i<frames[frame].parts.size();++i) {
  const SpritePart &part=frames[frame].parts[i];
  if(part.flagsY!=1) continue;
  int ox=x+part.x,oy=bottom+part.y;
  if(part.flagsX==0) { if(hit(part.image,ox,oy,px,py,depth+1,budget)) return true; }
  else if(part.flagsX==1) {
   SpriteImage bitmap; if(!image(part.image,bitmap)) continue;
   int dx=px-ox,dy=py-(oy-bitmap.height);
   if(dx>=0 && dy>=0 && dx<bitmap.width && dy<bitmap.height && bitmap.mask[dy*bitmap.width+dx]) return true;
  }
 }
 return false;
}
bool SpriteResource::paintFrame(uint frame,int x,int bottom,Common::Array<byte> &canvas,uint width,uint height) const {
 if (!width || !height || width>4096 || height>4096 || canvas.size()!=uint64(width)*height) return false;
 Common::Array<byte> result=canvas; uint budget=4096;
 if (!paint(frame,x,bottom,result,width,height,0,budget)) return false;
 canvas=result; return true;
}
// Map destination pixel coordinates around the feet anchor, including negative
// offsets. Painting and picking deliberately share the same inverse mapping.
static int scaleFloor(int value,int divisor) {
 return value>=0 ? value/divisor : -((-value+divisor-1)/divisor);
}
bool SpriteResource::hitScaledFrame(uint frame,int x,int bottom,uint scale,int px,int py) const {
 if(!scale || scale>400) return false;
 return hitFrame(frame,0,0,scaleFloor((px-x)*100,scale),scaleFloor((py-bottom)*100,scale));
}
bool SpriteResource::paintScaledFrame(uint frame,int x,int bottom,uint scale,Common::Array<byte> &canvas,uint width,uint height) const {
 if(scale==100) return paintFrame(frame,x,bottom,canvas,width,height);
 if(!scale || scale>400 || !width || !height || width>4096 || height>4096 || canvas.size()!=uint64(width)*height) return false;
 Common::Rect bounds; bool any=false; uint budget=4096;
 if(!this->bounds(frame,0,0,bounds,any,0,budget)) return false;
 if(!any || bounds.isEmpty()) return true;
 if(bounds.width()>4096 || bounds.height()>4096) return false;
 // Two backgrounds recover coverage without treating opaque palette zero as
 // transparency, and compose compound frames before resampling their edges.
 Common::Array<byte> low,high; low.resize(bounds.width()*bounds.height(),0); high.resize(low.size(),255);
 if(!paintFrame(frame,-bounds.left,-bounds.top,low,bounds.width(),bounds.height()) ||
    !paintFrame(frame,-bounds.left,-bounds.top,high,bounds.width(),bounds.height())) return false;
 int left=MAX(0,x-scaleFloor(-bounds.left*int(scale),100));
 int right=MIN(int(width),x-scaleFloor(-bounds.right*int(scale),100));
 int top=MAX(0,bottom-scaleFloor(-bounds.top*int(scale),100));
 int end=MIN(int(height),bottom-scaleFloor(-bounds.bottom*int(scale),100));
 for(int py=top;py<end;++py) for(int px=left;px<right;++px) {
  int sx=scaleFloor((px-x)*100,scale)-bounds.left,sy=scaleFloor((py-bottom)*100,scale)-bounds.top;
  uint offset=sy*bounds.width()+sx;
  if(low[offset]==high[offset]) canvas[py*width+px]=low[offset];
 }
 return true;
}
bool SpriteResource::paint(uint frame,int x,int bottom,Common::Array<byte> &canvas,uint width,uint height,uint depth,uint &budget) const {
 if(frame>=frames.size() || depth>=16 || !budget) return false;
 --budget;
 for(uint i=0;i<frames[frame].parts.size();++i)
  if(!paintOne(frames[frame].parts[i],x,bottom,canvas,width,height,depth,budget)) return false;
 return true;
}
bool SpriteResource::paintPart(uint frame,uint part,int x,int bottom,Common::Array<byte> &canvas,uint width,uint height) const {
 if(frame>=frames.size() || part>=frames[frame].parts.size() || !width || !height || width>4096 || height>4096 || canvas.size()!=uint64(width)*height) return false;
 Common::Array<byte> result=canvas; uint budget=4096;
 if(!paintOne(frames[frame].parts[part],x,bottom,result,width,height,0,budget)) return false;
 canvas=result; return true;
}
bool SpriteResource::paintOne(const SpritePart &part,int x,int bottom,Common::Array<byte> &canvas,uint width,uint height,uint depth,uint &budget) const {
  if(part.flagsY!=1) return false;
  int px=x+part.x, py=bottom+part.y;
  // 0xb521 distinguishes frame references (kind 0) from bitmaps (kind 1).
  // The bitmap rectangle ends at its Y anchor (0x1f67c).
  if(part.flagsX==0) { if(!paint(part.image,px,py,canvas,width,height,depth+1,budget)) return false; }
  else if(part.flagsX==1) {
   SpriteImage bitmap; if(!image(part.image,bitmap)) return false;
   py-=bitmap.height;
   for(uint row=0;row<bitmap.height;++row) {
    int dy=py+row; if(dy<0 || dy>=int(height)) continue;
    for(uint col=0;col<bitmap.width;++col) {
     int dx=px+col; if(dx<0 || dx>=int(width)) continue;
     uint source=row*bitmap.width+col;
     if(bitmap.mask[source]) canvas[dy*width+dx]=bitmap.pixels[source];
    }
   }
  } else return false;
 return true;
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
 if (decodeSprite(nullptr, 0, image)) return false;
 // Original one-image, one-frame, one-sequence container. All offsets are
 // generated independently of the parser; negative placement and opaque zero
 // are intentional. No commercial resource bytes are used in this fixture.
 const uint32 b = 38, c = b + 6 + sizeof(valid), p = c + 14;
 Common::Array<byte> resource;
 resource.resize(p + 768, 0);
 WRITE_LE_UINT32(&resource[0], 24);
 WRITE_LE_UINT32(&resource[4], b);
 WRITE_LE_UINT32(&resource[8], c);
 WRITE_LE_UINT32(&resource[16], p);
 WRITE_LE_UINT32(&resource[20], resource.size());
 WRITE_LE_UINT32(&resource[24], 4);
 resource[29] = 1; resource[30] = 1; resource[31] = 1;
 WRITE_LE_UINT16(&resource[34], 0xfffd);
 WRITE_LE_UINT16(&resource[36], 4);
 WRITE_LE_UINT32(&resource[b], 6);
 memcpy(&resource[b+6], valid, sizeof(valid));
 WRITE_LE_UINT32(&resource[c], 4);
 resource[c+5] = 1;
 WRITE_LE_UINT16(&resource[c+8], 5);
 WRITE_LE_UINT16(&resource[c+10], 0xfffe);
 resource[p+7] = 42;
 SpriteResource parsed;
 if (!parsed.load(resource) || parsed.frames.size() != 1 || parsed.sequences.size() != 1 ||
     parsed.frames[0].parts[0].x != -3 || parsed.frames[0].parts[0].y != 4 ||
     parsed.sequences[0].steps[0].ticks != 5 || parsed.sequences[0].steps[0].x != -2 ||
     parsed.palette[7] != 42 || !parsed.image(0, image) || image.mask[1] != 255 ||
     parsed.image(1, image) || image.width) return false;
 // Bottom anchored composition preserves transparent destination pixels,
 // including when a literal palette-zero pixel lies beside transparency.
 Common::Array<byte> canvas; canvas.resize(16,9);
 if(!parsed.paintFrame(0,3,-2,canvas,4,4) || canvas[0]!=9 || canvas[1]!=0 || canvas[2]!=7 || canvas[4]!=9) return false;
 canvas.clear(); canvas.resize(16,9);
 if(!parsed.paintFrame(0,1,-2,canvas,4,4) || canvas[0]!=7 || canvas[1]!=9) return false;
 Common::Rect frameRect; if(!parsed.frameBounds(0,frameRect) || frameRect!=Common::Rect(-3,2,0,4)) return false;
 if(!parsed.hitFrame(0,3,-2,1,0) || !parsed.hitFrame(0,3,-2,2,0) ||
    parsed.hitFrame(0,3,-2,0,0) || parsed.hitFrame(0,3,-2,1,1) || parsed.hitFrame(5,3,-2,1,0)) return false;
 // Scaled rendering agrees with picking at every pixel, including clipped
 // negative offsets, opaque zero, shrinking and enlargement.
 const uint scales[]={60,100,150,200};
 for(uint n=0;n<4;++n) {
  Common::Array<byte> scaled; scaled.resize(64,9);
  if(!parsed.paintScaledFrame(0,4,-1,scales[n],scaled,8,8)) return false;
  for(int y=0;y<8;++y) for(int x=0;x<8;++x)
   if((scaled[y*8+x]!=9)!=parsed.hitScaledFrame(0,4,-1,scales[n],x,y)) return false;
 }
 Common::Array<byte> doubled; doubled.resize(64,9);
 if(!parsed.paintScaledFrame(0,6,-4,200,doubled,8,8) || doubled[0]!=9 || doubled[2]!=0 || doubled[3]!=0 || doubled[4]!=7 || doubled[5]!=7 || doubled[10]!=0 || doubled[18]!=9) return false;
 // Compound frame references use the same anchor; recursive cycles fail
 // transactionally instead of leaving a partially painted destination.
 SpriteFrame compound; SpritePart reference;
 reference.image=0; reference.x=0; reference.y=0; reference.flagsX=0; reference.flagsY=1;
 compound.parts.push_back(reference); parsed.frames.push_back(compound);
 canvas.clear(); canvas.resize(16,9);
 if(!parsed.paintFrame(1,3,-2,canvas,4,4) || canvas[1]!=0 || canvas[2]!=7) return false;
 Common::Array<byte> partCanvas; partCanvas.resize(16,9);
 if(!parsed.paintPart(1,0,3,-2,partCanvas,4,4) || partCanvas!=canvas || parsed.paintPart(1,1,3,-2,partCanvas,4,4)) return false;
 parsed.frames[1].parts[0].image=1;
 if(parsed.paintPart(1,0,3,-2,partCanvas,4,4)) return false;
 if(partCanvas!=canvas) return false;
 canvas.clear(); canvas.resize(16,9);
 if(parsed.paintFrame(1,3,-2,canvas,4,4)) return false;
 for(uint i=0;i<canvas.size();++i) if(canvas[i]!=9) return false;
 parsed.frames[0].parts[0].flagsY=2;
 if(parsed.paintFrame(0,3,-2,canvas,4,4)) return false;
 for(uint i=0;i<canvas.size();++i) if(canvas[i]!=9) return false;
 for (uint n = 0; n < resource.size(); ++n) {
  Common::Array<byte> truncated = resource;
  truncated.resize(n);
  if (parsed.load(truncated) || !parsed.frames.empty() || !parsed.sequences.empty() ||
      !parsed.palette.empty() || parsed.image(0, image)) return false;
 }
 // Bad header/order, offset tables, reserved padding, references and counts.
 const uint32 positions[] = {0,4,8,12,16,20,24,28,29,32,b,b+4,c,c+4,c+5,c+6};
 for (uint i = 0; i < sizeof(positions)/sizeof(positions[0]); ++i) {
  Common::Array<byte> corrupted = resource;
  corrupted[positions[i]] ^= 0x80;
  if (!parsed.load(resource) || parsed.load(corrupted) || !parsed.frames.empty() ||
      !parsed.sequences.empty() || !parsed.palette.empty()) return false;
 }
 // An unused empty sequence is legal in a resource, but cannot be selected.
 Common::Array<byte> empty = resource;
 for (uint i = 0; i < 8; ++i) empty.remove_at(c+6);
 empty[c+5] = 0;
 WRITE_LE_UINT32(&empty[16], p-8);
 WRITE_LE_UINT32(&empty[20], empty.size());
 return parsed.load(empty) && !parsed.hasSequence(0) && !parsed.hasSequence(-1) &&
        parsed.load(resource) && parsed.hasSequence(0) && !parsed.hasSequence(1) && parsed.image(0, image);
}
}
