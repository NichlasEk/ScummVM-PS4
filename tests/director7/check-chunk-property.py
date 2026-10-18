#!/usr/bin/env python3
"""Exercise the production D7 repeated-selector adapter with synthetic room data."""
from pathlib import Path
import subprocess,sys,tempfile
s=(Path(sys.argv[1])/'engines/director/lingo/lingo-builtins.cpp').read_text()
a=s.index('static Datum ps4JonssonChunkPropertySource(')
b=s.index('\nvoid LB::b_getPropRef',a)
code=r'''
#include <cassert>
#include <memory>
#include <string>
#include <vector>
enum ChunkType{kChunkItem,kChunkLine};enum{STRING,CHUNKREF};
struct Ref;struct Datum{int type=STRING;struct{Ref*cref=nullptr;}u;std::string text;};
struct Ref{Datum source;ChunkType type;};
'''+s[a:b]+r'''
std::vector<std::unique_ptr<Ref>> refs;
Datum text(std::string s){Datum d;d.text=s;return d;}
Datum select(Datum source,ChunkType kind,int index,char delimiter){
 Datum base=ps4JonssonChunkPropertySource(source,kind);
 size_t begin=0,end=0;std::string value;
 for(int n=1;n<=index;++n){end=base.text.find(delimiter,begin);if(n==index){value=base.text.substr(begin,end==std::string::npos?end:end-begin);break;}if(end==std::string::npos){value="";break;}begin=end+1;}
 Datum d=text(value);d.type=CHUNKREF;refs.push_back(std::make_unique<Ref>(Ref{base,kind}));d.u.cref=refs.back().get();return d;
}
int main(){
 Datum data=text("Stage;Room;[10,20]\rAnim;barrel -- comment;1;100000\rExit;door");
 auto line=select(data,kChunkLine,2,'\r');
 auto item=select(line,kChunkItem,2,';');assert(item.text=="barrel -- comment");
 assert(select(item,kChunkItem,3,';').text=="1");
 assert(select(item,kChunkItem,4,';').text=="100000");
 auto withSpace=select(line,kChunkItem,2,' ');
 auto first=select(withSpace,kChunkItem,1,' ');assert(first.text=="Anim;barrel");
 // Assignment to a string materializes the value before another selector.
 assert(select(text(first.text),kChunkItem,2,';').text=="barrel");
 assert(item.text=="barrel -- comment"); // references are not mutated
 assert(select(text("a,b;c,d"),kChunkItem,2,';').text=="c,d");
 assert(select(text(item.text),kChunkItem,2,' ').text=="--");
 assert(select(line,kChunkItem,99,';').text.empty());
 assert(ps4JonssonChunkPropertySource(item,kChunkLine).u.cref==item.u.cref);
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(code)
 subprocess.run(['c++','-std=c++17','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Chunk selector regression passed (adapter and synthetic room-loader sequence)')
