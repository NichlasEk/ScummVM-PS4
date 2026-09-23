// SPDX-License-Identifier: GPL-3.0-or-later
#include "sotc/openingscene.h"
#include "sotc/poserenderer.h"
#include "sotc/animation.h"
#include "sotc/script.h"
#include "common/textconsole.h"
#include "common/debug.h"
namespace Sotc {
bool OpeningScene::fail(const char *reason) {
 warning("[sotc-opening] animated task stopped pc=%u: %s",_tasks[_activeTask].pc,reason);
 _done=_failed=true; return false;
}
bool OpeningScene::start(Cc4View archive,unsigned pc,const Common::Array<byte> &background) {
 Cc4View room,script;
 if (!cc4Entry(archive,4,0,room) || !cc4Entry(room,2,2,script) || pc>=script.size ||
     background.size()!=64000 || !_resources.open("A05.PAK") || !_rooms.open("D05.PAK")) return fail("cannot load task/resources");
 for (unsigned i=1;i<3;++i) {
  if (!cc4Entry(room,2,i,script)) return fail("missing task");
  _tasks[i].script.resize(script.size); memcpy(_tasks[i].script.data(),script.data,script.size);
 }
 _tasks[2].pc=pc; _tasks[2].enabled=true; _background=background; _pixels=background;
 debug("[sotc-opening] animated task starts pc=%u",pc);
 return true;
}
bool OpeningScene::commands(unsigned id) {
 _activeTask=id; Task &task=_tasks[id];
 if (!task.enabled) return true;
 Common::Array<byte> &_script=task.script;
 unsigned &_pc=task.pc,&_actor=task.actor,&_wait=task.wait,&_waitingActor=task.waitingActor;
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
  if (op==3 || op==10) {
   ScriptInstruction instruction;
   if (decodeScript(Cc4View(_script.data(),_script.size()),_pc,instruction)!=kInstruction) return fail("invalid branch");
   if (op==10 && (instruction.variable!=33 || instruction.valueIsVariable || instruction.comparison!=0)) return fail("unsupported comparison");
   _pc=(op==3 || _randomValue!=instruction.value) ? instruction.target : instruction.next;
   continue;
  }
  unsigned width=op==41 || op==2 ? 1 : op==7 || op==45 || op==100 ? 3 : 2;
  if (_script.size()-_pc<width) return fail("truncated operand");
  unsigned value=width>1 ? _script[_pc+1] : 0;
  Actor &a=_actors[_actor];
  switch (op) {
  case 44:
   if (id!=2 || value!=1) return fail("unsupported room transition");
   debug("[sotc-opening] opening block complete; next story block=%u",value);
   _done=true; return true;
  case 100:
   if (!_sound.repeatedEffect(value,_script[_pc+2])) return fail("repeated sample unavailable");
   break;
  case 2: task.enabled=false; return true;
  case 27:
   if (value!=1) return fail("unsupported task activation");
   _tasks[value].pc=0; _tasks[value].wait=0; _tasks[value].waitingActor=11; _tasks[value].enabled=true;
   debug("[sotc-opening] task=%u activated",value); break;
  case 28:
   if (value!=1) return fail("unsupported task deactivation");
   _tasks[value].enabled=false; debug("[sotc-opening] task=%u disabled",value); break;
  case 79:
   _randomValue=0;
   if (value>1) { _randomState=_randomState*uint32(0x015a4e35)+1; _randomValue=((_randomState>>16)&0x7fff)%value; }
   break;
  case 45: {
   unsigned count=va2SequenceLength(a.data.data(),a.data.size(),value),step=_script[_pc+2];
   if (!count || step>=count) return fail("invalid held animation");
   a.sequence=value; a.count=count; a.step=step; a.gate=step; break;
  }
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
   a.sequence=value; a.step=0; a.count=count; a.gate=255;
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
  if (!renderer.pose(a.data,step.pose,a.x+mx,a.y+my,0,0,319,199,-1,nextPose,fraction)) {
   warning("[sotc-opening] actor=%u: %s",order[i],"pose failed"); return fail("unsupported pose");
  }
 }
 return true;
}
bool OpeningScene::advanceActors() {
 for (unsigned id=1;id<11;++id) {
  Actor &a=_actors[id]; if (!a.visible) continue;
  SequenceStep step;
  if (!va2Step(a.data.data(),a.data.size(),a.sequence,a.step,step) ||
      !advanceAnimation(step.control,2,a.gate,a.count,a.step,a.substep)) return fail("cannot advance animation");
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
  for (unsigned i=1;i<3;++i) if (!commands(i) || _done) return;
  if (!render() || !advanceActors()) return;
 }
}
}
