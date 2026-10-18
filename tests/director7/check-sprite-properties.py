#!/usr/bin/env python3
"""Compile production sprite property branches against small behavior fixtures."""
from pathlib import Path
import subprocess,sys,tempfile
s=(Path(sys.argv[1])/'engines/director/lingo/lingo-the.cpp').read_text()
def branch(start):
 t=s[s.index(start):];a=t.index('} else if (isPS4Jonsson()');b=t.index('\n\t} else {',a) if start.startswith('void Lingo::set') else t.index('\n\t\tg_lingo->push(d);',a)
 return t[a+7:b]
get=branch('void Lingo::getObjectProp(')
set_=branch('void Lingo::setObjectProp(')
code=r"""
#include <cassert>
#include <map>
#include <string>
#include <vector>
enum { INT, OBJECT };
struct Obj;
struct Datum {int type=INT; union U {int i;Obj*obj;U():i(0){}} u;};
struct Obj {std::map<std::string,Datum> p; bool hasProp(std::string n){return p.count(n);} Datum getProp(std::string n){return p.at(n);}void setProp(std::string n,Datum d){p[n]=d;}};
struct Channel {std::vector<Datum> _scriptInstanceList;};
struct Score {Channel ch;Channel*getChannelById(int n){return n==47?&ch:nullptr;}};
struct Movie {Score score;Score*getScore(){return &score;}};
struct VM {Movie movie;bool active=true;Movie*getCurrentMovie(){return active?&movie:nullptr;}};
VM vm;VM*_vm=&vm;bool enabled=true;bool isPS4Jonsson(){return enabled;}
Datum read(Datum obj,std::string propName){Datum d;
"""+get+r"""
return d;}
void write(Datum obj,std::string propName,Datum val){
"""+set_+r"""
}
int main(){Obj first,second,target;Datum zero,value,a,b,ref;ref.u.i=47;value.type=OBJECT;value.u.obj=&target;a.type=b.type=OBJECT;a.u.obj=&first;b.u.obj=&second;first.p["pObjekt"]=zero;second.p["pObjekt"]=zero;vm.movie.score.ch._scriptInstanceList={zero,a,b};write(ref,"pObjekt",value);assert(read(ref,"pObjekt").u.obj==&target);assert(second.p["pObjekt"].type==INT);write(ref,"missing",value);assert(!first.hasProp("missing"));enabled=false;write(ref,"pObjekt",zero);assert(read(ref,"pObjekt").type==INT);enabled=true;assert(read(ref,"pObjekt").type==OBJECT);ref.u.i=0;assert(read(ref,"pObjekt").type==INT);vm.active=false;write(ref,"pObjekt",value);assert(read(ref,"pObjekt").type==INT);}
"""
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(code)
 subprocess.run(['c++','-std=c++17','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Sprite behavior property regression passed')
