#!/usr/bin/env python3
"""Exercise the diagnostic route planner, including original INIT collision data."""
import subprocess,sys,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
save=Path(sys.argv[1]).read_bytes()[64:]
state=bytes(v^((i+1)*5&255) for i,v in enumerate(save))
with tempfile.TemporaryDirectory() as t:
 t=Path(t);(t/'state').write_bytes(state)
 (t/'test.cpp').write_text(r'''
#include "engines/sotc/route.h"
#include "engines/sotc/collision.h"
#include <cassert>
#include <cstdio>
int main(int argc,char **argv) {
 assert(argc==2); Sotc::Route r;
 assert(r.find(1,1,8,1,[](int x,int y,unsigned){return x!=4 || y==5;}));
 assert(r.count==15);
 assert(!r.find(1,1,8,1,[](int x,int,unsigned){return x!=4;})); assert(!r.count);
 assert(!r.find(-1,1,8,1,[](int,int,unsigned){return true;}));
 assert(r.find(1,1,1,1,[](int,int,unsigned){return false;}));assert(!r.count);
 // Open ground should be a straight diagonal, not two orthogonal legs.
 auto open=[](int,int,unsigned){return true;};
 assert(r.find(10,10,90,90,open));r.smooth(10,10,open);assert(r.count==80);
 for(unsigned i=0;i<r.count;++i)assert(r.points[i]%320==r.points[i]/320);
 // A blocked side of a diagonal must prevent corner cutting.
 auto corner=[](int x,int y,unsigned){return !(x==11 && y==10);};
 assert(!Sotc::Route::clearStep(10,10,11,11,corner));
 assert(r.find(10,10,20,20,corner));r.smooth(10,10,corner);
 int cx=10,cy=10;for(unsigned i=r.count;i;--i){int nx=r.points[i-1]%320,ny=r.points[i-1]/320;assert(Sotc::Route::clearStep(cx,cy,nx,ny,corner));cx=nx;cy=ny;}
 unsigned char state[3902];FILE *f=fopen(argv[1],"rb");assert(f);assert(fread(state,1,sizeof(state),f)==sizeof(state));fclose(f);
 auto byte=[&](unsigned a){return state[a-0x2dee];};
 auto word=[&](unsigned a){return byte(a)|(byte(a+1)<<8);};
 Sotc::CollisionActor actors[11];
 for(unsigned i=0;i<11;++i) {unsigned a=0x2e54+i*52;
  actors[i]={Sotc::footprint(Sotc::walkWord(word(a)),Sotc::walkWord(word(a+2)),byte(a+21),byte(a+22)),word(a+26)!=0,byte(a+23)==8};
 }
 auto clear=[&](int x,int y,unsigned d){auto p=Sotc::footprint(x,y,4,2);return !Sotc::terrainCollision(state+0x339e - 0x2dee,p,d) && !Sotc::obstacleCollision(state+0x3544-0x2dee,byte(0x3542),p) && !Sotc::actorCollision(actors,0,p);};
 assert(!clear(240,177,2));
 assert(r.find(220,177,310,150,clear));
 unsigned originalSteps=r.count; r.smooth(220,177,clear);assert(r.count<originalSteps);
 int x=220,y=177; unsigned turns=0,last=0;
 for(unsigned i=r.count;i;--i) {unsigned p=r.points[i-1];int nx=p%320,ny=p/320;
  int distance=(nx-x)*(nx-x)+(ny-y)*(ny-y);assert(distance==1 || distance==2);
  assert(Sotc::Route::clearStep(x,y,nx,ny,clear));
  unsigned d=nx>x?2:nx<x?4:ny>y?3:1; assert(clear(nx,ny,d));
  if(last && last!=d) ++turns;last=d;x=nx;y=ny;
 }
 assert(x==310 && y==150);
 printf("Original scene detour: %u clear steps, %u turns; blocked/unreachable/identity cases pass\n",r.count,turns);
}
''')
 subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I',str(root),str(t/'test.cpp'),'-o',str(t/'test')],check=True)
 subprocess.run([str(t/'test'),str(t/'state')],check=True)
