#!/usr/bin/env python3
"""Sanitizer fixtures for the actual Mjolner findPos and variable-offset code."""
from pathlib import Path
import subprocess, sys, tempfile
root = Path(sys.argv[1]) / 'engines/director/lingo'
s = (root / 'lingo-builtins.cpp').read_text()
a = s.index('void LB::b_findPos(int nargs) {')
findpos = s[a:s.index('\nvoid LB::b_findPosNear', a)]
s = (root / 'lingo-bytecode.cpp').read_text()
a = s.index('\t\t\tint stride = 6;')
b = s.index('\n\t\t\tCommon::Array<Common::String> *varNames', a)
offset = s[a:b]
assert 'findVarV4(varType, varID, true)' in s[s.index('void LC::cb_delete()'):s.index('void LC::cb_hilite()')]
code = r'''
#include <cassert>
#include <string>
#include <vector>
#include <cstdio>
namespace Common { using String=std::string; }
enum {INT,STRING,ARRAY,PARRAY};
struct Array;
struct Datum {
 int type=INT; std::string s;
 struct {Array *farr=nullptr;int i=0;} u;
 Datum(int v=0){u.i=v;}
 Datum(const char *v):type(STRING),s(v) {}
 int asInt() const { return type==INT ? u.i : 0; }
};
struct Array {bool _sorted=false;std::vector<Datum> arr;};
struct Director {int version=700;const char *game="jonssonligan1";
 int getVersion(){return version;} const char *getGameId(){return game;}} director;
auto g_director=&director;
bool enabled=true;
bool isPS4Jonsson(){return enabled;}
void warning(const char *,...) {}
struct Lingo {
 std::vector<Datum> stack;
 Datum pop(){assert(!stack.empty());auto d=stack.back();stack.pop_back();return d;}
 void push(Datum d){stack.push_back(d);} Datum getVoid(){return Datum();}
} lingo;
auto g_lingo=&lingo;
#define TYPECHECK2(d,a,b) assert(d.type==a || d.type==b)
namespace LC {
 Datum eqData(Datum a,Datum b){return Datum(a.type==b.type && (a.type==STRING ? a.s==b.s : a.u.i==b.u.i));}
 Datum compareArrays(Datum (*compare)(Datum,Datum),Datum a,Datum b,bool){
  for(unsigned i=0;i<a.u.farr->arr.size();++i)if(compare(a.u.farr->arr[i],b).u.i)return Datum(i+1);
  return Datum(-1);
 }
}
namespace LB {void b_findPos(int);}
'''+findpos+r'''
int slot(int raw, bool byteOffsets) {
 Datum id(raw);int res=-1,varType=5;
'''+offset+r'''
 return varIndex;
}
int find(Array &a, Datum name) {
 assert(lingo.stack.empty());Datum list;list.type=ARRAY;list.u.farr=&a;
 lingo.push(list);lingo.push(name);LB::b_findPos(2);
 assert(lingo.stack.size()==1);return lingo.pop().asInt();
}
int main(){
 Array a; a.arr={Datum("other"),Datum("kylskap1"),Datum("last")};
 assert(find(a,Datum("kylskap1"))==2);
 assert(find(a,Datum("missing"))==0);
 assert(find(a,Datum(2))==2);
 assert(find(a,Datum(4))==0);
 a.arr[1]=Datum("kylskap2");assert(find(a,Datum("kylskap2"))==2);
 a._sorted=true;assert(find(a,Datum("kylskap2"))==2);
 enabled=false;assert(find(a,Datum("kylskap2"))==2);
 a._sorted=false;assert(find(a,Datum(2))==2);
 assert(find(a,Datum("kylskap2"))==0); // unchanged legacy behavior when disabled
 enabled=true;director.game="jonssonligan2";
 assert(find(a,Datum("kylskap2"))==0);
 director.game="jonssonligan1";
 enabled=true;
 assert(slot(24,true)==3);assert(slot(8,true)==1);
 assert(slot(1,false)==1);assert(slot(8,false)==8);
 assert(slot(7,true)==-1);
 enabled=false;assert(slot(24,true)==3);assert(slot(24,false)==3);
 director.version=400;assert(slot(18,true)==3);
 puts("PASS: object name lookup, miss, numeric behavior, mutation; D7 delete byte offsets and put slot IDs; ASan/UBSan");
}
'''
with tempfile.TemporaryDirectory() as t:
    p=Path(t);(p/'test.cpp').write_text(code)
    subprocess.run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
