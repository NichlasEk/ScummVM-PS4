// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_STARTSTATE_H
#define SOTC_STARTSTATE_H
#include "cc4.h"
#include "snapshot.h"
namespace Sotc {
// DOS save layout verified against SHADOW.EXE 0x2508..0x25ee and 0xe4b5.
// Only the serialized state is retained; DOS far pointers are never followed.
class StartState {
 uint8_t _bytes[0xf3e];
 bool _valid;
public:
 StartState() : _valid(false) {}
 bool load(Cc4View file) {
  _valid = false;
  if (!file.data || file.size != 64 + sizeof(_bytes)) return false;
  const uint8_t *p = file.data;
  if (p[0] != 'N' || p[1] != 'E' || p[2] != 'D' || p[3] != 0) return false;
  for (unsigned i = 0; i < sizeof(_bytes); ++i)
   _bytes[i] = p[64 + i] ^ uint8_t((i + 1) * 5); // Original 0xe26a.
  _valid = true;
  if (word(0x2df2) > 9 || word(0x2e02) > 17 || byte(0x34de) > 20) _valid = false;
  return _valid;
 }
 bool valid() const { return _valid; }
 const uint8_t *range(unsigned address, unsigned length) const {
  if (!_valid || address < 0x2dee || address > 0x3d2c || length > 0x3d2c - address) return nullptr;
  return _bytes + address - 0x2dee;
 }
 unsigned byte(unsigned address) const { const uint8_t *p = range(address, 1); return p ? p[0] : 0; }
 unsigned word(unsigned address) const { const uint8_t *p = range(address, 2); return p ? cc4Word(p, 2) : 0; }
 int signedWord(unsigned address) const { unsigned v = word(address); return v < 32768 ? int(v) : int(v) - 65536; }
 bool setByte(unsigned address,unsigned value) {
  if (!range(address,1) || value>255) return false;
  _bytes[address-0x2dee]=uint8_t(value); return true;
 }
 bool setWord(unsigned address,unsigned value) {
  if (!range(address,2) || value>65535) return false;
  setByte(address,value&255); setByte(address+1,value>>8); return true;
 }
 bool snapshot(Snapshot &s) {
  s.require(_valid);
  for(unsigned i=0;i<sizeof(_bytes);++i) {
   unsigned v=_bytes[i]; s.value(v,255); if(s.reading()) _bytes[i]=uint8_t(v);
  }
  s.require(word(0x2df2)<=9 && word(0x2df8)<=255 && word(0x2e02)<=17 && byte(0x34de)<=20);
  return s.ok();
 }
 // Saved IP - saved base, rebased onto the corresponding current CC4 script.
 // Matches original 0xe12c. Segment values are intentionally ignored.
 bool taskPC(Cc4View block, unsigned task, uint32_t &pc) const {
  Cc4View script;
  if (!_valid || task >= word(0x2e02) || !cc4Entry(block, 2, task, script)) return false;
  unsigned address = 0x3110 + task * 22;
  int delta = int(word(address + 4)) - int(word(address));
  if (delta < 0 || unsigned(delta) >= script.size) return false;
  pc = delta; return true;
 }
};
}
#endif
