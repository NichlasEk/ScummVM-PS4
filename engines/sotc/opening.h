// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_OPENING_H
#define SOTC_OPENING_H
#include "cc4.h"
namespace Sotc {
// The supported prefix ends before the first animated credit actor. This is
// deliberately not an interpreter that silently discards unknown commands.
struct OpeningLogo { unsigned background, fadeIn, hold, fadeOut, black; };
struct OpeningPlan {
 OpeningLogo logos[2]; unsigned music, musicDelay, continuation;
 bool load(Cc4View archive) {
  Cc4View room, init, main, audio;
  if (!cc4Entry(archive,4,0,room) || !cc4Entry(room,2,0,init) ||
      !cc4Entry(room,2,2,main) || !cc4Entry(room,2,3,audio)) return false;
  OpeningPlan candidate = {};
  unsigned pc=0, op=0, arg=0; bool background=false, fade=false;
  while (pc<init.size) {
   op=init.data[pc++];
   if (op==2) break;
   if (pc>=init.size) return false;
   arg=init.data[pc++];
   switch (op) {
   case 58: if (background) return false; candidate.logos[0].background=arg*2; background=true; break;
   case 93: if (!arg || fade) return false; candidate.logos[0].fadeIn=arg; fade=true; break;
   case 96: case 92: case 18: case 27: break; // No speech, actors or controls in this logo-only prefix.
   default: return false;
   }
  }
  if (op!=2 || !background || !fade || pc!=init.size) return false;
  pc=0;
  auto operand=[&](unsigned expected,unsigned &value) {
   if (pc>=main.size || main.size-pc<2 || main.data[pc]!=expected) return false;
   value=main.data[pc+1]; pc+=2; return true;
  };
  if (!operand(9,candidate.logos[0].hold) || !operand(94,candidate.logos[0].fadeOut) ||
      !operand(58,arg)) return false;
  candidate.logos[1].background=arg*2;
  if (!operand(9,candidate.logos[1].black) || !operand(93,candidate.logos[1].fadeIn) ||
      !operand(9,candidate.logos[1].hold) || !operand(94,candidate.logos[1].fadeOut)) return false;
  if (pc>=main.size || main.data[pc]!=90 || !candidate.logos[0].fadeOut ||
      !candidate.logos[1].fadeIn || !candidate.logos[1].fadeOut) return false;
  if (audio.size<4 || audio.data[0]!=9 || audio.data[2]!=77) return false;
  candidate.continuation=pc;
  candidate.musicDelay=audio.data[1]; candidate.music=audio.data[3];
  if (candidate.music==255) return false;
  *this=candidate; return true;
 }
};
// UI clock: 10 Hz scene timers and 60 Hz palette steps. The original DOS
// timer/retrace cadence has not yet been reproduced exactly.
class OpeningClock {
 OpeningPlan _plan;
 unsigned _logo=0, _phase=0, _elapsed=0, _total=0;
 bool _done=false;
 unsigned duration() const {
  const OpeningLogo &l=_plan.logos[_logo];
  if (_phase==0) return ((255+l.fadeIn-1)/l.fadeIn)*16;
  if (_phase==1) return l.hold*100;
  if (_phase==2) return ((255+l.fadeOut-1)/l.fadeOut)*16;
  return _plan.logos[1].black*100;
 }
public:
 explicit OpeningClock(const OpeningPlan &plan) : _plan(plan) {}
 bool done() const { return _done; }
 unsigned logo() const { return _logo; }
 bool musicDue() const { return _total>=_plan.musicDelay*100; }
 unsigned brightness() const {
  if (_done || _phase==3) return 0;
  if (_phase==1) return 255;
  unsigned value=(_elapsed/16)*(_phase==0 ? _plan.logos[_logo].fadeIn : _plan.logos[_logo].fadeOut);
  if (value>255) value=255;
  return _phase==0 ? value : 255-value;
 }
 void advance(unsigned milliseconds) {
  if (_done) return;
  _total+=milliseconds; _elapsed+=milliseconds;
  for (unsigned budget=0;budget<8 && !_done && _elapsed>=duration();++budget) {
   _elapsed-=duration();
   if (_phase==3) { _logo=1; _phase=0; }
   else if (_phase<2) ++_phase;
   else if (!_logo) _phase=3;
   else _done=true;
  }
 }
};
}
#endif
