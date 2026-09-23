// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_STARTUP_H
#define SOTC_STARTUP_H
#include "common/array.h"
#include "common/str.h"
#include "sotc/scriptmachine.h"
namespace Sotc {
class ScriptMachine;

class StartupScene : public ScriptAnimation {
 StartState _state;
 Common::Array<byte> _background, _scenery, _actorData[20];
 struct Animation { unsigned sequence, substep, mode, gate; } _animations[11];
 bool _ready, _visible[11];
public:
 StartupScene() : _ready(false) {}
 bool load(Common::Array<byte> &pixels, Common::String &status, ScriptMachine &machine);
 bool render(Common::Array<byte> &pixels, Common::String &status, const ScriptMachine &machine);
 unsigned advance(ScriptMachine &machine);
 bool hide(unsigned actor) override;
 bool sequence(unsigned actor, unsigned sequence, ScriptActor &state) override;
};
Common::String stepStartupScripts(ScriptMachine &machine, unsigned tick);
}
#endif
