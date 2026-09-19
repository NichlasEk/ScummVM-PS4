// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PICE_VOICE_H
#define PICE_VOICE_H
#include "common/array.h"
namespace Pice {
// Locate a bounded RIFF/WAVE stream, optionally in the observed EDITLS wrapper.
// Extra EDITLS chunks are not interpreted or passed to the audio decoder.
bool voicePayload(const Common::Array<byte> &data,uint32 &offset,uint32 &size);
bool runVoiceTests();
}
#endif
