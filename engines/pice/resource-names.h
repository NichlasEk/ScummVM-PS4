// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PICE_RESOURCE_NAMES_H
#define PICE_RESOURCE_NAMES_H
#include "common/array.h"
#include "common/str.h"
namespace Pice {
struct ResourceName { Common::String name; uint32 base; };
// Supported English CD language-index variant: length, chained XOR payload,
// trailing zero; payload is count followed by 8-byte names and uint32 bases.
class ResourceNames {
public:
 bool load(const Common::Array<byte> &encoded, uint32 archiveCount);
 bool resolve(const Common::String &name, uint32 local, uint32 &resource) const;
 uint count() const { return _entries.size(); }
 const ResourceName &entry(uint i) const { return _entries[i]; }
private:
 Common::Array<ResourceName> _entries;
 uint32 _archiveCount = 0;
};
bool runResourceNameTests();
}
#endif
