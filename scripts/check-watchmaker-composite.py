#!/usr/bin/env python3
"""Check production virtual bitmap compositing without game data or GL."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1];src=root/'.tools/scummvm-vulkan-src/engines/watchmaker'
h=(src/'3d/texture.h').read_text();h=h[h.index('struct gTexture {'):h.index('} // End of namespace')]
c=(src/'3d/render/opengl_2d.cpp').read_text();c=c[c.index('void gTexture::render('):c.index('void Renderer::blitScreenBuffer')]
preamble=r'''
#include <vector>
#include <string>
#include <algorithm>
#include <cassert>
#include <cstdio>
using uint=unsigned;
namespace Common {
using String=std::string;template<class T>using Array=std::vector<T>;
struct Rect {int left,top,right,bottom;
 Rect(int a=0,int b=0,int c=0,int d=0):left(a),top(b),right(c),bottom(d){}
 int width()const{return right-left;}int height()const{return bottom-top;}
 bool isEmpty()const{return right<=left||bottom<=top;}
 void clip(const Rect&r){left=std::max(left,r.left);top=std::max(top,r.top);right=std::min(right,r.right);bottom=std::min(bottom,r.bottom);}
};}
struct Texture{};class WGame{};
'''
main=r'''
int main(){WGame game;Texture texture;gTexture glyph,caption,screen;
 glyph._texture=&texture;caption.DimX=100;caption.DimY=20;
 assert(!caption.isEmpty());assert(screen.isEmpty());
 caption.blitInto(&glyph,{10,20,30,40},{40,0,60,20});
 caption.render(game,{45,0,55,20},{200,100,220,140});
 assert(calls==1 && lastSrc.left==15 && lastSrc.right==25 && lastSrc.top==20);
 assert(lastDst.left==200 && lastDst.right==220 && lastDst.top==100 && lastDst.bottom==140);
 caption.render(game,{0,0,10,20},{0,0,10,20});assert(calls==1);
 screen.blitInto(&caption,{0,0,100,20},{100,80,200,100});
 screen.render(game,{0,0,800,600},{0,0,800,600});
 assert(calls==2 && lastDst.left==140 && lastDst.top==80 && lastDst.right==160);
 screen.clear();screen.render(game,{0,0,800,600},{0,0,800,600});assert(calls==2);
 puts("PASS production bitmap composition: reservation, nested offsets, clipping, scaling and clearing");
}
'''
source=preamble+h+'\nint calls=0;Common::Rect lastSrc,lastDst;void renderTexture(WGame&,gTexture&,Common::Rect src,Common::Rect dst){++calls;lastSrc=src;lastDst=dst;}\n'+c+main
work=root/'build/watchmaker-data-test';work.mkdir(parents=True,exist_ok=True);p=work/'composite.cpp';p.write_text(source)
subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p),'-o',str(work/'composite')],check=True)
subprocess.run([str(work/'composite')],check=True)
