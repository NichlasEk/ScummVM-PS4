// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PICE_SCRIPT_H
#define PICE_SCRIPT_H
#include "common/array.h"
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
 bool codeAddress(int64 offset) const;
 bool word(uint32 offset, uint16 &value) const;
private:
 bool evaluate(uint32 &pc, int16 &value, uint depth, uint &budget, const ScriptVariables *variables) const;
 Common::Array<byte> _data;
 Common::Array<uint32> _entries;
};
// One script bank, stepped explicitly. Resource ownership, timing and full
// scheduler ordering are not implemented yet.
class ScriptRunner {
public:
 enum Result { kAdvanced, kStopped, kUnsupported, kInvalid, kBudgetExhausted };
 explicit ScriptRunner(const ScriptResource &bank, ScriptVariables *variables=nullptr) : _bank(bank), _variables(variables ? variables : &_ownedVariables) { _tasks.resize(bank.count()); }
 bool start(uint index);
 bool running(uint index) const { return index<_tasks.size() && _tasks[index].active; }
 uint32 position(uint index) const { return _tasks[index].pc; }
 Result step(uint index);
 Result run(uint index, uint instructionBudget);
 ScriptVariables &variables() { return *_variables; }
private:
 struct Task { uint32 pc=0; bool active=false; };
 const ScriptResource &_bank;
 Common::Array<Task> _tasks;
 ScriptVariables _ownedVariables;
 ScriptVariables *_variables;
};
bool runScriptTests();
}
#endif
