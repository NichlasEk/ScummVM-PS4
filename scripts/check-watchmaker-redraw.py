#!/usr/bin/env python3
"""Exercise production Regen across menu removal and repeated frames."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
s=(root/'.tools/scummvm-vulkan-src/engines/watchmaker/ll/ll_regen.cpp').read_text()
s=s[s.index('void Regen(WGame &game) {'):s.index('/* -----------------16/11/98')]
source=r'''
#include <cassert>
#include <cstring>
#include <vector>
#include <cstdio>
struct Rect {int tnum,px,py,ox,oy,dx,dy;};
Rect PaintRect[4]{},OldPaintRect[4]{};
constexpr int BACK_BUFFER=0;
struct Bitmap {std::vector<int> draws;void clear(){draws.clear();}};
struct Renderer {struct {Bitmap bitmaps[1];} _bitmapList;std::vector<int> presented;
 void blitScreenBuffer(){presented=_bitmapList.bitmaps[0].draws;}};
struct WGame {Renderer *_renderer;};
void rResetExtends(){}
void rBlitter(WGame &g,int,int src,int,int,int,int,int,int){g._renderer->_bitmapList.bitmaps[0].draws.push_back(src);}
'''+s+r'''
int main(){Renderer renderer;WGame game{&renderer};
 PaintRect[0]={41,0,0,0,0,800,600};PaintRect[1]={42,0,0,0,0,20,20};
 for(int i=0;i<2000;++i){Regen(game);assert(renderer.presented==std::vector<int>({41,42}));}
 PaintRect[0]={};PaintRect[1]={};Regen(game);assert(renderer.presented.empty());
 PaintRect[0]={72,0,0,0,0,300,40};Regen(game);assert(renderer.presented==std::vector<int>({72}));
 puts("PASS production redraw: bounded repeated frames, removed menu disappears, new overlay replaces old");}
'''
work=root/'build/watchmaker-data-test';work.mkdir(parents=True,exist_ok=True)
p=work/'redraw.cpp';p.write_text(source)
subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p),'-o',str(work/'redraw')],check=True)
subprocess.run([str(work/'redraw')],check=True)
