// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/room.h"
#include <math.h>
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
bool RoomNavigation::setEnabled(int16 kind,int16 id,int16 enabled) {
 if(id<0 || (enabled!=0 && enabled!=1)) return false;
 if(kind==0) { if(uint(id)>=edges.size() || !edges[id].active) return false; edges[id].enabled=enabled; }
 else if(kind==1) { if(uint(id)>=triangles.size() || !triangles[id].active) return false; triangles[id].enabled=enabled; }
 else return false;
 return true;
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
bool RoomNavigation::route(Common::Point from,Common::Point to,Common::Array<Common::Point> &points) const {
 points.clear();
 auto nodeAt=[&](Common::Point p) {
  for(uint i=0;i<nodes.size();++i) if(nodes[i].active && p==Common::Point(nodes[i].x,nodes[i].y)) return int(i);
  return -1;
 };
 auto connected=[&](Common::Point a,Common::Point b) {
  int na=nodeAt(a),nb=nodeAt(b);
  if(na>=0 && nb>=0) {
   // Original 0x5957 never generates a replacement for an existing link,
   // including a disabled one. Explicit links may leave the triangle floor.
   for(uint i=0;i<edges.size();++i) if(edges[i].active &&
      ((edges[i].a==na && edges[i].b==nb) || (edges[i].a==nb && edges[i].b==na))) return edges[i].enabled!=0;
   if(!generatedEnabled) return false;
  }
  return segment(a,b);
 };
 if((region(from)<0 && nodeAt(from)<0) || (region(to)<0 && nodeAt(to)<0)) return false;
 if(from==to) return true;
 if(connected(from,to)) { points.push_back(to); return true; }
 Common::Array<Common::Point> vertices; vertices.push_back(from); vertices.push_back(to);
 for(uint i=0;i<nodes.size();++i) if(nodes[i].active) {
  Common::Point p(nodes[i].x,nodes[i].y); if(p!=from && p!=to) vertices.push_back(p);
 }
 // Explicit room links plus generated floor visibility. Generated links
 // cannot cross holes or override an explicitly disabled link.
 Common::Array<double> cost; cost.resize(vertices.size(),1e30); cost[0]=0;
 Common::Array<int> previous; previous.resize(vertices.size(),-1);
 Common::Array<byte> visited; visited.resize(vertices.size(),0);
 for(uint visit=0;visit<vertices.size();++visit) {
  int current=-1;
  for(uint i=0;i<vertices.size();++i) if(!visited[i] && cost[i]<1e30 && (current<0 || cost[i]<cost[current])) current=i;
  if(current<0) return false;
  if(current==1) break;
  visited[current]=1;
  for(uint i=0;i<vertices.size();++i) {
   if(visited[i] || !connected(vertices[current],vertices[i])) continue;
   double dx=int32(vertices[current].x)-vertices[i].x,dy=int32(vertices[current].y)-vertices[i].y;
   double candidate=cost[current]+sqrt(dx*dx+dy*dy);
   if(candidate<cost[i]) { cost[i]=candidate; previous[i]=current; }
  }
 }
 if(previous[1]<0) return false;
 Common::Array<Common::Point> reverse;
 for(int i=1;i!=0;i=previous[i]) {
  if(i<0 || reverse.size()>=vertices.size()) return false;
  reverse.push_back(vertices[i]);
 }
 for(uint i=reverse.size();i>0;--i) points.push_back(reverse[i-1]);
 return true;
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
 Common::Array<Common::Point> route;
 if(room.route(Common::Point(1,1),Common::Point(21,1),route) || !route.empty()) return false;
 room.clear();
 // L-shaped floor: endpoints are inside, their direct segment crosses the
 // missing upper-left square. A valid route must turn at the inner corner.
 const int16 xy[][2]={{0,0},{20,0},{20,20},{10,20},{10,10},{0,10}};
 for(uint i=0;i<6;++i) if(!room.node(i,xy[i][0],xy[i][1],100)) return false;
 const int16 links[][2]={{0,1},{1,4},{4,0},{4,5},{5,0},{1,2},{2,4},{2,3},{3,4}};
 for(uint i=0;i<9;++i) if(!room.edge(i,links[i][0],links[i][1],1)) return false;
 if(!room.triangle(0,0,1,4,1) || !room.triangle(1,0,4,5,1) || !room.triangle(2,1,2,4,1) || !room.triangle(3,2,3,4,1) ||
    room.segment(Common::Point(2,8),Common::Point(18,18)) || !room.route(Common::Point(2,8),Common::Point(18,18),route) ||
    route.size()!=2 || route[0]!=Common::Point(10,10) || route[1]!=Common::Point(18,18)) return false;
 // A room object outside the floor is reached only through enabled links.
 if(!room.node(6,40,20,100) || !room.edge(9,2,6,0) ||
    room.route(Common::Point(18,18),Common::Point(40,20),route) ||
    !room.setEnabled(0,9,1) || !room.route(Common::Point(18,18),Common::Point(40,20),route) ||
    route.back()!=Common::Point(40,20) || !room.setEnabled(0,9,0) ||
    room.route(Common::Point(18,18),Common::Point(40,20),route) ||
    room.setEnabled(0,449,1) || room.setEnabled(2,0,1) || room.setEnabled(1,0,2)) return false;
 // Disabling a region removes its interior from walkable floor immediately.
 if(!room.setEnabled(1,3,0) || room.region(Common::Point(12,19))>=0 || !room.setEnabled(1,3,1)) return false;
 room.generatedEnabled=false;
 if(!room.route(Common::Point(0,0),Common::Point(20,20),route) || route.size()<2) return false;
 room.clear(); return room.generatedEnabled && room.region(Common::Point(2,2))==-1 && !room.nearest(Common::Point(),nearest);
}
}
