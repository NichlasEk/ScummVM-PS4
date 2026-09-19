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
 return true;
}
}
