#!/usr/bin/env python3
"""Compare extracted optimized Director queries to their pre-optimization ordering."""
from pathlib import Path
import subprocess, sys, tempfile
src=Path(sys.argv[1]);s=(src/'engines/director/score.cpp').read_text()
def method(name):
 a=s.index(name);a=s.rfind('\n',0,a)+1;b=s.index('{',a);level=1;i=b+1
 while level:
  if s[i]=='{':level+=1
  if s[i]=='}':level-=1
  i+=1
 return '\n'.join(line for line in s[a:i].splitlines() if 'TEMP_PERF' not in line)
methods=['const Common::Array<int> &Score::getPS4ChannelOrder()', 'uint16 Score::getSpriteIDFromPos(', 'uint16 Score::getMouseSpriteIDFromPos(', 'uint16 Score::getActiveSpriteIDFromPos(', 'Common::Array<Channel *> Score::getSpriteIntersections(']
code=r'''
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>
using uint=unsigned;using uint16=uint16_t;
namespace Common {
template<class T>using Array=std::vector<T>;
template<class It,class... A>void sort(It a,It b,A...args){std::sort(a,b,args...);}
struct Point{int x,y;};
struct Rect{int l,t,r,b;bool isEmpty()const{return l>=r||t>=b;}Rect findIntersectingRect(Rect o)const{return {std::max(l,o.l),std::max(t,o.t),std::min(r,o.r),std::min(b,o.b)};}bool contains(Point p)const{return p.x>=l&&p.x<r&&p.y>=t&&p.y<b;}};
}
enum CollisionTest{kCollisionNo,kCollisionYes,kCollisionHole};
struct Sprite{bool responds=false,active=false;bool respondsToMouse(){return responds;}bool isActive(){return active;}};
struct Channel{int _priority;Sprite sp;Sprite*_sprite=&sp;bool empty=false,editable=false;CollisionTest collision=kCollisionYes;Common::Rect bbox{0,0,100,100};bool isEmpty(){return empty;}bool getEditable(){return editable;}Common::Rect getBbox(){return bbox;}CollisionTest isMouseIn(Common::Point p){return !empty&&bbox.contains(p)?collision:kCollisionNo;}};
bool enabled=true;bool isPS4Jonsson(){return enabled;}
struct Score{Common::Array<Channel*>_channels;Common::Array<int>_ps4ChannelOrder,_ps4ChannelPriorities;
const Common::Array<int>&getPS4ChannelOrder();uint16 getSpriteIDFromPos(Common::Point);uint16 getMouseSpriteIDFromPos(Common::Point);uint16 getActiveSpriteIDFromPos(Common::Point);Common::Array<Channel*>getSpriteIntersections(const Common::Rect&);};
// Reference is the original stable insertion ordering; do not share cache logic.
std::vector<int> referenceOrder(Score &s){std::vector<int> o;for(int i=int(s._channels.size())-1;i>=0;--i){auto it=o.begin();if(enabled)while(it!=o.end()&&s._channels[*it]->_priority>=s._channels[i]->_priority)++it;else it=o.end();o.insert(it,i);}return o;}
int referenceHit(Score&s,Common::Point p,int kind){for(int i:referenceOrder(s)){auto*c=s._channels[i];auto hit=c->isMouseIn(p);if(hit==kCollisionYes&&(kind==0||(kind==1?c->_sprite->respondsToMouse():c->_sprite->isActive())))return i;else if(hit==kCollisionHole)break;}return 0;}
std::vector<Channel*>referenceRects(Score&s,Common::Rect r){std::vector<Channel*>v,a;for(auto*c:s._channels)if(!c->isEmpty()&&!r.findIntersectingRect(c->getBbox()).isEmpty()){if(c->getEditable())a.push_back(c);else if(enabled){auto it=v.begin();while(it!=v.end()&&(*it)->_priority<=c->_priority)++it;v.insert(it,c);}else v.push_back(c);}v.insert(v.end(),a.begin(),a.end());return v;}
'''+'\n'.join(method(m) for m in methods)+r'''
int main(){std::mt19937 rng(42);std::vector<Channel> storage(1001);Score s;
 for(int test=0;test<800;++test){enabled=test%7!=0;int n=test%31==0?0:1+rng()%1000;s._channels.resize(n);
  for(int i=0;i<n;++i){auto &c=storage[i];s._channels[i]=&c;c._priority=test%4==0?int(rng()%8):int(rng());c.empty=rng()%5==0;c.editable=rng()%13==0;c.sp.responds=rng()%2;c.sp.active=rng()%2;c.collision=CollisionTest(rng()%3);int x=rng()%90,y=rng()%90;c.bbox={x,y,x+30,y+30};}
  for(int pass=0;pass<3;++pass){if(n&&pass)s._channels[rng()%n]->_priority=int(rng());Common::Point p{int(rng()%100),int(rng()%100)};
   assert(s.getSpriteIDFromPos(p)==referenceHit(s,p,0));assert(s.getMouseSpriteIDFromPos(p)==referenceHit(s,p,1));assert(s.getActiveSpriteIDFromPos(p)==referenceHit(s,p,2));
   assert(s.getSpriteIntersections({0,0,75,75})==referenceRects(s,{0,0,75,75}));
  }
 }
 enabled=true;s._channels.resize(1000);for(int i=0;i<1000;++i){s._channels[i]=&storage[i];storage[i]._priority=i;storage[i].empty=false;storage[i].bbox={0,0,100,100};}
 volatile int sink=0;auto a=std::chrono::steady_clock::now();for(int i=0;i<300;++i)sink+=referenceHit(s,{200,200},0);auto b=std::chrono::steady_clock::now();for(int i=0;i<300;++i)sink+=s.getSpriteIDFromPos({200,200});auto c=std::chrono::steady_clock::now();
 printf("800 randomized scenes x 3 mutations: hit, mouse, active, intersections and editable ordering match.\n1000-channel/300-query host benchmark: old=%lld us new=%lld us (not PS4 timing)\n",(long long)std::chrono::duration_cast<std::chrono::microseconds>(b-a).count(),(long long)std::chrono::duration_cast<std::chrono::microseconds>(c-b).count());
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(code)
 subprocess.run(['c++','-std=c++17','-O2','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
