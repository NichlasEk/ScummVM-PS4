// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_COLLISION_H
#define SOTC_COLLISION_H
#include "walk.h"
namespace Sotc {
struct Footprint { int left, top, right, bottom; };
inline Footprint footprint(int x,int y,unsigned width,unsigned height) {
 Footprint r={walkWord(x-int(width)),walkWord(y-int(height)),walkWord(x+int(width)),y}; return r;
}
inline bool intersects(const Footprint &a,const Footprint &b) {
 return a.left<=b.right && a.right>=b.left && a.top<=b.bottom && a.bottom>=b.top;
}
// Original 0xcf9d. Outside the original routine's safe lookup range, reject
// rather than indexing beyond the 320-column terrain array.
inline unsigned terrainCollision(const unsigned char *terrain,const Footprint &r,unsigned direction) {
 int left=r.left<0 ? 0 : r.left, right=r.right>319 ? 319 : r.right;
 if (left>319 || right<0) return 0x100;
 unsigned result=0;
 if (direction==1 && (r.top<=terrain[left] || r.top<=terrain[right])) result=0x100;
 if (direction==2 && (terrain[right]>=r.top || right==319)) result=0x100;
 if (direction==4 && (terrain[left]>=r.top || left==0)) result=0x100;
 if (r.bottom>199) result=0x200;
 return result;
}
inline unsigned obstacleCollision(const unsigned char *rectangles,unsigned count,const Footprint &r) {
 for (unsigned i=0;i<count;++i) {
  const unsigned char *p=rectangles+i*4;
  Footprint b={p[0]*2,p[1],p[2]*2,p[3]};
  if (b.left!=b.right && intersects(r,b)) return 0x300|i;
 }
 return 0;
}
struct CollisionActor { Footprint bounds; bool visible, bypass; };
inline unsigned actorCollision(const CollisionActor *actors,unsigned self,const Footprint &r) {
 for (unsigned i=0;i<11;++i)
  if (i!=self && actors[i].visible && !actors[i].bypass && intersects(r,actors[i].bounds)) return 0x600|i;
 return 0;
}
}
#endif
