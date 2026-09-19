// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PICE_MOVIE_H
#define PICE_MOVIE_H
#include "common/array.h"
namespace Pice {
// The MUX container and two-stage image codec used by the DOS edition.
// Unsupported codec flags fail without changing the previous frame.
bool decodeMovieFrame(const Common::Array<byte> &data,Common::Array<byte> &pixels);
class MovieResource {
public:
 struct Chunk { uint16 type; uint32 offset,size; };
 bool load(const Common::Array<byte> &data);
 bool nextFrame();
 uint width=0,height=0,divisor=0;
 uint frame=0;
 bool failed=false;
 Common::Array<byte> pixels,palette,audio;
private:
 Common::Array<byte> _data;
 Common::Array<Chunk> _chunks;
 uint _next=0;
};
bool runMovieTests();
}
#endif
