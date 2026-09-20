// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PICE_SCRIPT_H
#define PICE_SCRIPT_H
#include "common/array.h"
#include "common/str.h"
#include "pice/room.h"
namespace Pice {
class StateIO;
class ScriptSceneSink;
// Two 16-bit tables: sizes established by save/load routines at 0xbe8d/0xcf49.
class ScriptVariables {
public:
 ScriptVariables() { _a.resize(250,0); _a[0]=-1; _b.resize(300,-1); }
 bool syncState(StateIO &io);
 bool read(bool second, int16 index, int16 &value) const;
 bool write(bool second, int16 index, int16 value);
private:
 Common::Array<int16> _a, _b;
};
// Entry addresses are uint16 word offsets; entry zero is also the table size.
// Verified against ICE640 object 2: 0x1de and 0x21e.
class ScriptResource {
public:
 bool load(const Common::Array<byte> &data);
 uint count() const { return _entries.size(); }
 uint32 entry(uint index) const { return _entries[index]; }
 // Decode verified expressions, including bounded random values. Failure
 // restores the caller's PC and RNG state. No scene effects are guessed.
 bool expression(uint32 &pc, int16 &value, const ScriptVariables *variables=nullptr, const ScriptSceneSink *scene=nullptr) const;
 bool string(uint32 &pc, Common::String &value) const;
 bool codeAddress(int64 offset) const;
 bool word(uint32 offset, uint16 &value) const;
private:
 bool evaluate(uint32 &pc, int16 &value, uint depth, uint &budget, const ScriptVariables *variables, const ScriptSceneSink *scene) const;
 Common::Array<byte> _data;
 Common::Array<uint32> _entries;
};
// Only verified resource commands. Return false without changing the scene
// when unsupported; the runner then stops without advancing the instruction.
class ScriptSceneSink {
public:
 virtual ~ScriptSceneSink() {}
 virtual bool randomValue(int16 bound,int16 &value) const { return false; }
 virtual uint32 randomState() const { return 0; }
 virtual void restoreRandomState(uint32 state) const {}
 virtual bool enterRoom(const Common::String &name,const Common::Array<int16> &args,int16 &actor) { return false; }
 virtual bool gameOver(int16 reason) { return false; }
 virtual bool pollMenu(int16 menu,bool &ready) { return false; }
 virtual bool stateValue(int16 kind,int16 id,int16 &value) const { return false; }
 virtual bool advanceRequested() const { return false; }
 virtual bool context(int16 index,int16 &value) const { return false; }
 virtual bool background(const Common::String &name,int16 local,int16 flags)=0;
 virtual bool scenery(const Common::String &name,int16 local) { return false; }
 virtual bool closeupDialogue(const Common::String &animationName,const Common::String &textName,const Common::String &key,const Common::Array<int16> &args,int16 &actor) { return false; }
 virtual bool requestSave(const Common::String &name,int16 local,const Common::String &key) { return false; }
 virtual bool playMovie(const Common::String &name,int16 before,int16 after) { return false; }
 virtual bool changeRoom(int16 chapter,int16 room) { return false; }
 virtual bool narrationBusy() const { return false; }
 virtual bool describeObject(int16 x,int16 y,const Common::String &name,int16 local,const Common::String &key) { return false; }
 virtual bool narrate(int16 x,int16 y,const Common::String &name,int16 local,const Common::String &key) { return false; }
 virtual bool dialogueBlocked(uint task) const { return false; }
 virtual bool speakDialogue(int16 actor,const Common::String &name,int16 local,const Common::String &key) { return false; }
 virtual bool objectDialogue(int16 actor,const Common::String &name,int16 local,const Common::String &key) { return false; }
 virtual bool animatedDialogue(const Common::String &textName,const Common::String &key,const Common::String &animationName,const Common::Array<int16> &args) { return false; }
 virtual bool actorCoordinate(int16 actor,bool vertical,int16 &value) const { return false; }
 virtual bool actorInsideHotspot(int16 actor,int16 hotspot,bool &inside) const { return false; }
 virtual bool actorStepReached(int16 actor,int16 step) const { return !actorBusy(actor); }
 virtual bool soundBusy(int16 sound) const { return false; }
 virtual bool actorBusy(int16 actor) const { return false; }
 virtual bool createActor(const Common::String &name,int16 local,int16 sequence,int16 node,int16 &id) { return false; }
 virtual bool invoke(uint16 opcode,const Common::String &name,const Common::Array<int16> &args,const Common::String &key,int16 &result) { return false; }
 virtual bool createMenu(const Common::String &name,int16 local,int16 &id) { return false; }
 virtual bool createItem(const Common::String &name,int16 local,int16 sequence,int16 &id) { return false; }
 virtual bool volume(int16 handle,int16 value) { return false; }
 virtual bool palette(const Common::String &name,int16 local)=0;
};
// One script bank, stepped explicitly. Resource ownership and full
// scheduler ordering are not implemented yet.
class ScriptRunner {
public:
 enum Result { kAdvanced, kStopped, kUnsupported, kInvalid, kBudgetExhausted, kWaiting };
 explicit ScriptRunner(const ScriptResource &bank, ScriptVariables *variables=nullptr, ScriptSceneSink *scene=nullptr) : _bank(bank), _variables(variables ? variables : &_ownedVariables), _scene(scene) { _tasks.resize(bank.count()); }
 bool syncState(StateIO &io);
 bool start(uint index);
 bool resumeStoppedAt(uint index,uint16 expectedOpcode);
 void prioritizeInitialization() { _entryInitializer=0; }
 int exclusiveTask() const {
  // Synchronous initialization completes before the entry continuation owns
  // the scheduler (original 0x1c728 / 0x6ac).
  if(_entryInitializer>=0 && running(_entryInitializer)) return _entryInitializer;
  return _exclusiveTask>=0 && running(_exclusiveTask) ? _exclusiveTask : -1;
 }
 bool running(uint index) const { return index<_tasks.size() && _tasks[index].active; }
 uint32 position(uint index) const { return _tasks[index].pc; }
 bool waiting(uint index) const { return running(index) && (_tasks[index].waiting || _tasks[index].actor!=-1 || _tasks[index].peer>=0 || _tasks[index].triggerActor>=0 || _tasks[index].locationActor>=0); }
 // One original dispatcher visit. An expiring wait yields this visit too.
 Result step(uint index, uint32 elapsedTicks=0);
 Result run(uint index, uint instructionBudget);
 RoomNavigation &navigation() { return _navigation; }
 ScriptVariables &variables() { return *_variables; }
private:
 bool value(uint32 &pc,int16 &result);
 // actor >= 0 waits for an actor; actor <= -2 encodes sound (-actor-2).
 struct Task { uint32 pc=0; bool active=false, waiting=false; int16 actor=-1,peer=-1,triggerActor=-1,triggerStep=0; bool barrierReady=false; int32 remainingTicks=0; int16 locationActor=-1,locationHotspot=-1; bool locationInside=false; };
 int _exclusiveTask=-1, _entryInitializer=-1;
 const ScriptResource &_bank;
 Common::Array<Task> _tasks;
 RoomNavigation _navigation;
 ScriptVariables _ownedVariables;
 ScriptVariables *_variables;
 ScriptSceneSink *_scene;
};
bool runScriptTests();
}
#endif
