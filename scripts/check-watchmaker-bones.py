#!/usr/bin/env python3
"""Exercise production skinning with a nonzero first animated vertex."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
s=(root/'.tools/scummvm-vulkan-src/engines/watchmaker/3d/geometry.cpp').read_text();a=s.index('void CalcBones(');b=s.index('\n/* -----------------',a);body=s[a:b]
code=r'''
#include <cassert>
#include <cstring>
#include <vector>
#include <string>
#include <cstdint>
#include <cstdio>
using int32=int32_t;using uint32=uint32_t;using uint8=uint8_t;using t3dF32=float;
#define warning(...) ((void)0)
constexpr unsigned T3D_MESH_CHARACTER=1;
struct t3dV3F {float x=0,y=0,z=0;};struct t3dM3X3F {};
struct gVertex {float x=0,y=0,z=0;unsigned diffuse=0;};
struct t3dBONE {t3dV3F *Trasl; t3dM3X3F *Matrix;std::vector<unsigned> ModVertices;};
struct t3dBONEANIM {int NumFrames=2,NumBones=2;t3dBONE *BoneTable;};
struct t3dMESH {unsigned NumVerts=4,Flags=T3D_MESH_CHARACTER,ExpressionFrame=0;std::string name="test";
gVertex *SavedVertexBuffer,*VBptr,*OldVertexBuffer;t3dV3F *VertsInterpolants;};
uint8 scratch[4];void *t3dAlloc(unsigned n){assert(n==4);return scratch;}void t3dDealloc(unsigned){}
void t3dAddExpression(t3dMESH*,unsigned,gVertex*){}
void t3dVectTransformInv(t3dV3F*d,t3dV3F*s,t3dM3X3F*){*d=*s;}
void t3dCalcVertsInterpolants(gVertex*,gVertex*,t3dV3F*,unsigned){}
'''+body+r'''
int main(){gVertex rest[4],out[4];rest[0].x=1000;rest[1].x=2000;rest[2].x=10;rest[3].x=20;
 memcpy(out,rest,sizeof out);t3dV3F translations[2][2];translations[0][1].x=2;translations[1][1].x=6;
 t3dM3X3F matrices[2][2];t3dBONE bones[2]={{translations[0],matrices[0],{2,3}},{translations[1],matrices[1],{3}}};
 t3dBONEANIM anim;anim.BoneTable=bones;t3dMESH mesh;mesh.SavedVertexBuffer=rest;mesh.VBptr=out;mesh.OldVertexBuffer=rest;mesh.VertsInterpolants=nullptr;
 CalcBones(&mesh,&anim,1);
 assert(out[0].x==1000 && out[1].x==2000);assert(out[2].x==12);assert(out[3].x==24);
 CalcBones(&mesh,&anim,1);assert(out[2].x==12 && out[3].x==24);
 puts("PASS production skinning: sparse absolute indices, shared-bone averaging, repeat stability");}
'''
work=root/'build/watchmaker-data-test';work.mkdir(parents=True,exist_ok=True);p=work/'bones.cpp';p.write_text(code)
subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-Wno-sign-compare','-fsanitize=address,undefined',str(p),'-o',str(work/'bones')],check=True)
subprocess.run([str(work/'bones')],check=True)
