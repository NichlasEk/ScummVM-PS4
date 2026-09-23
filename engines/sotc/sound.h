// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_SOUND_H
#define SOTC_SOUND_H
#include "sotc/scriptmachine.h"
#include "sotc/pak.h"
#include "sotc/music.h"
#include "audio/mixer.h"
namespace Sotc {
class SoundEffects : public ScriptSound {
 Audio::Mixer *_mixer;
 Audio::SoundHandle _handle;
 Pak _samples;
 Music _music;
 bool _opened;
public:
 explicit SoundEffects(Audio::Mixer *mixer) : _mixer(mixer), _music(mixer), _opened(false) {}
 ~SoundEffects() override { _mixer->stopHandle(_handle); }
 bool effect(unsigned index) override;
 bool music(unsigned index) override { return _music.request(index); }
};
}
#endif
