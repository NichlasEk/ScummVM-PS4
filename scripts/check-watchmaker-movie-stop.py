#!/usr/bin/env python3
"""Exercise the production material pause gate and script flag changes."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[1]
src = root / '.tools/scummvm-vulkan-src/engines/watchmaker'
s = (src / 'll/ll_mesh.cpp').read_text()
change = s[s.index('void ChangeMeshMaterialFlag('):s.index('/* -----------------', s.index('void ChangeMeshMaterialFlag('))]
s = (src / '3d/material.cpp').read_text()
a = s.index('\tif ((mat.Flags & T3D_MATERIAL_MOVIE)')
gate = s[a:s.index('\n\tif ((mat.NumFaces()', a)]
code = r'''
#include <cassert>
#include <cstdio>
#include <memory>
#include <vector>
using uint32 = unsigned; using int8 = signed char;
constexpr unsigned T3D_MATERIAL_MOVIE=1, T3D_MATERIAL_MOVIEPAUSED=8192;
constexpr int MM_ADD_MAT_FLAGS=1, MM_REMOVE_MAT_FLAGS=2;
unsigned now=10000; unsigned timeGetTime(){return now;}
struct Movie {
 unsigned _curFrame=0xffff, _frameRate=10, _startTime=0, updates=0;
 bool _paused=false;
 void setFrame(unsigned f){_curFrame=f;}
 void updateMovie(){++updates;_curFrame=(now-_startTime)/100;}
};
struct Material {
 unsigned Flags=T3D_MATERIAL_MOVIE;
 std::shared_ptr<::Movie> Movie=std::make_shared<::Movie>();
 void addProperty(unsigned f){Flags|=f;} void clearFlag(unsigned f){Flags&=~f;}
};
struct Face {std::shared_ptr<Material> m; std::shared_ptr<Material> getMaterial(){return m;}};
struct t3dMESH {
 std::vector<Face> FList; const char *name="fixture";
 bool hasFaceMaterial(){return !FList.empty() && bool(FList[0].m);}
};
struct VM {unsigned calls=0; void addMeshModifier(const char*,int,unsigned*){++calls;}} vm,*_vm=&vm;
''' + change + '\nvoid render(Material &mat){\n' + gate + r'''
}
int main(){
 t3dMESH empty; ChangeMeshMaterialFlag(nullptr,1,8192);ChangeMeshMaterialFlag(&empty,1,8192);
 empty.FList.push_back({nullptr});ChangeMeshMaterialFlag(&empty,1,8192);assert(vm.calls==0);
 auto m=std::make_shared<Material>();t3dMESH mesh;mesh.FList.push_back({m});auto &v=*m->Movie;
 ChangeMeshMaterialFlag(&mesh,1,T3D_MATERIAL_MOVIEPAUSED);render(*m);
 assert(v._curFrame==0 && v.updates==0 && v._paused);
 ChangeMeshMaterialFlag(&mesh,-1,T3D_MATERIAL_MOVIEPAUSED);render(*m);
 now+=200;render(*m);assert(v._curFrame==2);
 ChangeMeshMaterialFlag(&mesh,1,T3D_MATERIAL_MOVIEPAUSED);render(*m);
 auto count=v.updates;now+=30000;render(*m);assert(v._curFrame==2 && v.updates==count);
 ChangeMeshMaterialFlag(&mesh,-1,T3D_MATERIAL_MOVIEPAUSED);render(*m);assert(v._curFrame==2);
 now+=100;render(*m);assert(v._curFrame==3 && vm.calls==4);
 m->Movie.reset();render(*m);
 puts("PASS production movie stop: valid/empty meshes, initial paused image, held frame and resume clock");
}
'''
work=root/'build/watchmaker-data-test';work.mkdir(parents=True,exist_ok=True)
p=work/'movie-stop.cpp';p.write_text(code)
subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p),'-o',str(work/'movie-stop')],check=True)
subprocess.run([str(work/'movie-stop')],check=True)
