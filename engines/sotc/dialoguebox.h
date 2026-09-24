// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_DIALOGUEBOX_H
#define SOTC_DIALOGUEBOX_H
#include "common/str.h"
#include "sotc/journal.h"
namespace Sotc {
// Original text uses CTU.PFN, three authored lines per page (0xb701),
// and the nine BULLE.VA2 border/interior poses (0xb8a2).
class DialogueBox {
 JournalFont _font;
 Common::Array<byte> _bubble;
 Common::Array<Common::String> _lines;
public:
 bool load(const Common::Array<byte> &font,const Common::Array<byte> &bubble) {
  if(!_font.load(font) || va2Records(bubble.data(),bubble.size(),0)<9)return false;
  _bubble=bubble;return true;
 }
 void text(const Common::String &text) {
  _lines.clear();Common::String line;
  // Preserve authored breaks; wrap only oversized lines to keep all text on screen.
  for(unsigned i=0;i<text.size();++i) {
   char ch=text[i];if(ch=='\r')continue;
   if(ch=='\n'){_lines.push_back(line);line.clear();continue;}
   line+=ch;
   if(_font.width(line.c_str())>300) {
    unsigned split=line.size()-1;
    for(unsigned j=split;j>0;--j)if(line[j]==' '){split=j;break;}
    _lines.push_back(Common::String(line.c_str(),split));
    unsigned start=split+(line[split]==' ' ? 1 : 0);
    line=Common::String(line.c_str()+start);
   }
  }
  if(!line.empty())_lines.push_back(line);
 }
 unsigned count() const {return _lines.size();}
 unsigned next(unsigned page) const {return page+3;}
 bool more(unsigned page) const {return next(page)<count();}
 unsigned pageWeight(unsigned page) const {
  unsigned ticks=0;
  for(unsigned i=page;i<count() && i<page+3;++i) {
   ticks+=_font.width(_lines[i].c_str())/4;
   if(ticks<100)ticks=100; // Original 0xb7fe..0xb80f.
  }
  return ticks;
 }
 unsigned pageDelay(unsigned page,unsigned speechMs) const {
  unsigned weight=pageWeight(page),total=0;
  for(unsigned i=0;i<count();i=next(i))total+=pageWeight(i);
  // The preview's scene clock is not the DOS clock. Distribute a voiced
  // recording over its pages; no word/page timestamps exist in this UI.
  if(speechMs && total)return unsigned(uint64_t(speechMs)*weight/total);
  return weight*20; // Readable fallback when no recording is available.
 }
 bool render(Common::Array<byte> &screen,unsigned page,int anchorX,int anchorY,bool portrait) const {
  if(page>=count())return false;
  unsigned end=page+3<count() ? page+3 : count();
  while(end>page+1 && _lines[end-1].empty())--end;
  unsigned width=0;for(unsigned i=page;i<end;++i){unsigned w=_font.width(_lines[i].c_str());if(w>width)width=w;}
  int w=((int(width)+13)/8)*8,h=((int(end-page)*int(_font.height())+13)/8)*8;
  if(w<24)w=24;if(w>319)w=319;
  int left=portrait ? 0 : anchorX-w/2,top=portrait ? 160 : anchorY;
  if(left<0)left=0;if(left+w>319)left=319-w;
  if(top<0)top=0;if(top+h>199)top=199-h;
  int right=left+w,bottom=top+h;
  PoseRenderer renderer(screen);
  for(int y=top+8;y<=bottom-8;y+=8)for(int x=left+8;x<right-8;x+=8)
   if(!renderer.pose(_bubble,8,x,y))return false;
  for(int x=left+8;x<right-8;x+=8)
   if(!renderer.pose(_bubble,3,x,top+8) || !renderer.pose(_bubble,4,x,bottom))return false;
  for(int y=top+16;y<bottom;y+=8)
   if(!renderer.pose(_bubble,1,left,y) || !renderer.pose(_bubble,6,right-16,y))return false;
  if(!renderer.pose(_bubble,0,left,top+8) || !renderer.pose(_bubble,5,right-16,top+8) ||
     !renderer.pose(_bubble,2,left,bottom) || !renderer.pose(_bubble,7,right-16,bottom))return false;
  for(unsigned i=page;i<end;++i)_font.draw(screen,_lines[i].c_str(),left+3,top+3+(i-page)*_font.height(),0);
  return true;
 }
};
class DialogueTimer {
 unsigned _remaining=0;
public:
 void reset(unsigned ms) {_remaining=ms;}
 bool advance(unsigned elapsed,bool active,bool lastPage,bool speechActive) {
  if(!active)return false;
  _remaining=elapsed>=_remaining ? 0 : _remaining-elapsed;
  return !_remaining && (!lastPage || !speechActive);
 }
};
}
#endif
