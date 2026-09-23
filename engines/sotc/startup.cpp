// SPDX-License-Identifier: GPL-3.0-or-later
#include "sotc/startup.h"
#include "sotc/dialogue.h"
#include "sotc/startstate.h"
#include "sotc/animation.h"
#include "sotc/draworder.h"
#include "sotc/walk.h"
#include "sotc/collision.h"
#include "sotc/sequence.h"
#include "sotc/poserenderer.h"
#include "sotc/scriptmachine.h"
#include "sotc/pak.h"
#include "common/file.h"
#include "common/debug.h"
#include "common/textconsole.h"
namespace Sotc {
static bool readFile(const char *name, Common::Array<byte> &data) {
 Common::File file;
 if (!file.open(Common::Path(name)) || file.size() < 1 || file.size() > 4 * 1024 * 1024) return false;
 data.resize(file.size()); return file.read(data.data(), data.size()) == data.size();
}
Common::String stepStartupScripts(ScriptMachine &machine, unsigned tick) {
 machine.tick(); unsigned waiting = 0, stopped = 0;
 for (unsigned i = 0; i < machine.count(); ++i) {
  const ScriptTask &task = *machine.task(i);
  if (task.result >= kTimerWait && task.result <= kRegionWait) ++waiting;
  else if (task.result >= kUnknownWait) ++stopped;
  debug("[sotc-vm] tick=%u task=%u pc=%u op=%u flags=%02x result=%u executed=%u", tick, i, task.pc, task.opcode, task.flags, unsigned(task.result), task.executed);
 }
 return Common::String::format("Script tick %u: %u waiting, %u stopped", tick, waiting, stopped);
}
bool StartupScene::load(Common::Array<byte> &pixels, Common::String &status, ScriptMachine &machine) {
 pixels.clear(); status = "INIT.SAV unavailable";
 Common::Array<byte> save, scripts;
 machine.stopSpeech();
 _textFile.clear(); _dialogueText.clear(); _talking=false;
 _portrait=false; _restoreTalk=false; _portraitHold=_portraitSequence=0;
 _speaker=_speechText=_talkSequence=_talkStep=_talkGate=0;
 cancelFade();
 _ready = false; _machine=&machine; cancelRoute();
 StartState &state = _state;
 if (!readFile("INIT.SAV", save) || !state.load(Cc4View(save.data(), save.size()))) return false;
 unsigned bank = state.word(0x2df2), blockIndex = state.word(0x2df8), background = state.word(0x2e14);
 Common::String roomName = Common::String::format("R%02u.CC4", bank);
 Cc4View block;
 if (!readFile(roomName.c_str(), scripts) || !cc4Entry(Cc4View(scripts.data(), scripts.size()), 4, blockIndex, block) ||
     cc4Count(block, 2) != state.word(0x2e02)) return false;
 if (!machine.load(state, block)) return false;
 // Verify every rebased PC before rendering any of this snapshot.
 for (unsigned i = 0; i < state.word(0x2e02); ++i) {
  uint32_t pc;
  if (!state.taskPC(block, i, pc)) return false;
  debug("[sotc-init-task] task=%u pc=%u actor=%u flags=%02x", i, pc, state.byte(0x3118 + i * 22), state.byte(0x3119 + i * 22));
 }
 Pak rooms, actors, resources;
 Common::Array<byte> &scenery = _scenery;
 Common::Array<byte> *actorData = _actorData;
 for (unsigned i=0;i<20;++i) actorData[i].clear();
 if (!rooms.open(Common::String::format("D%02u.PAK", bank)) || !actors.open(Common::String::format("A%02u.PAK", bank)) ||
     !resources.open("RES.PAK") || !rooms.load(background, pixels) || pixels.size() != 64000 || !rooms.load(background + 1, scenery)) {
  pixels.clear(); return false;
 }
 for (unsigned i = 0; i < 20; ++i) {
  unsigned slot = 0x3286 + i * 6, type = state.byte(slot);
  _types[i]=type; _indices[i]=state.byte(slot+1);
  // Resource ID/type metadata is portable; the serialized far pointer is not.
  bool occupied = state.word(slot + 2) || state.word(slot + 4);
  if (!occupied) continue;
  if (type == 0) actors.load(state.byte(slot + 1), actorData[i]);
  else if (type == 1) resources.load(2, actorData[i]); // 0x6cb5 -> 0x609e, initialized at 0xc96.
  else if (type == 2) actorData[i] = scenery;
 }
 _background = pixels;
 for (unsigned i=0;i<11;++i) {
  unsigned a=0x2e54+i*52;
  _moves[i].active=false; _moves[i].x=_moves[i].y=0;
  _collisionBypass[i]=state.byte(a+23)==8;
  _inDrawList[i]=state.byte(a+50)!=0;
  _slots[i]=state.byte(a+11);
  _clips[i][0]=state.word(a+44); _clips[i][1]=state.byte(a+48);
  _clips[i][2]=state.word(a+46); _clips[i][3]=state.byte(a+49);
  _visible[i] = state.word(a+26)!=0;
  _animations[i] = {state.word(a+12), state.word(a+16), state.word(a+6), state.byte(a+20), state.word(a+4), state.word(a+8)};
 }
 _inputLocks=state.word(0x2e28);
 _walkDirection=state.word(0x2e5c); _walking=false;
 _ready=true;
 return render(pixels,status,machine);
}
bool StartupScene::background(unsigned index) {
 Pak rooms; Common::Array<byte> pixels,scenery;
 if(!rooms.open(Common::String::format("D%02u.PAK",_state.word(0x2df2))) ||
    !rooms.load(index*2,pixels) || pixels.size()!=64000 || !rooms.load(index*2+1,scenery)) return false;
 const uint8_t *layers; unsigned count;
 if(!va2Record(scenery.data(),scenery.size(),0,0,layers,count) || count>20) return false;
 _state.setWord(0x2e14,index*2); _state.setByte(0x34de,count);
 for(unsigned i=0;i<count;++i) { _state.setByte(0x34e0+i*2,layers[i*8+6]); _state.setByte(0x34e1+i*2,0x10); }
 _background=pixels; _scenery=scenery;
 for(unsigned i=0;i<20;++i) if(_types[i]==2) _actorData[i]=scenery;
 return true;
}
bool StartupScene::object(unsigned id,unsigned x,unsigned y) {
 for(unsigned i=0;i<20;++i) {
  unsigned a=0x32fe + i*8;
  if(_state.byte(a+2)) continue;
  _state.setWord(a,id);_state.setByte(a+2,1);_state.setByte(a+3,0);
  _state.setWord(a+4,x);_state.setWord(a+6,y);return true;
 }
 return false;
}
bool StartupScene::removeObject(unsigned id) {
 for(unsigned i=0;i<20;++i) {
  unsigned a=0x32fe + i*8;
  if(_state.word(a)==id) _state.setByte(a+2,0);
 }
 return true;
}
bool StartupScene::obstacle(const unsigned *rectangle) {
 unsigned count=_state.byte(0x3542),address=0x3544+count*4;
 // The obstacle area ends where serialized local variables begin.
 if(count>=249 || address+4>0x392a) return false;
 for(unsigned i=0;i<4;++i) _state.setByte(address+i,rectangle[i]/(i%2 ? 1 : 2));
 return _state.setByte(0x3542,count+1);
}
bool StartupScene::terrain(Cc4View data) {
 // Original 0x104bc: integer line rasterization, including vertical segments.
 if(data.size<4 || data.size%2 || data.size>128 || data.data[0]!=0) return false;
 unsigned output=0;
 for(unsigned p=0;p+3<data.size;p+=2) {
  int dx=int(data.data[p+2])*2-int(data.data[p])*2;
  int y=data.data[p+1], dy=int(data.data[p+3])-y, sign=dy<0 ? -1 : 1;
  if(dx<0) return false;
  if(!dx) continue;
  if(dy<0) dy=-dy;
  int error=(dx>dy ? dx : dy)/2;
  if(dx>dy) for(int i=0;i<dx;++i) {
   if(output>=320) return false;
   _state.setByte(0x339e + output++,y); error+=dy;
   if(error>=dx) { error-=dx; y+=sign; }
  } else for(int i=0;i<dy;++i) {
   y+=sign; error+=dx;
   if(error>=dy) { error-=dy; if(output>=320) return false; _state.setByte(0x339e + output++,y); }
  }
 }
 while(output<320) _state.setByte(0x339e + output++,data.data[data.size-1]);
 return true;
}
bool StartupScene::exits(Cc4View data) {
 if(data.size%5 || data.size/5>16) return false;
 _state.setWord(0x2dfc,data.size/5);
 for(unsigned i=0;i<data.size/5;++i) {
  const uint8_t *p=data.data+i*5; unsigned a=0x3090+i*8;
  _state.setByte(a,p[0]); _state.setByte(a+1,p[1]); _state.setByte(a+2,p[2]); _state.setByte(a+3,0);
  _state.setWord(a+4,p[3]*2); _state.setWord(a+6,p[4]*2);
 }
 return true;
}
bool StartupScene::roomSnapshot(Snapshot &s,ScriptMachine &machine) {
 if(!_state.snapshot(s)) return false;
 s.value(_brightness,255); s.value(_fadeTarget,255); s.value(_fadeStep,255);
 s.require(_brightness==_fadeTarget || _fadeStep!=0);
 if(!s.ok() || !s.reading()) return s.ok();
 Common::Array<byte> scripts; Cc4View block;
 if(!readFile(Common::String::format("R%02u.CC4",_state.word(0x2df2)).c_str(),scripts) ||
    !cc4Entry(Cc4View(scripts.data(),scripts.size()),4,room(),block) ||
    !machine.enterRoom(block,_state.word(0x2df6),_state.word(0x2df0))) return false;
 // Restore the exact draw list after loading its external background resource.
 StartState saved=_state;
 if(!background(_state.word(0x2e14)/2)) return false;
 _state=saved; return true;
}
bool StartupScene::serviceRoom(ScriptMachine &machine,Common::String &status) {
 for(unsigned i=0;i<machine.count();++i) if(machine.task(i)->result>=kUnknownWait) {
  status=Common::String::format("Room %u script %u stopped: opcode %u at %u",room(),i,machine.task(i)->opcode,machine.task(i)->pc);
  cancelFade();return false;
 }
 if(fading()) {
  if(_brightness<_fadeTarget) _brightness=(_fadeTarget-_brightness<=_fadeStep) ? _fadeTarget : _brightness+_fadeStep;
  else _brightness=(_brightness-_fadeTarget<=_fadeStep) ? _fadeTarget : _brightness-_fadeStep;
  if(fading()) return true;
 }
 if(machine.pendingRoom()==255) return true;
 unsigned destination=machine.pendingRoom(),destinationBank=machine.pendingBank()==255 ? bank() : machine.pendingBank();
 Common::Array<byte> scripts; Cc4View block;
 if(!readFile(Common::String::format("R%02u.CC4",destinationBank).c_str(),scripts) ||
    !cc4Entry(Cc4View(scripts.data(),scripts.size()),4,destination,block)) {
  status="Room script unavailable"; cancelFade(); return false;
 }
 // Initialize a candidate so a missing background/actor cannot destroy the live room.
 ScriptMachine candidate; candidate.adopt(machine);
 StartupScene next=*this; next._machine=&candidate; candidate.setAnimation(&next);
 if(!candidate.enterRoom(block,room(),_state.word(0x2df2))) { status="Invalid room script"; cancelFade(); return false; }
 next._state.setWord(0x2df6,room()); next._state.setWord(0x2df0,_state.word(0x2df2));
 next._state.setWord(0x2df2,destinationBank);
 if(destinationBank!=bank()) {
  // Axx indices are local to a bank. Keep only global RES resources; a
  // cached A00 index must never masquerade as the same index in A01.
  for(unsigned i=0;i<20;++i) if(next._types[i]==0) next._actorData[i].clear();
  unsigned playerSlot=next._slots[0];
  if(playerSlot>=20) {status="Player resource unavailable";cancelFade();return false;}
  if(next._types[playerSlot]==0) {
   Pak actors;
   if(!actors.open(Common::String::format("A%02u.PAK",destinationBank)) || !actors.load(next._indices[playerSlot],next._actorData[playerSlot])) {
    status="Player bank resource unavailable";cancelFade();return false;
   }
  }
 }
 next._state.setWord(0x2df8,destination); next._state.setWord(0x2e02,candidate.count());
 next._state.setWord(0x3542,0); next._state.setWord(0x2dfc,0);
 for(unsigned i=0;i<20;++i) next._state.setByte(0x3300+i*8,0);
 next.cancelRoute(); next._walking=false; next._inputLocks=0; next._talking=false; next._portrait=false;
 next._restoreTalk=false; next._dialogueText.clear();
 for(unsigned i=0;i<11;++i) { next._moves[i].active=false; next._collisionBypass[i]=false; if(i) next._visible[i]=false; }
 next._inDrawList[0]=true;
 TaskResult result=candidate.visit(0);
 if(result>=kUnknownWait) {
  status=Common::String::format("Room %u init stopped: opcode %u at %u",destination,candidate.task(0)->opcode,candidate.task(0)->pc);
  debug("[sotc-room] %s",status.c_str()); cancelFade(); return false;
 }
 // Original 0x7e48 positions the player AFTER room initialization, even
 // when the cart sequence hides him. Keeping the previous coordinates can
 // spuriously trigger a door region in the new room.
 unsigned direction=1; int left=160,right=160,top=190,bottom=190;
 const unsigned entranceDirection[5]={0,3,4,1,2};
 for(unsigned i=0;i<next._state.word(0x2dfc);++i) {
  unsigned a=0x3090+i*8,type=next._state.byte(a);
  if(next._state.byte(a+2)!=room() || next._state.byte(a+1)!=_state.word(0x2df2)) continue;
  if(type<1 || type>4) { status="Invalid room entrance"; cancelFade(); return false; }
  direction=entranceDirection[type];
  if(direction!=next._animations[0].direction) continue;
  left=next._state.word(a+4);right=next._state.word(a+6);
  if(left>319 || right>319) { status="Invalid room entrance bounds"; cancelFade(); return false; }
  top=next._state.byte(0x339e + left);
  bottom=left==right ? 199 : int(next._state.byte(0x339e + right));
  if(left!=right && type==3) top=bottom=199;
  if(top>bottom) { int temp=top;top=bottom;bottom=temp; }
  break;
 }
 ScriptActor player=*candidate.actor(0);player.x=(left+right)/2;player.y=(top+bottom)/2;
 if(!next.sequence(0,direction-1,player)) { status="Player entrance sequence unavailable"; cancelFade(); return false; }
 next._animations[0].mode=0;next._animations[0].base=0;next._animations[0].direction=direction;
 next._walkDirection=direction;candidate.setActor(0,player);
 Common::Array<byte> preview; Common::String detail;
 if(!next.render(preview,detail,candidate)) {
  status=Common::String::format("Room %u cannot render: %s",destination,detail.c_str()); cancelFade(); return false;
 }
 machine.stopSpeech(); machine.adopt(candidate); adopt(next,machine);
 debug("[sotc-room] entered bank=%u room=%u",_state.word(0x2df2),room());
 return true;
}
bool StartupScene::snapshot(Snapshot &s,bool reloadResources) {
 s.require(_ready);
 for(unsigned i=0;i<11;++i) {
  s.value(_moves[i].active); s.value(_moves[i].x); s.value(_moves[i].y);
  Animation &a=_animations[i];
  s.value(a.sequence,65535); s.value(a.substep,65535); s.value(a.mode,2);
  s.value(a.gate,255); s.value(a.base,65535); s.value(a.direction,4);
  s.value(_slots[i],255); s.value(_visible[i]); s.value(_inDrawList[i]); s.value(_collisionBypass[i]);
  for(unsigned j=0;j<4;++j) s.value(_clips[i][j],65535);
 }
 s.value(_talking); s.value(_portrait); s.value(_restoreTalk);
 s.value(_portraitHold,10); s.value(_portraitSequence,3);
 s.value(_speaker,9); s.value(_speechText,65535); s.value(_talkSequence,65535); s.value(_talkStep,65535); s.value(_talkGate,255);
 s.value(_following); s.value(_scriptRoute); s.value(_inputLocks,15);
 s.value(_walkDirection,4); s.require(_walkDirection!=0); s.value(_walking);
 s.value(_route.count,64000);
 for(unsigned i=0;s.ok() && i<_route.count;++i) s.value(_route.points[i],63999);
 unsigned length=_dialogueText.size(); s.value(length,16384);
 if(s.reading()) _dialogueText.clear();
 for(unsigned i=0;s.ok() && i<length;++i) {
  unsigned c=s.reading() ? 0 : (unsigned char)_dialogueText[i]; s.value(c,255);
  if(s.reading()) _dialogueText+=char(c);
 }
 for(unsigned i=0;i<20;++i) {
  bool occupied=!_actorData[i].empty(); s.value(occupied); s.value(_types[i],255); s.value(_indices[i],65535);
  if(!s.ok()) return false;
  if(s.reading() && reloadResources) {
   _actorData[i].clear();
   if(!occupied) continue;
   Pak pak;
   if(_types[i]==0) {
    if(!pak.open(Common::String::format("A%02u.PAK",_state.word(0x2df2))) || !pak.load(_indices[i],_actorData[i])) return false;
   } else if(_types[i]==1) {
    if(!pak.open("RES.PAK") || !pak.load(2,_actorData[i])) return false;
   } else if(_types[i]==2) _actorData[i]=_scenery;
   else return false;
  }
 }
 return s.ok();
}
bool StartupScene::render(Common::Array<byte> &pixels, Common::String &status, const ScriptMachine &machine) {
 if (!_ready) return false;
 const StartState &state=_state;
 const Common::Array<byte> &scenery=_scenery;
 const Common::Array<byte> *actorData=_actorData;
 pixels=_background;
 PoseRenderer renderer(pixels);
 unsigned rendered = 0, unsupported = 0, sceneryLayer = 0;
 DrawOrder order;
 // Current room scenery depths come from its restored list. Original 0x7228
 // seeds those layers; 0x881d inserts eligible actors in ascending actor ID.
 for (unsigned i=0;i<state.byte(0x34de);++i) {
  unsigned id=state.byte(0x34e1+i*2);
  if ((id & 0xf0)==0x10) order.append(state.byte(0x34e0+i*2),id);
 }
 for (unsigned id=0;id<11;++id)
  if (_visible[id] && _inDrawList[id]) order.insert(machine.actor(id)->y,id);
 const uint8_t *sceneryLayers; unsigned sceneryCount;
 bool haveSceneryPose = va2Record(scenery.data(), scenery.size(), 0, 0, sceneryLayers, sceneryCount);
 for (unsigned i = 0; i < order.count; ++i) {
  unsigned id = order.entries[i].id;
  if ((id & 0xf0) == 0x10) {
   // Scene pose 0 is interleaved with actors by the serialized draw list.
   bool ok = false;
   if (haveSceneryPose && sceneryLayer < sceneryCount) {
    const uint8_t *layer = sceneryLayers + sceneryLayer * 8;
    if (layer[0] == 0 && layer[1] == 1) // Original 0xc806: nested pose.
     ok = renderer.pose(scenery, va2LE16(layer + 2), va2Signed16(layer + 4), va2Signed16(layer + 6));
    else if (layer[0] == 1 && layer[1] == 1)
     ok = renderer.pose(scenery, 0, 0, 0, 0, 0, 319, 199, sceneryLayer);
   }
   ++sceneryLayer;
   if (ok) ++rendered; else ++unsupported;
   continue;
  }
  if (id >= 11) { ++unsupported; continue; }
  if (!_visible[id]) continue;
  unsigned slot = _slots[id];
  if (slot >= 20 || actorData[slot].empty()) { ++unsupported; continue; }
  const Common::Array<byte> &data = actorData[slot];
  const ScriptActor &actor = *machine.actor(id);
  unsigned sequence = _animations[id].sequence, stepIndex = actor.step;
  SequenceStep step;
  int x = actor.x, y = actor.y, mx = 0, my = 0;
  int nextPose = -1; unsigned fraction = 0;
  bool ok = va2Step(data.data(), data.size(), sequence, stepIndex, step);
  if (ok && _animations[id].mode == 2) {
   unsigned mode = step.control >> 14;
   ok = mode <= 1 && va2Position(data.data(), data.size(), sequence, stepIndex, mx, my);
   if (ok && mode == 1) {
    SequenceStep next = step;
    if (stepIndex + 1 < actor.steps) ok = va2Step(data.data(), data.size(), sequence, stepIndex + 1, next);
    unsigned divisor = step.control & 0x3fff;
    fraction = ((_animations[id].substep << 8) & 65535) / (divisor ? divisor : 1);
    nextPose = next.pose;
   }
   x += mx; y += my;
  }
  ok = ok && renderer.pose(data, step.pose, x, y, _clips[id][0], _clips[id][1], _clips[id][2], _clips[id][3], -1, nextPose, fraction);

  if (ok) ++rendered; else ++unsupported;
 }
 status = Common::String::format("Room %u: %u drawn, %u unsupported", room(), rendered, unsupported);
 return unsupported == 0;
}
bool StartupScene::choiceText(unsigned index,Common::String &text) {
 text.clear();
 if(index==65535) return true;
 if(_textFile.empty() && !readFile("E.CC4",_textFile)) return false;
 byte decoded[16384];unsigned size=0;
 if(!dialogueRecord(Cc4View(_textFile.data(),_textFile.size()),_state.word(0x2e16)+3,index,decoded,sizeof(decoded),size)) return false;
 for(unsigned i=1;i<size-(decoded[size-1]==0 ? 1 : 0);++i) text+=decoded[i] ? char(decoded[i]) : ' ';
 return true;
}
bool StartupScene::dialogue(unsigned speaker,unsigned text,unsigned seq,unsigned portrait) {
 if (!_ready || !_machine || speaker>=10 || _talking) return false;
 if (_textFile.empty() && !readFile("E.CC4",_textFile)) return false;
 byte decoded[16384]; unsigned size=0;
 if (!dialogueRecord(Cc4View(_textFile.data(),_textFile.size()),_state.word(0x2e16)+3,text,decoded,sizeof(decoded),size)) return false;
 unsigned id=speaker;
 if (portrait!=256) {
  id=10; ScriptActor actor=*_machine->actor(id);
  if (!reset(id,actor) || !resource(id,portrait)) return false;
  actor.x=0; actor.y=199; _machine->setActor(id,actor);
 }
 ScriptActor actor=*_machine->actor(id);
 _talkSequence=_animations[id].sequence; _talkStep=actor.step; _talkGate=_animations[id].gate;
 if (seq!=255) {
  if (!sequence(id,seq,actor)) return false;
  _machine->setActor(id,actor);
 }
 _dialogueText.clear();
 for (unsigned i=1;i<size-(decoded[size-1]==0 ? 1 : 0);++i) _dialogueText+=decoded[i] ? char(decoded[i]) : '\n';
 _speechText=text; _speaker=speaker; _portrait=portrait!=256; _restoreTalk=seq!=255; _talking=true;
 if (!_machine->speech(_state.word(0x2e16),text)) warning("[sotc-speech] Missing or unsupported recording bank=%u text=%u; retaining subtitles",_state.word(0x2e16),text);
 debug("[sotc-dialogue] actor=%u text=%u seq=%u portrait=%u bytes=%u",speaker,text,seq,portrait,size);
 return true;
}
bool StartupScene::dialoguePending(bool &pending) {
 pending=_talking;
 if (pending) return true;
 if (_portrait) { _visible[10]=false; _inDrawList[_speaker]=true; _portrait=false; }
 else if (_restoreTalk) {
  ScriptActor actor=*_machine->actor(_speaker);
  if (!sequence(_speaker,_talkSequence,actor)) return false;
  actor.step=_talkStep; _animations[_speaker].gate=_talkGate;
  _machine->setActor(_speaker,actor);
 }
 _restoreTalk=false; _dialogueText.clear(); return true;
}
bool StartupScene::face(unsigned id,unsigned direction) {
 if (!_ready || id>=11 || direction>4) return false;
 Animation &a=_animations[id];
 // Opcode 1 clears scripted mode before 0x797b considers a direction change.
 a.mode=0;
 if (direction && direction!=a.direction) { a.direction=direction; a.mode=1; }
 if (!id) _walkDirection=a.direction;
 debug("[sotc-face] actor=%u direction=%u mode=%u",id,a.direction,a.mode);
 return true;
}
bool StartupScene::control(unsigned value,bool unlock,ScriptActor &actor) {
 if (!_ready || value>4) return false;
 if (unlock) {
  _inputLocks=0;
  if (_animations[0].mode==2) _animations[0].mode=0;
 } else if (value) {
  const unsigned masks[5]={0,1,8,2,4};
  _inputLocks|=masks[value];
 } else {
  // Original 18/0 stops actor 0 and disables all direction input.
  unsigned seq=_walkDirection-1,slot=_slots[0];
  if (_animations[0].mode!=2) {
   if (slot>=20) return false;
   unsigned count=va2SequenceLength(_actorData[slot].data(),_actorData[slot].size(),seq);
   if (!count) return false;
   actor.step=0; actor.steps=count; _animations[0].sequence=seq;
   _animations[0].mode=0; _animations[0].gate=255; _animations[0].base=0;
  }
  cancelRoute(); _walking=false; _inputLocks=15;
 }
 debug("[sotc-control] mask=%u unlock=%u",_inputLocks,unsigned(unlock));
 return true;
}
bool StartupScene::walk(unsigned direction, ScriptMachine &machine,bool scripted) {
 if (!_ready || direction>4) return false;
 const unsigned masks[5]={0,1,8,2,4};
 if (!scripted && direction && (_inputLocks & masks[direction])) return true;
 if (_animations[0].mode==2) return false;
 bool moving=direction!=0;
 if (moving==_walking && (!moving || direction==_walkDirection)) return true;
 unsigned facing=moving ? direction : _walkDirection;
 unsigned sequenceIndex=(moving ? 4 : 0)+facing-1;
 unsigned slot=_slots[0];
 if (slot>=20) return false;
 const Common::Array<byte> &data=_actorData[slot];
 unsigned count=va2SequenceLength(data.data(),data.size(),sequenceIndex);
 SequenceStep step;
 if (!count || !va2Step(data.data(),data.size(),sequenceIndex,0,step)) return false;
 ScriptActor actor=*machine.actor(0); actor.step=0; actor.steps=count;
 _animations[0].sequence=sequenceIndex; _animations[0].mode=0; _animations[0].gate=255;
 _animations[0].base=moving ? 4 : 0; _animations[0].direction=facing;
 _walkDirection=facing; _walking=moving;
 machine.setActor(0,actor);
 debug("[sotc-walk] direction=%u moving=%u sequence=%u",facing,unsigned(moving),sequenceIndex);
 return true;
}
unsigned StartupScene::collisionAt(int x,int y,unsigned direction,const ScriptMachine &machine,unsigned self) const {
 const ScriptActor &actor=*machine.actor(self);
 unsigned char terrain[320],obstacles[255*4];
 for (unsigned i=0;i<320;++i) terrain[i]=_state.byte(0x339e + i);
 unsigned count=_state.byte(0x3542);
 if (!_state.range(0x3544,count*4)) return 0xffff;
 for (unsigned i=0;i<count*4;++i) obstacles[i]=_state.byte(0x3544+i);
 Footprint candidate=footprint(x,y,actor.halfWidth,actor.footHeight);
 unsigned collision=terrainCollision(terrain,candidate,direction);
 // Scripted NPC departures may target X=320/324. Retain terrain height
 // checks, but do not turn the viewport edge into a wall on that departure.
 if(self && _moves[self].active && _moves[self].x>=320 && direction==2 && collision==0x100 &&
    candidate.right>=319 && candidate.top>terrain[319]) collision=0;
 if (!collision) collision=obstacleCollision(obstacles,count,candidate);
 if (!collision) {
  CollisionActor actors[11];
  for (unsigned i=0;i<11;++i) {
   const ScriptActor &a=*machine.actor(i);
   actors[i]={footprint(a.x,a.y,a.halfWidth,a.footHeight),_visible[i],_collisionBypass[i]};
  }
  collision=actorCollision(actors,self,candidate);
 }
 return collision;
}
bool StartupScene::destination(int x,int y,ScriptMachine &machine,bool scripted) {
 cancelRoute();
 if (!_ready || (!scripted && _inputLocks) || _animations[0].mode==2) return false;
 walk(0,machine);
 const ScriptActor &a=*machine.actor(0);
 bool found=_route.find(a.x,a.y,x,y,[&](int nx,int ny,unsigned d) { return collisionAt(nx,ny,d,machine)==0; });
 _following=found && _route.count; _scriptRoute=scripted && _following;
 debug("[sotc-route] target=%d,%d found=%u steps=%u",x,y,unsigned(found),_route.count);
 return found;
}
bool StartupScene::move(unsigned id,unsigned x,unsigned y,bool &pending) {
 if (!_ready || !_machine || id>=10) return false;
 if (id) {
  ScriptActor a=*_machine->actor(id);
  if (x>510 || y>255) return false;
  pending=int(x)!=a.x || int(y)!=a.y;
  if (!pending) { _moves[id].active=false; return true; }
  unsigned direction=int(x)>a.x ? 2 : int(x)<a.x ? 4 : int(y)>a.y ? 3 : 1;
  unsigned seq=direction+3,slot=_slots[id];
  if (slot>=20) return false;
  const Common::Array<byte> &data=_actorData[slot];
  unsigned count=va2SequenceLength(data.data(),data.size(),seq); SequenceStep first;
  if (!count || !va2Step(data.data(),data.size(),seq,0,first)) return false;
  Animation &animation=_animations[id];
  animation.sequence=seq; animation.direction=direction; animation.base=4; animation.mode=0; animation.gate=255;
  a.step=0; a.steps=count; _machine->setActor(id,a);
  _moves[id].x=x; _moves[id].y=y; _moves[id].active=true;
  debug("[sotc-actor-move] actor=%u target=%u,%u sequence=%u",id,x,y,seq);
  return true;
 }
 unsigned oldMode=_animations[0].mode; _animations[0].mode=0;
 if (!destination(x,y,*_machine,true)) { _animations[0].mode=oldMode; return false; }
 pending=_following; return true;
}
bool StartupScene::movementPending(unsigned id,bool &pending) {
 if (!_ready || id>=10) return false;
 pending=(id ? _moves[id].active : _following) && _visible[id];
 if (!pending) {
  // Original 0xaf9b selects sequence zero when movement wait completes.
  unsigned slot=_slots[id]; if (slot>=20) return false;
  unsigned count=va2SequenceLength(_actorData[slot].data(),_actorData[slot].size(),0);
  if (!count) return false;
  ScriptActor actor=*_machine->actor(id); actor.step=0; actor.steps=count;
  _animations[id].sequence=0; _animations[id].gate=255; _machine->setActor(id,actor);
 }
 return true;
}
bool StartupScene::moveOthers(ScriptMachine &machine) {
 for (unsigned id=1;id<10;++id) {
  if (!_moves[id].active) continue;
  if (!_visible[id]) { _moves[id].active=false; continue; }
  ScriptActor actor=*machine.actor(id); Animation &a=_animations[id];
  unsigned slot=_slots[id]; if (slot>=20 || a.mode==2) return false;
  const Common::Array<byte> &data=_actorData[slot]; SequenceStep step;
  if (!va2Step(data.data(),data.size(),a.sequence,actor.step,step)) return false;
  int dx=step.dx,dy=step.dy,speed=_state.byte(0x2e2a); if (speed>=128) speed-=256;
  walkDelta(a.direction,speed,_state.signedWord(0x2e24),_state.signedWord(0x2e26),
   _state.word(0x2df8)==_state.word(0x2dfa),dx,dy);
  int delta=(a.direction==2 || a.direction==4) ? dx : dy;
  unsigned budget=unsigned(delta<0 ? -delta : delta);
  while (budget-- && _moves[id].active) {
   int x=actor.x,y=actor.y;
   if (a.direction==1) --y; if (a.direction==2) ++x;
   if (a.direction==3) ++y; if (a.direction==4) --x;
   if (!_collisionBypass[id] && collisionAt(x,y,a.direction,machine,id)) {
    debug("[sotc-actor-move] blocked actor=%u candidate=%d,%d",id,x,y); return false;
   }
   actor.x=x; actor.y=y; machine.setActor(id,actor);
   if ((a.direction==2 || a.direction==4) && x==_moves[id].x && y!=_moves[id].y) {
    // Continue a two-axis scripted destination with its remaining vertical leg.
    // Walking still uses the resource's deltas and collision predicates.
    bool pending=false;
    if(!move(id,_moves[id].x,_moves[id].y,pending)) return false;
    break;
   }
   if (x==_moves[id].x && y==_moves[id].y) {
    _moves[id].active=false; a.base=0;
    debug("[sotc-actor-move] arrived actor=%u position=%d,%d",id,x,y);
   }
  }
 }
 return true;
}
bool StartupScene::movePlayer(ScriptMachine &machine) {
 if (!moveOthers(machine)) return false;
 const unsigned masks[5]={0,1,8,2,4};
 if (!_scriptRoute && _walking && (_inputLocks & masks[_walkDirection])) { cancelRoute(); return walk(0,machine); }
 if (_following) {
  if (!_route.count || _animations[0].mode==2) { cancelRoute(); return walk(0,machine); }
  const ScriptActor &a=*machine.actor(0); unsigned p=_route.points[_route.count-1];
  unsigned d=int(p%320)>a.x ? 2 : int(p%320)<a.x ? 4 : int(p/320)>a.y ? 3 : 1;
  if (!walk(d,machine,_scriptRoute)) { cancelRoute(); return false; }
 }
 if (!_walking) return true;
 if (_animations[0].mode==2) { _walking=false; return true; }
 unsigned slot=_slots[0]; if (slot>=20) return false;
 const Common::Array<byte> &data=_actorData[slot];
 ScriptActor actor=*machine.actor(0); SequenceStep step;
 if (!va2Step(data.data(),data.size(),_animations[0].sequence,actor.step,step)) return false;
 int dx=step.dx,dy=step.dy;
 int speed=_state.byte(0x2e2a); if (speed>=128) speed-=256;
 walkDelta(_walkDirection,speed,_state.signedWord(0x2e24),_state.signedWord(0x2e26),
           _state.word(0x2df8)==_state.word(0x2dfa),dx,dy);
 if (_following) {
  unsigned budget=unsigned(dx<0 ? -dx : dx)+unsigned(dy<0 ? -dy : dy);
  while (budget-- && _route.count) {
   unsigned p=_route.points[_route.count-1]; int x=p%320,y=p/320;
   unsigned d=x>actor.x ? 2 : x<actor.x ? 4 : y>actor.y ? 3 : 1;
   if (d!=_walkDirection) break;
   if (collisionAt(x,y,d,machine)) { cancelRoute(); return walk(0,machine); }
   actor.x=x; actor.y=y; machine.setActor(0,actor); --_route.count;
  }
  if (!_route.count) { debug("[sotc-route] arrived=%d,%d",actor.x,actor.y); cancelRoute(); return walk(0,machine); }
  return true;
 }
 int x=walkWord(actor.x+dx),y=walkWord(actor.y+dy);
 unsigned collision=collisionAt(x,y,_walkDirection,machine);
 // Boundary exits, sliding and automatic detours need the destination system.
 // For manual walking reject the candidate and retain the last valid position.
 if (collision || x<0 || x>319 || y<0 || y>199) {
  debug("[sotc-collision] blocked=%03x candidate=%d,%d retained=%d,%d",collision,x,y,actor.x,actor.y);
  return walk(0,machine);
 }
 actor.x=x; actor.y=y; machine.setActor(0,actor);
 debug("[sotc-walk] x=%d y=%d step=%u",x,y,actor.step);
 return true;
}
bool StartupScene::drawEnabled(unsigned id,unsigned enabled) {
 if (!_ready || id>=11) return false;
 _inDrawList[id]=enabled!=0; return true;
}
bool StartupScene::hide(unsigned id) {
 if (!_ready || id>=11) return false;
 if (id) _visible[id]=false;
 // Resource bytes may remain cached, but hidden actors neither draw nor advance.
 return true;
}
bool StartupScene::reset(unsigned id, ScriptActor &actor) {
 if (!_ready || id>=11) return false;
 if (!id) return true;
 _moves[id].active=false;
 // 0x78c8 clears the actor, selects sequence zero using the transient slot 0,
 // then installs resource slot 255. The following 91/87 bind and start it.
 unsigned count=va2SequenceLength(_actorData[0].data(),_actorData[0].size(),0);
 actor={0,0,4,2,0,count};
 _animations[id]={0,0,1,255,0,1}; _slots[id]=255; _collisionBypass[id]=false; _visible[id]=true; _inDrawList[id]=true;
 _clips[id][0]=_clips[id][1]=0; _clips[id][2]=319; _clips[id][3]=199;
 return true;
}
bool StartupScene::resource(unsigned id, unsigned index) {
 if (!_ready || id>=11) return false;
 for (unsigned slot=0;slot<20;++slot) {
  if (!_actorData[slot].empty() && _types[slot]==0 && _indices[slot]==index) {
   _slots[id]=slot; return true;
  }
 }
 // Keep immutable resource bytes cached; evict only slots without live users.
 for (unsigned slot=0;slot<20;++slot) {
  bool used=false;
  for (unsigned a=0;a<11;++a) if (_visible[a] && _slots[a]==slot) used=true;
  if (used || _types[slot]!=0) continue;
  Pak actors; Common::Array<byte> data;
  if (!actors.open(Common::String::format("A%02u.PAK",_state.word(0x2df2))) || !actors.load(index,data)) return false;
  _actorData[slot]=data; _types[slot]=0; _indices[slot]=index; _slots[id]=slot;
  return true;
 }
 return false;
}
bool StartupScene::sequence(unsigned id, unsigned sequenceIndex, ScriptActor &actor) {
 if (!_ready || id>=11) return false;
 unsigned slot=_slots[id];
 if (slot>=20) return false;
 const Common::Array<byte> &data=_actorData[slot];
 unsigned count=va2SequenceLength(data.data(),data.size(),sequenceIndex);
 SequenceStep first;
 if (!count || !va2Step(data.data(),data.size(),sequenceIndex,0,first)) return false;
 _animations[id].sequence=sequenceIndex; _animations[id].mode=2; _animations[id].gate=255;
 // Original 0x7857 preserves substep; it resets step/count and the advance gate.
 actor.step=0; actor.steps=count;
 debug("[sotc-sequence] actor=%u sequence=%u count=%u",id,sequenceIndex,count);
 return true;
}
unsigned StartupScene::advance(ScriptMachine &machine) {
 if (!_ready) return 0;
 unsigned unsupported=0;
 for (unsigned id=0;id<11;++id) {
  if (!_visible[id]) continue; // Original 0x7d3f update eligibility.
  unsigned slot=_slots[id];
  if (slot>=20) { ++unsupported; continue; }
  const Common::Array<byte> &data=_actorData[slot];
  Animation &animation=_animations[id];
  ScriptActor actor=*machine.actor(id);
  if (animation.mode==1) {
   // 0x7afc transitions after rendering, without also advancing a frame.
   unsigned seq=(animation.base+animation.direction-1)&65535;
   unsigned count=va2SequenceLength(data.data(),data.size(),seq);
   if (!count) { ++unsupported; continue; }
   animation.sequence=seq; animation.mode=0; animation.gate=255;
   actor.step=0; actor.steps=count; machine.setActor(id,actor); continue;
  }
  SequenceStep step; bool wrapped=false;
  if (!va2Step(data.data(),data.size(),animation.sequence,actor.step,step) ||
      !advanceAnimation(step.control,animation.mode,animation.gate,actor.steps,actor.step,animation.substep,&wrapped)) {
   ++unsupported; continue;
  }
  if (id==10 && wrapped && animation.sequence<4) {
   // Text-only branch of original portrait controller 0x7be0. Hold the idle
   // expression for ten loops; otherwise choose one of four mouth sequences.
   if (!_portraitHold) { _portraitSequence=machine.randomRange(4); if (!_portraitSequence) _portraitHold=1; }
   else if (++_portraitHold==10) _portraitHold=0;
   if (!sequence(id,_portraitSequence,actor)) { ++unsupported; continue; }
  }
  machine.setActor(id,actor);
 }
 return unsupported;
}

}
