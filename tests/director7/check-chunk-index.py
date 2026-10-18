#!/usr/bin/env python3
"""Compare the extracted large-text index against Director's original scan."""
from pathlib import Path
import subprocess, sys, tempfile
s=(Path(sys.argv[1])/'engines/director/lingo/lingo-code.cpp').read_text()
a=s.index('static bool indexedPS4ChunkBounds(')
b=s.index('\nDatum LC::chunkRef(',a)
scan_start=s.index('\n\t\t\tint idx = 0;',s.index('if (indexedPS4ChunkBounds(',b))
scan_end=s.index('\n\t\t}\n\t\tbreak;',scan_start)
code=r'''
#include <string>
#include <vector>
#include <algorithm>
#include <cassert>
#include <random>
#include <cstdio>
using uint=unsigned;
#define MIN(a,b) std::min(a,b)
static bool enabled=true;
bool isPS4Jonsson(){return enabled;}
namespace Common {
using U32String=std::u32string;
using u32char_type_t=char32_t;
template<class T> using Array=std::vector<T>;
}
'''+s[a:b]+r'''
void check(const Common::U32String &str, char32_t delimiter, int startChunk, int endChunk) {
 int chunkNum=0,chunkStartIdx=-1,chunkEndIdx=-1,exprStartIdx=-1,exprEndIdx=-1;
'''+s[scan_start:scan_end]+r'''
 int n=0,cs=-1,ce=-1,es=-1,ee=-1;
 assert(indexedPS4ChunkBounds(str,delimiter,startChunk,endChunk,n,cs,ce,es,ee));
 assert(n==chunkNum && cs==chunkStartIdx && ce==chunkEndIdx && es==exprStartIdx && ee==exprEndIdx);
}
int main(){
 std::mt19937 rng(702);
 for(int trial=0;trial<120;trial++) {
  Common::U32String s(1024+rng()%8192,U'a');
  for(auto &c:s) if(rng()%7==0)c=(rng()%2)?U'\r':U'£';
  for(auto d:{U'\r',U'£',U'a',U'x',U'\0'}) {
   for(int i=0;i<30;i++){int a=1+rng()%(s.size()+2),b=1+rng()%(s.size()+2);check(s,d,a,b);}
   check(s,d,-30000,-30000);check(s,d,1,1);
   s.back()=d;check(s,d,-30000,-30000); // mutation and final empty chunk
  }
 }
 for(auto size:{1024,1048576}) {Common::U32String s(size,U'\r');check(s,U'\r',-30000,-30000);check(s,U'x',1,1);}
 int n=0,cs=-1,ce=-1,es=-1,ee=-1;
 for(auto size:{0,1023,1048577})assert(!indexedPS4ChunkBounds(Common::U32String(size,U'a'),U'\r',1,1,n,cs,ce,es,ee));
 enabled=false;assert(!indexedPS4ChunkBounds(Common::U32String(4096,U'a'),U'\r',1,1,n,cs,ce,es,ee));
 puts("PASS: indexed chunk bounds match original scan; mutation, delimiter, ranges, limits and title gate");
}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.cpp').write_text(code)
 subprocess.run(['c++','-std=c++17','-O1','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
