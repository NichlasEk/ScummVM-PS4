// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_MUSIC_H
#define SOTC_MUSIC_H
#include "sotc/adlm.h"
#include "sotc/pak.h"
#include "audio/audiostream.h"
#include "audio/mixer.h"
#include "common/mutex.h"
namespace Sotc {
class MusicOPL;
// Own stream so music uses kMusicSoundType (including exact mute/volume),
// rather than Chip::start's kPlainSoundType channel.
class Music : private AdlmOutput, public Audio::AudioStream {
 Audio::Mixer *_mixer;
 Audio::SoundHandle _handle;
 MusicOPL *_opl;
 Common::Mutex _mutex;
 AdlmPlayer _player;
 Pak _archive;
 Common::Array<byte> _song, _driver;
 bool _opened, _pendingStart, _pendingStop, _playing;
 unsigned _ticks, _remaining, _clock;
 void reg(unsigned address, unsigned value) override;
 void onTimer();
public:
 explicit Music(Audio::Mixer *mixer);
 ~Music();
 bool request(unsigned index);
 int readBuffer(int16 *buffer, const int samples) override;
 int getRate() const override { return _mixer->getOutputRate(); }
 bool isStereo() const override { return false; }
 bool endOfData() const override { return false; }
};
}
#endif
