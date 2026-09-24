// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_ANIMATION_H
#define SOTC_ANIMATION_H
namespace Sotc {
// One completed render/update pair: EXE 0xc1c9 and 0x7b92.
// Ordinary frames do not interpret the control parameter as a delay.
inline bool advanceAnimation(unsigned control, unsigned mode, unsigned gate,
                             unsigned count, unsigned &step, unsigned &substep, bool *wrapped = nullptr) {
 if (wrapped) *wrapped=false;
 // Original 0x7b30 tests the hold gate before reading the sequence count.
 // Facing a held actor can leave mode 0 with count 0; it remains a valid pose.
 if(gate!=255 && (mode==0 || mode==2) && (control>>14)<=1){substep=0;return true;}
 if (!count || step >= count || mode > 2 || mode == 1 || (control >> 14) > 1) return false;
 unsigned kind = control >> 14, parameter = control & 0x3fff;
 if (mode == 2) {
  if (kind == 1) { substep = (substep + 1) & 65535; if (substep >= parameter) substep = 0; }
  else substep = 0;
 }
 if (gate != 255) { substep = 0; return true; }
 if (kind != 1 || substep >= (parameter ? parameter : 1) - 1) {
  if (kind == 1) substep = 0;
  if (wrapped) *wrapped=step+1==count;
  step = (step + 1) % count;
 }
 return true;
}
}
#endif
