// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_SCRIPTMACHINE_H
#define SOTC_SCRIPTMACHINE_H
#include "startstate.h"
#include "script.h"
#include "snapshot.h"
namespace Sotc {
enum TaskResult { kReady, kDisabled, kTimerWait, kAnimationWait, kRegionWait,
 kUnknownWait, kUnknownOpcode, kInvalidScript, kInstructionLimit, kUnknownVariable, kSoundUnavailable, kAnimationUnavailable };
struct ScriptTask {
 Cc4View script;
 uint32_t pc;
 unsigned actor, flags, timer, loop, rectangle[4], opcode, executed;
 TaskResult result;
 ScriptTask() : pc(0), actor(0), flags(0x20), timer(0), loop(0), rectangle{0,0,0,0}, opcode(0), executed(0), result(kDisabled) {}
};
struct ScriptActor {
 int x, y;
 unsigned halfWidth, footHeight, step, steps;
};
class ScriptAnimation {
public:
 virtual ~ScriptAnimation() {}
 virtual bool sequence(unsigned actor, unsigned sequence, ScriptActor &state) = 0;
 virtual bool hide(unsigned actor) = 0;
 virtual bool clip(unsigned,bool,unsigned,unsigned) {return false;}
 virtual bool drawEnabled(unsigned,unsigned) { return false; }
 virtual bool dialogue(unsigned,unsigned,unsigned,unsigned) { return false; }
 virtual bool dialoguePending(bool &) { return false; }
 virtual bool move(unsigned,unsigned,unsigned,bool &) { return false; }
 virtual bool movementPending(unsigned,bool &) { return false; }
 virtual bool face(unsigned,unsigned) { return false; }
 virtual bool control(unsigned, bool, ScriptActor &) { return false; }
 virtual bool reset(unsigned, ScriptActor &) { return false; }
 virtual bool typedResource(unsigned actor,unsigned index,unsigned type) {return type==0 && resource(actor,index);}
 virtual bool resource(unsigned, unsigned) { return false; }
 virtual bool background(unsigned) { return false; }
 virtual bool object(unsigned,unsigned,unsigned) { return false; }
 virtual bool removeObject(unsigned) { return false; }
 virtual bool obstacle(const unsigned *) { return false; }
 virtual bool actorLabel(unsigned,unsigned) { return false; }
 virtual bool terrain(Cc4View) { return false; }
 virtual bool exits(Cc4View) { return false; }
 virtual bool bypass(unsigned) { return false; }
 virtual bool speechBank(unsigned) { return false; }
 virtual bool fade(bool,unsigned) { return false; }
 virtual bool fading() const { return false; }

};
// A successful request includes ignoring a new effect while one is active,
// as the original single-channel driver does. 255 stops the channel.
class ScriptSound {
public:
 virtual ~ScriptSound() {}
 virtual bool speech(unsigned,unsigned) { return false; }
 virtual void stopSpeech() {}
 virtual bool effect(unsigned index) = 0;
 virtual bool music(unsigned index) = 0;
};
class ScriptMachine {
 ScriptSound *_sound;
 ScriptAnimation *_animation;
 StartState _initial;
 unsigned _globals[256], _locals[256];
 uint8_t _block[65536]={};
 ScriptTask _tasks[17];
 ScriptActor _actors[11];
 unsigned _count;
 unsigned _choiceCount=0,_choiceTask=0,_choiceHeader=65535,_choiceX=0,_choiceY=0,_choiceSelection=0;
 unsigned _choiceText[6]={},_choiceTarget[6]={};
 unsigned _resourceType=0;
 unsigned _previousRoom=0, _previousBank=0, _pendingRoom=255, _pendingBank=255;
 uint32_t _randomState;
 unsigned _randomValue;
 bool _randomKnown, _eventPending;
 static int signed16(unsigned v) { v &= 65535; return v < 32768 ? int(v) : int(v) - 65536; }
 bool variable(unsigned id, unsigned &value) const {
  if (id==0) { value=_previousRoom; return true; }
  if (id==34) { value=_previousBank; return true; }
  if (id == 33) { value = _randomValue; return _randomKnown; }
  if (id>=1000 && id<1256) { value=_globals[id-1000]; return true; }
  if (id>=2000 && id<2256) { value=_locals[id-2000]; return true; }
  if (id>=2 && id<=30 && id%3!=1) {
   const ScriptActor &a=_actors[(id-1)/3]; value=unsigned(id%3==2 ? a.x : a.y)&65535; return true;
  }
  unsigned address;
  if (id == 0) address = 0x2df6;
  else if (id == 34) address = 0x2df0;
  else if (id >= 1000 && id < 2000) address = 0x3b2a + (id - 1000) * 2;
  else if (id >= 2000 && id < 2256) address = 0x392a + (id - 2000) * 2;
  else return false; // Runtime variables and actor aliases need live bindings.
  if (!_initial.range(address, 2)) return false;
  value = _initial.word(address); return true;
 }
 bool writeVariable(unsigned id,unsigned value) {
  value&=65535;
  if (id>=1000 && id<1256) { _globals[id-1000]=value; return true; }
  if (id>=2000 && id<2256) { _locals[id-2000]=value; return true; }
  if (id==33) { _randomValue=value; _randomKnown=true; return true; }
  if (id>=2 && id<=30 && id%3!=1) {
   ScriptActor &a=_actors[(id-1)/3];
   if (id%3==2) a.x=signed16(value); else a.y=signed16(value);
   return true;
  }
  return false;
 }
 bool overlaps(const ScriptTask &task) const {
  const ScriptActor &a = _actors[0];
  // Original 0xa904 forms a footprint rectangle; 0x134ac includes its edges.
  return signed16(a.x - a.halfWidth) <= signed16(task.rectangle[2]) &&
   signed16(a.y - a.footHeight) <= signed16(task.rectangle[3]) &&
   signed16(a.x + a.halfWidth) >= signed16(task.rectangle[0]) && a.y >= signed16(task.rectangle[1]);
 }
 static bool compare(unsigned a, unsigned b, unsigned op) {
  int x = signed16(a), y = signed16(b);
  switch (op) {
  case 0: return x == y; case 1: return x != y;
  case 2: return x > y; case 3: return x >= y;
  case 4: return x < y; case 5: return x <= y;
  case 6: return (a & b) != 0; case 7: return (a | b) != 0;
  default: return false;
  }
 }
public:
 ScriptMachine() : _sound(nullptr), _animation(nullptr), _count(0), _randomState(1), _randomValue(0), _randomKnown(false), _eventPending(false) {}
 // Explicit diagnostic seed: original startup seed source is not yet bound.
 void seedRandom(uint32_t seed) { _randomState = seed; }
 unsigned randomRange(unsigned limit) {
  if (limit<2) return 0;
  _randomState=_randomState*uint32_t(0x015a4e35)+1;
  return ((_randomState>>16)&0x7fff)%limit;
 }
 uint32_t randomState() const { return _randomState; }
 bool randomValue(unsigned &value) const { value = _randomValue; return _randomKnown; }
 void setEventPending(bool pending) { _eventPending = pending; }
 bool eventPending() const { return _eventPending; }
 ScriptMachine(const ScriptMachine &) = delete;
 ScriptMachine &operator=(const ScriptMachine &) = delete;
 void setAnimation(ScriptAnimation *animation) { _animation = animation; }
 const ScriptActor *actor(unsigned index) const { return index < 11 ? &_actors[index] : nullptr; }
 bool speech(unsigned bank,unsigned text) { return _sound && _sound->speech(bank,text); }
 void stopSpeech() { if (_sound) _sound->stopSpeech(); }
 void setSound(ScriptSound *sound) { _sound = sound; }
 unsigned count() const { return _count; }
 const ScriptTask *task(unsigned i) const { return i < _count ? &_tasks[i] : nullptr; }
 bool setActor(unsigned index, const ScriptActor &actor) {
  if (index >= 11 || actor.x < -32768 || actor.x > 32767 || actor.y < -32768 || actor.y > 32767 ||
      actor.halfWidth > 255 || actor.footHeight > 255 || actor.steps > 65535 || actor.step > 65535) return false;
  _actors[index] = actor; return true;
 }
 bool load(const StartState &state, Cc4View block) {
  _count = 0; _choiceCount=0; _resourceType=0;
  _randomState = 1; _randomValue = 0; _randomKnown = false; _eventPending = false;
  if (!state.valid() || block.size > sizeof(_block) || cc4Count(block, 2) != state.word(0x2e02) || !state.word(0x2e02)) return false;
  for (unsigned i = 0; i < block.size; ++i) _block[i] = block.data[i];
  block.data = _block;
  _initial = state;
  _previousRoom=state.word(0x2df6); _previousBank=state.word(0x2df0); _pendingRoom=255; _pendingBank=255;
  for (unsigned i=0;i<256;++i) { _globals[i]=state.word(0x3b2a+i*2); _locals[i]=state.word(0x392a+i*2); }
  for (unsigned i = 0; i < state.word(0x2e02); ++i) {
   ScriptTask &t = _tasks[i]; t = ScriptTask();
   if (!cc4Entry(block, 2, i, t.script) || !state.taskPC(block, i, t.pc)) return false;
   unsigned a = 0x3110 + i * 22;
   t.actor = state.byte(a + 8); t.flags = state.byte(a + 9); t.timer = state.word(a + 10); t.loop=state.word(a+12);
   for (unsigned j = 0; j < 4; ++j) t.rectangle[j] = state.word(a + 14 + j * 2);
   t.result = t.flags & 0x20 ? kDisabled : kReady;
  }
  for (unsigned i = 0; i < 11; ++i) {
   unsigned a = 0x2e54 + i * 52;
   _actors[i] = {state.signedWord(a), state.signedWord(a + 2), state.byte(a + 21), state.byte(a + 22), state.word(a + 14), state.word(a + 18)};
  }
  _count = state.word(0x2e02); return true;
 }
 bool useObject(unsigned id) {
  if(id>=256 || (_locals[id]!=1 && _locals[id]!=2))return false;
  for(unsigned i=0;i<256;++i)if(_locals[i]==2)_locals[i]=1;
  _locals[id]=2;return true;
 }
 bool collectObject(unsigned id) {if(id>=256)return false;_locals[id]=1;return true;}
 unsigned inventoryState(unsigned id) const {return id<256 ? _locals[id] : 0;}
 unsigned journalProgress() const { return _globals[1]; }
 unsigned choiceCount() const { return _choiceCount; }
 unsigned choiceHeader() const { return _choiceHeader; }
 unsigned choiceX() const { return _choiceX; }
 unsigned choiceY() const { return _choiceY; }
 unsigned choiceSelection() const { return _choiceSelection; }
 unsigned choiceText(unsigned i) const { return i<_choiceCount ? _choiceText[i] : 65535; }
 void highlightChoice(unsigned i) { if(i<_choiceCount) _choiceSelection=i; }
 bool choose(unsigned i) {
  if(i>=_choiceCount || _choiceTask>=_count) return false;
  ScriptTask &t=_tasks[_choiceTask];
  if(!(t.flags&0x10) || _choiceTarget[i]>=t.script.size) return false;
  t.pc=_choiceTarget[i];t.flags&=~0x10U;t.result=kReady;_choiceCount=0;return true;
 }
 void requestRoom(unsigned bank,unsigned room) {_pendingBank=bank;_pendingRoom=room;}
 unsigned pendingBank() const { return _pendingBank; }
 unsigned pendingRoom() const { return _pendingRoom; }
 bool enterRoom(Cc4View block,unsigned previousRoom,unsigned previousBank) {
  unsigned count=cc4Count(block,2);
  if (!count || count>17 || block.size>sizeof(_block)) return false;
  for(unsigned i=0;i<count;++i) { Cc4View script; if(!cc4Entry(block,2,i,script) || !script.size) return false; }
  for(unsigned i=0;i<block.size;++i) _block[i]=block.data[i];
  block.data=_block; _count=count; _previousRoom=previousRoom; _previousBank=previousBank; _pendingRoom=255; _pendingBank=255;
  _eventPending=false; _choiceCount=0;
  for(unsigned i=0;i<count;++i) { _tasks[i]=ScriptTask(); cc4Entry(block,2,i,_tasks[i].script); }
  _tasks[0].flags=0; _tasks[0].result=kReady;
  return true;
 }
 // Copy validated state while keeping host audio/animation bindings and rebasing views.
 void adopt(const ScriptMachine &other) {
  _initial=other._initial; _count=other._count;
  _choiceCount=other._choiceCount;_choiceTask=other._choiceTask;_choiceHeader=other._choiceHeader;
  _choiceX=other._choiceX;_choiceY=other._choiceY;_choiceSelection=other._choiceSelection;
  for(unsigned i=0;i<6;++i) {_choiceText[i]=other._choiceText[i];_choiceTarget[i]=other._choiceTarget[i];}
  _resourceType=other._resourceType;
  _previousRoom=other._previousRoom; _previousBank=other._previousBank; _pendingRoom=other._pendingRoom; _pendingBank=other._pendingBank;
  _randomState=other._randomState; _randomValue=other._randomValue; _randomKnown=other._randomKnown; _eventPending=other._eventPending;
  for(unsigned i=0;i<256;++i) { _globals[i]=other._globals[i]; _locals[i]=other._locals[i]; }
  for(unsigned i=0;i<sizeof(_block);++i) _block[i]=other._block[i];
  for(unsigned i=0;i<_count;++i) { _tasks[i]=other._tasks[i]; _tasks[i].script.data=_block+(other._tasks[i].script.data-other._block); }
  for(unsigned i=0;i<11;++i) _actors[i]=other._actors[i];
 }
 // Loaded resource identities bind a save to this game's original snapshot/scripts.
 bool snapshot(Snapshot &s,unsigned version=5) {
  s.identity(Snapshot::hash(_initial.range(0x2dee,0xf3e),0xf3e));
  s.identity(_count);
  for(unsigned i=0;i<256;++i) { s.value(_globals[i],65535); s.value(_locals[i],65535); }
  s.value(_randomState); s.value(_randomValue); s.value(_randomKnown); s.value(_eventPending);
  for(unsigned i=0;i<_count;++i) {
   ScriptTask &t=_tasks[i]; s.identity(Snapshot::hash(t.script.data,t.script.size));
   s.value(t.pc,t.script.size); s.value(t.actor,10); s.value(t.flags,255);
   s.value(t.timer,65535); s.value(t.loop,65535);
   for(unsigned j=0;j<4;++j) s.value(t.rectangle[j],65535);
   s.value(t.opcode,65535); s.value(t.executed);
   unsigned result=t.result; s.value(result,kAnimationUnavailable); if(s.reading()) t.result=TaskResult(result);
  }
  for(unsigned i=0;i<11;++i) {
   ScriptActor &a=_actors[i]; s.value(a.x); s.value(a.y);
   s.require(a.x>=-32768 && a.x<=32767 && a.y>=-32768 && a.y<=32767);
   s.value(a.halfWidth,255); s.value(a.footHeight,255); s.value(a.step,65535); s.value(a.steps,65535);
  }
  if(version>=2) { s.value(_previousRoom,65535); s.value(_previousBank,65535); s.value(_pendingRoom,255); }
  if(version>=3) {
   s.value(_choiceCount,6);s.value(_choiceTask,16);s.value(_choiceHeader,65535);
   s.value(_choiceX,510);s.value(_choiceY,255);s.value(_choiceSelection,5);
   if(_choiceCount) s.require(_choiceTask<_count && (_tasks[_choiceTask].flags&0x10) && _choiceSelection<_choiceCount);
   for(unsigned i=0;s.ok() && i<_choiceCount;++i) {
    s.value(_choiceText[i],65535);s.value(_choiceTarget[i],_tasks[_choiceTask].script.size-1);
   }
  } else if(s.reading()) _choiceCount=0;
  if(version>=4) { s.value(_pendingBank,255);s.require(_pendingBank<=9 || _pendingBank==255); }
  else if(s.reading()) _pendingBank=255;
  if(version>=5)s.value(_resourceType,1);else if(s.reading())_resourceType=0;
  return s.ok();
 }
 TaskResult visit(unsigned index, unsigned budget = 128) {
  if (index >= _count) return kInvalidScript;
  ScriptTask &t = _tasks[index];
  if (t.flags & 0x20) return t.result = kDisabled;
  if (t.flags & ~0x77U) return t.result = kUnknownWait;
  if(t.flags&0x10) {
   if(!_choiceCount || _choiceTask!=index) return t.result=kInvalidScript;
   return t.result=kAnimationWait;
  }
  if (t.flags & 0x40) {
   bool pending=false;
   if (!_animation || !_animation->dialoguePending(pending)) return t.result=kAnimationUnavailable;
   if (pending) return t.result=kAnimationWait;
   t.flags &= ~0x40U;
  }
  bool movementWaiting=false;
  if (t.flags & 1) {
   bool pending=false;
   if (!_animation || !_animation->movementPending(t.actor,pending)) return t.result=kAnimationUnavailable;
   if (pending) movementWaiting=true; else t.flags &= ~1U;
  }
  bool timerWaiting = false, animationWaiting = false;
  if (t.flags & 2) {
   if (t.timer) --t.timer;
   if (!t.timer) t.flags &= ~2U; else timerWaiting = true;
  }
  if (t.flags & 4) {
   if (t.actor >= 11) return t.result = kInvalidScript;
   const ScriptActor &a = _actors[t.actor];
   if (a.step == ((a.steps - 1) & 65535)) t.flags &= ~4U; else animationWaiting = true;
  }
  if (movementWaiting) return t.result=kRegionWait;
  if (timerWaiting) return t.result = kTimerWait;
  if (animationWaiting) return t.result = kAnimationWait;
  for (unsigned i = 0; i < budget; ++i) {
   ScriptInstruction op;
   ScriptDecode decoded = decodeScript(t.script, t.pc, op); t.opcode = op.opcode;
   if (decoded != kInstruction) return t.result = decoded == kUnsupported ? kUnknownOpcode : kInvalidScript;
   uint32_t next = op.next;
   switch (op.opcode) {
   case 2: t.flags |= 0x20; ++t.executed; return t.result = kDisabled;
   case 3: next = op.target; break;
   case 6:
    t.loop=(t.loop+1)&65535;
    if (t.loop<op.argument) next=op.target; else t.loop=0;
    break;
   case 31: case 32: case 33: case 56: {
    unsigned value=op.value,current=0;
    if (op.valueIsVariable && !variable(op.value,value)) return t.result=kUnknownVariable;
    if (op.opcode!=31 && !variable(op.variable,current)) return t.result=kUnknownVariable;
    if (op.opcode==32) value=current+value;
    if (op.opcode==33) value=current-value;
    if (op.opcode==56) value=current|value; // Original 0x92cd: set flag bits without replacing others.
    if (!writeVariable(op.variable,value)) return t.result=kUnknownVariable;
    break;
   }
   case 53:
    if(_choiceCount) return t.result=kInvalidScript;
    _choiceCount=op.argument;_choiceTask=index;_choiceHeader=op.value;
    if(_choiceHeader==op.choiceText[0]) _choiceHeader=65535;
    _choiceX=op.variable;_choiceY=op.comparison;_choiceSelection=0;
    for(unsigned j=0;j<_choiceCount;++j) {_choiceText[j]=op.choiceText[j];_choiceTarget[j]=op.choiceTarget[j];}
    t.flags|=0x10;t.pc=next;++t.executed;return t.result=kAnimationWait;
   case 42: case 43:
    if(!_animation || !_animation->clip(t.actor,op.opcode==42,op.argument,op.value))return t.result=kAnimationUnavailable;
    break;
   case 25:
    if(!_animation || !_animation->object(op.argument,op.variable,op.value)) return t.result=kAnimationUnavailable;
    break;
   case 98:
    if(!_animation || !_animation->removeObject(op.argument)) return t.result=kAnimationUnavailable;
    break;
   case 14:
    for(unsigned j=0;j<4;++j) t.rectangle[j]=op.rectangle[j];
    if(!overlaps(t)) next=op.target;
    break;
   case 60:
    if (!_animation || !_animation->obstacle(op.rectangle)) return t.result=kAnimationUnavailable;
    break;
   case 69:
    if (!_animation || !_animation->actorLabel(t.actor,op.argument)) return t.result=kAnimationUnavailable;
    break;
   case 44: _pendingRoom=op.argument; break;
   case 80:
    if(op.argument>9 && op.argument!=255) return t.result=kInvalidScript;
    _pendingBank=op.argument;break;
   case 58:
    if (!_animation || !_animation->background(op.argument)) return t.result=kAnimationUnavailable;
    break;
   case 22: case 23: {
    Cc4View data(t.script.data+t.pc+2,op.next-t.pc-2);
    if (!_animation || !(op.opcode==22 ? _animation->terrain(data) : _animation->exits(data))) return t.result=kAnimationUnavailable;
    break;
   }
   case 34:
    if (!_animation || !_animation->bypass(t.actor)) return t.result=kAnimationUnavailable;
    break;
   case 96:
    if (!_animation || !_animation->speechBank(op.argument)) return t.result=kAnimationUnavailable;
    break;
   case 93: case 94:
    if (!_animation || !_animation->fade(op.opcode==93,op.argument)) return t.result=kAnimationUnavailable;
    break;
   case 92:
    if (t.actor>=11) return t.result=kInvalidScript;
    if (!_animation || !_animation->drawEnabled(t.actor,op.argument)) return t.result=kAnimationUnavailable;
    break;
   case 9:
    t.timer = op.argument; t.flags |= 2; t.pc = next; ++t.executed; return t.result = kTimerWait;
   case 10: {
    unsigned a, b = op.value;
    if (!variable(op.variable, a) || (op.valueIsVariable && !variable(op.value, b))) return t.result = kUnknownVariable;
    if (!compare(a, b, op.comparison)) next = op.target;
    break;
   }
   case 1:
    if (t.actor>=11 || op.argument>4) return t.result=kInvalidScript;
    if (!_animation || !_animation->face(t.actor,op.argument)) return t.result=kAnimationUnavailable;
    break;
   case 4: case 5: case 15: case 16: case 36: {
    if (t.actor>=11) return t.result=kInvalidScript;
    bool pending=false;
    unsigned x=op.argument,y=op.value;
    if (op.opcode==4) { x=op.argument*2; y=unsigned(_actors[t.actor].y)&65535; }
    if (op.opcode==5) { x=unsigned(_actors[t.actor].x)&65535; y=op.argument; }
    if (op.opcode==15) { x=unsigned(_actors[0].x+op.argument)&65535; y=unsigned(_actors[t.actor].y)&65535; }
    if (op.opcode==16) { x=unsigned(_actors[t.actor].x)&65535; y=unsigned(_actors[0].y+op.argument)&65535; }
    if (!_animation || !_animation->move(t.actor,x,y,pending)) return t.result=kAnimationUnavailable;
    if (pending) { t.flags|=1; t.pc=next; ++t.executed; return t.result=kRegionWait; }
    break;
   }
   case 82: case 89:
    if (op.argument>=10) return t.result=kInvalidScript;
    if (!_animation || !_animation->dialogue(op.argument,op.value,op.comparison,op.variable)) return t.result=kAnimationUnavailable;
    t.flags|=0x40; t.pc=next; ++t.executed; return t.result=kAnimationWait;
   case 18: case 19:
    if (op.opcode==18 && op.argument>4) return t.result=kInvalidScript;
    if (!_animation || !_animation->control(op.argument,op.opcode==19,_actors[0])) return t.result=kAnimationUnavailable;
    break;
   case 7:
    if (t.actor >= 11) return t.result = kInvalidScript;
    _actors[t.actor].x = op.argument; _actors[t.actor].y = op.value;
    break;
   case 90:
    if (op.argument >= 11) return t.result = kInvalidScript;
    if (op.argument && (!_animation || !_animation->reset(op.argument, _actors[op.argument]))) return t.result = kAnimationUnavailable;
    t.actor = op.argument; break;
   case 47:
    if(op.argument>1)return t.result=kInvalidScript;
    _resourceType=op.argument;
    break;
   case 91:
    if (t.actor >= 11) return t.result = kInvalidScript;
    if (!_animation || !_animation->typedResource(t.actor, op.argument,_resourceType)) return t.result = kAnimationUnavailable;
    _resourceType=0;break;
   case 21:
    if (op.argument >= 11) return t.result = kInvalidScript;
    t.actor = op.argument; break;
   case 27: case 28:
    if (op.argument >= _count) return t.result = kInvalidScript;
    if (op.opcode == 28) _tasks[op.argument].flags |= 0x20;
    else {
     ScriptTask &other = _tasks[op.argument]; other.pc = 0; other.flags = 0; other.timer = 0; other.loop=0; other.actor = 0; other.result = kReady;
     // Restarting self changes the original task IP too.
     if (op.argument == index) next = 0;
    }
    break;
   case 40:
    for (unsigned j = 0; j < 4; ++j) t.rectangle[j] = op.rectangle[j];
    if (!overlaps(t)) return t.result = kRegionWait;
    break;
   case 64: case 66:
    if (!_eventPending) next = op.target;
    else {
     if (op.opcode == 66) {
      for (unsigned j = 0; j < 4; ++j) t.rectangle[j] = op.rectangle[j];
     }
     if (op.opcode == 66 && !overlaps(t)) next = op.target;
     else _eventPending = false;
    }
    break;
   case 41:
    if (t.actor >= 11) return t.result = kInvalidScript;
    if (t.actor && (!_animation || !_animation->hide(t.actor))) return t.result = kAnimationUnavailable;
    break;
   case 87: case 88:
    if (t.actor >= 11) return t.result = kInvalidScript;
    if (!_animation || !_animation->sequence(t.actor, op.argument, _actors[t.actor])) return t.result = kAnimationUnavailable;
    if (op.opcode == 87) {
     t.flags |= 4; t.pc = next; ++t.executed; return t.result = kAnimationWait;
    }
    break;
   case 77:
    if (!_sound || !_sound->music(op.argument)) return t.result = kSoundUnavailable;
    break;
   case 99:
    if (!_sound || !_sound->effect(op.argument)) return t.result = kSoundUnavailable;
    break;
   case 79:
    // Borland rand at 0x17b5f; range helper 0x7051 does not draw for 0/1.
    _randomValue = randomRange(op.argument);
    _randomKnown = true;
    break;
   case 39:
    if (overlaps(t)) return t.result = kRegionWait;
    break;
   default: return t.result = kUnknownOpcode; // Recognized format is not implemented behavior.
   }
   t.pc = next; ++t.executed;
  }
  return t.result = kInstructionLimit;
 }
 void tick() {
  if (_pendingRoom!=255 || (_animation && _animation->fading())) return;
  if (_count && !(_tasks[0].flags&0x20)) {
   visit(0);
   if (!(_tasks[0].flags&0x20) || (_animation && _animation->fading())) return;
  }
  for (unsigned i=1;i<_count;++i) { visit(i); if(_pendingRoom!=255) break; }
 }
};
}
#endif
