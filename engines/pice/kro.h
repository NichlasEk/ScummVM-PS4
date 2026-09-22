// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PICE_KRO_H
#define PICE_KRO_H
#include "common/array.h"
#include "common/file.h"
#include "common/stream.h"
namespace Pice {
struct KroEntry { uint32 size, packedSize, offset, flags; };
bool parseKroIndex(Common::SeekableReadStream &stream, Common::Array<KroEntry> &entries, bool *mac=nullptr);
bool expandDemoIndex(Common::Array<KroEntry> &entries,bool sound);
bool decodeImageHeader(const Common::Array<byte> &data, uint16 &width, uint16 &height);
class KroArchive {
public:
 bool open(const Common::Path &path);
 bool read(uint index, Common::Array<byte> &out);
 bool useDemoLayout(bool sound) { return !_mac && expandDemoIndex(_entries,sound); }
 bool isMac() const { return _mac; }
 uint count() const { return _entries.size(); }
 const KroEntry &entry(uint index) const { return _entries[index]; }
private:
 bool _mac=false;
 Common::File _file;
 Common::Array<KroEntry> _entries;
};
bool runSelfTests();
}
#endif
