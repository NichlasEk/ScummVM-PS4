// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/pice.h"
#include "common/textconsole.h"
namespace Pice {
bool PiceEngine::context(int16 index,int16 &value) const {
 if(index!=9) return false;
 value=_actionContext; return true;
}
int PiceEngine::hotspotAt(Common::Point point) const {
 // Room actors use the original 640x480 anchors, offset into our 640x400 view.
 if(point.y<40 || point.y>=338) return -1;
 for(uint i=0;i<_hotspots.size();++i) {
  const Hotspot &h=_hotspots[i]; if(!h.active) continue;
  if(h.actor==-1) { if(h.bounds.contains(point.x,point.y+40)) return i; }
  else if(h.actor>=0 && uint(h.actor)<_actors.size()) {
   const RoomActor &a=_actors[h.actor];
   if(a.active && a.sprite.hasSequence(a.sequence) && a.step<a.sprite.sequences[a.sequence].steps.size() &&
      a.sprite.hitFrame(a.sprite.sequences[a.sequence].steps[a.step].frame,a.feet.x,a.feet.y-40,point.x,point.y)) return i;
  }
 }
 return -1;
}
bool PiceEngine::clickRoom(Common::Point point,bool rightButton) {
 if(!_scenePreview || !_roomRunner || _inputLocks || _dialogueActor>=0 || _pendingRoom>=0 || _actionEntry>=0 ||
    _roomRunner->exclusiveTask()>=0 || _player<0 || uint(_player)>=_actors.size() || actorBusy(_player)) return false;
 if(rightButton && _selectedItem>=0) { _selectedItem=-1; return true; }
 const int spot=hotspotAt(point);
 // Original input 0x1a128 / 0x1a188 selects verb 1 / 6. Exit hotspots
 // select verb 3. Full inventory and cursor-mode selection remain separate.
 int16 verb=_selectedItem>=0 ? 9 : rightButton ? 1 : 6;
 for(uint i=0;i<_actions.size();++i) if(_selectedItem<0 && _actions[i].hotspot==spot && _actions[i].verb==3) { verb=3; break; }
 int selected=-1,fallback=-1;
 for(uint i=0;i<_actions.size();++i) {
  const Action &a=_actions[i]; if(a.hotspot!=spot) continue;
  if(a.matches(verb,_selectedItem)) { selected=i; break; }
  if(a.verb!=verb && a.fallback) fallback=i;
 }
 bool fallbackUsed=selected<0;
 if(selected<0) selected=fallback;
 if(selected<0) {
  if(spot>=0 || point.y<40 || point.y>=338) return false;
  Common::Point target; if(!_navigation.nearest(Common::Point(point.x,point.y+40),target)) return false;
  if(target==_actors[_player].feet) return true;
  Common::Array<int16> args; args.push_back(_player); args.push_back(target.x); args.push_back(target.y); int16 result;
  return invoke(123,Common::String(),args,Common::String(),result);
 }
 const Action &action=_actions[selected];
 int16 entry=fallbackUsed ? action.fallback : action.entry;
 if(entry<0 || uint(entry)>=_roomBank.count() || _roomRunner->running(entry)) return false;
 if(action.approach>=0) {
  if(uint(action.approach)>=_navigation.nodes.size() || !_navigation.nodes[action.approach].active) return false;
  const RoomNode &node=_navigation.nodes[action.approach];
  if(_actors[_player].feet!=Common::Point(node.x,node.y)) {
   Common::Array<int16> args; args.push_back(_player); args.push_back(action.approach); int16 result;
   if(!invoke(50,Common::String(),args,Common::String(),result)) return false;
  }
 }
 _actionEntry=entry; _actionHotspot=spot; _actionContext=fallbackUsed ? (verb==9 ? _selectedItem : -verb) : -1; _actionWalking=true;
 debug("[pice] action queued hotspot=%d verb=%d entry=%d context=%d approach=%d",spot,verb,entry,_actionContext,action.approach);
 tickAction(); return true;
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
  _actionEntry=-1; _actionContext=-1; _actionHotspot=-1;
 }
}
}
