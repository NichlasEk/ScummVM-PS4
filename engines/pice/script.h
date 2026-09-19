// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PICE_SCRIPT_H
#define PICE_SCRIPT_H
#include "common/array.h"
namespace Pice {
// Entry addresses are uint16 word offsets; entry zero is also the table size.
// Verified against ICE640 object 2: 0x1de and 0x21e.
class ScriptResource {
public:
 bool load(const Common::Array<byte> &data);
 uint count() const { return _entries.size(); }
 uint32 entry(uint index) const { return _entries[index]; }
 // Decode a side-effect-free expression subset. Unsupported opcodes fail
 // without committing the caller's PC. No scene effects are guessed.
 bool expression(uint32 &pc, int16 &value) const;
 bool word(uint32 offset, uint16 &value) const;
private:
 bool evaluate(uint32 &pc, int16 &value, uint depth, uint &budget) const;
 Common::Array<byte> _data;
 Common::Array<uint32> _entries;
};
// One script bank, stepped explicitly. Resource ownership, timing and full
// scheduler ordering are not implemented yet.
class ScriptRunner {
public:
 enum Result { kAdvanced, kStopped, kUnsupported, kInvalid };
 explicit ScriptRunner(const ScriptResource &bank) : _bank(bank) { _tasks.resize(bank.count()); }
 bool start(uint index);
 bool running(uint index) const { return index<_tasks.size() && _tasks[index].active; }
 uint32 position(uint index) const { return _tasks[index].pc; }
 Result step(uint index);
private:
 struct Task { uint32 pc=0; bool active=false; };
 const ScriptResource &_bank;
 Common::Array<Task> _tasks;
};
bool runScriptTests();
}
#endif
