// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PICE_ROOM_H
#define PICE_ROOM_H
#include "common/array.h"
#include "common/rect.h"
namespace Pice {
// Original fixed limits: 50 nodes, 450 edges, 50 triangular regions.
struct RoomNode { bool active=false; int16 x=0,y=0; uint16 scale=100; };
struct RoomEdge { bool active=false; uint16 a=0,b=0; byte enabled=0; };
struct RoomTriangle { bool active=false; uint16 a=0,b=0,c=0; byte enabled=0; };
class RoomNavigation {
public:
 RoomNavigation() { clear(); }
 bool generatedEnabled=true;
 void clear() { generatedEnabled=true; nodes.clear(); nodes.resize(50); edges.clear(); edges.resize(450); triangles.clear(); triangles.resize(50); }
 bool node(int16 id,int16 x,int16 y,uint16 scale);
 bool edge(int16 id,int16 a,int16 b,byte enabled);
 bool triangle(int16 id,int16 a,int16 b,int16 c,byte enabled);
 bool setEnabled(int16 kind,int16 id,int16 enabled);
 bool contains(int id,Common::Point p) const;
 int region(Common::Point p) const;
 uint scaleAt(Common::Point p) const;
 bool segment(Common::Point from,Common::Point to) const;
 bool route(Common::Point from,Common::Point to,Common::Array<Common::Point> &points) const;
 bool nearest(Common::Point point,Common::Point &out) const;
 Common::Array<RoomNode> nodes;
 Common::Array<RoomEdge> edges;
 Common::Array<RoomTriangle> triangles;
};
bool runRoomTests();
}
#endif
