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
 unsigned rectangle[4];
 unsigned choiceText[6]={},choiceTarget[6]={};
 ScriptInstruction() : opcode(0), next(0), target(-1), argument(0),
  variable(0), comparison(0), value(0), valueIsVariable(false), rectangle{0, 0, 0, 0} {}
};
inline ScriptDecode decodeScript(Cc4View v, uint32_t pc, ScriptInstruction &out) {
 out = ScriptInstruction(); out.next = pc;
 if (!v.data || pc >= v.size) return kTruncated;
 out.opcode = v.data[pc];
 uint32_t p = pc + 1;
 switch (out.opcode) {
 case 2: // Disable current task, without advancing original IP (0x9f9f).
  return kInstruction;
 case 6: // Counted relative loop, then byte iteration limit.
 case 64: // Pending event, branch when absent (0xa713).
 case 3: // Relative jump; displacement base is the operand, not its end.
  break;
 case 31: case 32: case 33: case 56: {
  if (v.size-p<3) return kTruncated;
  out.variable=cc4Word(v.data+p,2); p+=2;
  unsigned tag=v.data[p++];
  if (tag!=11 && tag!=12 && tag!=13) return kUnsupported;
  unsigned width=tag==12 ? 1 : 2;
  if (v.size-p<width) return kTruncated;
  out.valueIsVariable=tag==11; out.value=cc4Word(v.data+p,width);
  out.next=p+width; return kInstruction;
 }
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
 case 53: { // Header text, X/2, Y, count, then (text, relative branch) pairs.
  if(v.size-p<5) return kTruncated;
  out.value=cc4Word(v.data+p,2);p+=2;
  out.variable=v.data[p++]*2;out.comparison=v.data[p++];out.argument=v.data[p++];
  if(!out.argument || out.argument>6) return kInvalid;
  if(v.size-p<out.argument*4) return kTruncated;
  for(unsigned i=0;i<out.argument;++i) {
   out.choiceText[i]=cc4Word(v.data+p,2);p+=2;
   unsigned raw=cc4Word(v.data+p,2);int target=int(p)+(raw<32768 ? int(raw) : int(raw)-65536);
   if(target<0 || unsigned(target)>=v.size) return kInvalid;
   out.choiceTarget[i]=target;p+=2;
  }
  out.next=p;return kInstruction;
 }
 case 82: case 89: // Speaker, LE16 text, sequence; portrait adds resource.
  if (v.size-p < (out.opcode==89 ? 5U : 4U)) return kTruncated;
  out.argument=v.data[p++]; out.value=cc4Word(v.data+p,2); p+=2;
  out.comparison=v.data[p++];
  out.variable=out.opcode==89 ? v.data[p++] : 256;
  out.next=p; return kInstruction;
 case 25: // Inventory hotspot: byte ID, X/2, Y (0x9e19).
  if(v.size-p<3) return kTruncated;
  out.argument=v.data[p++];out.variable=v.data[p++]*2;out.value=v.data[p++];
  out.next=p;return kInstruction;
 case 36: // Scripted actor destination, byte X doubled and byte Y.
 case 7: // Absolute actor position: byte X doubled, byte Y (0xa3ab).
  if (v.size - p < 2) return kTruncated;
  out.argument = v.data[p++] * 2; out.value = v.data[p++];
  out.next = p; return kInstruction;
 case 23: // Exit records: type, bank, room, left X, right X.
 case 22: // Byte count followed by byte pairs; original doubles X.
  if (p >= v.size) return kTruncated;
  out.argument = v.data[p++];
  if (v.size - p < out.argument * (out.opcode==23 ? 5 : 2)) return kTruncated;
  out.next = p + out.argument * (out.opcode==23 ? 5 : 2);
  return kInstruction;
 case 34: // Actor collision bypass, no operand (0x9369).
 case 19: // Unlock player controls (0xaac9).
 case 41: // Hide the current non-player actor (0x9e9e).
 case 39:
  out.next = p; return kInstruction;
 case 14: // Footprint rectangle and false branch (0xa60d).
 case 66: // Event plus footprint rectangle, then false-branch displacement.
 case 60: // Append obstacle rectangle (0xa82d).
 case 40:
  if (v.size - p < 4) return kTruncated;
  for (unsigned i = 0; i < 4; ++i) out.rectangle[i] = v.data[p + i] * ((i & 1) ? 1 : 2);
  p += 4;
  if (out.opcode == 66 || out.opcode == 14) break;
  out.next = p; return kInstruction;
 case 1: case 4: case 5: case 9: case 15: case 16: case 18: case 21: case 27: case 28: case 58: case 59:
 case 98: case 69: case 44: case 93: case 94: case 99: case 77: case 79: case 80: case 87: case 88: case 90: case 91: case 92: case 96:
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
 if (out.opcode==6) {
  if (out.next>=v.size) return kTruncated;
  out.argument=v.data[out.next++];
 }
 // Cross-script branches have not been verified; expose rather than follow.
 if (out.target < 0 || uint32_t(out.target) >= v.size) return kInvalid;
 return kInstruction;
}
}
#endif
