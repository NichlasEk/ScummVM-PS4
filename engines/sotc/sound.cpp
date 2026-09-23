// SPDX-License-Identifier: GPL-3.0-or-later
#include "sotc/sound.h"
#include "sotc/nar.h"
#include "common/file.h"
#include "audio/decoders/voc.h"
#include "audio/decoders/raw.h"
#include "common/memstream.h"
#include "common/debug.h"
namespace Sotc {
bool SoundEffects::speech(unsigned bank,unsigned index) {
 stopSpeech();
 _mixer->stopHandle(_handle); // Speech replaces the original sampled-audio channel.
 if (bank>7 || !_mixer->isReady()) return false;
 Common::File file;
 if (!file.open(Common::Path(Common::String::format("D%02u.NAR",bank))) || file.size()<4 || file.size()>0x10000000) return false;
 unsigned tableSize=0;
 for (unsigned p=0;p<65536 && p+4<=unsigned(file.size());p+=4) {
  tableSize=file.readUint32LE(); if (tableSize) break;
 }
 if (!tableSize || tableSize>65536 || tableSize>unsigned(file.size()) || tableSize%4) return false;
 Common::Array<byte> table,record; table.resize(tableSize); file.seek(0);
 if (file.read(table.data(),table.size())!=table.size()) return false;
 unsigned start,end;
 if (!narEntry(Cc4View(table.data(),table.size()),file.size(),index,start,end)) return false;
 if (!start) { debug("[sotc-speech] bank=%u text=%u no recording",bank,index); return true; }
 if (end-start>16*1024*1024) return false;
 record.resize(end-start); file.seek(start);
 if (file.read(record.data(),record.size())!=record.size()) return false;
 Cc4View view(record.data(),record.size()); NarChunk chunk;
 unsigned position=0,rate=0,samples=0,parts=0;
 while (position<view.size) {
  if (!narChunk(view,position,chunk) || (rate && rate!=chunk.rate)) return false;
  rate=chunk.rate; samples+=chunk.size; position=chunk.next; ++parts;
 }
 if (!samples) return false;
 byte *owned=static_cast<byte *>(malloc(samples));
 if (!owned) return false;
 unsigned written=0; position=0;
 while (position<view.size) {
  narChunk(view,position,chunk);
  memcpy(owned+written,view.data+chunk.offset,chunk.size);
  written+=chunk.size; position=chunk.next;
 }
 Audio::AudioStream *stream=Audio::makeRawStream(owned,samples,rate,Audio::FLAG_UNSIGNED,DisposeAfterUse::YES);
 if (!stream) { free(owned); return false; }
 _mixer->playStream(Audio::Mixer::kSpeechSoundType,&_speechHandle,stream);
 debug("[sotc-speech] bank=%u text=%u parts=%u samples=%u rate=%u active=%u",bank,index,parts,samples,rate,unsigned(_mixer->isSoundHandleActive(_speechHandle)));
 return true;
}
bool SoundEffects::effect(unsigned index) {
 if (index == 255) {
  _mixer->stopHandle(_handle);
  debug("[sotc-sfx] stop"); return true;
 }
 if (_mixer->isSoundHandleActive(_speechHandle)) return true; // Original DS:4626 gates samples during speech.
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
