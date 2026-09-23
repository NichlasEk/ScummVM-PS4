// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_ADLM_H
#define SOTC_ADLM_H
#include <stdint.h>
#include <string.h>
namespace Sotc {
class AdlmOutput {
public:
 virtual ~AdlmOutput() {}
 virtual void reg(unsigned address, unsigned value) = 0;
};
// ADLIBM.COM's music event interpreter, without executing original machine code.
// Instruments remain in ADD files; pitch/operator tables are read from the COM.
class AdlmPlayer {
 struct Channel {
  unsigned start, pc, delay, duration, instrument, pitch, note, attenuation;
  int oldInstrument, oldVolume, oldPitch, oldNote;
  bool active, first;
  Channel() : start(0), pc(0), delay(0), duration(0), instrument(0), pitch(0), note(0), attenuation(0),
   oldInstrument(-1), oldVolume(-1), oldPitch(-100), oldNote(-1), active(false), first(true) {}
 } _channels[11];
 const uint8_t *_data, *_driver;
 unsigned _size, _instruments, _rhythm;
 bool _melodic, _valid;
 AdlmOutput &_out;
 static unsigned word(const uint8_t *p) { return p[0] | (unsigned(p[1]) << 8); }
 static unsigned dword(const uint8_t *p) { return word(p) | (word(p+2) << 16); }
 unsigned driver(unsigned address) const { return _driver[address - 256]; }
 unsigned driverWord(unsigned address) const { return word(_driver + address - 256); }
 void write(unsigned r, unsigned v) { _out.reg(r, v & 255); }
 void invalidate(Channel &c) { c.oldInstrument = c.oldVolume = c.oldNote = -1; c.oldPitch = -100; }
 void operatorData(unsigned op, unsigned p) {
  unsigned channel = driver(0x6cf + op);
  if (channel != 255) write(0xc0 + channel, _data[p+2]);
  write(0x60+op, _data[p+4]); write(0x80+op, _data[p+5]);
  write(0x20+op, _data[p+1]); write(0xe0+op, _data[p+3]);
 }
 void level(unsigned op, unsigned p, unsigned volume) {
  if (op == 255) return;
  unsigned loudness = ((_data[p+1] & 63) * volume * 2 + 127) / 254;
  write(0x40 + op, (_data[p+1] & 192) | (63 - loudness));
 }
 void tone(unsigned channel, unsigned note, unsigned pitch, bool off, bool legato) {
  if (!legato) write(0xb0+channel, 0);
  unsigned base = driverWord(0x44b + (note & 15)*2);
  if (pitch & 128) {
   if (!(note & 15)) { base = 0x6bb; if (note & 0x70) note -= 16; }
  }
  int bend = pitch < 128 ? int(pitch) : int(pitch)-256;
  unsigned f = driverWord(base + bend*2);
  write(0xa0+channel, f & 255);
  write(0xb0+channel, ((note & 0x70) >> 2) + (f >> 8) + ((off || (note & 128)) ? 0 : 32));
 }
 void note(unsigned channel, unsigned value, unsigned pitch, bool off, bool legato) {
  if (_melodic || channel < 6) { tone(channel,value,pitch,off,legato); return; }
  if (channel == 6) tone(channel,value,pitch,true,legato);
  if (channel == 8 && !(value & 128)) {
   tone(8,value,pitch,true,legato);
   unsigned octave = value & 0x70, key = (value & 15)+7;
   if (key >= 12) { key -= 12; if (octave != 0x70) octave += 16; }
   tone(7,octave|key,pitch,true,legato);
  }
  unsigned bit = 16 >> (channel-6);
  _rhythm &= ~bit; write(0xbd,_rhythm);
  if (!off && !(value & 128)) { _rhythm |= bit; write(0xbd,_rhythm); }
 }
 void render(unsigned i) {
  Channel &c = _channels[i];
  if (!c.active) return;
  unsigned mapping = (_melodic ? 0x41d : 0x435) + i*2;
  unsigned op1 = driver(mapping), op2 = driver(mapping+1);
  if (op1 == 255 && op2 == 255) return;
  if (int(c.instrument) != c.oldInstrument) {
   unsigned p = _instruments + c.instrument*13+1;
   operatorData(op1,p); if (op2 != 255) operatorData(op2,p+6);
   c.oldInstrument = c.instrument; c.oldVolume = -1;
  }
  unsigned volume = c.attenuation > 127 ? 0 : 127-c.attenuation;
  if (int(volume) != c.oldVolume) {
   unsigned p = _instruments+c.instrument*13;
   level(op1,p,op2 == 255 ? volume : 127); level(op2,p+6,volume);
   c.oldVolume = volume;
  }
  bool sameNote = int(c.note) == c.oldNote;
  if (!sameNote || int(c.pitch) != c.oldPitch) {
   note(i,c.note & 255,c.pitch,false,sameNote);
   c.note |= 0x8000; c.oldNote = c.note; c.oldPitch = c.pitch;
  }
 }
public:
 explicit AdlmPlayer(AdlmOutput &out) : _data(nullptr), _driver(nullptr), _size(0),
  _instruments(0), _rhythm(0), _melodic(true), _valid(false), _out(out) {}
 bool load(const uint8_t *data, unsigned size, const uint8_t *driverData, unsigned driverSize) {
  _valid = false;
  if (!data || !driverData || size < 62 || memcmp(data,"ADLM",4) || driverSize < 0xe2b ||
      memcmp(driverData+0xe21,"IFGM ADLIB",10)) return false;
  _data=data; _size=size; _driver=driverData;
  for (unsigned key=0;key<12;++key) {
   unsigned address=driverWord(0x44b+key*2);
   if (address<0x463 || address+2>0x6bb) return false;
  }
  for (unsigned p=0x41d;p<0x44b;++p) if (driver(p)>21 && driver(p)!=255) return false;
  _instruments=dword(data+52); _melodic=data[61] != 0;
  _rhythm=data[60] | (_melodic ? 0 : 32);
  unsigned first=size, present=0;
  for (unsigned i=0;i<11;++i) {
   Channel &c=_channels[i]; c=Channel(); c.start=dword(data+8+i*4); c.pc=c.start;
   if (!c.start) continue;
   if (c.start < 62 || c.start >= size) return false;
   if (c.start < first) first=c.start;
   c.active=true; ++present;
  }
  if (!present || _instruments < 62 || _instruments >= first || (first-_instruments)%13) return false;
  // Validate every stream before starting the mixer; finite command runs only.
  for (unsigned i=0;i<11;++i) {
   Channel &c=_channels[i]; if (!c.active) continue;
   unsigned p=c.start, burst=0;
   for (;;) {
    if (p >= size || ++burst > 4096) return false;
    unsigned code=data[p++], op=code&127;
    if (code==128) break;
    if (p>=size) return false;
    unsigned value=data[p++];
    if (op > 5) return false; // Unused callback/no-op opcodes are not guessed.
    if (op==0) { if (!(code&128)) return false; break; }
    if (op==1) { if (p>=size) return false; ++p; }
    if (op==2 && (value&15)>11) return false;
    if (op==3 && value >= (first-_instruments)/13) return false;
    if (op==5) {
     int bend=value<128 ? int(value) : int(value)-256;
     for (unsigned key=0;key<12;++key) {
      int base=(value&128) && !key ? 0x6bb : driverWord(0x44b+key*2);
      int addr=base+bend*2;
      if (addr<0x463 || addr+2>0x6bb) return false;
     }
    }
    if (code&128) burst=0;
   }
  }
  _valid=true; return true;
 }
 void start() {
  if (!_valid) return;
  write(0xbd,_rhythm); write(4,96); write(4,128); write(8,0); write(1,32);
  for (unsigned i=0;i<9;++i) { write(0xa0+i,0); write(0xb0+i,0); }
 }
 bool playing() const { for (unsigned i=0;i<11;++i) if (_channels[i].active) return _valid; return false; }
 void stop() {
  for (unsigned i=0;i<11;++i) _channels[i].active=false;
  for (unsigned i=0;i<9;++i) write(0xb0+i,0);
  _rhythm &= 0xe0; write(0xbd,_rhythm);
 }
 bool tick() {
  if (!_valid) return false;
  for (unsigned i=0;i<11;++i) {
   Channel &c=_channels[i]; if (!c.active) continue;
   if (!c.first) { c.delay=(c.delay-1)&65535; if (c.delay) { render(i); continue; } }
   c.first=false;
   c.delay=c.duration;
   for (unsigned budget=0;budget<4096;++budget) {
    if (c.pc>=_size) { _valid=false; stop(); return false; }
    unsigned code=_data[c.pc++], value=code==128 ? 0 : _data[c.pc++];
    switch(code&127) {
    case 0: c.active=false; note(i,c.note&255,c.pitch,true,false); invalidate(c); break;
    case 1: c.duration=c.delay=value | (unsigned(_data[c.pc++])<<8); break;
    case 2: c.note=value; break;
    case 3: c.instrument=value; break;
    case 4: c.attenuation=value; break;
    case 5: c.pitch=value; break;
    default: _valid=false; stop(); return false;
    }
    if (code&128) break;
   }
   render(i);
  }
  return true;
 }
};
}
#endif
