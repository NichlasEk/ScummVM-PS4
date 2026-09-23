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
 unsigned _walkDirection; bool _walking;
 unsigned _slots[11], _clips[11][4], _types[20], _indices[20];
 bool _ready, _visible[11], _inDrawList[11];
public:
 StartupScene() : _walkDirection(1), _walking(false), _ready(false) {}
 bool load(Common::Array<byte> &pixels, Common::String &status, ScriptMachine &machine);
 bool render(Common::Array<byte> &pixels, Common::String &status, const ScriptMachine &machine);
 unsigned advance(ScriptMachine &machine);
 bool walk(unsigned direction, ScriptMachine &machine);
 bool movePlayer(ScriptMachine &machine);
 bool hide(unsigned actor) override;
 bool reset(unsigned actor, ScriptActor &state) override;
 bool resource(unsigned actor, unsigned index) override;
 bool sequence(unsigned actor, unsigned sequence, ScriptActor &state) override;
};
Common::String stepStartupScripts(ScriptMachine &machine, unsigned tick);
}
#endif
