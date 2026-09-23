// SPDX-License-Identifier: GPL-3.0-or-later
#include "sotc/openingscene.h"
#include "sotc/poserenderer.h"
#include "sotc/animation.h"
#include "common/textconsole.h"
#include "common/debug.h"
namespace Sotc {
bool OpeningScene::fail(const char *reason) {
 warning("[sotc-opening] animated task stopped pc=%u: %s",_pc,reason);
 _done=_failed=true; return false;
}
bool OpeningScene::start(Cc4View archive,unsigned pc,const Common::Array<byte> &background) {
 Cc4View room,script;
 if (!cc4Entry(archive,4,0,room) || !cc4Entry(room,2,2,script) || pc>=script.size ||
     background.size()!=64000 || !_resources.open("A05.PAK") || !_rooms.open("D05.PAK")) return fail("cannot load task/resources");
 _script.resize(script.size); memcpy(_script.data(),script.data,script.size);
 _pc=pc; _background=background; _pixels=background;
 debug("[sotc-opening] animated task starts pc=%u",_pc);
 return true;
}
bool OpeningScene::commands() {
 if (_waitingActor<11) {
  const Actor &a=_actors[_waitingActor];
  if (!a.count) return fail("empty animation wait");
  if (a.step!=a.count-1) return true;
  _waitingActor=11;
 }
 if (_wait) { --_wait; if (_wait) return true; }
 for (unsigned budget=0;budget<128;++budget) {
  if (_pc>=_script.size()) return fail("truncated instruction");
  unsigned op=_script[_pc];
  // The first not-yet-supported parallel task begins the star/eye scene.
  if (op==27) {
   debug("[sotc-opening] supported animation prefix ends pc=%u before parallel task",_pc);
   _done=true; return true;
  }
  unsigned width=op==41 ? 1 : op==7 ? 3 : 2;
  if (_script.size()-_pc<width) return fail("truncated operand");
  unsigned value=width>1 ? _script[_pc+1] : 0;
  Actor &a=_actors[_actor];
  switch (op) {
  case 90:
   if (!value || value>=11) return fail("invalid reset actor");
   _actors[value]=Actor(); _actors[value].visible=true; _actor=value; break;
  case 21:
   if (value>=11) return fail("invalid actor");
   _actor=value; break;
  case 91:
   if (!_resources.load(value,a.data)) return fail("missing actor resource");
   debug("[sotc-opening] actor=%u resource=%s",_actor,_resources.entries[value].name.c_str()); break;
  case 7: a.x=value*2; a.y=_script[_pc+2]; break;
  case 41: a.visible=false; break;
  case 58:
   if (!_rooms.load(value*2,_background) || _background.size()!=64000) return fail("missing background");
   break;
  case 87: case 88: {
   unsigned count=va2SequenceLength(a.data.data(),a.data.size(),value);
   SequenceStep first;
   if (!count || !va2Step(a.data.data(),a.data.size(),value,0,first)) return fail("invalid sequence");
   a.sequence=value; a.step=0; a.count=count;
   debug("[sotc-opening] actor=%u sequence=%u frames=%u",_actor,value,count);
   if (op==87) _waitingActor=_actor;
   break;
  }
  case 9: _wait=value; break;
  case 93: case 94:
   if (!value) return fail("zero fade step");
   _fade=op; _fadeStep=value; _fadeClock=0; break;
  case 99: if (!_sound.effect(value)) return fail("sample unavailable"); break;
  default: return fail("unsupported instruction");
  }
  _pc+=width;
  if (_fade || _wait || _waitingActor<11) return true;
 }
 return fail("instruction budget exceeded");
}
bool OpeningScene::render() {
 _pixels=_background;
 PoseRenderer renderer(_pixels);
 unsigned order[11],count=0;
 for (unsigned id=1;id<11;++id) if (_actors[id].visible) {
  unsigned n=count++;
  while (n && _actors[order[n-1]].y>_actors[id].y) { order[n]=order[n-1]; --n; }
  order[n]=id;
 }
 for (unsigned i=0;i<count;++i) {
  const Actor &a=_actors[order[i]];
  SequenceStep step,next; int mx=0,my=0,nextPose=-1; unsigned fraction=0;
  if (!va2Step(a.data.data(),a.data.size(),a.sequence,a.step,step) ||
      !va2Position(a.data.data(),a.data.size(),a.sequence,a.step,mx,my) || (step.control>>14)>1) return fail("unsupported animation step");
  if ((step.control>>14)==1) {
   next=step;
   if (a.step+1<a.count && !va2Step(a.data.data(),a.data.size(),a.sequence,a.step+1,next)) return fail("invalid next pose");
   unsigned divisor=step.control&0x3fff;
   fraction=((a.substep<<8)&65535)/(divisor ? divisor : 1); nextPose=next.pose;
  }
  if (!renderer.pose(a.data,step.pose,a.x+mx,a.y+my,0,0,319,199,-1,nextPose,fraction)) return fail("unsupported pose");
 }
 return true;
}
bool OpeningScene::advanceActors() {
 for (unsigned id=1;id<11;++id) {
  Actor &a=_actors[id]; if (!a.visible) continue;
  SequenceStep step;
  if (!va2Step(a.data.data(),a.data.size(),a.sequence,a.step,step) ||
      !advanceAnimation(step.control,2,255,a.count,a.step,a.substep)) return fail("cannot advance animation");
 }
 return true;
}
void OpeningScene::advance(unsigned elapsed) {
 if (_done) return;
 if (_fade) {
  _fadeClock+=elapsed;
  while (_fadeClock>=16 && _fade) {
   _fadeClock-=16;
   if (_fade==93) { _brightness=MIN(255U,_brightness+_fadeStep); if (_brightness==255) _fade=0; }
   else { _brightness=_brightness>_fadeStep ? _brightness-_fadeStep : 0; if (!_brightness) _fade=0; }
  }
  return;
 }
 _clock+=elapsed;
 // Bound catch-up after a suspended emulator. Never spin through a whole intro.
 if (_clock>1000) _clock=1000;
 while (_clock>=100 && !_done && !_fade) {
  _clock-=100;
  if (!commands() || _done || !render() || !advanceActors()) return;
 }
}
}
