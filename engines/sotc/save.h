// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_SAVE_H
#define SOTC_SAVE_H
#include "common/savefile.h"
#include "sotc/startup.h"
namespace Sotc {
struct SaveState {
 unsigned ticks=0, page=0, music=255;
 bool running=true, fault=false;
 void sync(Snapshot &s) { s.value(ticks); s.value(page,16384); s.value(music,255); s.value(running); s.value(fault); }
};
bool hasQuickSave(Common::SaveFileManager &files,const Common::String &prefix);
bool writeQuickSave(Common::SaveFileManager &files,const Common::String &prefix,ScriptMachine &machine,StartupScene &scene,SaveState &state);
bool readQuickSave(Common::SaveFileManager &files,const Common::String &prefix,ScriptMachine &machine,StartupScene &scene,SaveState &state,Common::Array<byte> &pixels,Common::String &status);
}
#endif
