// SPDX-License-Identifier: GPL-3.0-or-later
#include "sotc/music.h"
#include "audio/softsynth/opl/mame.h"
#ifndef DISABLE_DOSBOX_OPL
#include "audio/softsynth/opl/dosbox.h"
#endif
#include "common/file.h"
#include "common/debug.h"
namespace Sotc {
#ifndef DISABLE_DOSBOX_OPL
class MusicOPL : public OPL::DOSBox::OPL {
public:
 MusicOPL() : ::OPL::DOSBox::OPL(::OPL::Config::kOpl2) {}
#else
class MusicOPL : public OPL::MAME::OPL {
public:
#endif
 void samples(int16 *buffer, int count) { generateSamples(buffer,count); }
};
Music::Music(Audio::Mixer *mixer) : _mixer(mixer), _opl(nullptr), _player(*this), _opened(false), _pendingStart(false),
 _pendingStop(false), _playing(false), _ticks(0), _remaining(0), _clock(0) {}
Music::~Music() {
 // Detach the stream before destroying its state, without holding _mutex.
 _mixer->stopHandle(_handle);
 delete _opl;
}
void Music::reg(unsigned address, unsigned value) { if (_opl) _opl->writeReg(address,value); }
int Music::readBuffer(int16 *buffer, const int samples) {
 Common::StackLock lock(_mutex);
 int done=0;
 while (done<samples) {
  if (!_remaining) {
   onTimer();
   _clock += getRate(); _remaining=_clock/60; _clock%=60;
  }
  unsigned count=MIN<unsigned>(_remaining,samples-done);
  _opl->samples(buffer+done,count);
  done+=count; _remaining-=count;
 }
 return samples;
}
void Music::onTimer() {
 // OPL register writes occur exclusively on this callback thread.
 if (_pendingStop) {
  for (unsigned i=0;i<9;++i) reg(0xb0+i,0);
  reg(0xbd,0); _pendingStop=false;
 }
 if (_pendingStart) { _player.start(); _pendingStart=false; }
 if (!_playing) return;
 bool ok=_player.tick(); ++_ticks;
 _playing=ok && _player.playing();
 if (_ticks==1 || !_playing) debug("[sotc-music] ticks=%u playing=%d valid=%d",_ticks,_playing,ok);
}
bool Music::request(unsigned index) {
 bool startStream=false;
 {
  Common::StackLock lock(_mutex);
  if (index==255) { _pendingStop=true; _pendingStart=false; _playing=false; return true; }
  if (_playing) return true; // Opcode 77 ignores requests while music is active.
  if (!_opened) { _opened=_archive.open("MUS.PAK"); if (!_opened) return false; }
  if (_driver.empty()) {
   Common::File file;
   if (!file.open(Common::Path("ADLIBM.COM")) || file.size()<1 || file.size()>65536) return false;
   _driver.resize(file.size());
   if (file.read(_driver.data(),_driver.size())!=_driver.size()) { _driver.clear(); return false; }
  }
  if (!_archive.load(index,_song) || !_player.load(_song.data(),_song.size(),_driver.data(),_driver.size())) return false;
  if (!_opl) {
   if (!_mixer->isReady()) return false;
   _opl=new MusicOPL();
   if (!_opl->init()) { delete _opl; _opl=nullptr; return false; }
   startStream=true;
  }
  _pendingStart=true; _playing=true; _ticks=0;
  debug("[sotc-music] track=%u name=%s bytes=%u",index,_archive.entries[index].name.c_str(),_song.size());
 }
 // Never acquire the mixer lock while holding the stream mutex.
 if (startStream) _mixer->playStream(Audio::Mixer::kMusicSoundType,&_handle,this,-1,Audio::Mixer::kMaxChannelVolume,0,DisposeAfterUse::NO);
 return true;
}
}
