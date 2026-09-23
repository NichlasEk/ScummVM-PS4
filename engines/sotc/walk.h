// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_WALK_H
#define SOTC_WALK_H
namespace Sotc {
inline int walkWord(int v) { unsigned u=unsigned(v)&65535; return u<32768 ? int(u) : int(u)-65536; }
// Original 0x8242: adjust sequence deltas, with 16-bit arithmetic.
inline void walkDelta(unsigned direction, int speed, int driftX, int driftY,
                      bool sameRoom, int &dx, int &dy) {
 if (dy>=1) { dy=walkWord(dy+speed); if (dy<1) dy=1; }
 else if (dy<=-1) { dy=walkWord(dy-speed); if (dy>-1) dy=-1; }
 if (!sameRoom) return;
 if (direction==1) dx=walkWord(dx+driftX);
 if (direction==2) dy=walkWord(dy+driftY);
 if (direction==3) dx=walkWord(dx-driftX);
 if (direction==4) dy=walkWord(dy-driftY);
}
}
#endif
