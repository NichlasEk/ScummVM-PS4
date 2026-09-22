// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PICE_MIDI_H
#define PICE_MIDI_H
#include "audio/midiplayer.h"
#include "audio/midiparser.h"
#include "common/array.h"
namespace Pice {
class MusicMidi : public Audio::MidiPlayer {
public:
 void stop() override { Common::StackLock lock(_mutex); _paused=false; Audio::MidiPlayer::stop(); }
 void pause() override {
  Common::StackLock lock(_mutex);
  if(!_parser || !_isPlaying) return;
  _paused=true; _isPlaying=false; Audio::MidiPlayer::setVolume(0);
 }
 void resume() override {
  Common::StackLock lock(_mutex);
  if(!_paused || !_parser) return;
  _paused=false; Audio::MidiPlayer::setVolume(_requestedVolume); _isPlaying=true;
 }
 void setVolume(int volume) override {
  Common::StackLock lock(_mutex); _requestedVolume=volume;
  Audio::MidiPlayer::setVolume(_paused ? 0 : volume);
 }
 bool active() { Common::StackLock lock(_mutex); return _parser && (_isPlaying || _paused); }
 bool play(const Common::Array<byte> &data,uint loops,uint32 tick) {
  Common::StackLock lock(_mutex);
  stop();
  if(!_driver) {
   createDriver(MDT_ADLIB);
   if(_driver->open()!=0) { delete _driver; _driver=nullptr; return false; }
   _driver->sendGMReset(); _driver->setTimerCallback(this,&timerCallback);
  }
  _midiData=(byte *)malloc(data.size()); if(!_midiData) return false;
  memcpy(_midiData,data.data(),data.size());
  _parser=MidiParser::createParser_SMF();
  if(!_parser->loadMusic(_midiData,data.size())) { stop(); return false; }
  _parser->setTrack(0); _parser->setMidiDriver(this); _parser->setTimerRate(_driver->getBaseTempo());
  _parser->property(MidiParser::mpCenterPitchWheelOnUnload,1);
  _remaining=loops; _isLooping=loops==0; _isPlaying=true;
  if(tick && !_parser->jumpToTick(tick)) { stop(); return false; }
  return true;
 }
 uint32 tick() { Common::StackLock lock(_mutex); return _parser ? _parser->getTick() : 0; }
 uint remaining() { Common::StackLock lock(_mutex); return _remaining; }
protected:
 void endOfTrack() override {
  if(_remaining>1) { --_remaining; _parser->jumpToTick(0); }
  else Audio::MidiPlayer::endOfTrack();
 }
private:
 uint _remaining=0;
 bool _paused=false;
 int _requestedVolume=255;
};
}
#endif
