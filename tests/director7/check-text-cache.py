#!/usr/bin/env python3
"""Exercise the extracted cache's invalidation, bounds and title gate.
String adapters model conversion; actual UTF-8 decoding remains Common's code.
"""
from pathlib import Path
import subprocess, sys, tempfile
s=(Path(sys.argv[1])/'engines/director/lingo/lingo.cpp').read_text()
a=s.index('static Common::U32String decodePS4ChunkText(')
b=s.index('\nCommon::U32String Lingo::evalChunkRef(',a)
code=r'''
#include <string>
#include <cassert>
using uint=unsigned; static bool enabled=true; static unsigned decodes=0;
bool isPS4Jonsson(){return enabled;}
namespace Common {
enum {kUtf8}; using U32String=std::string;
struct String:std::string {
 using std::string::string;
 U32String decode(int)const{++decodes;return *this;}
};
}
'''+s[a:b]+r'''
int main(){
 Common::String a(4096,'a');
 assert(decodePS4ChunkText(a)==a); auto n=decodes;
 for(int i=0;i<1000;i++) assert(decodePS4ChunkText(a)==a);
 assert(decodes==n);
 // Same address, changed contents must invalidate the cached value.
 a[2048]='b'; assert(decodePS4ChunkText(a)==a); assert(decodes==n+1);
 // Different object with equal contents may reuse the decoded value.
 Common::String copy=a; n=decodes; assert(decodePS4ChunkText(copy)==copy);assert(decodes==n);
 for(char c='c';c<'j';c++){Common::String x(4096,c);assert(decodePS4ChunkText(x)==x);}
 n=decodes;assert(decodePS4ChunkText(a)==a);assert(decodes==n+1);
 for(auto len:{0,1023,1048577}){Common::String x(len,'x');n=decodes;decodePS4ChunkText(x);decodePS4ChunkText(x);assert(decodes==n+2);}
 enabled=false;n=decodes;assert(decodePS4ChunkText(a)==a);assert(decodePS4ChunkText(a)==a);assert(decodes==n+2);
}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.cpp').write_text(code)
 subprocess.run(['c++','-std=c++17','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: extracted text cache hits, mutation, replacement, bounds and title gate')
