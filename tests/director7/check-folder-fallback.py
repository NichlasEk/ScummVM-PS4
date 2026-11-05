#!/usr/bin/env python3
"""Exercise the production folder-selection branch without game data."""
from pathlib import Path
import subprocess, sys, tempfile
s=(Path(sys.argv[1])/'engines/director/lingo/lingo-builtins.cpp').read_text()
a=s.index('Common::Path path = findAbsolutePath(pathRaw, true);')
b=s.index('\n\t// for directory',a)
branch=s[a:b]
code=r'''
#include <cassert>
#include <string>
#include <vector>
#include <cstdio>
namespace Common {using String=std::string;using Path=std::string;}
bool enabled=true;int fallbacks=0;Common::Path primary,fallback;
struct Director {const char *game="jonssonligan1";const char *getGameId(){return game;}} director;
auto g_director=&director;
bool isPS4Jonsson(){return enabled;}
struct Datum {std::string s;Datum(const char *v):s(v){}};
struct Lingo {std::vector<Datum> stack;void push(Datum d){stack.push_back(d);}} lingo;
auto g_lingo=&lingo;
bool isAbsolutePath(Common::String s){return s.rfind("@\\",0)==0;}
Common::Path toSafePath(Common::String s){return s=="@\\"?"":s;}
Common::Path findAbsolutePath(Common::String,bool directory){assert(directory);return primary;}
Common::Path findPath(Common::String,bool current,bool search,bool directory){assert(current && search && directory);++fallbacks;return fallback;}
Common::Path enumerated;bool visited=false;
void lookup(const Common::String &pathRaw){
'''+branch+r'''
 visited=true;enumerated=path;
}
void reset(){visited=false;fallbacks=0;lingo.stack.clear();}
int main(){
 primary="";fallback="data/voice/Default";
 reset();lookup("@\\voice\\default\\");assert(visited && enumerated==fallback && fallbacks==1 && lingo.stack.empty());
 fallback="";reset();lookup("@\\missing\\");assert(!visited && fallbacks==1 && lingo.stack.size()==1 && lingo.stack[0].s.empty());
 primary="voice/Default";fallback="data/voice/Default";
 reset();lookup("@\\voice\\default\\");assert(visited && enumerated==primary && !fallbacks);
 primary="";reset();lookup("@\\");assert(visited && enumerated.empty() && !fallbacks);
 reset();lookup("relative");assert(visited && !fallbacks);
 enabled=false;reset();lookup("@\\voice\\default\\");assert(visited && !fallbacks);
 enabled=true;director.game="jonssonligan2";
 reset();lookup("@\\voice\\default\\");assert(visited && !fallbacks);
 puts("PASS: installed-folder fallback, missing-folder empty result, existing/root/relative paths and title/opt-in gates; ASan/UBSan");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(code)
 subprocess.run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
