// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_STARTUP_H
#define SOTC_STARTUP_H
#include "common/array.h"
#include "common/str.h"
namespace Sotc {
class ScriptMachine;
bool loadStartupScene(Common::Array<byte> &pixels, Common::String &status, ScriptMachine &machine);
Common::String stepStartupScripts(ScriptMachine &machine, unsigned tick);
}
#endif
