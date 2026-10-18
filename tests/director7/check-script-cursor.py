#!/usr/bin/env python3
"""Exercise the actual sprite-cursor follower without running game scripts."""
from pathlib import Path
import subprocess, sys, tempfile
s=(Path(sys.argv[1])/'engines/director/score.cpp').read_text()
a=s.index('bool Score::updatePS4ScriptCursor(');b=s.index('\nvoid Score::renderCursor',a)
code=r'''
#include <cassert>
#include <algorithm>
#include <string>
#include <vector>
template<class T>T CLIP(T x,T lo,T hi){return std::min(hi,std::max(lo,x));}
namespace Common {
struct Point{int x,y;bool operator==(Point o)const{return x==o.x&&y==o.y;}};
struct Rect{int left=0,top=0,right=640,bottom=480;int width()const{return right-left;}int height()const{return bottom-top;}};
struct String:std::string{using std::string::string;bool hasPrefix(const char*p){return find(p)==0;}};
}
enum{kPlayStarted,kPlayStopped,kCastBitmap,kCastOther};
struct CastMemberInfo{Common::String name="cursor_default_0000";};
struct Cast{int _type=kCastBitmap;CastMemberInfo info;CastMemberInfo*getInfo(){return &info;}};
struct Sprite{Cast*_cast;};
struct Channel{bool _visible=true;int _priority=0x7fffffff;Sprite*_sprite;Common::Point pos={0,0};bool dirty=false;
 Common::Point getPosition(){return pos;}void setPosition(int x,int y){pos={x,y};}void setNeedsDraw(){dirty=true;}};
struct VM{const char*id="jonssonligan2";const char*getGameId(){return id;}};
struct Window{bool _playbackPaused=false;};
struct Movie{bool _videoPlayback=false,_isEmbedded=false;Common::Rect _movieRect;};
bool enabled=true;bool isPS4Jonsson(){return enabled;}
struct Score{VM*_vm;Window*_window;Movie*_movie;int _playState=kPlayStarted;std::vector<Channel*>_channels;bool updatePS4ScriptCursor(Common::Point);};
''' + s[a:b] + r'''
int main(){VM vm;Window w;Movie m;Cast cast;Sprite sp{&cast};Channel ch;ch._sprite=&sp;Score sc{&vm,&w,&m};sc._channels={nullptr,&ch};
 assert(sc.updatePS4ScriptCursor({100,200}) && ch.pos==Common::Point({100,200}) && ch.dirty);
 ch.dirty=false;assert(!sc.updatePS4ScriptCursor({100,200}) && !ch.dirty);
 assert(sc.updatePS4ScriptCursor({-10,999}) && ch.pos==Common::Point({8,472}));
 // A saved desktop/movie origin must not create an invisible cursor wall.
 m._movieRect={224,96,864,576};
 assert(sc.updatePS4ScriptCursor({30,40}) && ch.pos==Common::Point({30,40}));
 assert(sc.updatePS4ScriptCursor({-10,999}) && ch.pos==Common::Point({8,472}));
 m._movieRect={-200,-100,440,380};
 assert(sc.updatePS4ScriptCursor({100,150}) && ch.pos==Common::Point({100,150}));
 assert(sc.updatePS4ScriptCursor({999,-10}) && ch.pos==Common::Point({632,8}));
 m._movieRect={0,0,640,480};
 cast.info.name="actor";assert(!sc.updatePS4ScriptCursor({200,200}));cast.info.name="cursor_ta_0001";
 ch._visible=false;assert(!sc.updatePS4ScriptCursor({200,200}));ch._visible=true;
 ch._priority=42;assert(!sc.updatePS4ScriptCursor({200,200}));ch._priority=0x7fffffff;
 w._playbackPaused=true;assert(!sc.updatePS4ScriptCursor({200,200}));w._playbackPaused=false;
 m._videoPlayback=true;assert(!sc.updatePS4ScriptCursor({200,200}));m._videoPlayback=false;
 m._isEmbedded=true;assert(!sc.updatePS4ScriptCursor({200,200}));m._isEmbedded=false;
 vm.id="jonssonligan1";assert(!sc.updatePS4ScriptCursor({200,200}));vm.id="jonssonligan2";
 enabled=false;assert(!sc.updatePS4ScriptCursor({200,200}));enabled=true;
 assert(sc.updatePS4ScriptCursor({200,200}));
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(code)
 subprocess.run(['c++','-std=c++17',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Script cursor: movement, bounds, unchanged position, identity and visibility/playback guards passed')
