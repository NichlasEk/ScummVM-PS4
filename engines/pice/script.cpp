// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/script.h"
#include "pice/state.h"
#include "common/endian.h"
namespace Pice {
bool ScriptVariables::syncState(StateIO &io) {
 for(uint i=0;i<_a.size();++i) io.number(_a[i]);
 for(uint i=0;i<_b.size();++i) io.number(_b[i]);
 return io.ok;
}
bool ScriptRunner::syncState(StateIO &io) {
 Common::Array<Task> tasks=_tasks; int exclusive=_exclusiveTask;
 uint count=tasks.size(); io.number(count); io.number(exclusive);
 if(!io.ok || count!=_tasks.size() || exclusive < -1 || exclusive>=int(count)) return false;
 bool locations=!io.loading || io.data.size()==8+count*48;
 if(io.loading && !locations && io.data.size()!=8+count*36) return false;
 for(uint i=0;i<count;++i) {
  Task &t=tasks[i]; io.number(t.pc); io.number(t.active); io.number(t.waiting);
  io.number(t.actor); io.number(t.peer); io.number(t.triggerActor); io.number(t.triggerStep);
  io.number(t.barrierReady); io.number(t.remainingTicks);
  if(locations) { io.number(t.locationActor); io.number(t.locationHotspot); io.number(t.locationInside); }
  else { t.locationActor=t.locationHotspot=-1; t.locationInside=false; }
  if(t.locationActor < -1 || t.locationActor>=20 || t.locationHotspot < -1 || t.locationHotspot>=50 ||
     ((t.locationActor==-1)!=(t.locationHotspot==-1))) return false;
  if(!io.ok || (t.active && !_bank.codeAddress(t.pc)) || t.actor < -27 || t.actor>=20 ||
     t.peer < -1 || t.peer>=int(count) || t.peer==int(i) || t.triggerActor < -1 || t.triggerActor>=20 || t.triggerStep<0) return false;
 }
 if(io.loading) { _tasks=tasks; _exclusiveTask=exclusive; }
 return true;
}
bool ScriptVariables::read(bool second,int16 index,int16 &value) const {
 value=0;
 const Common::Array<int16> &table=second ? _b : _a;
 if (index<0 || uint(index)>=table.size()) return false;
 value=table[index]; return true;
}
bool ScriptVariables::write(bool second,int16 index,int16 value) {
 Common::Array<int16> &table=second ? _b : _a;
 if (index<0 || uint(index)>=table.size()) return false;
 table[index]=value; return true;
}
bool ScriptResource::codeAddress(int64 offset) const {
 return !_entries.empty() && offset>=_entries[0] && !(offset&1) && offset+2<=_data.size();
}

bool ScriptResource::load(const Common::Array<byte> &data) {
 _data.clear(); _entries.clear();
 if (data.size()<4 || data.size()>131072) return false;
 uint count=READ_LE_UINT16(&data[0]);
 // Original scheduler allocates 50 task slots per script bank.
 if (!count || count>50 || count*2>data.size()) return false;
 Common::Array<uint32> entries;
 for (uint i=0;i<count;++i) {
  uint32 offset=uint32(READ_LE_UINT16(&data[i*2]))*2;
  if (offset<count*2 || offset+2>data.size() || (i && offset<entries.back())) return false;
  entries.push_back(offset);
 }
 _data=data; _entries=entries; return true;
}
bool ScriptResource::word(uint32 offset,uint16 &value) const {
 value=0;
 if (offset>_data.size() || _data.size()-offset<2) return false;
 value=READ_LE_UINT16(&_data[offset]); return true;
}
bool ScriptResource::string(uint32 &pc,Common::String &value) const {
 // Original 0x2121 includes NUL, then rounds consumed bytes up to even.
 value.clear(); Common::String decoded; uint32 next=pc;
 while (next<_data.size() && decoded.size()<255) {
  byte c=_data[next++];
  if (!c) {
   if ((next-pc)&1) { if (next>=_data.size()) return false; ++next; }
   pc=next; value=decoded; return true;
  }
  if (c<32 || c>126) return false;
  decoded+=char(c);
 }
 return false;
}
bool ScriptResource::expression(uint32 &pc,int16 &value,const ScriptVariables *variables,const ScriptSceneSink *scene) const {
 uint32 next=pc; uint budget=256; int16 result=0;
 value=0;
 uint32 seed=scene ? scene->randomState() : 0;
 if (!evaluate(next,result,0,budget,variables,scene)) { if(scene) scene->restoreRandomState(seed); return false; }
 pc=next; value=result; return true;
}
bool ScriptResource::evaluate(uint32 &pc,int16 &value,uint depth,uint &budget,const ScriptVariables *variables,const ScriptSceneSink *scene) const {
 if (depth>=32 || !budget) return false;
 --budget;
 uint16 op;
 if (!word(pc,op)) return false;
 pc+=2;
 if (op==20) { // literal, handler 0x2916
  uint16 literal; if (!word(pc,literal)) return false;
  pc+=2; value=int16(literal); return true;
 }
 if (op==5 || op==19) { // table A/B lookup, handlers 0x288f / 0x28ee
  uint16 index;
  if (!variables || !word(pc,index) || !variables->read(op==19,int16(index),value)) return false;
  pc+=2; return true;
 }
 if(op==46) { // 0x2a28: raw word index into runtime context pointer table.
  uint16 index;
  if(!scene || !word(pc,index) || !scene->context(int16(index),value)) return false;
  pc+=2; return true;
 }
 if(op==38) { // 0x2965: random() modulo the evaluated positive bound.
  int16 bound;
  return scene && evaluate(pc,bound,depth+1,budget,variables,scene) && bound>0 && scene->randomValue(bound,value);
 }
 if(op==91) { // 0xb1a: Boolean NOT, not bitwise complement.
  int16 operand;
  if(!evaluate(pc,operand,depth+1,budget,variables,scene)) return false;
  value=operand==0; return true;
 }
 if(op==86) { // 0x8af: two evaluated operands select a runtime state table.
  int16 kind,id;
  return scene && evaluate(pc,kind,depth+1,budget,variables,scene) &&
   evaluate(pc,id,depth+1,budget,variables,scene) && scene->stateValue(kind,id,value);
 }
 // Pure comparisons, eager Boolean operators, and arithmetic only.
 if ((op<7 || op>16) && op!=45) return false;
 int16 a,b;
 if (!evaluate(pc,a,depth+1,budget,variables,scene) || !evaluate(pc,b,depth+1,budget,variables,scene)) return false;
 switch(op) {
 case 45:value=a==b; break; // dispatch 45 -> 0x26de
 case 7: value=a!=b; break;
 case 8: value=a<b; break;
 case 9: value=a>b; break;
 case 10:value=a<=b; break;
 case 11:value=a>=b; break;
 case 12:value=a && b; break;
 case 13:value=a || b; break;
 case 14:value=int16(uint16(int32(a)+b)); break;
 case 15:value=int16(uint16(int32(a)-b)); break;
 case 16:value=int16(uint16(int32(a)*b)); break;
 default:return false;
 }
 return true;
}
// Expression argument layouts are shared by assignments and discarded-result
// statements, so adding a handler cannot accidentally support only one form.
static bool invocationShape(uint16 op,uint &before,uint &after,bool &key) {
 before=after=0; key=false;
 switch(op) {
 case 99: case 124: return true;
 case 68: case 69: case 33: case 118: case 50: case 51: case 60: case 97: case 110: case 114: before=2; return true;
 case 115: case 153: case 82: case 90: case 105: case 67: case 104: case 101: case 83: case 84: case 98: case 56: case 92: case 93: case 100: before=1; return true;
 case 89: case 81: case 58: case 95: before=4; return true;
 case 80: case 109: before=5; return true;
 case 116: case 123: case 135: case 136: before=3; return true;
 case 32: case 29: case 133: after=1; return true;
 case 137: after=2; return true;
 case 77: after=4; return true;
 case 130: before=1; after=5; return true;
 case 96: before=1; after=3; return true;
 case 117: before=1; after=4; return true;
 case 57: case 61: before=2; after=1; key=true; return true;
 case 59: before=5; after=1; key=true; return true;
 default: return false;
 }
}
bool ScriptRunner::value(uint32 &pc,int16 &result) {
 if (_bank.expression(pc,result,_variables,_scene)) return true;
 uint32 next=pc; uint16 op;
 if (!_bank.word(next,op)) return false;
 if(op==23 || op==24) {
  // Original 0xa38/0xaa2: prefix INC/DEC of a raw A/B variable reference.
  uint16 table,index; int16 previous;
  if(!_bank.word(next+2,table) || (table!=5 && table!=19) || !_bank.word(next+4,index) ||
     !_variables->read(table==19,int16(index),previous)) return false;
  result=int16(uint16(int32(previous)+(op==23 ? 1 : -1)));
  if(!_variables->write(table==19,int16(index),result)) return false;
  pc=next+6; return true;
 }
 if (!_scene) return false;
 uint before,after; bool hasKey;
 if(invocationShape(op,before,after,hasKey)) {
  next+=2; Common::String name,key; Common::Array<int16> args; int16 arg;
  for(uint i=0;i<before;++i) { if(!_bank.expression(next,arg,_variables,_scene)) return false; args.push_back(arg); }
  if(after) {
   if(!_bank.string(next,name)) return false;
   for(uint i=0;i<after;++i) { if(!_bank.expression(next,arg,_variables,_scene)) return false; args.push_back(arg); }
   if(hasKey && !_bank.string(next,key)) return false;
  }
  if(!_scene->invoke(op,name,args,key,result)) return false;
  pc=next; return true;
 }
 if(op!=55 && op!=63 && op!=66) return false;
 next+=2; Common::String name; int16 local,sequence=0,node=0;
 if (!_bank.string(next,name) || !_bank.expression(next,local,_variables,_scene) ||
     (op!=66 && !_bank.expression(next,sequence,_variables,_scene)) || (op==55 && !_bank.expression(next,node,_variables,_scene))) return false;
 if (local<0 || sequence<0) return false;
 bool ok=op==55 ? _scene->createActor(name,local,sequence,node,result) : op==63 ? _scene->createItem(name,local,sequence,result) : _scene->createMenu(name,local,result);
 if (!ok) return false;
 pc=next; return true;
}
bool ScriptRunner::resumeStoppedAt(uint index,uint16 expectedOpcode) {
 uint16 opcode;
 if(index>=_tasks.size() || _tasks[index].active || !_bank.word(_tasks[index].pc,opcode) || opcode!=expectedOpcode) return false;
 _tasks[index].active=true; return true;
}
bool ScriptRunner::start(uint index) {
 if (index>=_tasks.size()) return false;
 _tasks[index]=Task(); _tasks[index].pc=_bank.entry(index); _tasks[index].active=true; return true;
}
ScriptRunner::Result ScriptRunner::step(uint index,uint32 elapsedTicks) {
 if (index>=_tasks.size()) return kInvalid;
 if(exclusiveTask()>=0 && exclusiveTask()!=int(index)) return kWaiting;
 Task &task=_tasks[index];
 if (!task.active) return kStopped;
 if(task.triggerActor>=0) {
  if(!_scene || _scene->actorStepReached(task.triggerActor,task.triggerStep)) task.triggerActor=-1;
  return kWaiting;
 }
 if (task.peer>=0) {
  if(task.barrierReady) { task.peer=-1; task.barrierReady=false; }
  else {
   Task &peer=_tasks[task.peer];
   if(peer.active && peer.peer==int(index) && !peer.barrierReady) task.barrierReady=peer.barrierReady=true;
  }
  return kWaiting;
 }
 if(task.actor==-27) {
  if(!_scene || !_scene->narrationBusy()) task.actor=-1;
  return kWaiting;
 }
 if(task.actor<=-2) {
  if(!_scene || !_scene->soundBusy(-task.actor-2)) task.actor=-1;
  return kWaiting;
 }
 if (task.actor>=0) {
  if (!_scene || !_scene->actorBusy(task.actor)) task.actor=-1;
  return kWaiting;
 }
 if (task.waiting) {
  if (task.remainingTicks<=0 || elapsedTicks>=uint32(task.remainingTicks)) {
   task.remainingTicks=0; task.waiting=false;
  } else task.remainingTicks-=elapsedTicks;
  // State 2 returns to the scheduler even on the visit that expires it.
  return kWaiting;
 }
 if(task.locationActor>=0) {
  bool inside;
  if(!_scene || !_scene->actorInsideHotspot(task.locationActor,task.locationHotspot,inside)) { task.active=false; return kUnsupported; }
  if(inside!=task.locationInside) return kWaiting;
  task.locationActor=task.locationHotspot=-1;
 }
 uint16 opcode;
 if (!_bank.word(task.pc,opcode)) { task.active=false; return kInvalid; }
 uint32 next=task.pc;
 uint wrappers=0;
 while(opcode==6) { // 0x26c0 discards the following expression result
  if (++wrappers>32 || !_bank.word(next+2,opcode)) { task.active=false; return kInvalid; }
  next+=2;
 }
 // Speech contention is a scheduler wait, not an unsupported instruction.
 // Keep the PC and all operands untouched until the speech channel is free.
 if((opcode==52 || opcode==111 || opcode==122 || opcode==128 || opcode==129) && _scene && _scene->dialogueBlocked(index)) return kWaiting;
 int16 argument;
 if(opcode==112) {
  next+=2; int16 mode,actor,hotspot; bool inside;
  if(!_bank.expression(next,mode,_variables,_scene) || !_bank.expression(next,actor,_variables,_scene) ||
     !_bank.expression(next,hotspot,_variables,_scene) || actor<0 || actor>=20 || hotspot<0 || hotspot>=50) { task.active=false; return kInvalid; }
  if(!_scene || !_scene->actorInsideHotspot(actor,hotspot,inside)) { task.active=false; return kUnsupported; }
  task.locationActor=actor; task.locationHotspot=hotspot; task.locationInside=mode==0; task.pc=next; return kWaiting;
 }
 if(opcode==131) {
  next+=2;
  if(!_bank.expression(next,argument,_variables,_scene) || argument<0) { task.active=false; return kInvalid; }
  if(!_scene || !_scene->gameOver(argument)) { task.active=false; return kUnsupported; }
  for(uint i=0;i<_tasks.size();++i) _tasks[i].active=false;
  task.pc=next; _exclusiveTask=-1; return kStopped;
 }
 if(opcode==62) {
  next+=2;
  if(!_bank.expression(next,argument,_variables,_scene) || argument<0) { task.active=false; return kInvalid; }
  uint16 branch;
  if(!_bank.word(next,branch) || branch!=4 || !_bank.codeAddress(next+4)) { task.active=false; return kInvalid; }
  bool ready=false;
  if(!_scene || !_scene->pollMenu(argument,ready)) { task.active=false; return kUnsupported; }
  if(!ready) return kWaiting;
  // Dispatcher state 6 (0x5a7) skips the cancel jump after a selection.
  int16 choice; if(!_scene->context(6,choice)) { task.active=false; return kInvalid; }
  task.pc=choice>=0 ? next+4 : next; return kAdvanced;
 }
 if(opcode==79) {
  next+=2; Common::Array<int16> args; Common::String name;
  for(uint i=0;i<3;++i) { if(!_bank.expression(next,argument,_variables,_scene)) { task.active=false; return kInvalid; } args.push_back(argument); }
  if(!_bank.string(next,name)) { task.active=false; return kInvalid; }
  for(uint i=0;i<3;++i) { if(!_bank.expression(next,argument,_variables,_scene)) { task.active=false; return kInvalid; } args.push_back(argument); }
  if(args[5]<-1 || args[5]>=int(_tasks.size()) || args[5]==int(index)) { task.active=false; return kInvalid; }
  int16 actor=-1;
  if(!_scene || !_scene->enterRoom(name,args,actor)) { task.active=false; return kUnsupported; }
  if(actor>=0 && args[5]>=0) {
   start(args[5]); _tasks[args[5]].actor=actor;
   _exclusiveTask=args[5]; _entryInitializer=index;
  }
  task.pc=next; return kAdvanced;
 }
 if(opcode==122) {
  // 0x12a8: position, text resource and key; state 5 waits for text/voice.
  // Unlike actor speech this must not replace or restore an actor animation.
  next+=2; int16 x,y,local; Common::String name,key;
  if(!_bank.expression(next,x,_variables,_scene) || !_bank.expression(next,y,_variables,_scene) ||
     !_bank.string(next,name) || !_bank.expression(next,local,_variables,_scene) || local<0 || !_bank.string(next,key)) {
   task.active=false; return kInvalid;
  }
  if(!_scene || !_scene->narrate(x,y,name,local,key)) { task.active=false; return kUnsupported; }
  task.pc=next; task.actor=-27; return kWaiting;
 }
 if(opcode==111 || opcode==52) {
  next+=2; int16 actor,local; Common::String name,key;
  if(!_bank.expression(next,actor,_variables,_scene) || actor<0 ||
     !_bank.string(next,name) || !_bank.expression(next,local,_variables,_scene) || local<0 || !_bank.string(next,key)) {
   task.active=false; return kInvalid;
  }
  if(!_scene || !(opcode==52 ? _scene->speakDialogue(actor,name,local,key) : _scene->objectDialogue(actor,name,local,key))) { task.active=false; return kUnsupported; }
  task.pc=next; task.actor=actor; return kWaiting;
 }
 if(opcode==128) {
  if(!_scene) { task.active=false; return kUnsupported; }
  next+=2; Common::String animationName,textName,key; Common::Array<int16> args;
  for(uint i=0;i<2;++i) { if(!_bank.expression(next,argument,_variables,_scene)) { task.active=false; return kInvalid; } args.push_back(argument); }
  if(!_bank.string(next,animationName)) { task.active=false; return kInvalid; }
  for(uint i=0;i<3;++i) { if(!_bank.expression(next,argument,_variables,_scene)) { task.active=false; return kInvalid; } args.push_back(argument); }
  if(!_bank.string(next,textName) || !_bank.expression(next,argument,_variables,_scene)) { task.active=false; return kInvalid; }
  args.push_back(argument);
  if(!_bank.string(next,key)) { task.active=false; return kInvalid; }
  int16 actor;
  if(!_scene->closeupDialogue(animationName,textName,key,args,actor)) { task.active=false; return kUnsupported; }
  task.pc=next; task.actor=actor; return kWaiting;
 }
 if(opcode==129) {
  if(!_scene) { task.active=false; return kUnsupported; }
  next+=2; Common::String textName,key,animationName; Common::Array<int16> args;
  if(!_bank.expression(next,argument,_variables,_scene)) { task.active=false; return kInvalid; }
  args.push_back(argument);
  if(!_bank.string(next,textName) || !_bank.expression(next,argument,_variables,_scene)) { task.active=false; return kInvalid; }
  args.push_back(argument);
  if(!_bank.string(next,key) || !_bank.string(next,animationName)) { task.active=false; return kInvalid; }
  for(uint i=0;i<5;++i) {
   if(!_bank.expression(next,argument,_variables,_scene)) { task.active=false; return kInvalid; }
   args.push_back(argument);
  }
  if(!_scene->animatedDialogue(textName,key,animationName,args)) { task.active=false; return kUnsupported; }
  task.pc=next; task.actor=args[0]; return kWaiting;
 }
 if(opcode==98 || opcode==123 || opcode==50 || opcode==51 || opcode==96 || opcode==117) {
  uint32 actorPC=next+2; int16 actor;
  if(!_bank.expression(actorPC,actor,_variables,_scene) || actor<0) { task.active=false; return kInvalid; }
  if(!value(next,argument)) { task.active=false; return kUnsupported; }
  task.pc=next; task.actor=actor; return kWaiting;
 }
 uint before,after; bool hasKey;
 if(opcode==23 || opcode==24 || opcode==55 || opcode==63 || opcode==66 || invocationShape(opcode,before,after,hasKey)) {
  if(!value(next,argument)) { task.active=false; return kUnsupported; }
  task.pc=next; return kAdvanced;
 }
 if(opcode==126 || opcode==127) {
  // Both original dispatch entries point to 0x1619: advance two bytes,
  // return zero, with no other effect in this executable.
  task.pc=next+2; return kAdvanced;
 }
 if(opcode==134) {
  next+=2;
  if(!_bank.expression(next,argument,_variables,_scene) || argument<0 || argument>=25) { task.active=false; return kInvalid; }
  Common::Array<int16> args; args.push_back(argument); int16 result;
  if(!_scene || !_scene->invoke(100,Common::String(),args,Common::String(),result)) { task.active=false; return kUnsupported; }
  task.pc=next; task.actor=-argument-2; return kWaiting;
 }
 if(opcode==147) {
  next+=2; Common::Array<int16> args;
  for(uint n=0;n<4;++n) { if(!_bank.expression(next,argument,_variables,_scene)) { task.active=false; return kInvalid; } args.push_back(argument); }
  if(args[0]<0 || args[2]<0 || args[3]<0 || uint(args[3])>=_tasks.size() || uint(args[3])==index) { task.active=false; return kInvalid; }
  int16 result;
  if(!_scene || !_scene->invoke(opcode,Common::String(),args,Common::String(),result)) { task.active=false; return kUnsupported; }
  start(args[3]); _tasks[args[3]].triggerActor=args[0]; _tasks[args[3]].triggerStep=args[2];
  task.actor=args[0]; task.pc=next; return kWaiting;
 }
 if(opcode==142) {
  if(!_scene) { task.active=false; return kUnsupported; }
  next+=2;
  if(!_bank.expression(next,argument,_variables,_scene) || argument<0) { task.active=false; return kInvalid; }
  Common::Array<int16> args; args.push_back(argument); int16 result;
  if(!_scene->invoke(opcode,Common::String(),args,Common::String(),result)) { task.active=false; return kUnsupported; }
  task.pc=next; task.actor=argument; return kWaiting;
 }
 if(opcode==152) {
  next+=2; Common::String name,key; int16 local;
  if(!_bank.string(next,name) || !_bank.expression(next,local,_variables,_scene) || !_bank.string(next,key)) { task.active=false; return kInvalid; }
  if(!_scene || !_scene->requestSave(name,local,key)) { task.active=false; return kUnsupported; }
  task.pc=next; return kAdvanced;
 }
 if(opcode==138) {
  next+=2; Common::String name; int16 clearBefore,clearAfter;
  if(!_bank.string(next,name) || !_bank.expression(next,clearBefore,_variables,_scene) || !_bank.expression(next,clearAfter,_variables,_scene)) { task.active=false; return kInvalid; }
  if(!_scene || !_scene->playMovie(name,clearBefore,clearAfter)) { task.active=false; return kUnsupported; }
  task.pc=next; return kAdvanced;
 }
 if(opcode==34) {
  if(!_scene) { task.active=false; return kUnsupported; }
  next+=2; int16 chapter,room;
  if(!_bank.expression(next,chapter,_variables,_scene) || !_bank.expression(next,room,_variables,_scene)) { task.active=false; return kUnsupported; }
  if(!_scene->changeRoom(chapter,room)) { task.active=false; return kUnsupported; }
  task.pc=next;
  for(uint i=0;i<_tasks.size();++i) _tasks[i].active=false;
  _exclusiveTask=-1; return kStopped;
 }
 if(opcode==41 || opcode==42) {
  task.pc=next+2; _exclusiveTask=opcode==41 ? int(index) : -1; return kAdvanced;
 }
 if (opcode==44) { // 0x223e / dispatcher states 3 and 8: mutual rendezvous
  next+=2;
  if(!_bank.expression(next,argument,_variables,_scene) || argument<0 || uint(argument)>=_tasks.size() || uint(argument)==index) {
   task.active=false; return kInvalid;
  }
  task.pc=next; task.peer=argument; return kWaiting;
 }
 if (opcode==22) { // 0x266e: signed expression -> state 2 countdown
  next+=2;
  if (!_bank.expression(next,argument,_variables,_scene)) { task.active=false; return kInvalid; }
  task.pc=next; task.remainingTicks=argument; task.waiting=true;
  return kWaiting;
 }
 if(opcode==102 || opcode==85) {
  next+=2; int16 kind,id,enabled;
  if(!_bank.expression(next,kind,_variables,_scene)) { task.active=false; return kInvalid; }
  if(opcode==102) {
   if(kind!=0 && kind!=1) { task.active=false; return kInvalid; }
   _navigation.generatedEnabled=kind==0;
  } else {
   if(!_bank.expression(next,id,_variables,_scene) || !_bank.expression(next,enabled,_variables,_scene) ||
      !_navigation.setEnabled(kind,id,enabled)) { task.active=false; return kInvalid; }
  }
  task.pc=next; return kAdvanced;
 }
 if (opcode==47 || opcode==48 || opcode==49) {
  uint16 args[5]; uint count=opcode==49 ? 5 : 4; next+=2;
  for(uint i=0;i<count;++i) {
   if (!_bank.word(next,args[i])) { task.active=false; return kInvalid; }
   next+=2;
  }
  bool valid=opcode==47 ? _navigation.node(int16(args[0]),int16(args[1]),int16(args[2]),args[3]) :
     opcode==48 ? _navigation.edge(int16(args[0]),int16(args[1]),int16(args[2]),byte(args[3])) :
     _navigation.triangle(int16(args[0]),int16(args[1]),int16(args[2]),int16(args[3]),byte(args[4]));
  if (!valid) { task.active=false; return kInvalid; }
  task.pc=next; return kAdvanced;
 }
 if (opcode==30) {
  if (!_scene) { task.active=false; return kUnsupported; }
  next+=2; int16 value;
  if (!_bank.expression(next,argument,_variables,_scene) || !_bank.expression(next,value,_variables,_scene)) { task.active=false; return kInvalid; }
  if (!_scene->volume(argument,value)) { task.active=false; return kUnsupported; }
  task.pc=next; return kAdvanced;
 }
 if (opcode==1 || opcode==27 || opcode==78) {
  if (!_scene) { task.active=false; return kUnsupported; }
  next+=2; Common::String name; int16 flags=0;
  if (!_bank.string(next,name) || !_bank.expression(next,argument,_variables,_scene) ||
      (opcode==1 && !_bank.expression(next,flags,_variables,_scene))) {
   task.active=false; return kInvalid;
  }
  // 0x2187 -> 0x1c88b removes a filename extension; the appended suffix
  // is empty in this edition. Resource resolution belongs to the scene sink.
  for (int i=int(name.size())-1;i>=0;--i) {
   if (name[i]=='\\') break;
   if (name[i]=='.') { name=Common::String(name.c_str(),i); break; }
  }
  if (name.empty() || argument<0) { task.active=false; return kInvalid; }
  bool applied=opcode==1 ? _scene->background(name,argument,flags) : opcode==27 ? _scene->palette(name,argument) : _scene->scenery(name,argument);
  if (!applied) { task.active=false; return kUnsupported; }
  task.pc=next; return kAdvanced;
 }
 if (opcode==3 || opcode==31) {
  next+=2;
  if (!_bank.expression(next,argument,_variables,_scene) || argument<0 || uint(argument)>=_tasks.size()) {
   task.active=false; return kInvalid;
  }
  task.pc=next;
  // Original handlers 0x23a2 / 0x23c8 call task init / teardown. Only PC
  // and activity are modelled here; no actor/audio resources are attached.
  if (opcode==3) start(argument);
  else _tasks[argument].active=false;
  return task.active ? kAdvanced : kStopped;
 }
 if (opcode==25 || opcode==26) {
  uint16 destination;
  next+=2;
  if (!_bank.word(next,destination)) { task.active=false; return kInvalid; }
  next+=2;
  int16 oldValue;
  if (!_variables->read(opcode==26,int16(destination),oldValue) || !value(next,argument) || !_variables->write(opcode==26,int16(destination),argument)) {
   task.active=false; return kInvalid;
  }
  task.pc=next; return kAdvanced;
 }
 if (opcode==4 || opcode==21) {
  bool jump=true;
  if (opcode==21) {
   next+=2;
   if (!_bank.expression(next,argument,_variables,_scene)) { task.active=false; return kInvalid; }
   uint16 following;
   if (!_bank.word(next,following) || following!=4) { task.active=false; return kInvalid; }
   jump=argument==0;
  }
  uint16 displacement;
  if (!_bank.word(next+2,displacement)) { task.active=false; return kInvalid; }
  int64 target=int64(next)+4+(jump ? int64(int16(displacement))*2 : 0);
  if (!_bank.codeAddress(target)) { task.active=false; return kInvalid; }
  task.pc=uint32(target); return kAdvanced;
 }
 if (opcode==5 || opcode==19 || opcode==20 || opcode==45 || (opcode>=7 && opcode<=16)) {
  if (!_bank.expression(next,argument,_variables,_scene)) { task.active=false; return kInvalid; }
  task.pc=next; return kAdvanced;
 }
 task.active=false;
 return kUnsupported;
}
ScriptRunner::Result ScriptRunner::run(uint index,uint instructionBudget) {
 if (index>=_tasks.size()) return kInvalid;
 if (!running(index)) return kStopped;
 for(uint i=0;i<instructionBudget;++i) {
  Result result=step(index);
  if (result!=kAdvanced) return result;
 }
 return kBudgetExhausted;
}
bool runScriptTests() {
 // Prefix updates preserve 16-bit wraparound and return the new value.
 for(uint table : {5U,19U}) for(uint op : {23U,24U}) {
  const uint16 update[]={1,25,2,uint16(op),uint16(table),1,31,20,0};
  Common::Array<byte> bytes; bytes.resize(sizeof(update));
  for(uint i=0;i<sizeof(update)/2;++i) WRITE_LE_UINT16(&bytes[i*2],update[i]);
  ScriptResource bank; if(!bank.load(bytes)) return false;
  ScriptRunner runner(bank); runner.variables().write(table==19,1,op==23 ? 32767 : -32768); runner.start(0);
  int16 changed,result;
  if(runner.step(0)!=ScriptRunner::kAdvanced || runner.position(0)!=12 ||
     !runner.variables().read(table==19,1,changed) || changed!=(op==23 ? -32768 : 32767) ||
     !runner.variables().read(false,2,result) || result!=changed || runner.step(0)!=ScriptRunner::kStopped) return false;
  WRITE_LE_UINT16(&bytes[10],65535);
  ScriptResource invalid; if(!invalid.load(bytes)) return false;
  ScriptRunner rejected(invalid); rejected.start(0);
  if(rejected.step(0)!=ScriptRunner::kInvalid || rejected.position(0)!=2 ||
     !rejected.variables().read(false,2,result) || result!=0) return false;
 }
 // Two entries. Entry 0: (3 + -5) * 7; entry 1: literal 9.
 const uint16 words[]={2,10,16,14,20,3,20,0xfffb,20,7,20,9};
 Common::Array<byte> data; data.resize(sizeof(words));
 for(uint i=0;i<sizeof(words)/sizeof(words[0]);++i) WRITE_LE_UINT16(&data[i*2],words[i]);
 ScriptResource script; int16 value; uint32 pc;
 if (!script.load(data) || script.count()!=2 || script.entry(0)!=4 || script.entry(1)!=20) return false;
 pc=script.entry(0);
 if (!script.expression(pc,value) || value!=-14 || pc!=20) return false;
 pc=script.entry(1); if (!script.expression(pc,value) || value!=9 || pc!=24) return false;
 pc=23; if (script.expression(pc,value) || pc!=23) return false;
 Common::Array<byte> bad=data; WRITE_LE_UINT16(&bad[2],100);
 if (script.load(bad) || script.count()) return false;
 bad=data; WRITE_LE_UINT16(&bad[0],51); if (script.load(bad)) return false;
 bad=data; WRITE_LE_UINT16(&bad[4],65535);
 if (!script.load(bad)) return false;
 pc=4; if (script.expression(pc,value) || pc!=4) return false;
 // Verify every supported binary opcode, including signed comparison and
 // 16-bit wraparound, against independent expected values.
 const uint16 operands[][2]={{0xffff,2},{2,2},{0,7},{0x7fff,2}};
 const int16 expected[][10]={{1,1,0,1,0,1,1,1,-3,-2},
                            {0,0,0,1,1,1,1,4,0,4},
                            {1,1,0,1,0,0,1,7,-7,0},
                            {1,0,1,0,1,1,1,-32767,32765,-2}};
 for(uint row=0;row<4;++row) for(uint op=7;op<=16;++op) {
  Common::Array<byte> expression; expression.resize(12);
  const uint16 program[]={1,uint16(op),20,operands[row][0],20,operands[row][1]};
  for(uint i=0;i<6;++i) WRITE_LE_UINT16(&expression[i*2],program[i]);
  if (!script.load(expression)) return false;
  pc=2;
  if (!script.expression(pc,value) || value!=expected[row][op-7] || pc!=12) return false;
 }
 // Recursive malformed expression cannot overflow the native stack.
 bad.resize(202,0); WRITE_LE_UINT16(&bad[0],1);
 for(uint i=2;i<202;i+=2) WRITE_LE_UINT16(&bad[i],14);
 if (!script.load(bad)) return false;
 pc=2; if (script.expression(pc,value) || pc!=2) return false;
 // Bank: task 0 starts task 1, then stops itself. Task 1 stops itself.
 const uint16 control[]={2,8,3,20,1,31,20,0,31,20,1};
 Common::Array<byte> bytes; bytes.resize(sizeof(control));
 for(uint i=0;i<sizeof(control)/sizeof(control[0]);++i) WRITE_LE_UINT16(&bytes[i*2],control[i]);
 if (!script.load(bytes)) return false;
 ScriptRunner runner(script);
 if (runner.start(2) || !runner.start(0) || runner.running(1) ||
     runner.step(0)!=ScriptRunner::kAdvanced || !runner.running(1) || runner.position(0)!=10 ||
     runner.step(0)!=ScriptRunner::kStopped || runner.running(0) ||
     runner.step(1)!=ScriptRunner::kStopped || runner.running(1)) return false;
 // Unknown commands cannot silently advance into arbitrary scene data.
 WRITE_LE_UINT16(&bytes[4],65535);
 ScriptResource unknown; if (!unknown.load(bytes)) return false;
 ScriptRunner stopped(unknown); stopped.start(0);
 if (stopped.step(0)!=ScriptRunner::kUnsupported || stopped.running(0) || stopped.position(0)!=4) return false;
 WRITE_LE_UINT16(&bytes[4],3); WRITE_LE_UINT16(&bytes[8],2);
 ScriptResource invalid; if (!invalid.load(bytes)) return false;
 ScriptRunner rejected(invalid); rejected.start(0);
 if (rejected.step(0)!=ScriptRunner::kInvalid || rejected.running(0)) return false;
 // Loop increments A[0] three times, takes both conditional paths, and
 // copies the result into the last B slot before stopping its own entry.
 const uint16 loop[]={1,25,0,20,0,25,0,14,5,0,20,1,21,8,5,0,20,3,4,2,4,0xffef,26,299,5,0,31,20,0};
 bytes.resize(sizeof(loop));
 for(uint i=0;i<sizeof(loop)/sizeof(loop[0]);++i) WRITE_LE_UINT16(&bytes[i*2],loop[i]);
 ScriptResource looping; if (!looping.load(bytes)) return false;
 ScriptVariables variables; ScriptRunner machine(looping,&variables);
 if (!variables.read(false,0,value) || value!=-1 || !variables.read(true,299,value) || value!=-1 || !machine.start(0) ||
     machine.run(0,2)!=ScriptRunner::kBudgetExhausted || !machine.running(0) ||
     !variables.read(false,0,value) || value!=1 || machine.run(0,100)!=ScriptRunner::kStopped ||
     !variables.read(false,0,value) || value!=3 || !variables.read(true,299,value) || value!=3) return false;
 if (variables.read(false,-1,value) || variables.read(false,250,value) || variables.write(true,300,7)) return false;
 // Invalid RHS does not partially update the destination variable or PC.
 const uint16 invalidWrite[]={1,25,0,14,20,1,5,250};
 bytes.resize(sizeof(invalidWrite));
 for(uint i=0;i<sizeof(invalidWrite)/sizeof(invalidWrite[0]);++i) WRITE_LE_UINT16(&bytes[i*2],invalidWrite[i]);
 ScriptResource writeBank; if (!writeBank.load(bytes)) return false;
 ScriptRunner failedWrite(writeBank,&variables); failedWrite.start(0);
 if (failedWrite.step(0)!=ScriptRunner::kInvalid || failedWrite.position(0)!=2 ||
     !variables.read(false,0,value) || value!=3) return false;
 // An infinite backward jump yields at the instruction budget and can resume.
 bytes.resize(6); WRITE_LE_UINT16(&bytes[0],1); WRITE_LE_UINT16(&bytes[2],4); WRITE_LE_UINT16(&bytes[4],0xfffe);
 ScriptResource forever; if (!forever.load(bytes)) return false;
 ScriptRunner bounded(forever); bounded.start(0);
 if (bounded.run(0,32)!=ScriptRunner::kBudgetExhausted || !bounded.running(0) || bounded.position(0)!=2 ||
     bounded.run(0,0)!=ScriptRunner::kBudgetExhausted) return false;
 WRITE_LE_UINT16(&bytes[4],0xfffd); // would jump into the entry table
 ScriptResource badJump; if (!badJump.load(bytes)) return false;
 ScriptRunner fault(badJump); fault.start(0);
 if (fault.step(0)!=ScriptRunner::kInvalid || fault.position(0)!=2) return false;
 // Wait consumes a command, yields, then resumes on a later dispatcher visit.
 const uint16 timed[]={1,22,20,5,25,0,20,9,31,20,0};
 bytes.resize(sizeof(timed));
 for(uint i=0;i<sizeof(timed)/2;++i) WRITE_LE_UINT16(&bytes[i*2],timed[i]);
 ScriptResource timedBank; if (!timedBank.load(bytes)) return false;
 ScriptRunner timer(timedBank); timer.start(0);
 if (timer.run(0,100)!=ScriptRunner::kWaiting || timer.position(0)!=8 || !timer.waiting(0) ||
     timer.step(0,3)!=ScriptRunner::kWaiting || !timer.waiting(0) ||
     timer.step(0,2)!=ScriptRunner::kWaiting || timer.waiting(0) || timer.position(0)!=8 ||
     !timer.variables().read(false,0,value) || value!=-1 ||
     timer.run(0,10)!=ScriptRunner::kStopped || !timer.variables().read(false,0,value) || value!=9) return false;
 timer.start(0); timer.step(0); timer.step(0,1); timer.start(0);
 if (timer.waiting(0) || timer.position(0)!=2) return false;
 for(uint i=0;i<2;++i) {
  WRITE_LE_UINT16(&bytes[6],i ? 0xffff : 0); // negative and zero waits still yield
  ScriptResource zeroBank; if (!zeroBank.load(bytes)) return false;
  ScriptRunner zero(zeroBank); zero.start(0);
  if (zero.step(0)!=ScriptRunner::kWaiting || !zero.waiting(0) ||
      zero.step(0)!=ScriptRunner::kWaiting || zero.waiting(0) || zero.position(0)!=8) return false;
 }
 timer.start(0); timer.step(0);
 if (timer.step(0,0xffffffff)!=ScriptRunner::kWaiting || timer.waiting(0)) return false;
 // Equality is opcode 45, not opcode 7. Include signed and unequal inputs.
 const uint16 equal[]={1,45,20,0xffff,20,0xffff}; bytes.resize(sizeof(equal));
 for(uint i=0;i<sizeof(equal)/2;++i) WRITE_LE_UINT16(&bytes[i*2],equal[i]);
 ScriptResource equality; if (!equality.load(bytes)) return false;
 pc=2; if (!equality.expression(pc,value) || value!=1) return false;
 WRITE_LE_UINT16(&bytes[10],0); if (!equality.load(bytes)) return false;
 pc=2; if (!equality.expression(pc,value) || value!=0) return false;
 // Original-format resource instructions, with odd/even string padding.
 class Sink : public ScriptSceneSink {
 public:
  bool narrationPlaying=false; uint narrationCalls=0;
  bool narrationBusy() const override { return narrationPlaying; }
  bool narrate(int16 x,int16 y,const Common::String &name,int16 local,const Common::String &key) override {
   if(x!=160 || y!=100 || name!="F_M02" || local!=8 || key!="1320") return false;
   ++narrationCalls; narrationPlaying=true; return true;
  }
  bool speechBlocked=false;
  bool dialogueBlocked(uint task) const override { return speechBlocked; }
  bool inside=false;
  bool actorInsideHotspot(int16 actor,int16 hotspot,bool &value) const override { if(actor!=2 || hotspot!=3) return false; value=inside; return true; }
  bool owned=false;
  bool stateValue(int16 kind,int16 id,int16 &value) const override { if(kind!=2 || id!=12) return false; value=owned; return true; }
  int deaths=0;
  bool gameOver(int16 reason) override { if(reason!=3) return false; ++deaths; return true; }
  int speeches=0,itemActions=0,entries=0;
  mutable uint32 rng=0;
  bool randomValue(int16 bound,int16 &value) const override { ++rng; value=bound-1; return true; }
  uint32 randomState() const override { return rng; }
  void restoreRandomState(uint32 state) const override { rng=state; }
  bool enterRoom(const Common::String &name,const Common::Array<int16> &args,int16 &actor) override {
   if(name!="A02" || args.size()!=6 || args[0]!=2 || args[1]!=0 || args[2]!=28 || args[3]!=0 || args[4]!=3 || args[5]!=1) return false;
   ++entries; actor=2; return true;
  }
  bool speakDialogue(int16 actor,const Common::String &name,int16 local,const Common::String &key) override { return objectDialogue(actor,name,local,key); }
  bool objectDialogue(int16 actor,const Common::String &name,int16 local,const Common::String &key) override { if(actor!=2 || name!="F_M02" || local!=3 || key!="D09") return false; ++speeches; return true; }
  int backgrounds=0,palettes=0,calls=0,roomChanges=0,movies=0,saves=0;
  bool context(int16 index,int16 &value) const override { if(index!=9) return false; value=-6; return true; }
  bool requestSave(const Common::String &name,int16 local,const Common::String &key) override { if(name!="G_IceTxt" || local!=2 || key!="SAV00") return false; ++saves; return true; }
  bool playMovie(const Common::String &name,int16 before,int16 after) override { if(name!="clip.mux" || before!=1 || after!=0) return false; ++movies; return true; }
  bool changeRoom(int16 chapter,int16 room) override { if(chapter!=2 || room!=24) return false; ++roomChanges; return true; }
  bool busy=true,soundPlaying=true; int animationStep=0,triggerCalls=0,soundCalls=0;
  bool soundBusy(int16 sound) const override { return sound==3 && soundPlaying; }
  bool actorStepReached(int16 actor,int16 target) const override { return !actorBusy(actor) || animationStep>=target; }
  bool actorBusy(int16 actor) const override { return actor==2 && busy; }
  bool invoke(uint16 opcode,const Common::String &name,const Common::Array<int16> &args,const Common::String &key,int16 &result) override {
   if(opcode==81 && args.size()==4 && args[0]==0 && args[1]==2 && args[2]==-1 && args[3]==11) { ++itemActions; result=0; return true; }
   if(opcode==100 && args.size()==1 && args[0]==3) { ++soundCalls; result=3; return true; }
   if(opcode==147 && args.size()==4 && args[0]==2 && args[1]==3 && args[2]==5 && args[3]==1) { ++triggerCalls; result=2; return true; }
   if(opcode==109 && args.size()==5 && args[0]==2 && args[1]==3 && args[2]==75 && args[3]==8 && args[4]==1 && name.empty() && key.empty()) {
    ++calls; result=17; return true;
   }
   if(opcode==51 && args.size()==2 && args[0]==2 && args[1]==3) { ++calls; result=2; return true; }
   return false;
  }
  bool background(const Common::String &name,int16 local,int16 flags) override {
   if (name!="B00" || local!=3 || flags) return false;
   ++backgrounds; return true;
  }
  bool palette(const Common::String &name,int16 local) override {
   if (name!="B00" || local!=4) return false;
   ++palettes; return true;
  }
 } sink;
 // A competing line waits without consuming its operands or killing its task.
 for(uint i=0;i<5;++i) {
  const uint16 ops[]={52,111,122,128,129};
  bytes.resize(4); WRITE_LE_UINT16(&bytes[0],1); WRITE_LE_UINT16(&bytes[2],ops[i]);
  ScriptResource speechBank; if(!speechBank.load(bytes)) return false;
  ScriptRunner speechRunner(speechBank,nullptr,&sink); speechRunner.start(0); sink.speechBlocked=true;
  for(uint n=0;n<3;++n) if(speechRunner.step(0)!=ScriptRunner::kWaiting || speechRunner.position(0)!=2 || !speechRunner.running(0)) return false;
  sink.speechBlocked=false;
  if(speechRunner.step(0)!=ScriptRunner::kInvalid || speechRunner.running(0)) return false;
 }
 // Narration waits for the text/voice channel, not an actor animation.
 const uint16 narrationWords[]={1,122,20,160,20,100,0x5f46,0x304d,0x0032,20,8,0x3331,0x3032,0,31,20,0};
 bytes.resize(sizeof(narrationWords)); for(uint i=0;i<sizeof(narrationWords)/2;++i) WRITE_LE_UINT16(&bytes[i*2],narrationWords[i]);
 ScriptResource narrationBank; if(!narrationBank.load(bytes)) return false;
 ScriptRunner narrationRunner(narrationBank,nullptr,&sink); narrationRunner.start(0);
 if(narrationRunner.step(0)!=ScriptRunner::kWaiting || narrationRunner.position(0)!=28 || sink.narrationCalls!=1 ||
    narrationRunner.step(0)!=ScriptRunner::kWaiting || !narrationRunner.waiting(0) || sink.narrationCalls!=1) return false;
 sink.narrationPlaying=false;
 if(narrationRunner.step(0)!=ScriptRunner::kWaiting || narrationRunner.step(0)!=ScriptRunner::kStopped || sink.narrationCalls!=1) return false;
 // A region wait yields, survives a checkpoint, then releases on entry.
 const uint16 regionWords[]={1,112,20,0,20,2,20,3,25,4,20,42,31,20,0};
 bytes.resize(sizeof(regionWords)); for(uint i=0;i<sizeof(regionWords)/2;++i) WRITE_LE_UINT16(&bytes[i*2],regionWords[i]);
 ScriptResource regionBank; if(!regionBank.load(bytes)) return false;
 ScriptRunner regionRunner(regionBank,nullptr,&sink); regionRunner.start(0);
 if(regionRunner.step(0)!=ScriptRunner::kWaiting || regionRunner.step(0)!=ScriptRunner::kWaiting || !regionRunner.waiting(0)) return false;
 Common::Array<byte> regionState; StateIO regionOut(regionState,false); if(!regionRunner.syncState(regionOut)) return false;
 ScriptRunner resumedRegion(regionBank,nullptr,&sink); StateIO regionIn(regionState,true);
 if(!resumedRegion.syncState(regionIn) || !regionIn.finished() || resumedRegion.step(0)!=ScriptRunner::kWaiting) return false;
 sink.inside=true;
 if(resumedRegion.step(0)!=ScriptRunner::kAdvanced || !resumedRegion.variables().read(false,4,value) || value!=42 || resumedRegion.step(0)!=ScriptRunner::kStopped) return false;
 // Nested NOT/inventory predicates must change when the item is picked up.
 const uint16 inventoryPredicate[]={1,12,91,86,20,2,20,12,45,20,1,20,1};
 bytes.resize(sizeof(inventoryPredicate));
 for(uint i=0;i<sizeof(inventoryPredicate)/2;++i) WRITE_LE_UINT16(&bytes[i*2],inventoryPredicate[i]);
 ScriptResource predicateBank; if(!predicateBank.load(bytes)) return false;
 pc=2; if(!predicateBank.expression(pc,value,nullptr,&sink) || value!=1 || pc!=sizeof(inventoryPredicate)) return false;
 sink.owned=true; pc=2;
 if(!predicateBank.expression(pc,value,nullptr,&sink) || value!=0) return false;
 sink.owned=false; pc=2;
 if(predicateBank.expression(pc,value) || pc!=2) return false;
 WRITE_LE_UINT16(&bytes[14],65535); if(!predicateBank.load(bytes)) return false;
 pc=2; if(predicateBank.expression(pc,value,nullptr,&sink) || pc!=2) return false;
 // A death transition halts every room task, including an unrelated one.
 const uint16 deathWords[]={2,8,131,20,3,31,20,0,31,20,1};
 bytes.resize(sizeof(deathWords));
 for(uint i=0;i<sizeof(deathWords)/2;++i) WRITE_LE_UINT16(&bytes[i*2],deathWords[i]);
 ScriptResource deathBank; if(!deathBank.load(bytes)) return false;
 ScriptRunner deathRunner(deathBank,nullptr,&sink); deathRunner.start(0); deathRunner.start(1);
 if(deathRunner.step(0)!=ScriptRunner::kStopped || sink.deaths!=1 || deathRunner.running(0) || deathRunner.running(1)) return false;
 ScriptRunner noRecovery(deathBank); noRecovery.start(0);
 if(noRecovery.step(0)!=ScriptRunner::kUnsupported || noRecovery.resumeStoppedAt(0,80) ||
    !noRecovery.resumeStoppedAt(0,131) || noRecovery.resumeStoppedAt(0,131) ||
    noRecovery.step(0)!=ScriptRunner::kUnsupported) return false;
 // An entry task waits for the player, while room initialization continues.
 const uint16 entryWords[]={2,20,79,20,2,20,0,20,28,0x3041,0x0032,20,0,20,3,20,1,31,20,0,31,20,1};
 bytes.resize(sizeof(entryWords));
 for(uint i=0;i<sizeof(entryWords)/2;++i) WRITE_LE_UINT16(&bytes[i*2],entryWords[i]);
 ScriptResource entryBank; if(!entryBank.load(bytes)) return false;
 ScriptRunner entryRunner(entryBank,nullptr,&sink); entryRunner.start(0);
 if(entryRunner.step(0)!=ScriptRunner::kAdvanced || sink.entries!=1 || !entryRunner.waiting(1) ||
    entryRunner.step(0)!=ScriptRunner::kStopped || entryRunner.exclusiveTask()!=1 || entryRunner.step(1)!=ScriptRunner::kWaiting) return false;
 sink.busy=false;
 if(entryRunner.step(1)!=ScriptRunner::kWaiting || entryRunner.step(1)!=ScriptRunner::kStopped || entryRunner.exclusiveTask()!=-1) return false;
 sink.busy=true;
 // Nested random expression, positive bounds, and rollback after a bad RHS.
 const uint16 randomWords[]={1,14,38,20,1000,20,200};
 bytes.resize(sizeof(randomWords));
 for(uint i=0;i<sizeof(randomWords)/2;++i) WRITE_LE_UINT16(&bytes[i*2],randomWords[i]);
 ScriptResource randomBank; if(!randomBank.load(bytes)) return false;
 pc=2; if(!randomBank.expression(pc,value,nullptr,&sink) || value!=1199 || sink.rng!=1) return false;
 WRITE_LE_UINT16(&bytes[10],65535); if(!randomBank.load(bytes)) return false;
 pc=2; if(randomBank.expression(pc,value,nullptr,&sink) || pc!=2 || sink.rng!=1) return false;
 WRITE_LE_UINT16(&bytes[8],0); if(!randomBank.load(bytes)) return false;
 pc=2; if(randomBank.expression(pc,value,nullptr,&sink) || sink.rng!=1) return false;
 const uint16 itemWords[]={1,81,20,0,20,2,20,65535,20,11,31,20,0};
 bytes.resize(sizeof(itemWords));
 for(uint i=0;i<sizeof(itemWords)/2;++i) WRITE_LE_UINT16(&bytes[i*2],itemWords[i]);
 ScriptResource itemBank; if(!itemBank.load(bytes)) return false;
 ScriptRunner itemRunner(itemBank,nullptr,&sink); itemRunner.start(0);
 if(itemRunner.step(0)!=ScriptRunner::kAdvanced || itemRunner.position(0)!=20 || sink.itemActions!=1 ||
    itemRunner.step(0)!=ScriptRunner::kStopped) return false;
 for(uint n=4;n<20;++n) {
  Common::Array<byte> partial=bytes; partial.resize(n); ScriptResource shortItem;
  if(!shortItem.load(partial)) return false;
  ScriptRunner runner(shortItem,nullptr,&sink); runner.start(0);
  if(runner.step(0)!=ScriptRunner::kUnsupported || runner.position(0)!=2 || sink.itemActions!=1) return false;
 }
 // Speech must dispatch once, wait for completion, and reject truncated
 // operands without starting audio or advancing the instruction pointer.
 const uint16 speechWords[]={1,111,20,2,0x5f46,0x304d,0x0032,20,3,0x3044,0x0039,31,20,0};
 bytes.resize(sizeof(speechWords));
 for(uint i=0;i<sizeof(speechWords)/2;++i) WRITE_LE_UINT16(&bytes[i*2],speechWords[i]);
 ScriptResource speechBank; if(!speechBank.load(bytes)) return false;
 ScriptRunner speechRunner(speechBank,nullptr,&sink); speechRunner.start(0);
 if(speechRunner.step(0)!=ScriptRunner::kWaiting || speechRunner.position(0)!=22 || sink.speeches!=1 ||
    speechRunner.step(0)!=ScriptRunner::kWaiting || sink.speeches!=1) return false;
 sink.busy=false;
 if(speechRunner.step(0)!=ScriptRunner::kWaiting || speechRunner.step(0)!=ScriptRunner::kStopped) return false;
 sink.busy=true;
 for(uint n=4;n<22;++n) {
  Common::Array<byte> partial=bytes; partial.resize(n); ScriptResource shortSpeech;
  if(!shortSpeech.load(partial)) return false;
  ScriptRunner runner(shortSpeech,nullptr,&sink); runner.start(0);
  if(runner.step(0)!=ScriptRunner::kInvalid || runner.position(0)!=2 || sink.speeches!=1) return false;
 }
 // Ordinary speech shares operand decoding and actor completion waits.
 for(uint i=0;i<sizeof(speechWords)/2;++i) WRITE_LE_UINT16(&bytes[i*2],speechWords[i]);
 WRITE_LE_UINT16(&bytes[2],52);
 ScriptResource plainBank; if(!plainBank.load(bytes)) return false;
 ScriptRunner plain(plainBank,nullptr,&sink); plain.start(0);
 if(plain.step(0)!=ScriptRunner::kWaiting || sink.speeches!=2 || plain.position(0)!=22 || plain.step(0)!=ScriptRunner::kWaiting) return false;
 sink.busy=false; if(plain.step(0)!=ScriptRunner::kWaiting || plain.step(0)!=ScriptRunner::kStopped) return false; sink.busy=true;
 // Runtime context is a raw index, and works inside nested comparisons.
 const uint16 contextWords[]={1,45,46,9,20,65530};
 bytes.resize(sizeof(contextWords));
 for(uint i=0;i<sizeof(contextWords)/2;++i) WRITE_LE_UINT16(&bytes[i*2],contextWords[i]);
 ScriptResource contextBank; if(!contextBank.load(bytes)) return false;
 uint32 contextPc=2; int16 contextValue;
 if(!contextBank.expression(contextPc,contextValue,nullptr,&sink) || contextValue!=1 || contextPc!=12) return false;
 contextPc=2; if(contextBank.expression(contextPc,contextValue) || contextPc!=2) return false;
 WRITE_LE_UINT16(&bytes[6],10); if(!contextBank.load(bytes)) return false;
 if(contextBank.expression(contextPc,contextValue,nullptr,&sink) || contextPc!=2) return false;
 bytes.resize(7); if(!contextBank.load(bytes) || contextBank.expression(contextPc,contextValue,nullptr,&sink) || contextPc!=2) return false;
 // Saving is requested once, after decoding all arguments; the caller's
 // PC advances before the engine performs the deferred write.
 const uint16 saveWords[]={1,152,0x5f47,0x6349,0x5465,0x7478,0,20,2,0x4153,0x3056,0x0030,31,20,0};
 bytes.resize(sizeof(saveWords));
 for(uint i=0;i<sizeof(saveWords)/2;++i) WRITE_LE_UINT16(&bytes[i*2],saveWords[i]);
 ScriptResource saveBank; if(!saveBank.load(bytes)) return false;
 ScriptRunner saveRunner(saveBank,nullptr,&sink); saveRunner.start(0);
 if(saveRunner.step(0)!=ScriptRunner::kAdvanced || saveRunner.position(0)!=24 || sink.saves!=1 || saveRunner.step(0)!=ScriptRunner::kStopped) return false;
 for(uint n=4;n<24;++n) {
  Common::Array<byte> partial=bytes; partial.resize(n); ScriptResource shortSave; if(!shortSave.load(partial)) return false;
  ScriptRunner shortRunner(shortSave,nullptr,&sink); shortRunner.start(0);
  if(shortRunner.step(0)!=ScriptRunner::kInvalid || shortRunner.position(0)!=2 || sink.saves!=1) return false;
 }
 // A movie callback owns playback through completion; truncated arguments
 // must never start it, and rejection leaves the instruction unconsumed.
 const uint16 movieWords[]={1,138,0x6c63,0x7069,0x6d2e,0x7875,0,20,1,20,0,31,20,0};
 bytes.resize(sizeof(movieWords));
 for(uint i=0;i<sizeof(movieWords)/2;++i) WRITE_LE_UINT16(&bytes[i*2],movieWords[i]);
 ScriptResource movieBank; if(!movieBank.load(bytes)) return false;
 ScriptRunner movieRunner(movieBank,nullptr,&sink); movieRunner.start(0);
 if(movieRunner.step(0)!=ScriptRunner::kAdvanced || movieRunner.position(0)!=22 || sink.movies!=1 || movieRunner.step(0)!=ScriptRunner::kStopped) return false;
 for(uint n=4;n<22;++n) {
  Common::Array<byte> partial=bytes; partial.resize(n);
  ScriptResource truncatedMovie; if(!truncatedMovie.load(partial)) return false;
  ScriptRunner truncatedRunner(truncatedMovie,nullptr,&sink); truncatedRunner.start(0);
  if(truncatedRunner.step(0)!=ScriptRunner::kInvalid || truncatedRunner.position(0)!=2 || sink.movies!=1) return false;
 }
 WRITE_LE_UINT16(&bytes[20],2);
 ScriptResource rejectedMovie; if(!rejectedMovie.load(bytes)) return false;
 ScriptRunner rejectedMovieRunner(rejectedMovie,nullptr,&sink); rejectedMovieRunner.start(0);
 if(rejectedMovieRunner.step(0)!=ScriptRunner::kUnsupported || rejectedMovieRunner.position(0)!=2 || sink.movies!=1) return false;
 const uint16 sceneWords[]={1,1,0x3042,0x0030,20,3,20,0,27,0x3042,0x2e30,0x0078,20,4,31,20,0};
 bytes.resize(sizeof(sceneWords));
 for(uint i=0;i<sizeof(sceneWords)/2;++i) WRITE_LE_UINT16(&bytes[i*2],sceneWords[i]);
 ScriptResource sceneBank; if (!sceneBank.load(bytes)) return false;
 ScriptRunner sceneRunner(sceneBank,nullptr,&sink); sceneRunner.start(0);
 if (sceneRunner.run(0,10)!=ScriptRunner::kStopped || sink.backgrounds!=1 || sink.palettes!=1) return false;
 ScriptRunner noSink(sceneBank); noSink.start(0);
 if (noSink.step(0)!=ScriptRunner::kUnsupported || noSink.position(0)!=2) return false;
 WRITE_LE_UINT16(&bytes[14],1); // unsupported background flags must not commit
 ScriptResource unsupportedScene; if (!unsupportedScene.load(bytes)) return false;
 ScriptRunner rejectedScene(unsupportedScene,nullptr,&sink); rejectedScene.start(0);
 if (rejectedScene.step(0)!=ScriptRunner::kUnsupported || rejectedScene.position(0)!=2 || sink.backgrounds!=1) return false;
 // A wrapped side-effectful RHS is invoked exactly once, after validating
 // its destination. Invalid destinations and truncated arguments cannot
 // allocate resources or advance the task PC.
 const uint16 bind[]={1,6,25,249,109,20,2,20,3,20,75,20,8,20,1,31,20,0};
 bytes.resize(sizeof(bind));
 for(uint i=0;i<sizeof(bind)/2;++i) WRITE_LE_UINT16(&bytes[i*2],bind[i]);
 ScriptResource bindingBank; if(!bindingBank.load(bytes)) return false;
 ScriptRunner bindingRunner(bindingBank,nullptr,&sink); bindingRunner.start(0);
 if(bindingRunner.run(0,10)!=ScriptRunner::kStopped || sink.calls!=1 ||
    !bindingRunner.variables().read(false,249,value) || value!=17) return false;
 WRITE_LE_UINT16(&bytes[6],250);
 ScriptResource badBinding; if(!badBinding.load(bytes)) return false;
 ScriptRunner invalidBinding(badBinding,nullptr,&sink); invalidBinding.start(0);
 if(invalidBinding.step(0)!=ScriptRunner::kInvalid || invalidBinding.position(0)!=2 || sink.calls!=1) return false;
 WRITE_LE_UINT16(&bytes[6],249); bytes.resize(28);
 ScriptResource shortBinding; if(!shortBinding.load(bytes)) return false;
 ScriptRunner incompleteBinding(shortBinding,nullptr,&sink); incompleteBinding.start(0);
 if(incompleteBinding.step(0)!=ScriptRunner::kInvalid || sink.calls!=1) return false;
 // Waiting for an actor does not re-run the animation command; completion
 // yields one visit before the script continues, just like timed waits.
 const uint16 animate[]={1,51,20,2,20,3,31,20,0};
 bytes.resize(sizeof(animate));
 for(uint i=0;i<sizeof(animate)/2;++i) WRITE_LE_UINT16(&bytes[i*2],animate[i]);
 ScriptResource animationBank; if(!animationBank.load(bytes)) return false;
 ScriptRunner animationRunner(animationBank,nullptr,&sink); animationRunner.start(0);
 if(animationRunner.step(0)!=ScriptRunner::kWaiting || animationRunner.position(0)!=12 || sink.calls!=2 ||
    animationRunner.step(0,100)!=ScriptRunner::kWaiting || animationRunner.position(0)!=12 || sink.calls!=2) return false;
 sink.busy=false;
 if(animationRunner.step(0)!=ScriptRunner::kWaiting || animationRunner.step(0)!=ScriptRunner::kStopped || sink.calls!=2) return false;
 // Play-and-wait invokes the sound once and survives a snapshot while
 // waiting for the mixer to finish, without replaying it on restoration.
 const uint16 soundWords[]={1,134,20,3,31,20,0};
 bytes.resize(sizeof(soundWords));
 for(uint i=0;i<sizeof(soundWords)/2;++i) WRITE_LE_UINT16(&bytes[i*2],soundWords[i]);
 ScriptResource soundBank; if(!soundBank.load(bytes)) return false;
 ScriptRunner soundRunner(soundBank,nullptr,&sink); soundRunner.start(0);
 if(soundRunner.step(0)!=ScriptRunner::kWaiting || soundRunner.position(0)!=8 || sink.soundCalls!=1 ||
    soundRunner.step(0)!=ScriptRunner::kWaiting || sink.soundCalls!=1) return false;
 Common::Array<byte> soundState; StateIO soundWriter(soundState,false);
 if(!soundRunner.syncState(soundWriter)) return false;
 ScriptRunner soundRestore(soundBank,nullptr,&sink); StateIO soundReader(soundState,true);
 if(!soundRestore.syncState(soundReader) || !soundRestore.waiting(0)) return false;
 sink.soundPlaying=false;
 if(soundRestore.step(0)!=ScriptRunner::kWaiting || soundRestore.step(0)!=ScriptRunner::kStopped || sink.soundCalls!=1) return false;
 // A concurrent task starts only when its actor reaches the requested
 // step, including a catch-up jump over that step. The caller waits longer.
 const uint16 triggered[]={2,14,147,20,2,20,3,20,5,20,1,31,20,0,25,1,20,99,31,20,1};
 bytes.resize(sizeof(triggered));
 for(uint i=0;i<sizeof(triggered)/2;++i) WRITE_LE_UINT16(&bytes[i*2],triggered[i]);
 ScriptResource triggerBank; if(!triggerBank.load(bytes)) return false;
 ScriptRunner triggerRunner(triggerBank,nullptr,&sink); triggerRunner.start(0); sink.busy=true; sink.animationStep=0;
 if(triggerRunner.step(0)!=ScriptRunner::kWaiting || sink.triggerCalls!=1 || !triggerRunner.running(1) ||
    triggerRunner.step(1)!=ScriptRunner::kWaiting || triggerRunner.position(1)!=28) return false;
 // A save taken while both tasks wait restores the same scheduler state.
 Common::Array<byte> savedTasks; StateIO taskWriter(savedTasks,false);
 if(!triggerRunner.syncState(taskWriter)) return false;
 ScriptVariables restoredVars=triggerRunner.variables(); ScriptRunner restoredRunner(triggerBank,&restoredVars,&sink);
 StateIO taskReader(savedTasks,true); if(!restoredRunner.syncState(taskReader) || !taskReader.finished()) return false;
 Common::Array<byte> roundTrip; StateIO roundWriter(roundTrip,false);
 if(!restoredRunner.syncState(roundWriter) || roundTrip!=savedTasks) return false;
 for(uint n=0;n<savedTasks.size();++n) {
  Common::Array<byte> partial=savedTasks; partial.resize(n); StateIO partialReader(partial,true);
  if(restoredRunner.syncState(partialReader)) return false;
  StateIO unchangedWriter(roundTrip,false);
  if(!restoredRunner.syncState(unchangedWriter) || roundTrip!=savedTasks) return false;
 }
 sink.animationStep=6;
 if(restoredRunner.step(1)!=ScriptRunner::kWaiting || restoredRunner.step(1)!=ScriptRunner::kAdvanced ||
    !restoredVars.read(false,1,value) || value!=99 || restoredRunner.step(0)!=ScriptRunner::kWaiting) return false;
 sink.animationStep=6;
 if(triggerRunner.step(1)!=ScriptRunner::kWaiting || triggerRunner.step(1)!=ScriptRunner::kAdvanced ||
    !triggerRunner.variables().read(false,1,value) || value!=99 || triggerRunner.step(0)!=ScriptRunner::kWaiting) return false;
 sink.busy=false;
 if(triggerRunner.step(0)!=ScriptRunner::kWaiting || triggerRunner.step(0)!=ScriptRunner::kStopped) return false;
 WRITE_LE_UINT16(&bytes[20],0); // cannot replace the currently executing task
 ScriptResource selfTrigger; if(!selfTrigger.load(bytes)) return false;
 ScriptRunner selfTriggerRunner(selfTrigger,nullptr,&sink); selfTriggerRunner.start(0);
 if(selfTriggerRunner.step(0)!=ScriptRunner::kInvalid || sink.triggerCalls!=1 || selfTriggerRunner.position(0)!=4) return false;
 // Room initialization remains foreground across waits, before entry tasks
 // can unlock input or inspect partially-created actors/hotspots.
 const uint16 initializing[]={2,7,22,20,1,31,20,0,25,1,20,99,31,20,1};
 // Entry 1 starts at word 8 (the assignment), not the initializer's argument.
 bytes.resize(sizeof(initializing)); for(uint i=0;i<sizeof(initializing)/2;++i) WRITE_LE_UINT16(&bytes[i*2],initializing[i]);
 WRITE_LE_UINT16(&bytes[2],8);
 ScriptResource initBank; if(!initBank.load(bytes)) return false;
 ScriptRunner initRunner(initBank); initRunner.prioritizeInitialization(); initRunner.start(0); initRunner.start(1);
 if(initRunner.step(0)!=ScriptRunner::kWaiting || initRunner.exclusiveTask()!=0 || initRunner.step(1)!=ScriptRunner::kWaiting ||
    initRunner.step(0,1)!=ScriptRunner::kWaiting || initRunner.step(0)!=ScriptRunner::kStopped ||
    initRunner.exclusiveTask()!=-1 || initRunner.step(1)!=ScriptRunner::kAdvanced) return false;
 // Exclusive script sections prevent interleaving, but still yield at the
 // caller's budget. Ending or faulting the owner releases the scheduler.
 const uint16 exclusive[]={2,12,41,25,0,20,7,42,31,20,0,20,25,1,20,9,31,20,1};
 bytes.resize(sizeof(exclusive));
 for(uint i=0;i<sizeof(exclusive)/2;++i) WRITE_LE_UINT16(&bytes[i*2],exclusive[i]);
 ScriptResource exclusiveBank; if(!exclusiveBank.load(bytes)) return false;
 ScriptRunner serial(exclusiveBank); serial.start(0); serial.start(1);
 if(serial.run(0,1)!=ScriptRunner::kBudgetExhausted || serial.exclusiveTask()!=0 || serial.step(1)!=ScriptRunner::kWaiting ||
    serial.position(1)!=24 || serial.step(0)!=ScriptRunner::kAdvanced || serial.step(0)!=ScriptRunner::kAdvanced ||
    serial.exclusiveTask()!=-1 || serial.step(1)!=ScriptRunner::kAdvanced || !serial.variables().read(false,1,value) || value!=9) return false;
 serial.start(0); serial.step(0); // section entered again
 WRITE_LE_UINT16(&bytes[4],41); WRITE_LE_UINT16(&bytes[6],65535);
 ScriptResource faultExclusive; if(!faultExclusive.load(bytes)) return false;
 ScriptRunner serialFault(faultExclusive); serialFault.start(0);
 if(serialFault.step(0)!=ScriptRunner::kAdvanced || serialFault.step(0)!=ScriptRunner::kUnsupported || serialFault.exclusiveTask()!=-1) return false;
 // A validated room-change request stops every old-bank task, including
 // an exclusive owner, before the engine replaces the bank at a safe point.
 const uint16 transition[]={2,9,41,34,20,2,20,24,42,25,0,20,99};
 bytes.resize(sizeof(transition));
 for(uint i=0;i<sizeof(transition)/2;++i) WRITE_LE_UINT16(&bytes[i*2],transition[i]);
 ScriptResource transitionBank; if(!transitionBank.load(bytes)) return false;
 ScriptRunner transfer(transitionBank,nullptr,&sink); transfer.start(0); transfer.start(1);
 if(transfer.run(0,8)!=ScriptRunner::kStopped || transfer.running(1) || transfer.exclusiveTask()!=-1 || sink.roomChanges!=1 ||
    !transfer.variables().read(false,0,value) || value!=-1) return false;
 WRITE_LE_UINT16(&bytes[14],40);
 ScriptResource deniedTransition; if(!deniedTransition.load(bytes)) return false;
 ScriptRunner stay(deniedTransition,nullptr,&sink); stay.start(0); stay.start(1);
 if(stay.run(0,8)!=ScriptRunner::kUnsupported || !stay.running(1) || sink.roomChanges!=1 || stay.position(0)!=6) return false;
 // Verified no-effect commands consume exactly one opcode each.
 const uint16 noEffects[]={1,126,127,31,20,0}; bytes.resize(sizeof(noEffects));
 for(uint i=0;i<sizeof(noEffects)/2;++i) WRITE_LE_UINT16(&bytes[i*2],noEffects[i]);
 ScriptResource noEffectBank; if(!noEffectBank.load(bytes)) return false;
 ScriptRunner noEffectRunner(noEffectBank); noEffectRunner.start(0);
 if(noEffectRunner.run(0,3)!=ScriptRunner::kStopped) return false;
 // Two tasks rendezvous after different delays; neither can pass a barrier
 // until the other reaches the matching instruction.
 const uint16 barrier[]={2,8,44,20,1,31,20,0,22,20,2,44,20,0,31,20,1};
 bytes.resize(sizeof(barrier));
 for(uint i=0;i<sizeof(barrier)/2;++i) WRITE_LE_UINT16(&bytes[i*2],barrier[i]);
 ScriptResource barrierBank; if(!barrierBank.load(bytes)) return false;
 ScriptRunner rendezvous(barrierBank); rendezvous.start(0); rendezvous.start(1);
 if(rendezvous.step(0)!=ScriptRunner::kWaiting || rendezvous.step(1)!=ScriptRunner::kWaiting ||
    rendezvous.step(0)!=ScriptRunner::kWaiting || !rendezvous.waiting(0)) return false;
 rendezvous.step(1,2); rendezvous.step(1); // task 1 reaches the barrier
 rendezvous.step(0); // releases both sides, but neither executes beyond it
 if(!rendezvous.running(0) || !rendezvous.running(1)) return false;
 rendezvous.step(0); rendezvous.step(1);
 if(rendezvous.step(0)!=ScriptRunner::kStopped || rendezvous.step(1)!=ScriptRunner::kStopped) return false;
 // Node arguments are raw signed words, rather than expression opcodes.
 const uint16 nodeProgram[]={1,47,2,0xfffb,7,100,31,20,0};
 bytes.resize(sizeof(nodeProgram));
 for(uint i=0;i<sizeof(nodeProgram)/2;++i) WRITE_LE_UINT16(&bytes[i*2],nodeProgram[i]);
 ScriptResource nodeBank; if(!nodeBank.load(bytes)) return false;
 ScriptRunner nodeRunner(nodeBank); nodeRunner.start(0);
 if(nodeRunner.run(0,10)!=ScriptRunner::kStopped || !nodeRunner.navigation().nodes[2].active ||
    nodeRunner.navigation().nodes[2].x!=-5 || nodeRunner.navigation().nodes[2].y!=7) return false;
 // Navigation transactions retain explicit link edits while generated
 // visibility links are disabled, then rebuild on demand when resumed.
 const uint16 editProgram[]={1,102,20,1,85,20,0,20,0,20,1,102,20,0,31,20,0};
 bytes.resize(sizeof(editProgram)); for(uint i=0;i<sizeof(editProgram)/2;++i) WRITE_LE_UINT16(&bytes[i*2],editProgram[i]);
 ScriptResource editBank; if(!editBank.load(bytes)) return false;
 ScriptRunner editor(editBank); editor.navigation().node(0,0,0,100); editor.navigation().node(1,10,0,100); editor.navigation().edge(0,0,1,0); editor.start(0);
 if(editor.step(0)!=ScriptRunner::kAdvanced || editor.navigation().generatedEnabled ||
    editor.step(0)!=ScriptRunner::kAdvanced || !editor.navigation().edges[0].enabled ||
    editor.step(0)!=ScriptRunner::kAdvanced || !editor.navigation().generatedEnabled || editor.step(0)!=ScriptRunner::kStopped) return false;
 for(uint n=10;n<22;++n) {
  Common::Array<byte> partial=bytes; partial.resize(n); ScriptResource shortEdit; if(!shortEdit.load(partial)) return false;
  ScriptRunner rejectedEdit(shortEdit); rejectedEdit.navigation().node(0,0,0,100); rejectedEdit.navigation().node(1,10,0,100); rejectedEdit.navigation().edge(0,0,1,0); rejectedEdit.start(0);
  if(rejectedEdit.step(0)!=ScriptRunner::kAdvanced || rejectedEdit.step(0)!=ScriptRunner::kInvalid || rejectedEdit.position(0)!=8 || rejectedEdit.navigation().edges[0].enabled) return false;
 }
 // Terminator without its required alignment byte is truncated.
 bytes.resize(5); WRITE_LE_UINT16(&bytes[0],1); bytes[2]='A'; bytes[3]='B'; bytes[4]=0;
 ScriptResource shortString; if (!shortString.load(bytes)) return false;
 pc=2; Common::String text;
 if (shortString.string(pc,text) || pc!=2 || !text.empty()) return false;
 return true;
}
}
