// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/script.h"
#include "common/endian.h"
namespace Pice {
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
bool ScriptResource::expression(uint32 &pc,int16 &value,const ScriptVariables *variables) const {
 uint32 next=pc; uint budget=256; int16 result=0;
 value=0;
 if (!evaluate(next,result,0,budget,variables)) return false;
 pc=next; value=result; return true;
}
bool ScriptResource::evaluate(uint32 &pc,int16 &value,uint depth,uint &budget,const ScriptVariables *variables) const {
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
 // Pure comparisons, eager Boolean operators, and arithmetic only.
 if (op<7 || op>16) return false;
 int16 a,b;
 if (!evaluate(pc,a,depth+1,budget,variables) || !evaluate(pc,b,depth+1,budget,variables)) return false;
 switch(op) {
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
bool ScriptRunner::start(uint index) {
 if (index>=_tasks.size()) return false;
 _tasks[index].pc=_bank.entry(index); _tasks[index].active=true; return true;
}
ScriptRunner::Result ScriptRunner::step(uint index) {
 if (index>=_tasks.size()) return kInvalid;
 Task &task=_tasks[index];
 if (!task.active) return kStopped;
 uint16 opcode;
 if (!_bank.word(task.pc,opcode)) { task.active=false; return kInvalid; }
 uint32 next=task.pc;
 int16 argument;
 if (opcode==3 || opcode==31) {
  next+=2;
  if (!_bank.expression(next,argument,_variables) || argument<0 || uint(argument)>=_tasks.size()) {
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
  uint16 index;
  next+=2;
  if (!_bank.word(next,index)) { task.active=false; return kInvalid; }
  next+=2;
  if (!_bank.expression(next,argument,_variables) || !_variables->write(opcode==26,int16(index),argument)) {
   task.active=false; return kInvalid;
  }
  task.pc=next; return kAdvanced;
 }
 if (opcode==4 || opcode==21) {
  bool jump=true;
  if (opcode==21) {
   next+=2;
   if (!_bank.expression(next,argument,_variables)) { task.active=false; return kInvalid; }
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
 if (opcode==5 || opcode==19 || opcode==20 || (opcode>=7 && opcode<=16)) {
  if (!_bank.expression(next,argument,_variables)) { task.active=false; return kInvalid; }
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
 bad=data; WRITE_LE_UINT16(&bad[4],138);
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
 WRITE_LE_UINT16(&bytes[4],138);
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
 return fault.step(0)==ScriptRunner::kInvalid && fault.position(0)==2;
}
}
