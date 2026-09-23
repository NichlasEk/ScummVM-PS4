// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_JOURNAL_H
#define SOTC_JOURNAL_H
#include "sotc/dialogue.h"
#include "sotc/poserenderer.h"
namespace Sotc {
// CTU.PFN: original font setup/drawing at 0x153ad/0x152a2.
class JournalFont {
 Common::Array<byte> _data;
 unsigned _first=0,_height=0,_stride=0,_table=0;
 unsigned glyph(unsigned c) const {
  if(c<_first || _table+(c-_first)*2+2>_data.size()) return 0;
  unsigned p=_table+(c-_first)*2;return unsigned(_data[p])*256+_data[p+1];
 }
public:
 bool load(const Common::Array<byte> &data) {
  if(data.size()<8) return false;
  _data=data;_first=data[0];_height=data[2];_stride=data[3] ? data[3] : va2LE16(data.data()+4);
  _table=unsigned(data[6])*256+data[7];
  if(!_height || !_stride || _table<8 || _table>data.size() || 8+_height*_stride>_table) return false;
  for(unsigned c=_first;c<256;++c) {
   unsigned g=glyph(c);
   if((g&4095)+(g>>12)>_stride*8) return false;
  }
  return true;
 }
 unsigned width(const char *s) const {
  unsigned n=0;for(;*s;++s) {unsigned w=glyph(byte(*s))>>12;n+=w ? w+1 : 3;}return n;
 }
 void draw(Common::Array<byte> &screen,const char *s,int x,int y,byte color) const {
  if(screen.size()<64000) return;
  for(;*s;++s) {
   unsigned g=glyph(byte(*s)),w=g>>12,offset=g&4095;
   for(unsigned row=0;row<_height;++row) for(unsigned col=0;col<w;++col) {
    unsigned bit=offset+col,p=8+row*_stride+bit/8;
    if(p<_table && (_data[p]&(128>>(bit%8))) && x+int(col)>=0 && x+int(col)<320 && y+int(row)>=0 && y+int(row)<200)
     screen[(y+row)*320+x+col]=color;
   }
   x+=w ? w+1 : 3;
  }
 }
};
class Journal {
 Common::Array<byte> _icons,_text;
 JournalFont _font;
 unsigned _page=0,_latest=0;
public:
 bool load(const Common::Array<byte>&icons,const Common::Array<byte>&font,const Common::Array<byte>&text) {
  if(!_font.load(font))return false;
  _icons=icons;_text=text;return true;
 }
 bool open(unsigned latest) {
  byte decoded[2048];unsigned size=0;
  // Original 0x375d reads global[1]; never expose future diary records.
  if(latest>33 || !dialogueRecord(Cc4View(_text.data(),_text.size()),2,latest,decoded,sizeof(decoded),size))return false;
  _page=_latest=latest;return true;
 }
 unsigned page() const {return _page;}
 bool turn(int direction) {
  if((direction<0 && !_page) || (direction>0 && _page>=_latest) || !direction)return false;
  _page=unsigned(int(_page)+(direction<0 ? -1 : 1));return true;
 }
 bool render(Common::Array<byte>&screen) {
  byte decoded[2048];unsigned size=0;
  if(!dialogueRecord(Cc4View(_text.data(),_text.size()),2,_page,decoded,sizeof(decoded),size))return false;
  PoseRenderer renderer(screen);
  if(!renderer.pose(_icons,30,0,0,0,0,319,199))return false;
  if(_page<_latest && !renderer.pose(_icons,37,0,0,0,0,319,199))return false;
  // Original 0x3424: 106px columns at X=58/173, 13 lines per page.
  unsigned pos=1;
  for(unsigned line=0;line<26 && pos<size && decoded[pos]!='*';++line) {
   char text[2048];unsigned n=0;
   while(pos<size && decoded[pos] && decoded[pos]!='*')text[n++]=decoded[pos++];
   text[n]=0;if(pos<size)++pos;
   int left=line<13 ? 58 : 173;
   _font.draw(screen,text,left+(106-int(_font.width(text)))/2,48+(line%13)*10,64);
  }
  char number[16];
  for(unsigned side=0;side<2;++side) {
   unsigned value=_page*2+1+side,n=0;char digits[8];
   do {digits[n++]=char('0'+value%10);value/=10;}while(value);
   unsigned p=0;number[p++]='-';number[p++]=' ';
   while(n)number[p++]=digits[--n];number[p++]=' ';number[p++]='-';number[p]=0;
   _font.draw(screen,number,58+side*115+(106-int(_font.width(number)))/2,180,58);
  }
  return true;
 }
};
}
#endif
