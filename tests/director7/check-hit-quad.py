#!/usr/bin/env python3
"""Exercise production hotspot geometry, including camera translation and ink bypass."""
from pathlib import Path
import subprocess,sys,tempfile
s=(Path(sys.argv[1])/'engines/director/channel.cpp').read_text()
a=s.index('void Channel::setPS4HitQuad(');b=s.index('\nCommon::Rect Channel::getRollOverBbox',a)
c=s.index('CollisionTest Channel::isMouseIn(');d=s.index('\nbool Channel::isMatteIntersect',c)
code=r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
#include <random>
using uint=unsigned;using int64=int64_t;
#define MIN(a,b) std::min(a,b)
#define MAX(a,b) std::max(a,b)
namespace Common {
template<class T>using Array=std::vector<T>;
struct Point{int x=0,y=0;Point(){}Point(int a,int b):x(a),y(b){}Point operator-(Point o)const{return {x-o.x,y-o.y};}Point&operator-=(Point o){x-=o.x;y-=o.y;return *this;}Point&operator+=(Point o){x+=o.x;y+=o.y;return *this;}bool operator==(Point o)const{return x==o.x&&y==o.y;}};
struct Rect{int left=0,top=0,right=0,bottom=0;Rect(){}Rect(int l,int t,int r,int b):left(l),top(t),right(r),bottom(b){}bool contains(Point p)const{return p.x>=left&&p.x<right&&p.y>=top&&p.y<bottom;}};
struct String:std::string{using std::string::string;bool equalsIgnoreCase(const char*p){std::string a=*this,b=p;for(auto&c:a)c=tolower(c);for(auto&c:b)c=tolower(c);return a==b;}};
}
enum CollisionTest{kCollisionNo,kCollisionYes,kCollisionHole};enum{kCastBitmap,kCastOther};
struct CastMemberInfo{Common::String name="Ruta";};struct Cast{int _type=kCastBitmap;CastMemberInfo info;CastMemberInfo*getInfo(){return &info;}CollisionTest isWithin(Common::Rect r,Common::Point p,int){return r.contains(p)?kCollisionYes:kCollisionNo;}};
struct Sprite{Cast*_cast;int _ink=36;};
struct Channel{Common::Array<Common::Point>_ps4HitQuad;Sprite*_sprite;bool _visible=true;Common::Point origin{100,200};Common::Point getPosition(){return origin;}Common::Rect getBbox(){return {origin.x,origin.y,origin.x+1,origin.y+1};}void clearPS4HitQuad(){_ps4HitQuad.clear();}
void setPS4HitQuad(const Common::Array<Common::Point>&);Common::Array<Common::Point> getPS4HitQuad();bool hasPS4HitQuad();bool isInPS4HitQuad(const Common::Point&);Common::Rect getPS4HitQuadBounds();CollisionTest isMouseIn(const Common::Point&);};
'''+s[a:b]+s[c:d]+r'''
bool reference(const std::vector<Common::Point>&q,Common::Point p){double angle=0;for(unsigned i=0,j=3;i<4;j=i++){double ax=q[j].x-p.x,ay=q[j].y-p.y,bx=q[i].x-p.x,by=q[i].y-p.y;angle+=atan2(ax*by-ay*bx,ax*bx+ay*by);}return std::abs(angle)>3.0;}
int main(){Cast cast;Sprite sp{&cast};Channel c;c._sprite=&sp;
 std::vector<Common::Point> q{{100,180},{160,200},{140,260},{80,240}};c.setPS4HitQuad(q);assert(c.getPS4HitQuad()==q);assert(c.hasPS4HitQuad());
 assert(c.isMouseIn({120,220})==kCollisionYes);assert(c.isMouseIn({81,181})==kCollisionNo);assert(c.isMouseIn({100,180})==kCollisionYes);
 auto b=c.getPS4HitQuadBounds();assert(b.left==80&&b.top==180&&b.right==160&&b.bottom==260);
 for(int y=179;y<263;y+=2)for(int x=79;x<163;x+=2)assert(c.isInPS4HitQuad({x,y})==reference(q,{x,y}));
 c.origin={-200,-100};assert(c.isMouseIn({-180,-80})==kCollisionYes);assert(c.isMouseIn({120,220})==kCollisionNo);
 std::reverse(q.begin(),q.end());c.setPS4HitQuad(q);assert(c.isMouseIn({120,220})==kCollisionYes);
 c._visible=false;assert(c.isMouseIn({120,220})==kCollisionNo);c._visible=true;
 cast.info.name="actor";assert(!c.hasPS4HitQuad());assert(c.isMouseIn({120,220})==kCollisionNo);cast.info.name="Ruta";
 c.clearPS4HitQuad();assert(!c.hasPS4HitQuad());assert(c.isMouseIn({120,220})==kCollisionNo);
 c.setPS4HitQuad({{1,1},{2,2},{3,3},{4,4}});assert(!c.isInPS4HitQuad({2,2}));
 c.setPS4HitQuad(q);c.setPS4HitQuad({{1,1}});assert(c.getPS4HitQuad()==q);
}
'''
code='#include <cmath>\n'+code
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(code)
 subprocess.run(['c++','-std=c++17','-O2','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Quad hit test passed: exact polygon, bounds, translation, winding, visibility, member gate, reset and degenerate input')
