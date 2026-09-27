#!/usr/bin/env python3
"""Verify production normal initialization preserves shared face planes."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
s=(root/'.tools/scummvm-vulkan-src/engines/watchmaker/3d/t3d_body.cpp').read_text()
a=s.index('void t3dBODY::initNormals(');b=s.index('\n}',a)+2;s=s[a:b]
source=r'''
#include <vector>
#include <memory>
#include <cstdint>
#include <cassert>
#include <cstdio>
using uint=unsigned;using uint16=uint16_t;
namespace Common {struct SeekableReadStream {int next=10;};template<class T> using SharedPtr=std::shared_ptr<T>;}
struct t3dNORMAL {int plane=0;t3dNORMAL()=default;t3dNORMAL(Common::SeekableReadStream &s):plane(s.next++) {}};
struct Vertex {int diffuse;};
struct t3dMESH {std::vector<Common::SharedPtr<t3dNORMAL>> NList;uint16 NumVerts=2,NumNormals=0,NumVerticesNormals=0;Vertex VertexBuffer[2]{{2},{0}};Vertex *VBptr=nullptr;};
struct t3dBODY {uint NumNormals=3,NumVerticesNormals=0;std::vector<Common::SharedPtr<t3dNORMAL>> NList;std::vector<t3dMESH> MeshTable;
uint NumMeshes(){return MeshTable.size();}void initNormals(Common::SeekableReadStream &);};
'''+s+r'''
int main(){t3dBODY body;
 for(int i=0;i<3;++i)body.NList.push_back(std::make_shared<t3dNORMAL>());
 body.MeshTable.resize(2);
 for(auto &mesh:body.MeshTable)mesh.NList=body.NList;
 auto faceNormal=body.NList[0];Common::SeekableReadStream stream;body.initNormals(stream);
 for(int i=0;i<3;++i)assert(body.NList[i]->plane==10+i);
 assert(faceNormal->plane==10);
 for(auto &mesh:body.MeshTable){assert(mesh.NList.size()==2);assert(mesh.NList[0]->plane==12);assert(mesh.NList[1]->plane==10);assert(mesh.NList[0]!=body.NList[2]);assert(mesh.VBptr==nullptr);}
 body.MeshTable[0].NList[0]->plane=99;
 assert(body.NList[2]->plane==12 && body.MeshTable[1].NList[0]->plane==12);
 puts("PASS production normal loading: shared planes unchanged; mesh normals independently owned");}
'''
work=root/'build/watchmaker-data-test';work.mkdir(parents=True,exist_ok=True)
p=work/'normals.cpp';p.write_text(source)
subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p),'-o',str(work/'normals')],check=True)
subprocess.run([str(work/'normals')],check=True)
