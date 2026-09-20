// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/pice.h"
#include "common/system.h"
#include "common/events.h"
#include "graphics/cursorman.h"
#include "graphics/paletteman.h"
#include "common/textconsole.h"
namespace Pice {
bool PiceEngine::ambientDialogue() const {
 return dialogueActive() && !_inputLocks && _actionEntry<0 && _activeMenu<0 &&
  _roomRunner && _roomRunner->exclusiveTask()<0;
}
bool PiceEngine::dialogueBlocked(uint task) const {
 // A background speaker must yield to a player action rather than stopping
 // its script, replacing foreground speech, or reopening speech over a menu.
 return dialogueActive() || _activeMenu>=0 || (_actionEntry>=0 && task!=uint(_actionEntry));
}
void PiceEngine::finishDialogue() {
 if(!dialogueActive()) return;
 _mixer->stopHandle(_speechHandle);
 if(_dialogueActor>=0) {
  Common::Point feet=_actors[_dialogueActor].feet;
  _actors[_dialogueActor]=_dialogueRestore; _actors[_dialogueActor].feet=feet;
 }
 debug("[pice] scripted-dialogue complete actor=%d",_dialogueActor);
 _dialogueActor=-1; _narration=false; _subtitle.clear(); _closeup=false;
}

bool PiceEngine::actorCoordinate(int16 actor,bool vertical,int16 &value) const {
 if(actor<0 || uint(actor)>=_actors.size() || !_actors[actor].active) return false;
 value=vertical ? _actors[actor].feet.y : _actors[actor].feet.x; return true;
}
bool PiceEngine::actorInsideHotspot(int16 actor,int16 hotspot,bool &inside) const {
 if(actor<0 || uint(actor)>=_actors.size() || !_actors[actor].active || hotspot<0 || uint(hotspot)>=_hotspots.size()) return false;
 const Common::Rect &rect=_hotspots[hotspot].bounds; const Common::Point &p=_actors[actor].feet;
 inside=p.x>rect.left && p.x<rect.right && p.y>rect.top && p.y<rect.bottom; return true;
}
bool PiceEngine::stateValue(int16 kind,int16 id,int16 &value) const {
 // Original 0x916 reads the current script bank's task-active byte.
 // Waiting tasks remain active; this query does not advance the scheduler.
 if(kind==3) {
  if(!_roomRunner || id<0 || uint(id)>=_roomBank.count()) return false;
  value=_roomRunner->running(id) ? 1 : 0; return true;
 }
 // Opcode 86 kind 2: inventory record status byte == 1 (0x8fa).
 if(kind!=2 || id<0 || uint(id)>=_ownedItems.size()) return false;
 value=_ownedItems[id]!=0; return true;
}
bool PiceEngine::context(int16 index,int16 &value) const {
 if(index==5) { value=_chapter; return true; }
 if(index==4) { value=int(_roomBankId)-(_chapter==2 ? 1140 : 1182); return true; }
 if(index==6) { value=_lastMenuChoice; return true; }
 if(index!=9) return false;
 value=_actionContext; return true;
}
bool PiceEngine::pollMenu(int16 menu,bool &ready) {
 ready=false;
 if(menu<0 || uint(menu)>=_menus.size() || uint(menu)>=_menuEnabled.size() || (_activeMenu>=0 && _activeMenu!=menu)) return false;
 if(_activeMenu<0 && _menuChoice==-2) {
  _lastMenuChoice=-1; _menuChoice=-1; ready=true; return true;
 }
 if(_activeMenu<0) {
  bool any=false; for(uint i=0;i<_menuEnabled[menu].size();++i) any|=_menuEnabled[menu][i]!=0;
  if(!any) return false;
  _activeMenu=menu; _menuChoice=-1; _menuOpenedAt=_system->getMillis();
  debug("[pice] menu opened id=%d",menu);
 }
 if(_menuChoice!=-1) {
  _lastMenuChoice=_menuChoice>=0 ? _menuChoice : -1; _activeMenu=-1; _menuChoice=-1; ready=true;
 }
 return true;
}
bool PiceEngine::clickMenu(Common::Point point) {
 if(_activeMenu<0 || _menuChoice!=-1 || point.x<360 || point.x>=632 || point.y<60) return false;
 int row=(point.y-60)/24,visible=0;
 for(uint i=0;i<_menuEnabled[_activeMenu].size();++i) if(_menuEnabled[_activeMenu][i]) {
  if(visible++==row) { _menuChoice=i; debug("[pice] menu selected id=%d option=%u",_activeMenu,i); return true; }
 }
 return false;
}
int PiceEngine::hotspotAt(Common::Point point) const {
 point.x+=cameraX();
 // Room actors use the original 640x480 anchors, offset into our 640x400 view.
 if(point.y<(!_playerMovement || _player<0 ? 0 : 40) || point.y>=(!_playerMovement || _player<0 ? 400 : 338)) return -1;
 for(uint i=0;i<_hotspots.size();++i) {
  const Hotspot &h=_hotspots[i]; if(!h.active) continue;
  if(h.actor==-1) { if(h.bounds.contains(point.x,point.y+40)) return i; }
  else if(h.actor>=0 && uint(h.actor)<_actors.size()) {
   const RoomActor &a=_actors[h.actor];
   if(a.active && a.sprite.hasSequence(a.sequence) && a.step<a.sprite.sequences[a.sequence].steps.size() &&
      a.sprite.hitScaledFrame(a.sprite.sequences[a.sequence].steps[a.step].frame,a.feet.x,a.feet.y-40,actorScale(a),point.x,point.y)) return i;
  }
 }
 return -1;
}
bool PiceEngine::inventoryVisible() const {
 if(!_inventoryEnabled) return false;
 return _scenePreview && _roomRunner && _roomRunner->exclusiveTask()<0 && !_gameOver && _activeMenu<0 && (!dialogueActive() || ambientDialogue()) && !_inputLocks && _actionEntry<0 && _eventMan->getMousePos().y<40;
}
int PiceEngine::inventoryAt(Common::Point point) const {
 if(point.y<0 || point.y>=40 || point.x<0 || point.x>=600) return -1;
 uint slot=0;
 for(uint i=0;i<_ownedItems.size();++i) if(_ownedItems[i]) {
  if(int(slot++)==point.x/50) return i;
 }
 return -1;
}
void PiceEngine::updateHover() {
 int spot=-1; _hoverItem=-1;
 if(_scenePreview && !_gameOver && _activeMenu<0 && (!dialogueActive() || ambientDialogue()) && !_inputLocks && _actionEntry<0 && _pendingRoom<0) {
  Common::Point point=_eventMan->getMousePos();
  if(point.y<40) _hoverItem=inventoryAt(point);
  int hit=point.y<40 ? -1 : hotspotAt(point);
  for(uint i=0;i<_actions.size();++i) if(hit>=0 && _actions[i].hotspot==hit && (_actions[i].entry>=0 || _actions[i].fallback>0 || _actions[i].exitChapter>=0)) { spot=hit; break; }
 }
 _hoverHotspot=spot;
 int selected=(!_gameOver && _activeMenu<0 && (!dialogueActive() || ambientDialogue())) ? _selectedItem : -1;
 if(selected>=0 && uint(selected)<_items.size() && uint(selected)<_ownedItems.size() && _ownedItems[selected]) {
  if(_cursorItem==selected) return;
  const RoomActor &item=_items[selected]; Common::Rect bounds;
  if(item.sprite.hasSequence(item.sequence)) {
   uint frame=item.sprite.sequences[item.sequence].steps[0].frame;
   if(item.sprite.frameBounds(frame,bounds) && bounds.width()>0 && bounds.height()>0 && bounds.width()<=512 && bounds.height()<=512) {
    Common::Array<byte> low,high; low.resize(bounds.width()*bounds.height(),0); high.resize(low.size(),255);
    if(item.sprite.paintFrame(frame,-bounds.left,-bounds.top,low,bounds.width(),bounds.height()) &&
       item.sprite.paintFrame(frame,-bounds.left,-bounds.top,high,bounds.width(),bounds.height())) {
     uint w=MIN<int>(40,bounds.width()),h=MIN<int>(34,bounds.height());
     byte pixels[40*34],opaque[40*34],used[256]={0};
     for(uint y=0;y<h;++y) for(uint x=0;x<w;++x) {
      uint src=(y*bounds.height()/h)*bounds.width()+x*bounds.width()/w, dst=y*w+x;
      pixels[dst]=low[src]; opaque[dst]=low[src]==high[src]; if(opaque[dst]) used[pixels[dst]]=1;
     }
     uint transparent=0; while(transparent<256 && used[transparent]) ++transparent;
     if(transparent<256) {
      for(uint i=0;i<w*h;++i) if(!opaque[i]) pixels[i]=transparent;
      byte palette[768]; _system->getPaletteManager()->grabPalette(palette,0,256);
      CursorMan.replaceCursorPalette(palette,0,256); CursorMan.replaceCursor(pixels,w,h,w/2,h/2,transparent,false);
      _cursorItem=selected; return;
     }
    }
   }
  }
 }
 bool reset=_cursorItem!=-1; _cursorItem=-1;
 bool hot=spot>=0 || _hoverItem>=0;
 if(!reset && hot==_hotCursor) return;
 _hotCursor=hot;
 if(!hot) { CursorMan.setDefaultArrowCursor(false); return; }
 // Original native pointer artwork: outlined hand, independent of room palette.
 static const char *rows[]={
  "     XX         ", "    X22X        ", "    X22X        ", "    X22X        ",
  "    X22XXXXX    ", "    X22X22X2X   ", "    X22X22X2XX  ", "    X2222222X2X ",
  " XX X222222222X ", "X22XX222222222X ", "X2222222222222X ", " X222222222222X ",
  "  X22222222222X ", "  X2222222222X  ", "   X222222222X  ", "    X2222222X   ",
  "    X3333333X   ", "    X3333333X   ", "     XXXXXXX    "};
 byte pixels[16*19];
 for(uint y=0;y<19;++y) for(uint x=0;x<16;++x) pixels[y*16+x]=rows[y][x]=='X' ? 1 : rows[y][x]=='2' ? 2 : rows[y][x]=='3' ? 3 : 0;
 const byte colors[]={0,0,0, 0,0,0, 255,255,255, 255,220,64};
 CursorMan.replaceCursorPalette(colors,0,4);
 CursorMan.replaceCursor(pixels,16,19,5,0,0,false);
}
bool PiceEngine::clickRoom(Common::Point point,bool rightButton) {
 if(ambientDialogue()) finishDialogue();
 if(!_scenePreview || !_roomRunner || _inputLocks || dialogueActive() || _pendingRoom>=0 || _actionEntry>=0 ||
    _roomRunner->exclusiveTask()>=0 || (_player>=0 && (uint(_player)>=_actors.size() || actorBusy(_player)))) return false;
 if(rightButton && _selectedItem>=0) { _selectedItem=-1; return true; }
 const int spot=hotspotAt(point);
 // Original input 0x1a128 / 0x1a188 selects verb 1 / 6. Exit hotspots
 // select verb 3. Full inventory and cursor-mode selection remain separate.
 int16 verb=_selectedItem>=0 ? 9 : rightButton ? 1 : 6;
 for(uint i=0;i<_actions.size();++i) if(_selectedItem<0 && _actions[i].hotspot==spot && _actions[i].verb==3) { verb=3; break; }
 int selected=-1,fallback=-1;
 for(uint i=0;i<_actions.size();++i) {
  const Action &a=_actions[i]; if(a.hotspot<0 || a.hotspot!=spot) continue;
  if(a.matches(verb,_selectedItem)) { selected=i; break; }
  if(a.verb!=verb && a.fallback>0) fallback=i;
 }
 bool fallbackUsed=selected<0;
 if(selected<0) selected=fallback;
 if(selected<0) {
  if(!_playerMovement || _player<0 || spot>=0 || point.y<40 || point.y>=338) return false;
  Common::Point target; if(!_navigation.nearest(Common::Point(point.x+cameraX(),point.y+40),target)) return false;
  if(target==_actors[_player].feet) return true;
  Common::Array<int16> args; args.push_back(_player); args.push_back(target.x); args.push_back(target.y); int16 result;
  return invoke(123,Common::String(),args,Common::String(),result);
 }
 const Action &action=_actions[selected];
 int16 entry=fallbackUsed ? action.fallback : action.entry;
 if(entry==-1 && !fallbackUsed && action.exitChapter>=0 && action.approach==-1) return changeRoom(action.exitChapter,action.exitRoom);
 if(entry<0 || uint(entry)>=_roomBank.count() || _roomRunner->running(entry)) return false;
 if(action.approach>=0 && _playerMovement) {
  if(_player<0) return false;
  if(uint(action.approach)>=_navigation.nodes.size() || !_navigation.nodes[action.approach].active) return false;
  const RoomNode &node=_navigation.nodes[action.approach];
  if(_actors[_player].feet!=Common::Point(node.x,node.y)) {
   Common::Array<int16> args; args.push_back(_player); args.push_back(action.approach); int16 result;
   if(!invoke(50,Common::String(),args,Common::String(),result)) return false;
  }
 }
 _actionExitChapter=fallbackUsed ? -1 : action.exitChapter; _actionExitRoom=fallbackUsed ? -1 : action.exitRoom;
 _actionEntry=entry; _actionHotspot=spot; _actionContext=fallbackUsed ? (verb==9 ? _selectedItem : -verb) : -1; _actionWalking=true;
 debug("[pice] action queued hotspot=%d verb=%d entry=%d context=%d approach=%d",spot,verb,entry,_actionContext,action.approach);
 tickAction(); return true;
}
void PiceEngine::releaseActionInput(uint task) {
 // A foreground script can explicitly return control while its timer keeps
 // running (e.g. the opportunity to steal the doctor's manual).
 // Exit actions retain ownership until their deferred transition completes.
 if(_actionEntry!=int(task) || _actionWalking || _inputLocks || _actionExitChapter>=0) return;
 debug("[pice] action released input entry=%d",_actionEntry);
 _actionEntry=-1; _actionHotspot=-1;
}
void PiceEngine::tickAction() {
 if(_actionEntry<0 || !_roomRunner) return;
 if(_actionWalking) {
  if(actorBusy(_player) || _inputLocks || _roomRunner->exclusiveTask()>=0) return;
  if(_actionHotspot<0 || uint(_actionHotspot)>=_hotspots.size() || !_hotspots[_actionHotspot].active) {
   _actionEntry=-1; _actionContext=-1; _actionWalking=false; return;
  }
  _roomRunner->start(_actionEntry); _actionWalking=false;
  debug("[pice] action started entry=%d context=%d",_actionEntry,_actionContext);
 } else if(!_roomRunner->running(_actionEntry)) {
  debug("[pice] action finished entry=%d",_actionEntry);
  // Original 0x7c59 stages the destination before running the exit script;
  // 0x1c586 applies it once the bank's tasks finish. Never follow a failed action.
  if(_actionExitChapter>=0 && !_gameOver && _pendingRoom<0 && !changeRoom(_actionExitChapter,_actionExitRoom))
   _roomStatus="Exit destination is not supported yet";
  _actionExitChapter=_actionExitRoom=-1;
  _actionEntry=-1; _actionContext=-1; _actionHotspot=-1;
 }
}
}
