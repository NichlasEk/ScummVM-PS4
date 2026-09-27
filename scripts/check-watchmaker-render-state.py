#!/usr/bin/env python3
"""Exercise production render-state translation with recorded GL calls."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
s=(root/'.tools/scummvm-vulkan-src/engines/watchmaker/3d/render/opengl_renderer.cpp').read_text()
s=s[s.index('void OpenGLRenderer::setRenderState('):s.index('GLenum translateBlendFactorToGL')]
source=r'''
#include <cassert>
#include <cmath>
#include <cstdio>
enum {GL_LEQUAL=1,GL_DEPTH_TEST,GL_BLEND,GL_GREATER,GL_TRUE,GL_FALSE};
enum class RenderState {ZENABLE,ALPHAREF,ALPHABLEND,ZWRITE_ENABLE,LIGHT,CLIP,EXTENT,TEXTUREFACTOR};
int depthFunc=0,alphaFunc=0; float alphaRef=-1; bool blend=false,depth=false,write=false;
void glDepthFunc(int f){depthFunc=f;}
void glAlphaFunc(int f,float r){alphaFunc=f;alphaRef=r;}
void glDepthMask(int v){write=v==GL_TRUE;}
void setGlFeature(int f,int v){if(f==GL_BLEND)blend=v;else if(f==GL_DEPTH_TEST)depth=v;else assert(false);}
struct OpenGLRenderer{void setRenderState(RenderState,int);};
'''+s+r'''
int main(){OpenGLRenderer r;
 // Alpha cutoff must neither force blend on nor switch it off.
 r.setRenderState(RenderState::ALPHABLEND,0);
 r.setRenderState(RenderState::ALPHAREF,85);
 assert(!blend && alphaFunc==GL_GREATER && std::fabs(alphaRef-1.f/3)<.00001f);
 r.setRenderState(RenderState::ALPHABLEND,1);
 r.setRenderState(RenderState::ALPHAREF,0); assert(blend && alphaRef==0);
 r.setRenderState(RenderState::ALPHAREF,255);assert(alphaRef==1);
 // Opaque pass reestablishes depth; lightmap pass can disable writes alone.
 r.setRenderState(RenderState::ZENABLE,1);r.setRenderState(RenderState::ZWRITE_ENABLE,1);
 assert(depth && write && depthFunc==GL_LEQUAL);
 r.setRenderState(RenderState::ZWRITE_ENABLE,0);assert(depth && !write);
 puts("PASS production render state: normalized alpha cutoff, independent blend, depth writes");}
'''
work=root/'build/watchmaker-data-test';work.mkdir(parents=True,exist_ok=True)
p=work/'render-state.cpp';p.write_text(source)
subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p),'-o',str(work/'render-state')],check=True)
subprocess.run([str(work/'render-state')],check=True)
