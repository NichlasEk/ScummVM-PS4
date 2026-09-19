// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/voice.h"
#include "common/endian.h"
namespace Pice {
bool voicePayload(const Common::Array<byte> &data,uint32 &offset,uint32 &size) {
 offset=0; size=0;
 uint32 start=0, available=data.size();
 static const char header[]="EDITLS:05/03/95_1.0";
 if (data.size()>=sizeof(header) && !memcmp(&data[0],header,sizeof(header))) {
  if (data.size()<36) return false;
  uint32 span=READ_LE_UINT32(&data[20]);
  if (span<16 || span>data.size()-20) return false;
  start=24; available=span-4;
 }
 if (available<12 || READ_BE_UINT32(&data[start])!=MKTAG('R','I','F','F') ||
     READ_BE_UINT32(&data[start+8])!=MKTAG('W','A','V','E')) return false;
 uint32 riff=READ_LE_UINT32(&data[start+4]);
 if (riff<4 || riff>available-8 || (start && riff!=available-8)) return false;
 offset=start; size=riff+8; return true;
}
bool runVoiceTests() {
 Common::Array<byte> raw; raw.resize(12,0);
 memcpy(&raw[0],"RIFF",4); WRITE_LE_UINT32(&raw[4],4); memcpy(&raw[8],"WAVE",4);
 uint32 offset,size;
 if (!voicePayload(raw,offset,size) || offset || size!=12) return false;
 Common::Array<byte> wrapped; wrapped.resize(40,0);
 memcpy(&wrapped[0],"EDITLS:05/03/95_1.0",20); WRITE_LE_UINT32(&wrapped[20],16);
 memcpy(&wrapped[24],&raw[0],12);
 if (!voicePayload(wrapped,offset,size) || offset!=24 || size!=12) return false;
 WRITE_LE_UINT32(&wrapped[20],0xffffffff); if (voicePayload(wrapped,offset,size) || offset || size) return false;
 WRITE_LE_UINT32(&wrapped[20],17); if (voicePayload(wrapped,offset,size)) return false;
 WRITE_LE_UINT32(&wrapped[20],16); wrapped[17]='1'; if (voicePayload(wrapped,offset,size)) return false;
 WRITE_LE_UINT32(&raw[4],0xffffffff); if (voicePayload(raw,offset,size)) return false;
 raw.resize(3); if (voicePayload(raw,offset,size)) return false;
 return true;
}
}
