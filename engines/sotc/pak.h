// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_PAK_H
#define SOTC_PAK_H
#include "common/array.h"
#include "common/str.h"
namespace Sotc {
struct Entry { Common::String name; uint32 payload, packed, size; byte method, flags; };
class Pak {
 Common::Array<byte> _data;
public:
 Common::Array<Entry> entries;
 bool open(const Common::String &name);
 bool load(uint index, Common::Array<byte> &out) const;
};
}
#endif
