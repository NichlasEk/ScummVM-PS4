#!/usr/bin/env python3
"""Compile the prepared D7 channel splitter against bounded stream probes."""
import pathlib
import subprocess
import sys
import tempfile

source = pathlib.Path(sys.argv[1]) / 'engines/director/frame.cpp'
text = source.read_text()
start = text.index('void Frame::readChannelD7(')
end = text.index('\nvoid Frame::readMainChannelsD7(', start)
method = text[start:end]
preamble = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
using uint16 = uint16_t;
using byte = uint8_t;
#define MIN std::min
constexpr unsigned kMainChannelSizeD7 = 288, kSprChannelSizeD7 = 48;
namespace Common { struct MemoryReadStreamEndian { unsigned remaining; }; }
struct Frame {
 unsigned calls = 0;
 void readChannelD7(Common::MemoryReadStreamEndian &, uint16, uint16);
 void readMainChannelsD7(Common::MemoryReadStreamEndian &s, uint16 o, uint16 n) {
  assert(o + n <= 288 && n <= s.remaining); s.remaining -= n;
 }
 void readSpriteD7(Common::MemoryReadStreamEndian &s, uint16 o, uint16 n) {
  assert(o >= 288 && n > 0 && n <= 48 - ((o - 288) % 48));
  assert(n <= s.remaining); s.remaining -= n; ++calls;
 }
};
'''
main = r'''
int main() {
 // Every possible two-byte delta start, including channels 256 through 999.
 for (unsigned channel = 0; channel < 1000; ++channel)
  for (unsigned field = 0; field < 48; field += 2) {
   const unsigned length = std::min(144u, (1000 - channel) * 48 - field);
   Frame f; Common::MemoryReadStreamEndian stream{length};
   f.readChannelD7(stream, 288 + channel * 48 + field, length);
   assert(stream.remaining == 0);
   assert(f.calls == (field + length + 47) / 48);
  }
 Frame f; Common::MemoryReadStreamEndian stream{244};
 f.readChannelD7(stream, 284, 244);
 assert(stream.remaining == 0 && f.calls == 5);
}
'''
with tempfile.TemporaryDirectory(prefix='director7-split-') as tmp:
    cpp = pathlib.Path(tmp) / 'probe.cpp'
    binary = pathlib.Path(tmp) / 'probe'
    cpp.write_text(preamble + method + main)
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('D7 channel split: 24000 delta starts and header crossing passed')
