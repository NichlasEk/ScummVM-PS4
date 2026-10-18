#!/usr/bin/env python3
"""Exercise the extracted missing-property dialogue guard with list fixtures."""
from pathlib import Path
import subprocess,sys,tempfile
s=(Path(sys.argv[1])/'engines/director/lingo/lingo-builtins.cpp').read_text()
a=s.index('\t\t\tconst Common::Array<CFrame *> &frames',s.index('void LB::b_getProp('));b=s.index('\n\t\tbreak;',a)
branch=s[a:b].rsplit('\n\t}',1)[0].rsplit('\n\t\t}',1)[0]
code=r'''
#include <cassert>
#include <string>
#include <vector>
#include <algorithm>
namespace Common {template<class T> using Array=std::vector<T>;struct String:std::string {using std::string::string;bool equalsIgnoreCase(const char *b)const{std::string a=*this;std::transform(a.begin(),a.end(),a.begin(),::tolower);std::string c=b;std::transform(c.begin(),c.end(),c.begin(),::tolower);return a==c;}};}
struct List;
struct Datum {Common::String text;struct {int i=0;List*parr=nullptr;}u;Datum(){}Datum(const char*s):text(s){}Common::String asString()const{return text;}};
struct List {std::vector<std::string> arr;};
struct CFrame {struct {Common::String *name;}sp;};
struct State {Common::Array<CFrame*> callstack;};
struct VM {State state;State*_state=&state;int pushes=0,errors=0;void push(Datum){pushes++;}void lingoError(const char*,const char*){errors++;}}vm,*g_lingo=&vm;
bool enabled=true;bool isPS4Jonsson(){return enabled;}
namespace LC {int eqData;Datum compareArrays(int,Datum l,Datum p,bool){Datum d;auto&a=l.u.parr->arr;auto i=std::find(a.begin(),a.end(),p.text);d.u.i=i==a.end()?0:i-a.begin()+1;return d;}}
void probe(Datum list,Datum prop){
'''+branch+r'''
}
int main(){
 Common::String cue="CuePassed",other="update";CFrame f;f.sp.name=&cue;vm.state.callstack.push_back(&f);
 List players{{"sickan","vanis","harry"}};Datum list;list.u.parr=&players;
 probe(list,Datum("doris"));assert(vm.pushes==1&&vm.errors==0);
 f.sp.name=&other;probe(list,Datum("doris"));assert(vm.errors==1);
 f.sp.name=&cue;enabled=false;probe(list,Datum("doris"));assert(vm.errors==2);enabled=true;
 players.arr[0]="someone";probe(list,Datum("doris"));assert(vm.errors==3);
 vm.state.callstack.clear();probe(list,Datum("doris"));assert(vm.errors==4);
 assert(vm.pushes==1);
}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.cpp').write_text(code)
 subprocess.run(['c++','-std=c++17','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: dialogue probe returns one VOID only for the Jonsson player-list fallback')
