// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/room.h"
namespace Pice {
bool RoomNavigation::node(int16 id,int16 x,int16 y,uint16 scale) {
 if (id<0 || uint(id)>=nodes.size()) return false;
 RoomNode n; n.active=true; n.x=x; n.y=y; n.scale=scale; nodes[id]=n; return true;
}
bool RoomNavigation::edge(int16 id,int16 a,int16 b,byte enabled) {
 if (id<0 || uint(id)>=edges.size() || a<0 || b<0 || a==b || uint(a)>=nodes.size() || uint(b)>=nodes.size() ||
     !nodes[a].active || !nodes[b].active || enabled>1) return false;
 RoomEdge e; e.active=true; e.a=a; e.b=b; e.enabled=enabled; edges[id]=e; return true;
}
static int64 side(const RoomNode &a,const RoomNode &b,Common::Point p) {
 return int64(int32(b.x)-a.x)*(int32(p.y)-a.y)-int64(int32(b.y)-a.y)*(int32(p.x)-a.x);
}
bool RoomNavigation::triangle(int16 id,int16 a,int16 b,int16 c,byte enabled) {
 if (id<0 || uint(id)>=triangles.size() || a<0 || b<0 || c<0 || a==b || a==c || b==c ||
     uint(a)>=nodes.size() || uint(b)>=nodes.size() || uint(c)>=nodes.size() ||
     !nodes[a].active || !nodes[b].active || !nodes[c].active || enabled>1 ||
     !side(nodes[a],nodes[b],Common::Point(nodes[c].x,nodes[c].y))) return false;
 // Original 0x70ac resolves all three existing links before creating a region.
 const int ends[3][2]={{a,b},{b,c},{c,a}};
 for(uint n=0;n<3;++n) {
  bool found=false;
  for(uint i=0;i<edges.size();++i) if(edges[i].active &&
      ((edges[i].a==ends[n][0] && edges[i].b==ends[n][1]) ||
       (edges[i].a==ends[n][1] && edges[i].b==ends[n][0]))) { found=true; break; }
  if (!found) return false;
 }
 RoomTriangle t; t.active=true; t.a=a; t.b=b; t.c=c; t.enabled=enabled; triangles[id]=t; return true;
}
bool RoomNavigation::contains(int id,Common::Point p) const {
 if(id<0 || uint(id)>=triangles.size() || !triangles[id].active || !triangles[id].enabled) return false;
 const RoomTriangle &t=triangles[id];
 int64 a=side(nodes[t.a],nodes[t.b],p), b=side(nodes[t.b],nodes[t.c],p), c=side(nodes[t.c],nodes[t.a],p);
 return (a>=0 && b>=0 && c>=0) || (a<=0 && b<=0 && c<=0);
}
int RoomNavigation::region(Common::Point p) const {
 for(uint i=0;i<triangles.size();++i) if(contains(i,p)) return i;
 return -1;
}
bool RoomNavigation::segment(Common::Point from,Common::Point to) const {
 // Clip the segment against each convex triangle, then check that the union
 // of covered intervals has no gaps. This avoids walking through a wall
 // merely because both endpoints lie on the floor.
 struct Span { double lo,hi; };
 Common::Array<Span> spans;
 for(uint i=0;i<triangles.size();++i) {
  const RoomTriangle &t=triangles[i]; if(!t.active || !t.enabled) continue;
  uint v[3]={t.a,t.b,t.c}; double lo=0,hi=1;
  int sign=side(nodes[t.a],nodes[t.b],Common::Point(nodes[t.c].x,nodes[t.c].y))>0 ? 1 : -1;
  bool valid=true;
  for(uint e=0;e<3;++e) {
   double a=double(side(nodes[v[e]],nodes[v[(e+1)%3]],from))*sign;
   double b=double(side(nodes[v[e]],nodes[v[(e+1)%3]],to))*sign;
   if(a<0 && b<0) { valid=false; break; }
   if(a<0) lo=MAX(lo,a/(a-b));
   if(b<0) hi=MIN(hi,a/(a-b));
  }
  if(valid && lo<=hi) { Span span={lo,hi}; spans.push_back(span); }
 }
 double covered=0;
 for(uint pass=0;pass<spans.size();++pass) {
  double previous=covered;
  for(uint i=0;i<spans.size();++i) if(spans[i].lo<=covered+1e-9) covered=MAX(covered,spans[i].hi);
  if(covered>=1-1e-9) return true;
  if(previous==covered) break;
 }
 return false;
}
bool RoomNavigation::nearest(Common::Point point,Common::Point &out) const {
 if(region(point)>=0) { out=point; return true; }
 bool found=false; int64 best=0;
 for(uint i=0;i<triangles.size();++i) {
  const RoomTriangle &t=triangles[i]; if(!t.active || !t.enabled) continue;
  uint v[3]={t.a,t.b,t.c};
  for(uint e=0;e<3;++e) {
   const RoomNode &a=nodes[v[e]], &b=nodes[v[(e+1)%3]];
   int64 dx=int32(b.x)-a.x,dy=int32(b.y)-a.y,den=dx*dx+dy*dy;
   if(!den) continue;
   int64 num=(int32(point.x)-a.x)*dx+(int32(point.y)-a.y)*dy;
   num=MAX<int64>(0,MIN(num,den));
   Common::Point p(a.x+dx*num/den,a.y+dy*num/den);
   // Integer truncation can move a projection off a sloped boundary. Fall
   // back to its endpoint instead of returning a point outside all regions.
   if(region(p)<0) p=Common::Point(a.x,a.y);
   int64 x=int32(point.x)-p.x,y=int32(point.y)-p.y,d=x*x+y*y;
   if(!found || d<best) { found=true; best=d; out=p; }
  }
 }
 return found;
}
bool runRoomTests() {
 RoomNavigation room;
 if(room.node(-1,0,0,100) || room.node(50,0,0,100) || room.edge(0,0,1,1)) return false;
 if(!room.node(0,0,0,100) || !room.node(1,10,0,100) || !room.node(2,0,10,100) ||
    room.triangle(0,0,1,2,1) || !room.edge(0,0,1,1) || !room.edge(1,1,2,1) ||
    !room.edge(2,2,0,1) || !room.triangle(0,0,1,2,1)) return false;
 if(room.region(Common::Point(2,2))!=0 || room.region(Common::Point(0,0))!=0 ||
    room.region(Common::Point(8,8))!=-1 || room.region(Common::Point(-1,1))!=-1 ||
    room.triangle(0,0,0,2,1) || !room.contains(0,Common::Point(2,2))) return false;
 if(!room.triangle(1,2,1,0,0) || room.contains(1,Common::Point(2,2))) return false;
 Common::Point nearest;
 if(!room.segment(Common::Point(1,1),Common::Point(5,4)) || room.segment(Common::Point(1,1),Common::Point(8,8)) ||
    !room.nearest(Common::Point(-3,5),nearest) || nearest!=Common::Point(0,5)) return false;
 // Disconnected floor islands must not create a traversable shortcut.
 if(!room.node(3,20,0,100) || !room.node(4,30,0,100) || !room.node(5,20,10,100) ||
    !room.edge(3,3,4,1) || !room.edge(4,4,5,1) || !room.edge(5,5,3,1) || !room.triangle(2,3,4,5,1) ||
    room.segment(Common::Point(1,1),Common::Point(21,1))) return false;
 room.clear(); return room.region(Common::Point(2,2))==-1 && !room.nearest(Common::Point(),nearest);
}
}
