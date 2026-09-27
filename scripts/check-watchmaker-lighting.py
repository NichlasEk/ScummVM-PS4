#!/usr/bin/env python3
"""Check production packed colors used by vertex lighting and lightmap markers."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[1]
src = root / '.tools/scummvm-vulkan-src/engines/watchmaker'
s = (src / 'windows_hacks.h').read_text()
macros = s[s.index('#define RGBA_MAKE'):s.index('#define TRUE')]
source = r'''
#include <cstdint>
#include <cassert>
#include <cstdio>
using uint32 = uint32_t;
using uint8 = uint8_t;
''' + macros + r'''
int main() {
 // Saved body lighting uses byte channels, while runtime lighting adds to them.
 for (unsigned i = 0; i < 256; ++i) {
  auto color = RGBA_MAKE(i, 255-i, i^85, 173);
  assert(RGBA_GETRED(color) == i);
  assert(RGBA_GETGREEN(color) == 255-i);
  assert(RGBA_GETBLUE(color) == (i^85));
  assert(RGBA_GETALPHA(color) == 173);
 }
 auto dark = RGBA_MAKE(40, 24, 12, 255);
 auto half = RGBA_MAKE((RGBA_GETRED(dark)*128)>>8,
                      (RGBA_GETGREEN(dark)*128)>>8,
                      (RGBA_GETBLUE(dark)*128)>>8, RGBA_GETALPHA(dark));
 assert(half == 0x140c06ffu);
 // 254 marks vertices whose second UV set comes from the lightmap file.
 auto marker = RGBA_MAKE(254, 254, 254, 85);
 assert(RGBA_GETRED(marker)==254 && RGBA_GETGREEN(marker)==254 && RGBA_GETBLUE(marker)==254);
 assert(RGBA_MAKE(255,255,255,RGBA_GETALPHA(marker))==0xffffff55u);
 assert(RGBA_MAKE(255,255,255,255)==0xffffffffu);
 puts("PASS production lighting colors: all byte values, half-intensity, lightmap marker and alpha");
}
'''
work = root / 'build/watchmaker-data-test'
work.mkdir(parents=True, exist_ok=True)
p = work / 'lighting.cpp'
p.write_text(source)
subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p),'-o',str(work/'lighting')],check=True)
subprocess.run([str(work/'lighting')],check=True)
