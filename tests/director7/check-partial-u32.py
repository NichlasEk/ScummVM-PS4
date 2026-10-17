#!/usr/bin/env python3
"""Exercise the actual D7 partial-field decoder in both byte orders."""
import pathlib
import subprocess
import sys
import tempfile
text = (pathlib.Path(sys.argv[1]) / 'engines/director/frame.cpp').read_text()
start = text.index('static uint32 readSpriteD7Uint32(')
end = text.index('\nvoid readSpriteDataD7(', start)
probe = r'''
#include <cassert>
#include <cstdint>
using uint32 = uint32_t;
namespace Common {
struct SeekableReadStreamEndian {
 bool be; int cursor = 0;
 explicit SeekableReadStreamEndian(bool endian) : be(endian) {}
 int pos() const { return cursor; }
 bool isBE() const { return be; }
 uint8_t readByte() { return 0xa0 + cursor++; }
};
}
'''
main = r'''
int main() {
 for (bool be : {false, true})
  for (int offset = 0; offset < 4; ++offset)
   for (int count = 1; count <= 4 - offset; ++count) {
    Common::SeekableReadStreamEndian stream(be);
    uint32 result = readSpriteD7Uint32(stream, 0x12345678, offset, count);
    assert(stream.cursor == count);
    uint8_t bytes[] = {0x12, 0x34, 0x56, 0x78};
    if (!be) { bytes[0]=0x78; bytes[1]=0x56; bytes[2]=0x34; bytes[3]=0x12; }
    for (int i=0; i<count; ++i) bytes[offset+i]=0xa0+i;
    uint32 expected = 0;
    for (int i=0; i<4; ++i) expected = (expected << 8) | bytes[be ? i : 3-i];
    assert(result == expected);
   }
}
'''
with tempfile.TemporaryDirectory(prefix='director7-u32-') as tmp:
    cpp=pathlib.Path(tmp)/'probe.cpp'; exe=pathlib.Path(tmp)/'probe'
    cpp.write_text('#include <initializer_list>\n'+probe+text[start:end]+main)
    subprocess.run(['c++','-std=c++11','-Wall','-Wextra',str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('D7 partial uint32: all 20 byte-order/offset/length combinations passed')
