// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/state.h"
namespace Pice {
bool runStateTests() {
 Common::Array<byte> bytes; StateIO write(bytes,false);
 int16 negative=-123; uint32 large=0xfedcba98; bool enabled=true; Common::String label="Snapshot";
 write.number(negative); write.number(large); write.number(enabled); write.string(label);
 if(!write.finished()) return false;
 int16 restored=0; uint32 number=0; bool flag=false; Common::String text;
 StateIO read(bytes,true); read.number(restored); read.number(number); read.number(flag); read.string(text);
 if(!read.finished() || restored!=negative || number!=large || flag!=enabled || text!=label) return false;
 for(uint n=0;n<bytes.size();++n) {
  Common::Array<byte> partial=bytes; partial.resize(n); StateIO truncated(partial,true);
  truncated.number(restored); truncated.number(number); truncated.number(flag); truncated.string(text);
  if(truncated.finished()) return false;
 }
 Common::Array<byte> bad; bad.resize(4,0); bad[0]=2;
 StateIO boolean(bad,true); boolean.number(flag); if(boolean.ok) return false;
 bad[0]=0; bad[2]=1; StateIO narrow(bad,true); narrow.number(restored); if(narrow.ok) return false;
 bad[0]=bad[1]=bad[2]=bad[3]=255; StateIO oversized(bad,true); oversized.string(text); if(oversized.ok) return false;
 return true;
}
}
