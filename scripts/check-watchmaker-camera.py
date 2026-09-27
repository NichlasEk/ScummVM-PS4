#!/usr/bin/env python3
"""Exercise the production camera-grid selection at edges under ASan/UBSan."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
s=(root/'.tools/scummvm-vulkan-src/engines/watchmaker/classes/do_camera.cpp').read_text()
s=s[s.index('if (!ForcedCamera && ((col < 0'):]
s=s[:s.index('//\tMi salvo')].replace('return;', 'return 255;').replace('return ;', 'return 255;')
source=r'''
#include <vector>
#include <cassert>
#include <cstdio>
struct Room { struct { int Col,Row;std::vector<unsigned char> Grid; } CameraGrid; };
int select(Room *croom,int col,int row,bool ForcedCamera,int t3dLastCameraIndex=255){
 int cc=255,a,b;
'''+s+r'''
 return cc;
}
int main(){
 for(int width=1;width<=8;++width)for(int height=1;height<=8;++height){
  Room r{{width,height,std::vector<unsigned char>(width*height,255)}};
  for(int x=-3;x<width+3;++x)for(int y=-3;y<height+3;++y){
   assert(select(&r,x,y,true)==255);
   assert(select(&r,x,y,false)==255);
  }
  r.CameraGrid.Grid.back()=7;
  for(int x=0;x<width;++x)for(int y=0;y<height;++y)
   assert(select(&r,x,y,false)==7);
 }
 puts("PASS production camera grid: forced off-grid cameras, all four search edges and nearby selection");
}
'''
work=root/'build/watchmaker-data-test';work.mkdir(parents=True,exist_ok=True)
p=work/'camera.cpp';p.write_text(source)
subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-D_GLIBCXX_ASSERTIONS','-fsanitize=address,undefined',str(p),'-o',str(work/'camera')],check=True)
subprocess.run([str(work/'camera')],check=True)
