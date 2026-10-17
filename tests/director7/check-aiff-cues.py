#!/usr/bin/env python3
"""Compile the real linked-AIFF cue reader with a small memory-stream adapter."""
import pathlib
import struct
import subprocess
import sys
import tempfile
source = (pathlib.Path(sys.argv[1]) / 'engines/director/sound.cpp').read_text()
a = source.index('void loadLinkedAIFFCues(')
b = source.index('\nDirectorSound::DirectorSound', a)
function = source[a:b]
harness = r'''
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <iterator>
#include <cassert>
using uint=unsigned; using uint16=uint16_t; using uint32=uint32_t;
using uint64=uint64_t; using int32=int32_t;
#define MKTAG(a,b,c,d) ((uint32(a)<<24)|(uint32(b)<<16)|(uint32(c)<<8)|uint32(d))
namespace Common {
using String=std::string;
template<class T> using Array=std::vector<T>;
using StringArray=Array<String>;
class SeekableReadStream {
 std::vector<unsigned char> bytes; size_t offset=0;
public:
 explicit SeekableReadStream(const String &path) {
  std::ifstream f(path, std::ios::binary);
  bytes.assign(std::istreambuf_iterator<char>(f), {});
 }
 size_t size() const {return bytes.size();}
 size_t pos() const {return offset;}
 void seek(size_t p) {assert(p<=bytes.size()); offset=p;}
 void skip(size_t n) {seek(offset+n);}
 unsigned char readByte() {assert(offset<bytes.size()); return bytes[offset++];}
 uint16 readUint16BE() {uint16 v=readByte(); return (v<<8)|readByte();}
 uint32 readUint32BE() {uint32 v=readUint16BE(); return (v<<16)|readUint16BE();}
 uint64 readUint64BE() {uint64 v=readUint32BE(); return (v<<32)|readUint32BE();}
};
struct MacResManager {static SeekableReadStream *openFileOrDataFork(const String &p) {return new SeekableReadStream(p);}};
}
static Common::String findAudioPath(const Common::String &p) {return p;}
''' + function + r'''
int main(int argc, char **argv) {
 Common::Array<int32> times; Common::StringArray names;
 loadLinkedAIFFCues(argv[1],times,names);
 int expected=std::stoi(argv[2]);
 assert(times.size()==(size_t)expected && names.size()==times.size());
 if(expected) {assert(times[0]==100 && times[1]==500); assert(names[0]=="first" && names[1]=="slut");}
}
'''
def chunk(tag, data):
 return tag + struct.pack('>I', len(data)) + data + (b'\0' if len(data)&1 else b'')
def form(data, kind=b'AIFF'):
 return b'FORM'+struct.pack('>I',len(data)+4)+kind+data
def marker(n, pos, name):
 return struct.pack('>HIB', n, pos, len(name))+name+(b'\0' if (len(name)+1)&1 else b'')
comm=chunk(b'COMM',struct.pack('>HIHHQ',1,22050,16,0x400d,22050<<49))
mark=chunk(b'MARK',struct.pack('>H',2)+marker(1,2205,b'first')+marker(2,11025,b'slut'))
cases=[(form(comm+mark),2),(form(mark+comm,b'AIFC'),2),
       (form(chunk(b'JUNK',b'x')+comm+mark),2),
       (form(comm+mark)[:-1],0),(form(mark),0),
       (form(comm+chunk(b'MARK',b'\x00\x01\x00')),0),
       (b'FORM'+b'\xff'*4+b'AIFF',0),(b'',0),
       (form(comm+mark,b'WAVE'),0)]
with tempfile.TemporaryDirectory() as tmp:
 d=pathlib.Path(tmp); cpp=d/'test.cpp'; exe=d/'test'
 cpp.write_text(harness)
 subprocess.run(['clang++','-std=c++11','-O2',str(cpp),'-o',str(exe)],check=True)
 for i,(data,count) in enumerate(cases):
  path=d/f'{i}.aif';path.write_bytes(data)
  subprocess.run([str(exe),str(path),str(count)],check=True)
print(f'AIFF cue reader: {len(cases)} order, padding, timing and malformed-input cases passed')
