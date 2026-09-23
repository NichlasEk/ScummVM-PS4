// SPDX-License-Identifier: GPL-3.0-or-later
#include "sotc/pak.h"
#include "sotc/explode.h"
#include "common/file.h"
#include "common/endian.h"
namespace Sotc {
bool Pak::open(const Common::String &name) {
 entries.clear(); _data.clear(); Common::File f;
 if (!f.open(Common::Path(name)) || f.size() < 8 || f.size() > 64 * 1024 * 1024) return false;
 _data.resize(f.size()); if (f.read(_data.data(), _data.size()) != _data.size()) return false;
 uint32 first = READ_LE_UINT32(_data.data() + 4);
 if (first < 8 || first > _data.size() || first % 4) return false;
 for (uint32 pos = 4; pos < first; pos += 4) {
  uint32 offset = READ_LE_UINT32(_data.data() + pos);
  uint32 end = pos + 4 < first ? READ_LE_UINT32(_data.data() + pos + 4) : _data.size();
  if (offset < first || end > _data.size() || offset >= end || end - offset < 16) return false;
  uint32 extra = READ_LE_UINT32(_data.data() + offset);
  if (extra && (extra < 4 || extra > end - offset - 12)) return false;
  uint32 header = offset + (extra ? extra : 4);
  Entry e; e.packed = READ_LE_UINT32(_data.data() + header); e.size = READ_LE_UINT32(_data.data() + header + 4);
  e.method = _data[header + 8]; e.flags = _data[header + 9];
  uint32 metadata = READ_LE_UINT16(_data.data() + header + 10);
  e.payload = header + 12 + metadata;
  if (e.payload > end || e.packed > end - e.payload || e.size > 16 * 1024 * 1024) return false;
  if (metadata > 2) for (uint32 i = header + 14; i < e.payload && _data[i]; ++i) e.name += char(_data[i]);
  entries.push_back(e);
 }
 return !entries.empty();
}
bool Pak::load(uint index, Common::Array<byte> &out) const {
 out.clear(); if (index >= entries.size()) return false;
 const Entry &e = entries[index];
 if (e.method == 0) { out.resize(e.packed); if (e.packed) memcpy(out.data(), _data.data() + e.payload, e.packed); return true; }
 if (e.method != 1 || !e.size) return false;
 out.resize(e.size);
 if (explode(_data.data() + e.payload, e.packed, out.data(), e.size, e.flags)) return true;
 out.clear(); return false;
}
}
