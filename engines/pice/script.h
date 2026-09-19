// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PICE_SCRIPT_H
#define PICE_SCRIPT_H
#include "common/array.h"
#include "common/str.h"
namespace Pice {
// Two 16-bit tables: sizes established by save/load routines at 0xbe8d/0xcf49.
class ScriptVariables {
public:
 ScriptVariables() { _a.resize(250,0); _a[0]=-1; _b.resize(300,-1); }
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
 // Decode a side-effect-free expression subset. Unsupported opcodes fail
 // without committing the caller's PC. No scene effects are guessed.
 bool expression(uint32 &pc, int16 &value, const ScriptVariables *variables=nullptr) const;
 bool string(uint32 &pc, Common::String &value) const;
 bool codeAddress(int64 offset) const;
 bool word(uint32 offset, uint16 &value) const;
private:
 bool evaluate(uint32 &pc, int16 &value, uint depth, uint &budget, const ScriptVariables *variables) const;
 Common::Array<byte> _data;
 Common::Array<uint32> _entries;
};
// Only verified resource commands. Return false without changing the scene
// when unsupported; the runner then stops without advancing the instruction.
class ScriptSceneSink {
public:
 virtual ~ScriptSceneSink() {}
 virtual bool background(const Common::String &name,int16 local,int16 flags)=0;
 virtual bool palette(const Common::String &name,int16 local)=0;
};
// One script bank, stepped explicitly. Resource ownership and full
// scheduler ordering are not implemented yet.
class ScriptRunner {
public:
 enum Result { kAdvanced, kStopped, kUnsupported, kInvalid, kBudgetExhausted, kWaiting };
 explicit ScriptRunner(const ScriptResource &bank, ScriptVariables *variables=nullptr, ScriptSceneSink *scene=nullptr) : _bank(bank), _variables(variables ? variables : &_ownedVariables), _scene(scene) { _tasks.resize(bank.count()); }
 bool start(uint index);
 bool running(uint index) const { return index<_tasks.size() && _tasks[index].active; }
 uint32 position(uint index) const { return _tasks[index].pc; }
 bool waiting(uint index) const { return running(index) && _tasks[index].waiting; }
 // One original dispatcher visit. An expiring wait yields this visit too.
 Result step(uint index, uint32 elapsedTicks=0);
 Result run(uint index, uint instructionBudget);
 ScriptVariables &variables() { return *_variables; }
private:
 struct Task { uint32 pc=0; bool active=false, waiting=false; int32 remainingTicks=0; };
 const ScriptResource &_bank;
 Common::Array<Task> _tasks;
 ScriptVariables _ownedVariables;
 ScriptVariables *_variables;
 ScriptSceneSink *_scene;
};
bool runScriptTests();
}
#endif
