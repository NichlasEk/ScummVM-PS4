// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/kro.h"
#include "common/endian.h"
#include "common/compression/deflate.h"
namespace Pice {
bool parseKroIndex(Common::SeekableReadStream &s, Common::Array<KroEntry> &out) {
 out.clear();
 if (!s.seek(0) || s.size() < 8 || s.readUint32BE() != MKTAG('B','u','r','p')) return false;
 uint32 count = s.readUint32LE();
 if (count == 0 || count > 65536 || uint64(count) * 20 + 8 > uint64(s.size())) return false;
 Common::Array<KroEntry> entries;
 const uint64 tableEnd = 8 + uint64(count) * 20;
 uint64 previousEnd = tableEnd;
 for (uint32 i = 0; i < count; ++i) {
  KroEntry e;
  e.size = s.readUint32LE(); e.packedSize = s.readUint32LE();
  uint32 reserved = s.readUint32LE();
  e.offset = s.readUint32LE(); e.flags = s.readUint32LE();
  if (s.err() || reserved != 0 || (e.flags != 0 && e.flags != 4) ||
      e.size > 64 * 1024 * 1024 || e.packedSize > 64 * 1024 * 1024 ||
      e.offset < previousEnd || uint64(e.offset) + e.packedSize > uint64(s.size()) ||
      (e.flags == 0 && e.size != e.packedSize) ||
      (e.flags == 4 && (e.size == 0 || e.packedSize == 0))) return false;
  previousEnd = uint64(e.offset) + e.packedSize;
  entries.push_back(e);
 }
 out = entries;
 return true;
}
bool KroArchive::open(const Common::Path &path) {
 _file.close(); _entries.clear();
 return _file.open(path) && parseKroIndex(_file, _entries);
}
bool KroArchive::read(uint index, Common::Array<byte> &out) {
 out.clear();
 if (index >= _entries.size()) return false;
 const KroEntry &e = _entries[index];
 if (!_file.seek(e.offset)) return false;
 Common::Array<byte> packed; packed.resize(e.packedSize);
 if (e.packedSize && _file.read(&packed[0], e.packedSize) != e.packedSize) return false;
 if (e.flags == 0) { out = packed; return true; }
 out.resize(e.size);
 uint size = e.size;
 if (!Common::inflateZlibHeaderless(&out[0], &size, &packed[0], e.packedSize) || size != e.size) {
  out.clear(); return false;
 }
 return true;
}
bool decodeImageHeader(const Common::Array<byte> &data, uint16 &w, uint16 &h) {
 w = h = 0;
 if (data.size() < 4) return false;
 const uint16 width = READ_LE_UINT16(&data[0]), height = READ_LE_UINT16(&data[2]);
 if (!width || !height || width > 4096 || height > 4096 || uint64(width) * height + 4 != data.size()) return false;
 w = width; h = height; return true;
}
}
