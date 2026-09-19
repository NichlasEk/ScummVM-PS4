// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PICE_DIALOGUE_H
#define PICE_DIALOGUE_H
#include "common/array.h"
#include "common/str.h"
namespace Pice {
struct DialogueLine {
 Common::String voiceGroup;
 int16 voice = -1;
 Common::Array<Common::Array<Common::String> > pages;
};
// Text directory and page decoder, not dialogue scheduling or actor animation.
bool decodeDialogue(const Common::Array<byte> &data, const Common::String &key, DialogueLine &line);
bool runDialogueTests();
}
#endif
