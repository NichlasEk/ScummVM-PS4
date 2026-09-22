// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/lipsync.h"
#include "pice/voice.h"
#include "pice/formats.h"
namespace Pice {
bool VoiceAnimation::load(const Common::Array<byte> &voice) {
 unit=0; tracks.clear(); uint32 offset,size;
 if(!voicePayload(voice,offset,size) || (offset!=24 && offset!=28)) return false;
 bool mac=offset==28;
 uint32 start=offset+size;
 if(voice.size()-start<8) return false;
 VoiceAnimation parsed;
 parsed.unit=resource32(&voice[start],mac); uint32 base=start+4;
 uint32 first=resource32(&voice[base],mac);
 if(!parsed.unit || parsed.unit>10000 || !first || first%4 || first/4>16 || first>voice.size()-base) return false;
 uint count=first/4,previous=0;
 for(uint i=0;i<count;++i) {
  uint32 relative=resource32(&voice[base+i*4],mac);
  if(relative<first || relative>voice.size()-base || (i && relative<=previous)) return false;
  previous=relative;
  uint32 pos=base+relative;
  uint64 end=i+1<count ? base+uint64(resource32(&voice[base+i*4+4],mac)) : voice.size();
  if(end>voice.size() || pos>end || end-pos<9) return false;
  uint32 bytes=resource32(&voice[pos],mac);
  if(!bytes || bytes%2 || bytes!=end-pos-9) return false;
  VoiceTrack track; track.independent=voice[pos+4]; track.dependency=int16(resource16(&voice[pos+5],mac)); track.multiplier=int16(resource16(&voice[pos+7],mac));
  if(track.independent>1 || track.dependency<-1 || track.dependency>=int(count) || track.multiplier<0) return false;
  for(uint j=0;j<bytes;j+=2) {
   VoiceFrame frame; frame.sequence=voice[pos+9+j]; frame.duration=voice[pos+10+j];
   // Original reads signed bytes. Unsupported negative values are rejected.
   if(frame.sequence>127 || !frame.duration || frame.duration>127) return false;
   track.frames.push_back(frame);
  }
  parsed.tracks.push_back(track);
 }
 *this=parsed; return true;
}
bool VoiceAnimation::sample(uint32 ticks,Common::Array<int16> &sequences) const {
 sequences.clear(); if(!unit || tracks.empty()) return false;
 Common::Array<int16> current;
 for(uint i=0;i<tracks.size();++i) {
  const VoiceTrack &track=tracks[i]; if(track.frames.empty()) return false;
  uint64 elapsed=ticks; uint step=0;
  while(step+1<track.frames.size() && elapsed>=uint64(track.frames[step].duration)*unit) {
   elapsed-=uint64(track.frames[step].duration)*unit; ++step;
  }
  current.push_back(track.frames[step].sequence);
 }
 Common::Array<int16> result;
 for(uint i=0;i<tracks.size();++i) {
  const VoiceTrack &track=tracks[i];
  if(track.dependency==-1) continue;
  if(track.dependency<0 || uint(track.dependency)>=current.size()) return false;
  int32 value=current[i];
  if(!track.independent) value+=int32(current[track.dependency])*track.multiplier;
  if(value<0 || value>32767) return false;
  result.push_back(value);
 }
 sequences=result; return true;
}
bool runLipSyncTests() {
 // Minimal synthetic EDITLS/WAVE envelope with one independent track.
 Common::Array<byte> data; data.resize(61,0);
 memcpy(&data[0],"EDITLS:05/03/95_1.0",20); WRITE_LE_UINT32(&data[20],16);
 memcpy(&data[24],"RIFF",4); WRITE_LE_UINT32(&data[28],4); memcpy(&data[32],"WAVE",4);
 WRITE_LE_UINT32(&data[36],8); WRITE_LE_UINT32(&data[40],4);
 WRITE_LE_UINT32(&data[44],8); data[48]=1; WRITE_LE_UINT16(&data[49],0); WRITE_LE_UINT16(&data[51],6);
 const byte frames[]={2,1,1,3,5,2,0,1}; memcpy(&data[53],frames,8);
 VoiceAnimation animation; Common::Array<int16> sequences;
 if(!animation.load(data) || !animation.sample(0,sequences) || sequences.size()!=1 || sequences[0]!=2 ||
    !animation.sample(7,sequences) || sequences[0]!=2 || !animation.sample(8,sequences) || sequences[0]!=1 ||
    !animation.sample(32,sequences) || sequences[0]!=5 || !animation.sample(0xffffffff,sequences) || sequences[0]!=0) return false;
 for(uint n=0;n<data.size();++n) {
  Common::Array<byte> shortData=data; shortData.resize(n);
  if(animation.load(shortData) || animation.unit || !animation.tracks.empty()) return false;
 }
 Common::Array<byte> bad=data; WRITE_LE_UINT32(&bad[40],0xfffffffc);
 if(animation.load(bad)) return false;
 bad=data; bad[54]=0; if(animation.load(bad)) return false;
 bad=data; bad[49]=1; if(animation.load(bad)) return false;
 if(!animation.load(data)) return false;
 // A dependent track combines its own frame with another track's frame.
 VoiceTrack second=animation.tracks[0]; second.independent=0; second.dependency=0; second.multiplier=6;
 animation.tracks.push_back(second);
 if(!animation.sample(0,sequences) || sequences.size()!=2 || sequences[1]!=14) return false;
 animation.tracks[1].dependency=-1;
 return animation.sample(0,sequences) && sequences.size()==1;
}
}
