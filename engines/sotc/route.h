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
 // Subpixel travel credit is transient; saved routes retain their pixel points.
 unsigned credit=0;
 Route() : _parent(new unsigned char[64000]), _queue(new unsigned[64000]), points(new unsigned[64000]), count(0) {}
 ~Route() { delete[] _parent; delete[] _queue; delete[] points; }
 // Only the chosen route persists; search scratch buffers are rebuilt by find().
 Route(const Route &other) : Route() { *this=other; }
 Route &operator=(const Route &other) {
  credit=other.credit; count=other.count; for(unsigned i=0;i<count;++i) points[i]=other.points[i]; return *this;
 }
 // Test both sides of a diagonal so a smoothed line cannot cut a corner.
 template<class Passable> static bool clearStep(int x,int y,int nx,int ny,Passable passable) {
  unsigned horizontal=nx>x ? 2 : 4,vertical=ny>y ? 3 : 1;
  if(nx!=x && !passable(nx,ny,horizontal))return false;
  if(ny!=y && !passable(nx,ny,vertical))return false;
  return nx==x || ny==y || (passable(nx,y,horizontal) && passable(x,ny,vertical));
 }
 template<class Passable> bool line(int x,int y,int gx,int gy,Passable passable,unsigned &written,bool write) {
  int dx=gx>x ? gx-x : x-gx,dy=gy>y ? y-gy : gy-y;
  int sx=x<gx ? 1 : -1,sy=y<gy ? 1 : -1,error=dx+dy;
  while(x!=gx || y!=gy) {
   int nx=x,ny=y,twice=error*2;
   if(twice>=dy){error+=dy;nx+=sx;}
   if(twice<=dx){error+=dx;ny+=sy;}
   if(!clearStep(x,y,nx,ny,passable))return false;
   if(write){if(written>=64000)return false;_queue[written++]=ny*320+nx;}
   x=nx;y=ny;
  }
  return true;
 }
 template<class Passable> void smooth(int x,int y,Passable passable) {
  unsigned remaining=count,written=0;
  while(remaining) {
   // Bound line-of-sight work even for long, maze-like routes.
   unsigned chosen=remaining>256 ? remaining-256 : 0,dummy=0;
   while(chosen<remaining-1 && !line(x,y,points[chosen]%320,points[chosen]/320,passable,dummy,false))++chosen;
   unsigned p=points[chosen];
   if(!line(x,y,p%320,p/320,passable,written,true))return; // Keep original on failure.
   x=p%320;y=p/320;remaining=chosen;
  }
  count=written;credit=0;
  for(unsigned i=0;i<count;++i)points[i]=_queue[count-1-i];
 }
 unsigned facing(int x,int y) const {
  if(!count)return 0;
  unsigned p=points[count>8 ? count-8 : 0];
  int dx=int(p%320)-x,dy=int(p/320)-y;
  int ax=dx<0 ? -dx : dx,ay=dy<0 ? -dy : dy;
  return ax>=ay && dx ? (dx>0 ? 2 : 4) : dy ? (dy>0 ? 3 : 1) : 0;
 }
 template<class Passable> bool find(int sx,int sy,int gx,int gy,Passable passable,bool nearest=false) {
  count=0;credit=0;
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
