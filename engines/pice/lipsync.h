// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PICE_LIPSYNC_H
#define PICE_LIPSYNC_H
#include "common/array.h"
namespace Pice {
struct VoiceFrame { byte sequence=0,duration=0; };
struct VoiceTrack { byte independent=0; int16 dependency=-1,multiplier=0; Common::Array<VoiceFrame> frames; };
class VoiceAnimation {
public:
 bool load(const Common::Array<byte> &voice);
 bool sample(uint32 ticks,Common::Array<int16> &sequences) const;
 uint32 unit=0;
 Common::Array<VoiceTrack> tracks;
};
bool runLipSyncTests();
}
#endif
