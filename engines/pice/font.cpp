// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/font.h"
#include "common/endian.h"
#include "graphics/surface.h"
namespace Pice {
bool OriginalFont::load(const Common::Array<byte> &bytes) {
 _data.clear(); _characters.clear(); _height=_ascent=_maxWidth=0;
 if(bytes.size()<8 || READ_LE_UINT16(&bytes[0])!=1 || bytes[2]>bytes[3] || !bytes[4] || bytes[4]>64 || bytes[5]>64) return false;
 uint count=uint(bytes[3])-bytes[2]+1, tableEnd=8+count*10;
 if(bytes.size()<tableEnd) return false;
 for(uint i=0;i<count;++i) {
  const byte *g=&bytes[8+i*10]; uint32 offset=READ_LE_UINT32(g);
  uint width=g[4],height=g[5];
  if(width>64 || height>64 || g[8]>64 || g[9] || g[6]>64 || g[7]>64) return false;
  if(offset && (!width || !height || offset<tableEnd || offset>bytes.size() || width*height>bytes.size()-offset)) return false;
  _maxWidth=MAX(_maxWidth,int(g[8]));
  char code=char(bytes[2]+i); _characters+=Common::String(&code,1).decode(Common::kDos850);
 }
 _ascent=bytes[4]; _height=bytes[4]+bytes[5]; _data=bytes; return true;
}
int OriginalFont::glyph(uint32 chr) const {
 for(uint i=0;i<_characters.size();++i) if(_characters[i]==chr) return i;
 return '?' >= _data[2] && '?' <= _data[3] ? '?' - _data[2] : 0;
}
int OriginalFont::getCharWidth(uint32 chr) const {
 return ready() ? _data[8+glyph(chr)*10+8] : 0;
}
void OriginalFont::drawChar(Graphics::Surface *dst,uint32 chr,int x,int y,uint32 color) const {
 if(!ready() || !dst || dst->format.bytesPerPixel!=1) return;
 const byte *g=&_data[8+glyph(chr)*10]; uint32 offset=READ_LE_UINT32(g);
 if(!offset) return;
 x+=int8(g[6]); y+=_ascent-g[7];
 for(uint yy=0;yy<g[5];++yy) for(uint xx=0;xx<g[4];++xx) {
  int dx=x+xx,dy=y+yy;
  if(dx<0 || dy<0 || dx>=dst->w || dy>=dst->h) continue;
  byte pixel=_data[offset+yy*g[4]+xx];
  if(pixel) *static_cast<byte *>(dst->getBasePtr(dx,dy))=pixel==96 ? _shadow : byte(color);
 }
}
bool runFontTests() {
 // Synthetic two-pixel glyph, baseline and shadow; no original resource bytes.
 Common::Array<byte> data; data.resize(20,0); WRITE_LE_UINT16(&data[0],1);
 data[2]=data[3]='A'; data[4]=2; data[5]=1;
 WRITE_LE_UINT32(&data[8],18); data[12]=2; data[13]=1; data[15]=1; data[16]=3;
 data[18]=255; data[19]=96;
 OriginalFont font; if(!font.load(data) || font.getCharWidth('A')!=3 || font.getFontHeight()!=3) return false;
 Graphics::Surface screen; screen.create(2,3,Graphics::PixelFormat::createFormatCLUT8()); screen.fillRect(Common::Rect(0,0,2,3),0);
 font.drawChar(&screen,'A',0,0,246);
 bool ok=*static_cast<byte *>(screen.getBasePtr(0,1))==246 && *static_cast<byte *>(screen.getBasePtr(1,1))==241;
 font.setShadow(17); font.drawChar(&screen,'A',0,0,29);
 ok=ok && *static_cast<byte *>(screen.getBasePtr(0,1))==29 && *static_cast<byte *>(screen.getBasePtr(1,1))==17;
 font.drawChar(&screen,'A',-1,-1,246); screen.free();
 for(uint n=0;n<data.size();++n) { Common::Array<byte> shortData=data; shortData.resize(n); if(font.load(shortData)) return false; }
 WRITE_LE_UINT32(&data[8],0xfffffff0); return ok && !font.load(data) && !font.ready();
}
}
