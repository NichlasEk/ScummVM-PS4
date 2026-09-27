#!/usr/bin/env python3
"""Production positional direction and flare-pass alpha regression checks."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1];src=root/'.tools/scummvm-vulkan-src/engines/watchmaker'
s=(src/'3d/animation.cpp').read_text();a=s.index('uint8 GetLightDirection(');b=s.index('\n\n/*',a);direction=s[a:b]
s=(src/'3d/render/render.cpp').read_text();a=s.index('\t\t// Ordinary flare meshes');b=s.index('\n\t\tif (bb->Flags1 & T3D_MATERIAL_FLARE_SUN)',a);flare=s[a:b]
code=r'''
#include <cassert>
#include <vector>
#include <string>
#include <cstdio>
using uint8=unsigned char;
#define warning(...) ((void)0)
struct t3dV3F {float x=0,y=0,z=0;};
struct Light {t3dV3F Dir;};
struct Room {std::string name="r92";std::vector<Light> lights;auto getPositionalLight(uint8){return lights;}} room,*t3dCurRoom=&room;
float CurFloorY=17;
''' + direction + r'''
constexpr unsigned T3D_MATERIAL_SMOKE=4096;
enum class RenderState {ALPHAREF};
struct Renderer {unsigned cutoff=255;void setRenderState(RenderState,unsigned v){cutoff=v;}} renderer,*g_renderer=&renderer;
struct Batch {unsigned Flags1;};
void configure(Batch *bb) {
''' + flare + r'''
}
int main(){
 for(auto v: {t3dV3F{20,0,-70},t3dV3F{-100,0,15}}) {
  room.lights={{v}};t3dV3F out;assert(GetLightDirection(&out,1)==1);
  assert(out.x==v.x && out.z==v.z && out.y==CurFloorY);
 }
 t3dV3F sentinel{1,2,3};assert(GetLightDirection(&sentinel,0)==0 && sentinel.z==3);
 Batch lamp{0x600};configure(&lamp);assert(renderer.cutoff==0);
 assert(2>renderer.cutoff); // ordinary mesh alpha survives
 assert(!(0>renderer.cutoff)); // fully transparent texels still rejected
 Batch smoke{T3D_MATERIAL_SMOKE};configure(&smoke);assert(renderer.cutoff==85);
 puts("PASS production direction preserves distinct X/Z; flare marker survives while smoke cutoff remains intact");
}
'''
with tempfile.TemporaryDirectory(prefix='watchmaker-door-lamp-') as d:
 p=Path(d);(p/'test.cpp').write_text(code)
 subprocess.run(['c++','-std=c++14','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
