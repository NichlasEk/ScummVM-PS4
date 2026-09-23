// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_EXPLODE_H
#define SOTC_EXPLODE_H
#include <stdint.h>
namespace Sotc {
// ZIP method 6 (implode), not the incompatible PKWARE DCL stream format.
class ImplodeBits {
 const uint8_t *_data; uint32_t _size, _pos; unsigned _bits, _buffer;
public:
 bool ok;
 ImplodeBits(const uint8_t *data, uint32_t size) : _data(data), _size(size), _pos(0), _bits(0), _buffer(0), ok(true) {}
 unsigned get(unsigned n) {
  while (_bits < n) {
   if (_pos == _size) { ok = false; return 0; }
   _buffer |= unsigned(_data[_pos++]) << _bits; _bits += 8;
  }
  unsigned value = _buffer & ((1U << n) - 1); _buffer >>= n; _bits -= n; return value;
 }
};
class ImplodeTree {
 unsigned _count[17], _first[17], _start[17], _symbols[256];
public:
 bool read(ImplodeBits &bits, unsigned size) {
  unsigned lengths[256], used = 0;
  for (unsigned i = 0; i <= 16; ++i) _count[i] = 0;
  unsigned pairs = bits.get(8) + 1;
  for (unsigned i = 0; i < pairs; ++i) {
   unsigned pair = bits.get(8), n = (pair >> 4) + 1, len = (pair & 15) + 1;
   if (!bits.ok || used + n > size) return false;
   while (n--) { lengths[used++] = len; ++_count[len]; }
  }
  if (used != size) return false;
  unsigned code = 0, start = 0;
  for (unsigned len = 1; len <= 16; ++len) {
   code = (code + _count[len - 1]) << 1;
   if (code + _count[len] > (1U << len)) return false;
   _first[len] = code; _start[len] = start;
   for (unsigned symbol = 0; symbol < size; ++symbol)
    if (lengths[symbol] == len) _symbols[start++] = symbol;
  }
  return code + _count[16] == 65536;
 }
 unsigned decode(ImplodeBits &bits) const {
  unsigned code = 0;
  for (unsigned len = 1; len <= 16; ++len) {
   code = (code << 1) | (bits.get(1) ^ 1);
   if (!bits.ok) return 0;
   if (code >= _first[len] && code - _first[len] < _count[len])
    return _symbols[_start[len] + code - _first[len]];
  }
  bits.ok = false; return 0;
 }
};
inline bool explode(const uint8_t *src, uint32_t packed, uint8_t *dst, uint32_t size, unsigned flags) {
 if (flags & ~6U) return false;
 ImplodeBits bits(src, packed); ImplodeTree literal, length, distance;
 if ((flags & 4) && !literal.read(bits, 256)) return false;
 if (!length.read(bits, 64) || !distance.read(bits, 64)) return false;
 unsigned lowBits = (flags & 2) ? 7 : 6;
 uint32_t pos = 0;
 while (pos < size && bits.ok) {
  if (bits.get(1)) dst[pos++] = (flags & 4) ? literal.decode(bits) : bits.get(8);
  else {
   unsigned low = bits.get(lowBits);
   unsigned back = low + (distance.decode(bits) << lowBits) + 1;
   unsigned symbol = length.decode(bits);
   unsigned n = symbol + ((flags & 4) ? 3 : 2);
   if (symbol == 63) n += bits.get(8);
   if (!bits.ok || n > size - pos) return false;
   while (n--) { dst[pos] = back > pos ? 0 : dst[pos - back]; ++pos; }
  }
 }
 return bits.ok && pos == size;
}
}
#endif
