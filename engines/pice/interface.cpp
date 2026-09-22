// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/pice.h"
#include "common/events.h"
#include "common/system.h"
#include "graphics/cursorman.h"
#include "graphics/surface.h"
#include "graphics/paletteman.h"
namespace Pice {
Common::Point PiceEngine::pointerPosition() const {
 Common::Point point=_eventMan->getMousePos(); point.y-=40; return point;
}
void PiceEngine::presentFrame() {
 // Keep the tested 640x400 world coordinates inside a 640x480 safe viewport.
 // The top margin brings the inventory away from television overscan.
 byte palette[768]; _system->getPaletteManager()->grabPalette(palette,0,256);
 byte black=0; int minimum=766;
 for(int i=0;i<256;++i) { int sum=palette[i*3]+palette[i*3+1]+palette[i*3+2]; if(sum<minimum) { minimum=sum; black=i; } }
 Graphics::Surface *screen=_system->lockScreen();
 for(int y=399;y>=0;--y) memcpy(screen->getBasePtr(0,y+40),screen->getBasePtr(0,y),640);
 screen->fillRect(Common::Rect(0,0,640,40),black);
 screen->fillRect(Common::Rect(0,440,640,480),black);
 _system->unlockScreen(); _system->updateScreen();
}
Common::Rect PiceEngine::dialogueMenuBounds() const {
 int width=0,rows=0;
 if(_activeMenu>=0) for(uint i=0;i<_menuEnabled[_activeMenu].size();++i) if(_menuEnabled[_activeMenu][i]) {
  width=MAX(width,uiFont().getStringWidth(displayText(_menus[_activeMenu][i]))); ++rows;
 }
 width=CLIP(width+24,80,624);
 return Common::Rect(MIN(360,632-width),60,MIN(360,632-width)+width,MIN(396,60+16+rows*uiFont().getFontHeight()));
}
bool PiceEngine::originalCursor(uint frame) {
 Common::Rect bounds;
 if(!_originalCursor.frameBounds(frame,bounds) || bounds.isEmpty() || bounds.width()>128 || bounds.height()>128 || _originalCursor.palette.size()!=768) return false;
 Common::Array<byte> low,high; low.resize(bounds.width()*bounds.height(),0); high.resize(low.size(),255);
 if(!_originalCursor.paintFrame(frame,-bounds.left,-bounds.top,low,bounds.width(),bounds.height()) ||
    !_originalCursor.paintFrame(frame,-bounds.left,-bounds.top,high,bounds.width(),bounds.height())) return false;
 byte used[256]={0};
 for(uint i=0;i<low.size();++i) if(low[i]==high[i]) used[low[i]]=1;
 uint transparent=0; while(transparent<256 && used[transparent]) ++transparent;
 if(transparent==256) return false;
 for(uint i=0;i<low.size();++i) if(low[i]!=high[i]) low[i]=transparent;
 CursorMan.replaceCursorPalette(&_originalCursor.palette[0],0,256);
 if(_saturn) {
  Common::Array<byte> doubled; doubled.resize(low.size()*4);
  for(int y=0;y<bounds.height()*2;++y) for(int x=0;x<bounds.width()*2;++x) doubled[y*bounds.width()*2+x]=low[(y/2)*bounds.width()+x/2];
  CursorMan.replaceCursor(doubled.data(),bounds.width()*2,bounds.height()*2,CLIP(-bounds.left,0,int(bounds.width())-1)*2,CLIP(-bounds.top,0,int(bounds.height())-1)*2,transparent,false);
 } else {
 CursorMan.replaceCursor(&low[0],bounds.width(),bounds.height(),CLIP(-bounds.left,0,int(bounds.width())-1),CLIP(-bounds.top,0,int(bounds.height())-1),transparent,false);
 }
 return true;
}
}
