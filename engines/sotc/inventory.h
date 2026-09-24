// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_INVENTORY_H
#define SOTC_INVENTORY_H
#include "sotc/journal.h"
#include "common/str.h"
namespace Sotc {
class Inventory {
 Common::Array<byte> _frame,_objects;
 JournalFont _font;
public:
 enum { kRows=10, kTop=63, kRowHeight=12 };
 bool load(const Common::Array<byte>&frame,const Common::Array<byte>&objects,const Common::Array<byte>&font) {
  _frame=frame;_objects=objects;return _font.load(font);
 }
 int itemAt(int x,int y,unsigned selected,unsigned count) const {
  if(x<73 || x>=254 || y<kTop || y>=kTop+kRows*kRowHeight)return -1;
  unsigned i=selected/kRows*kRows+(y-kTop)/kRowHeight;
  return i<count ? int(i) : -1;
 }
 bool render(Common::Array<byte>&screen,const Common::Array<unsigned>&ids,const Common::Array<Common::String>&names,unsigned selected) const {
  if(screen.size()!=64000 || ids.size()!=names.size())return false;
  PoseRenderer renderer(screen);
  // ICONE.VA2's wide inventory panel, with OBJECTS.VA2 item-id poses.
  if(!renderer.pose(_frame,16,0,0,0,0,319,199))return false;
  unsigned first=selected/kRows*kRows;
  for(unsigned i=first;i<ids.size() && i<first+kRows;++i) {
   int y=kTop+(i-first)*kRowHeight;
   if(i==selected) {
    for(int x=73;x<254;++x)screen[y*320+x]=screen[(y+11)*320+x]=22;
    for(int row=y;row<=y+11;++row)screen[row*320+73]=screen[row*320+253]=22;
   }
   if(!renderer.pose(_objects,ids[i],74,y-3,74,y,89,y+11))return false;
   Common::String label=names[i];
   while(!label.empty() && _font.width(label.c_str())>159)label.erase(label.size()-1);
   _font.draw(screen,label.c_str(),92,y+1,119);
  }
  // Original small arrows indicate another page without covering an item.
  if(first && !renderer.pose(_frame,53,0,0,0,0,319,199))return false;
  if(first+kRows<ids.size() && !renderer.pose(_frame,52,0,0,0,0,319,199))return false;
  return true;
 }
};
}
#endif
