// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_SOUND_H
#define SOTC_SOUND_H
#include "sotc/scriptmachine.h"
#include "sotc/pak.h"
#include "audio/mixer.h"
namespace Sotc {
class SoundEffects : public ScriptSound {
 Audio::Mixer *_mixer;
 Audio::SoundHandle _handle;
 Pak _samples;
 bool _opened;
public:
 explicit SoundEffects(Audio::Mixer *mixer) : _mixer(mixer), _opened(false) {}
 ~SoundEffects() override { _mixer->stopHandle(_handle); }
 bool effect(unsigned index) override;
};
}
#endif
