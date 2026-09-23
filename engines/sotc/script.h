// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_SCRIPT_H
#define SOTC_SCRIPT_H
#include "cc4.h"
namespace Sotc {
// Decoding only: no guessed variable state, scheduling or command execution.
enum ScriptDecode { kInstruction, kUnsupported, kTruncated, kInvalid };
struct ScriptInstruction {
 unsigned opcode;
 uint32_t next;
 int32_t target;
 unsigned argument, variable, comparison, value;
 bool valueIsVariable;
 ScriptInstruction() : opcode(0), next(0), target(-1), argument(0),
  variable(0), comparison(0), value(0), valueIsVariable(false) {}
};
inline ScriptDecode decodeScript(Cc4View v, uint32_t pc, ScriptInstruction &out) {
 out = ScriptInstruction(); out.next = pc;
 if (!v.data || pc >= v.size) return kTruncated;
 out.opcode = v.data[pc];
 uint32_t p = pc + 1;
 switch (out.opcode) {
 case 2: // Disable current task, without advancing original IP (0x9f9f).
  return kInstruction;
 case 3: // Relative jump; displacement base is the operand, not its end.
  break;
 case 10: { // Compare variable with literal/variable, branch when false.
  if (v.size - p < 4) return kTruncated;
  out.variable = cc4Word(v.data + p, 2); p += 2;
  out.comparison = v.data[p++];
  if (out.comparison > 7) return kInvalid;
  unsigned token = v.data[p++];
  out.valueIsVariable = token == 11;
  // The EXE treats other tags as word literals. Restrict to proven 12/13.
  if (token != 11 && token != 12 && token != 13) return kUnsupported;
  unsigned width = token == 12 ? 1 : 2;
  if (v.size - p < width) return kTruncated;
  out.value = cc4Word(v.data + p, width); p += width;
  break;
 }
 case 22: // Byte count followed by byte pairs; original doubles X.
  if (p >= v.size) return kTruncated;
  out.argument = v.data[p++];
  if (v.size - p < out.argument * 2) return kTruncated;
  out.next = p + out.argument * 2;
  return kInstruction;
 case 1: case 4: case 5: case 27: case 28: case 58: case 59:
 case 80: case 87: case 88: case 96:
  if (p >= v.size) return kTruncated;
  out.argument = v.data[p++]; out.next = p;
  return kInstruction;
 default:
  return kUnsupported; // Includes expression tags and original no-op handlers.
 }
 if (v.size - p < 2) return kTruncated;
 unsigned raw = cc4Word(v.data + p, 2);
 int32_t relative = raw < 32768 ? int32_t(raw) : int32_t(raw) - 65536;
 // CC4 room records fit in 16 bits; avoid overflow on hostile larger inputs.
 if (p > 65535) return kInvalid;
 out.target = int32_t(p) + relative;
 out.next = p + 2;
 // Cross-script branches have not been verified; expose rather than follow.
 if (out.target < 0 || uint32_t(out.target) >= v.size) return kInvalid;
 return kInstruction;
}
}
#endif
