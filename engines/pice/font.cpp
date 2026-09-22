// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/font.h"
#include "pice/formats.h"
#include "graphics/surface.h"
namespace Pice {
bool OriginalFont::load(const Common::Array<byte> &bytes) {
 _data.clear(); _characters.clear(); _height=_ascent=_maxWidth=0;
 _mac=bytes.size()>=8 && READ_BE_UINT16(&bytes[0])==0;
 if(bytes.size()<8 || (!_mac && READ_LE_UINT16(&bytes[0])!=1) || bytes[2]>bytes[3] || !bytes[4] || bytes[4]>64 || bytes[5]>64) return false;
 uint count=uint(bytes[3])-bytes[2]+1, tableEnd=8+count*10;
 if(bytes.size()<tableEnd) return false;
 for(uint i=0;i<count;++i) {
  const byte *g=&bytes[8+i*10]; uint32 offset=resource32(g,_mac);
  uint width=g[4],height=g[5];
  if(width>64 || height>64 || g[8]>64 || g[9] || g[6]>64 || g[7]>64) return false;
  if(offset && (!width || !height || offset<tableEnd || offset>bytes.size() || width*height>bytes.size()-offset)) return false;
  _maxWidth=MAX(_maxWidth,int(g[8]));
  char code=char(bytes[2]+i); _characters+=Common::String(&code,1).decode(_mac ? Common::kMacRoman : Common::kDos850);
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
 const byte *g=&_data[8+glyph(chr)*10]; uint32 offset=resource32(g,_mac);
 if(!offset) return;
 x+=int8(g[6]); y+=_ascent-g[7];
 for(uint yy=0;yy<g[5];++yy) for(uint xx=0;xx<g[4];++xx) {
  int dx=x+xx,dy=y+yy;
  if(dx<0 || dy<0 || dx>=dst->w || dy>=dst->h) continue;
  byte pixel=_data[offset+yy*g[4]+xx];
  if(pixel) *static_cast<byte *>(dst->getBasePtr(dx,dy))=pixel==96 ? _shadow : byte(color);
 }
}
bool UnicodeBitmapFont::load(const Common::Array<byte> &bytes) {
 _data.clear(); _count=0;
 if(bytes.size()<8 || memcmp(bytes.data(),"PJF1",4)) return false;
 uint count=READ_LE_UINT32(bytes.data()+4);
 if(!count || count>65536 || 8+uint64(count)*38!=bytes.size()) return false;
 uint32 last=0;
 for(uint i=0;i<count;++i) {
  const byte *g=bytes.data()+8+i*38; uint32 ch=READ_LE_UINT32(g);
  if(ch>0x10ffff || (ch>=0xd800 && ch<=0xdfff) || (i && ch<=last) || !g[4] || g[4]>16 || g[5]) return false;
  last=ch;
 }
 _data=bytes; _count=count; return true;
}
int UnicodeBitmapFont::find(uint32 chr) const {
 int lo=0,hi=int(_count)-1;
 while(lo<=hi) {
  int mid=lo+(hi-lo)/2; uint32 code=READ_LE_UINT32(_data.data()+8+mid*38);
  if(code==chr) return mid;
  if(code<chr) lo=mid+1; else hi=mid-1;
 }
 return chr=='?' ? -1 : find('?');
}
int UnicodeBitmapFont::getCharWidth(uint32 chr) const {
 int i=find(chr); return i<0 ? 9 : _data[8+i*38+4]+1;
}
void UnicodeBitmapFont::drawChar(Graphics::Surface *dst,uint32 chr,int x,int y,uint32 color) const {
 int i=find(chr); if(i<0 || !dst || dst->format.bytesPerPixel!=1) return;
 const byte *bits=_data.data()+8+i*38+6;
 // One-pixel outline remains readable over light movie/room backgrounds.
 for(int pass=0;pass<2;++pass) for(int row=0;row<16;++row) for(int col=0;col<16;++col) {
  if(!(bits[row*2+col/8]&(0x80>>(col%8)))) continue;
  for(int dy=pass ? 0 : -1;dy<=(pass ? 0 : 1);++dy) for(int dx=pass ? 0 : -1;dx<=(pass ? 0 : 1);++dx) {
   int px=x+col+dx+1,py=y+row+dy+1;
   if(px>=0 && py>=0 && px<dst->w && py<dst->h) *static_cast<byte *>(dst->getBasePtr(px,py))=pass ? byte(color) : 0;
  }
 }
}
bool runUnicodeFontTests() {
 Common::Array<byte> b; b.resize(46,0); memcpy(b.data(),"PJF1",4); WRITE_LE_UINT32(b.data()+4,1);
 WRITE_LE_UINT32(b.data()+8,0x65e5); b[12]=16; b[14]=0x80;
 UnicodeBitmapFont font; if(!font.load(b) || font.getCharWidth(0x65e5)!=17) return false;
 Graphics::Surface screen; screen.create(4,4,Graphics::PixelFormat::createFormatCLUT8());
 screen.fillRect(Common::Rect(4,4),99); font.drawChar(&screen,0x65e5,0,0,42);
 bool ok=*static_cast<byte *>(screen.getBasePtr(1,1))==42 && *static_cast<byte *>(screen.getBasePtr(0,0))==0;
 font.drawChar(&screen,0x65e5,-16,-16,42); screen.free();
 b.pop_back(); if(font.load(b)) return false;
 b.push_back(0); b[12]=17; return ok && !font.load(b);
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
