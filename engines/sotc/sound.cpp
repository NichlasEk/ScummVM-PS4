// SPDX-License-Identifier: GPL-3.0-or-later
#include "sotc/sound.h"
#include "audio/decoders/voc.h"
#include "audio/decoders/raw.h"
#include "common/memstream.h"
#include "common/debug.h"
namespace Sotc {
bool SoundEffects::effect(unsigned index) {
 if (index == 255) {
  _mixer->stopHandle(_handle);
  debug("[sotc-sfx] stop"); return true;
 }
 // Original 0x9506 tests driver busy before loading a sample; no queue or restart.
 if (_mixer->isSoundHandleActive(_handle)) {
  debug("[sotc-sfx] busy, ignoring sample=%u", index); return true;
 }
 if (!_mixer->isReady()) return false;
 if (!_opened) { _opened = _samples.open("SMP.PAK"); if (!_opened) return false; }
 Common::Array<byte> data;
 if (!_samples.load(index, data) || data.empty()) return false;
 // The audio thread outlives this call. Give its stream an independent buffer.
 byte *owned = static_cast<byte *>(malloc(data.size()));
 if (!owned) return false;
 memcpy(owned, data.data(), data.size());
 Audio::SeekableAudioStream *stream = Audio::makeVOCStream(
  new Common::MemoryReadStream(owned, data.size(), DisposeAfterUse::YES),
  Audio::FLAG_UNSIGNED, DisposeAfterUse::YES);
 if (!stream) return false;
 const int rate = stream->getRate();
 _mixer->playStream(Audio::Mixer::kSFXSoundType, &_handle, stream);
 debug("[sotc-sfx] sample=%u name=%s rate=%d bytes=%u active=%d", index,
  _samples.entries[index].name.c_str(), rate, data.size(), _mixer->isSoundHandleActive(_handle));
 return true;
}
}
