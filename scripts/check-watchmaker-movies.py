#!/usr/bin/env python3
"""Exercise production movie seeking and script/timer ownership."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1];src=root/'.tools/scummvm-vulkan-src/engines/watchmaker'
s=(src/'3d/movie.cpp').read_text();seek=s[s.index('bool gMovie::setFrame('):s.index('//*********************************************************************************************',s.index('bool gMovie::setFrame('))]
s=(src/'render.cpp').read_text();control=s[s.index('bool rSetMovieFrame('):s.index('int rCreateSurface',s.index('bool rSetMovieFrame('))]
code=r'''
#include <cassert>
#include <cstring>
#include <cstdio>
#include <memory>
using uint16=unsigned short;using uint32=unsigned;using byte=unsigned char;
#define warning(...) ((void)0)
#define debug(...) ((void)0)
struct Name{const char *c_str(){return "fixture";}};
struct Header{unsigned dataSize(){return 1;}};
struct Stream{void seek(int,int){}};
std::shared_ptr<int> loadDdsTexture(Stream&,Header&){return std::make_shared<int>(0);}
struct Texture{void assignData(int){}};
unsigned timeGetTime(){return 10000;}
struct gMovie {
 Name _name;Header _header;Stream stream,*_frameStream=&stream;Texture tex,*_texture=&tex;
 byte _buffer[1]{},_surfaceBuffer[1]{};uint16 _numFrames=9,_keyFrame=3,_curFrame=0xffff,_frameRate=10;
 uint32 _startTime=0;bool _paused=false;
 void loadThisFrameData(uint16 f){_buffer[0]=f%3 ? 1 : 10+f;}
 void buildNewFrame(byte *p,uint16 f){assert(*p==10+f-1);++*p;}
 bool setFrame(uint16);
};
struct Material{std::shared_ptr<gMovie> Movie;};using MaterialPtr=std::shared_ptr<Material>;
'''+seek+control+r'''
int main(){auto m=std::make_shared<Material>();m->Movie=std::make_shared<gMovie>();auto &v=*m->Movie;
 assert(rSetMovieFrame(m,2));assert(v._surfaceBuffer[0]==12 && !v._paused);
 assert(rSetMovieFrame(m,1));assert(v._surfaceBuffer[0]==11 && !v._paused);
 assert(rSetMovieFrame(m,7));assert(v._surfaceBuffer[0]==17 && !v._paused);
 assert(!rSetMovieFrame(m,9));assert(v._curFrame==7 && !v._paused);
 assert(rSetMovieFrame(m,0xffff));assert(!v._paused && v._curFrame==7 && v._startTime==9300);
 assert(rSetMovieFrame(m,0));assert(v._surfaceBuffer[0]==10 && !v._paused);
 assert(!rSetMovieFrame(nullptr,0));
 puts("PASS production movie control: cold delta seek, backwards seek, keyframe crossing, timed seek and resume");}
'''
work=root/'build/watchmaker-data-test';work.mkdir(parents=True,exist_ok=True);p=work/'movies.cpp';p.write_text(code)
subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p),'-o',str(work/'movies')],check=True)
subprocess.run([str(work/'movies')],check=True)
