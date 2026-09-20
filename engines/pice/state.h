// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PICE_STATE_H
#define PICE_STATE_H
#include "common/array.h"
#include "common/str.h"
#include "common/rect.h"
namespace Pice {
// Small bounded, endian-independent snapshot stream. All scalar values use
// 32 bits; narrowing and non-canonical booleans are rejected on input.
class StateIO {
public:
 explicit StateIO(Common::Array<byte> &bytes,bool reading) : data(bytes),loading(reading) { if(!loading) data.clear(); }
 template<class T> void number(T &value) {
  if(!ok) return;
  uint32 raw=uint32(value);
  if(loading) {
   if(data.size()-pos<4) { ok=false; return; }
   raw=uint32(data[pos]) | uint32(data[pos+1])<<8 | uint32(data[pos+2])<<16 | uint32(data[pos+3])<<24;
   value=T(raw); if(uint32(value)!=raw) ok=false; pos+=4;
  } else {
   if(data.size()>16*1024*1024-4) { ok=false; return; }
   for(uint i=0;i<4;++i) data.push_back(byte(raw>>(8*i)));
  }
 }
 void string(Common::String &value) {
  uint n=value.size(); number(n); if(!ok || n>4096 || (loading && n>data.size()-pos)) { ok=false; return; }
  if(loading) { value.clear(); for(uint i=0;i<n;++i) { byte c=data[pos++]; if(!c) ok=false; value+=char(c); } }
  else for(uint i=0;i<n;++i) data.push_back(value[i]);
 }
 void bytes(Common::Array<byte> &value,uint maximum) {
  uint n=value.size(); number(n); if(!ok || n>maximum || (loading && n>data.size()-pos)) { ok=false; return; }
  if(loading) { value.resize(n); for(uint i=0;i<n;++i) value[i]=data[pos++]; }
  else for(uint i=0;i<n;++i) data.push_back(value[i]);
 }
 template<class T> bool count(Common::Array<T> &array,uint maximum) {
  uint n=array.size(); number(n); if(!ok || n>maximum) { ok=false; return false; }
  if(loading) array.resize(n); return true;
 }
 void point(Common::Point &p) { number(p.x); number(p.y); }
 bool finished() const { return ok && (!loading || pos==data.size()); }
 Common::Array<byte> &data;
 bool loading,ok=true;
 uint pos=0;
};
bool runStateTests();
}
#endif
