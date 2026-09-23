// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_ROUTE_H
#define SOTC_ROUTE_H
namespace Sotc {
// Bounded four-neighbour search. This is a diagnostic planner, not the DOS
// destination algorithm. The caller supplies verified collision predicates.
class Route {
 unsigned char *_parent;
 unsigned *_queue;
public:
 unsigned *points, count;
 Route() : _parent(new unsigned char[64000]), _queue(new unsigned[64000]), points(new unsigned[64000]), count(0) {}
 ~Route() { delete[] _parent; delete[] _queue; delete[] points; }
 // Only the chosen route persists; search scratch buffers are rebuilt by find().
 Route(const Route &other) : Route() { *this=other; }
 Route &operator=(const Route &other) {
  count=other.count; for(unsigned i=0;i<count;++i) points[i]=other.points[i]; return *this;
 }
 template<class Passable> bool find(int sx,int sy,int gx,int gy,Passable passable,bool nearest=false) {
  count=0;
  if (sx<0 || sx>=320 || sy<0 || sy>=200 || gx<0 || gx>=320 || gy<0 || gy>=200) return false;
  for (unsigned i=0;i<64000;++i) _parent[i]=0;
  unsigned start=sy*320+sx,goal=gy*320+gx,head=0,tail=1;
  _queue[0]=start; _parent[start]=255;
  unsigned closest=start;int best=(sx-gx)*(sx-gx)+(sy-gy)*(sy-gy);
  const int dx[4]={0,1,0,-1},dy[4]={-1,0,1,0};
  while (head<tail && !_parent[goal]) {
   unsigned p=_queue[head++]; int x=p%320,y=p/320;
   int distance=(x-gx)*(x-gx)+(y-gy)*(y-gy);
   if(distance<best){best=distance;closest=p;}
   for (unsigned d=0;d<4;++d) {
    int nx=x+dx[d],ny=y+dy[d];
    if (nx<0 || nx>=320 || ny<0 || ny>=200) continue;
    unsigned n=ny*320+nx;
    if (_parent[n] || !passable(nx,ny,d+1)) continue;
    _parent[n]=d+1; _queue[tail++]=n;
   }
  }
  if (!_parent[goal]) {if(!nearest || closest==start)return false;goal=closest;}
  for (unsigned p=goal;p!=start;) {
   points[count++]=p;
   unsigned d=_parent[p]-1;
   p=unsigned(int(p)-dx[d]-dy[d]*320);
  }
  return true;
 }
};
}
#endif
