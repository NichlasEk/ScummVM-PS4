// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_SNAPSHOT_H
#define SOTC_SNAPSHOT_H
#include <stdint.h>
namespace Sotc {
// Explicit little-endian fields, bounded on both read and write. No host pointers.
class Snapshot {
 uint8_t *_data; unsigned _size, _pos=0; bool _reading, _ok=true;
public:
 Snapshot(uint8_t *data,unsigned size,bool reading):_data(data),_size(size),_reading(reading) {}
 bool reading() const { return _reading; }
 bool ok() const { return _ok; }
 unsigned size() const { return _pos; }
 void require(bool condition) { if (!condition) _ok=false; }
 void value(unsigned &v,unsigned max=0xffffffffU) {
  if (!_ok || _pos>_size || _size-_pos<4) { _ok=false; return; }
  if (_reading) { v=0; for(unsigned i=0;i<4;++i) v|=unsigned(_data[_pos+i])<<(i*8); }
  else for(unsigned i=0;i<4;++i) _data[_pos+i]=uint8_t(v>>(i*8));
  _pos+=4; require(v<=max);
 }
 void value(int &v) { unsigned n=unsigned(v); value(n); if (_reading) v=n<=0x7fffffffU ? int(n) : -1-int(~n); }
 void value(bool &v) { unsigned n=v; value(n,1); if (_reading) v=n!=0; }
 void identity(unsigned expected) { unsigned n=expected; value(n); require(n==expected); }
 static unsigned hash(const uint8_t *data,unsigned size) {
  unsigned h=2166136261U; for(unsigned i=0;i<size;++i) h=(h^data[i])*16777619U; return h;
 }
};
}
#endif
