// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PICE_FILM_H
#define PICE_FILM_H
#include "common/array.h"
#include "common/stream.h"
namespace Pice {
// Sega FILM container. Independent of room IDs, menus and console platform.
struct FilmSample { uint32 offset=0,size=0,time=0,duration=0; bool audio=false; };
struct FilmIndex {
 uint width=0,height=0,rate=0,channels=0,bits=0;
 uint32 clock=0,videoCount=0,audioBytes=0,endMs=0;
 Common::Array<FilmSample> samples;
};
bool readFilmIndex(Common::SeekableReadStream &stream,FilmIndex &out);
bool readFilmSample(Common::SeekableReadStream &stream,const FilmSample &sample,Common::Array<byte> &data);
bool validateFilmFrame(const Common::Array<byte> &data,const FilmIndex &film,uint &strips);
bool runFilmTests();
}
#endif
