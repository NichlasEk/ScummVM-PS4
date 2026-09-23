// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_SCRIPTMACHINE_H
#define SOTC_SCRIPTMACHINE_H
#include "startstate.h"
#include "script.h"
namespace Sotc {
enum TaskResult { kReady, kDisabled, kTimerWait, kAnimationWait, kRegionWait,
 kUnknownWait, kUnknownOpcode, kInvalidScript, kInstructionLimit, kUnknownVariable, kSoundUnavailable, kAnimationUnavailable };
struct ScriptTask {
 Cc4View script;
 uint32_t pc;
 unsigned actor, flags, timer, rectangle[4], opcode, executed;
 TaskResult result;
 ScriptTask() : pc(0), actor(0), flags(0x20), timer(0), rectangle{0,0,0,0}, opcode(0), executed(0), result(kDisabled) {}
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
 virtual bool reset(unsigned, ScriptActor &) { return false; }
 virtual bool resource(unsigned, unsigned) { return false; }
};
// A successful request includes ignoring a new effect while one is active,
// as the original single-channel driver does. 255 stops the channel.
class ScriptSound {
public:
 virtual ~ScriptSound() {}
 virtual bool effect(unsigned index) = 0;
 virtual bool music(unsigned index) = 0;
};
class ScriptMachine {
 ScriptSound *_sound;
 ScriptAnimation *_animation;
 StartState _initial;
 uint8_t _block[65536];
 ScriptTask _tasks[17];
 ScriptActor _actors[11];
 unsigned _count;
 uint32_t _randomState;
 unsigned _randomValue;
 bool _randomKnown, _eventPending;
 static int signed16(unsigned v) { v &= 65535; return v < 32768 ? int(v) : int(v) - 65536; }
 bool variable(unsigned id, unsigned &value) const {
  if (id == 33) { value = _randomValue; return _randomKnown; }
  unsigned address;
  if (id == 0) address = 0x2df6;
  else if (id == 34) address = 0x2df0;
  else if (id >= 1000 && id < 2000) address = 0x3b2a + (id - 1000) * 2;
  else if (id >= 2000 && id < 2256) address = 0x392a + (id - 2000) * 2;
  else return false; // Runtime variables and actor aliases need live bindings.
  if (!_initial.range(address, 2)) return false;
  value = _initial.word(address); return true;
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
 uint32_t randomState() const { return _randomState; }
 bool randomValue(unsigned &value) const { value = _randomValue; return _randomKnown; }
 void setEventPending(bool pending) { _eventPending = pending; }
 bool eventPending() const { return _eventPending; }
 ScriptMachine(const ScriptMachine &) = delete;
 ScriptMachine &operator=(const ScriptMachine &) = delete;
 void setAnimation(ScriptAnimation *animation) { _animation = animation; }
 const ScriptActor *actor(unsigned index) const { return index < 11 ? &_actors[index] : nullptr; }
 void setSound(ScriptSound *sound) { _sound = sound; }
 unsigned count() const { return _count; }
 const ScriptTask *task(unsigned i) const { return i < _count ? &_tasks[i] : nullptr; }
 bool setActor(unsigned index, const ScriptActor &actor) {
  if (index >= 11 || actor.x < -32768 || actor.x > 32767 || actor.y < -32768 || actor.y > 32767 ||
      actor.halfWidth > 255 || actor.footHeight > 255 || actor.steps > 65535 || actor.step > 65535) return false;
  _actors[index] = actor; return true;
 }
 bool load(const StartState &state, Cc4View block) {
  _count = 0;
  _randomState = 1; _randomValue = 0; _randomKnown = false; _eventPending = false;
  if (!state.valid() || block.size > sizeof(_block) || cc4Count(block, 2) != state.word(0x2e02) || !state.word(0x2e02)) return false;
  for (unsigned i = 0; i < block.size; ++i) _block[i] = block.data[i];
  block.data = _block;
  _initial = state;
  for (unsigned i = 0; i < state.word(0x2e02); ++i) {
   ScriptTask &t = _tasks[i]; t = ScriptTask();
   if (!cc4Entry(block, 2, i, t.script) || !state.taskPC(block, i, t.pc)) return false;
   unsigned a = 0x3110 + i * 22;
   t.actor = state.byte(a + 8); t.flags = state.byte(a + 9); t.timer = state.word(a + 10);
   for (unsigned j = 0; j < 4; ++j) t.rectangle[j] = state.word(a + 14 + j * 2);
   t.result = t.flags & 0x20 ? kDisabled : kReady;
  }
  for (unsigned i = 0; i < 11; ++i) {
   unsigned a = 0x2e54 + i * 52;
   _actors[i] = {state.signedWord(a), state.signedWord(a + 2), state.byte(a + 21), state.byte(a + 22), state.word(a + 14), state.word(a + 18)};
  }
  _count = state.word(0x2e02); return true;
 }
 TaskResult visit(unsigned index, unsigned budget = 128) {
  if (index >= _count) return kInvalidScript;
  ScriptTask &t = _tasks[index];
  if (t.flags & 0x20) return t.result = kDisabled;
  if (t.flags & ~0x26U) return t.result = kUnknownWait;
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
   case 9:
    t.timer = op.argument; t.flags |= 2; t.pc = next; ++t.executed; return t.result = kTimerWait;
   case 10: {
    unsigned a, b = op.value;
    if (!variable(op.variable, a) || (op.valueIsVariable && !variable(op.value, b))) return t.result = kUnknownVariable;
    if (!compare(a, b, op.comparison)) next = op.target;
    break;
   }
   case 7:
    if (t.actor >= 11) return t.result = kInvalidScript;
    _actors[t.actor].x = op.argument; _actors[t.actor].y = op.value;
    break;
   case 90:
    if (op.argument >= 11) return t.result = kInvalidScript;
    if (op.argument && (!_animation || !_animation->reset(op.argument, _actors[op.argument]))) return t.result = kAnimationUnavailable;
    t.actor = op.argument; break;
   case 91:
    if (t.actor >= 11) return t.result = kInvalidScript;
    if (!_animation || !_animation->resource(t.actor, op.argument)) return t.result = kAnimationUnavailable;
    break;
   case 21:
    if (op.argument >= 11) return t.result = kInvalidScript;
    t.actor = op.argument; break;
   case 27: case 28:
    if (op.argument >= _count) return t.result = kInvalidScript;
    if (op.opcode == 28) _tasks[op.argument].flags |= 0x20;
    else {
     ScriptTask &other = _tasks[op.argument]; other.pc = 0; other.flags = 0; other.timer = 0; other.actor = 0; other.result = kReady;
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
   case 99:
    if (!_sound || !_sound->music(op.argument)) return t.result = kSoundUnavailable;
    break;
   case 77:
    if (!_sound || !_sound->effect(op.argument)) return t.result = kSoundUnavailable;
    break;
   case 79:
    // Borland rand at 0x17b5f; range helper 0x7051 does not draw for 0/1.
    _randomValue = 0;
    if (op.argument >= 2) {
     _randomState = _randomState * uint32_t(0x015a4e35) + 1;
     _randomValue = ((_randomState >> 16) & 0x7fff) % op.argument;
    }
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
 void tick() { for (unsigned i = 1; i < _count; ++i) visit(i); } // Original 0xb31c skips task 0.
};
}
#endif
