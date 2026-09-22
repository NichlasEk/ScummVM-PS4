// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PICE_DIALOGUE_H
#define PICE_DIALOGUE_H
#include "common/array.h"
#include "common/str.h"
#include "common/ustr.h"
namespace Pice {
// Game strings and snapshots retain original DOS bytes; decode only for display.
Common::U32String displayText(const Common::String &text,bool mac=false);
struct DialogueLine {
 Common::String voiceGroup;
 int16 voice = -1;
 Common::Array<Common::Array<Common::String> > pages;
};
// Text directory and page decoder, not dialogue scheduling or actor animation.
bool decodeDialogue(const Common::Array<byte> &data, const Common::String &key, DialogueLine &line);
Common::String objectDialogueKey(const Common::String &base,int16 context,int16 item,bool genericItem=false);
// Successful empty selection means absent; malformed directories fail.
bool selectObjectDialogueKey(const Common::Array<byte> &data,const Common::String &base,int16 context,int16 item,Common::String &key);
bool genericDialogueKey(const Common::Array<byte> &data,int16 context,uint32 random,Common::String &key);
bool runDialogueTests();
}
#endif
