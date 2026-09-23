// SPDX-License-Identifier: GPL-3.0-or-later
#include "sotc/openingscene.h"
#include "sotc/poserenderer.h"
#include "sotc/animation.h"
#include "sotc/script.h"
#include "sotc/dialogue.h"
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
bool OpeningScene::selectSequence(unsigned id,unsigned sequence,unsigned mode) {
 Actor &a=_actors[id];
 unsigned count=va2SequenceLength(a.data.data(),a.data.size(),sequence);
 if (!count) return fail("invalid sequence");
 a.sequence=sequence; a.step=0; a.count=count; a.gate=255; a.mode=mode;
 return true;
}
void OpeningScene::finishDialogue() {
 _talking=false; _dialogue.clear(); _sound.stopSpeech();
 if (_portrait) { _actors[10].visible=false; _actors[_speaker].visible=_speakerVisible; _portrait=false; }
}
bool OpeningScene::startStory(Cc4View archive,Cc4View text) {
 for (unsigned i=0;i<11;++i) _actors[i]=Actor();
 for (unsigned i=0;i<3;++i) _tasks[i]=Task();
 _done=_failed=_talking=_portrait=false; _story=true; _dialogue.clear();
 _brightness=_fade=_fadeStep=_fadeClock=_clock=0; _activeTask=0;
 if (!text.data || !text.size || !_resources.open("A05.PAK") || !_rooms.open("D05.PAK")) return fail("story resources missing");
 _textFile.resize(text.size); memcpy(_textFile.data(),text.data,text.size);
 Cc4View room,script;
 if (!cc4Entry(archive,4,1,room)) return fail("story room missing");
 for (unsigned i=0;i<2;++i) {
  if (!cc4Entry(room,2,i,script)) return fail("story task missing");
  _tasks[i].script.resize(script.size); memcpy(_tasks[i].script.data(),script.data,script.size);
 }
 // Original startup 0x773c..0x776a: RES resource 2, actor 0 at (160,190).
 Pak common;
 if (!common.open("RES.PAK") || !common.load(2,_actors[0].data)) return fail("player resource missing");
 _actors[0].x=160; _actors[0].y=190;
 if (!selectSequence(0,0,0)) return false;
 _tasks[0].enabled=true;
 _background.resize(64000); memset(_background.data(),0,64000); _pixels=_background;
 return commands(0) && render();
}
bool OpeningScene::commands(unsigned id) {
 _activeTask=id; Task &task=_tasks[id];
 if (!task.enabled || _talking) return true;
 Common::Array<byte> &_script=task.script;
 unsigned &_pc=task.pc,&_actor=task.actor,&_wait=task.wait,&_waitingActor=task.waitingActor;
 if (_actors[_actor].moving) return true;
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
  unsigned width=op==41 || op==2 ? 1 : op==89 ? 6 : op==82 ? 5 : op==42 || op==7 || op==45 || op==100 || op==70 ? 3 : 2;
  if (_script.size()-_pc<width) return fail("truncated operand");
  unsigned value=width>1 ? _script[_pc+1] : 0;
  Actor &a=_actors[_actor];
  switch (op) {
  case 96: _speechBank=value; break;
  case 77: if (!_sound.music(value)) return fail("music unavailable"); break;
  case 18:
   if (!_story || value!=0) return fail("unsupported player control");
   _actors[0].moving=false; break; // Script owns the player throughout this scene.
  case 92:
   if (!_story || value>1) return fail("unsupported draw flag");
   a.visible=value!=0; break;
  case 42: a.clipLeft=value*2; a.clipRight=_script[_pc+2]*2; break;
  case 4: case 5: {
   if (!_story || _actor!=0) return fail("unsupported story destination");
   a.targetX=op==4 ? int(value*2) : a.x; a.targetY=op==5 ? int(value) : a.y;
   unsigned direction=a.targetX>a.x ? 2 : a.targetX<a.x ? 4 : a.targetY>a.y ? 3 : 1;
   a.moving=a.targetX!=a.x || a.targetY!=a.y;
   if (a.moving && !selectSequence(_actor,direction+3,0)) return false;
   debug("[sotc-story] walk=%d,%d -> %d,%d",a.x,a.y,a.targetX,a.targetY);
   break;
  }
  case 69: _textColor=value; break;
  case 70: _textX=value*2; _textY=_script[_pc+2]; break;
  case 82: case 89: {
   if (!_story || value>=10 || (op==82 && _script[_pc+4]!=255)) return fail("unsupported story dialogue mode");
   if (op==89) {
    _actors[10]=Actor(); Actor &portrait=_actors[10];
    if (!_resources.load(_script[_pc+5],portrait.data)) return fail("portrait resource missing");
    portrait.y=199; portrait.visible=true;
    if (!selectSequence(10,_script[_pc+4])) return false;
    _speaker=value; _speakerVisible=_actors[value].visible; _actors[value].visible=false; _portrait=true;
   }
   unsigned text=unsigned(_script[_pc+2])|(unsigned(_script[_pc+3])<<8),length=0; byte decoded[16384];
   if (!dialogueRecord(Cc4View(_textFile.data(),_textFile.size()),_speechBank+3,text,decoded,sizeof(decoded),length) || length<2) return fail("invalid story text");
   _dialogue.clear();
   for (unsigned i=1;i<length-(decoded[length-1]==0 ? 1 : 0);++i) _dialogue+=decoded[i] ? char(decoded[i]) : '\n';
   _talking=true;
   if (!_sound.speech(_speechBank,text)) warning("[sotc-story] text=%u: %s",text,"speech unavailable; retaining subtitles");
   debug("[sotc-story] bank=%u speaker=%u text=%u",_speechBank,value,text);
   break;
  }
  case 44:
   if (_story) {
    if (id!=1 || value!=0 || _script.size()-_pc<5 || _script[_pc+2]!=80 || _script[_pc+3]!=0 || _script[_pc+4]!=2)
     return fail("unsupported story transition");
    debug("[sotc-story] office complete; handing over at new-game request");
    _done=true; return true;
   }
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
   a.sequence=value; a.step=0; a.count=count; a.gate=255; a.mode=2;
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
  if (_fade || _wait || _waitingActor<11 || _talking || a.moving) return true;
 }
 return fail("instruction budget exceeded");
}
bool OpeningScene::render() {
 _pixels=_background;
 PoseRenderer renderer(_pixels);
 unsigned order[11],count=0;
 for (unsigned id=0;id<11;++id) if (_actors[id].visible && _actors[id].clipRight>_actors[id].clipLeft) {
  unsigned n=count++;
  while (n && _actors[order[n-1]].y>_actors[id].y) { order[n]=order[n-1]; --n; }
  order[n]=id;
 }
 for (unsigned i=0;i<count;++i) {
  const Actor &a=_actors[order[i]];
  SequenceStep step,next; int mx=0,my=0,nextPose=-1; unsigned fraction=0;
  if (!va2Step(a.data.data(),a.data.size(),a.sequence,a.step,step) ||
      (a.mode==2 && !va2Position(a.data.data(),a.data.size(),a.sequence,a.step,mx,my)) || (step.control>>14)>1) return fail("unsupported animation step");
  if (a.mode==2 && (step.control>>14)==1) {
   next=step;
   if (a.step+1<a.count && !va2Step(a.data.data(),a.data.size(),a.sequence,a.step+1,next)) return fail("invalid next pose");
   unsigned divisor=step.control&0x3fff;
   fraction=((a.substep<<8)&65535)/(divisor ? divisor : 1); nextPose=next.pose;
  }
  if (!renderer.pose(a.data,step.pose,a.x+mx,a.y+my,a.clipLeft,0,a.clipRight,199,-1,nextPose,fraction)) {
   warning("[sotc-opening] actor=%u: %s",order[i],"pose failed"); return fail("unsupported pose");
  }
 }
 return true;
}
bool OpeningScene::advanceActors() {
 for (unsigned id=0;id<11;++id) {
  Actor &a=_actors[id]; if (!a.visible) continue;
  SequenceStep step;
  if (!va2Step(a.data.data(),a.data.size(),a.sequence,a.step,step) ||
      !advanceAnimation(step.control,a.mode,a.gate,a.count,a.step,a.substep)) return fail("cannot advance animation");
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
  for (unsigned i=_story ? 0 : 1;i<3;++i) if (!commands(i) || _done) return;
  for (unsigned i=0;i<11;++i) {
   Actor &a=_actors[i]; if (!a.moving || _talking) continue;
   SequenceStep step;
   if (!va2Step(a.data.data(),a.data.size(),a.sequence,a.step,step)) { fail("invalid walking frame"); return; }
   int remaining=a.targetX!=a.x ? a.targetX-a.x : a.targetY-a.y;
   int delta=a.targetX!=a.x ? step.dx : step.dy;
   if ((remaining>0 && delta<0) || (remaining<0 && delta>0)) { fail("walking delta has wrong direction"); return; }
   if (remaining>0 && delta>remaining) delta=remaining;
   if (remaining<0 && delta<remaining) delta=remaining;
   if (a.targetX!=a.x) a.x+=delta; else a.y+=delta;
   if (a.x==a.targetX && a.y==a.targetY) {
    a.moving=false;
    unsigned standing=a.sequence-4;
    if (!selectSequence(i,standing,0)) return;
    debug("[sotc-story] arrived=%d,%d",a.x,a.y);
   }
  }
  if (!render() || !advanceActors()) return;
 }
}
}
