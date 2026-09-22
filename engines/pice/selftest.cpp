// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/kro.h"
#include "common/memstream.h"
#include "common/endian.h"
namespace Pice {
bool runSelfTests() {
 byte raw[32] = {'B','u','r','p'};
 WRITE_LE_UINT32(raw + 4, 1); WRITE_LE_UINT32(raw + 8, 4);
 WRITE_LE_UINT32(raw + 12, 4); WRITE_LE_UINT32(raw + 20, 28);
 Common::Array<KroEntry> entries;
 Common::MemoryReadStream valid(raw, sizeof(raw));
 if (!parseKroIndex(valid, entries) || entries.size() != 1) return false;
 Common::MemoryReadStream truncated(raw, 27);
 if (parseKroIndex(truncated, entries) || !entries.empty()) return false;
 WRITE_LE_UINT32(raw + 4, 0xffffffff);
 if (parseKroIndex(valid, entries)) return false;
 WRITE_LE_UINT32(raw + 4, 1); WRITE_LE_UINT32(raw + 20, 0xfffffff0);
 if (parseKroIndex(valid, entries)) return false;
 WRITE_LE_UINT32(raw + 20, 8); // payload points inside index
 if (parseKroIndex(valid, entries)) return false;
 WRITE_LE_UINT32(raw + 20, 28); WRITE_LE_UINT32(raw + 24, 99);
 if (parseKroIndex(valid, entries)) return false;
 WRITE_LE_UINT32(raw + 24, 0); WRITE_LE_UINT32(raw + 8, 5);
 if (parseKroIndex(valid, entries)) return false;
 Common::Array<byte> image; image.resize(8);
 WRITE_LE_UINT16(&image[0], 2); WRITE_LE_UINT16(&image[2], 2);
 uint16 w, h;
 if (!decodeImageHeader(image, w, h) || w != 2 || h != 2) return false;
 image.resize(7);
 if (decodeImageHeader(image, w, h)) return false;
 WRITE_LE_UINT16(&image[0], 65535);
 if (decodeImageHeader(image, w, h)) return false;
 // Verify range boundaries and holes in the demo's virtual resource table.
 for(uint sound=0;sound<2;++sound) {
  Common::Array<KroEntry> demo; demo.resize(sound ? 83 : 320);
  for(uint i=0;i<demo.size();++i) demo[i]=KroEntry{1,1,i+1,0};
  if(!expandDemoIndex(demo,sound!=0)) return false;
  if(sound) {
   if(demo.size()!=353 || demo[45].offset!=7 || demo[112].offset!=74 ||
      demo[113].size || demo[305].offset!=75 || demo[308].offset!=78 ||
      demo[313].offset!=83 || demo[314].size) return false;
  } else if(demo.size()!=1338 || demo[178].offset!=179 || demo[179].size ||
     demo[815].offset!=180 || demo[881].offset!=246 || demo[882].size ||
     demo[1138].offset!=247 || demo[1179].offset!=288 || demo[1180].size ||
     demo[1306].offset!=289 || demo[1337].offset!=320) return false;
  if(expandDemoIndex(demo,sound!=0)) return false;
 }
 return true;
}
}
