// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_STARTUP_H
#define SOTC_STARTUP_H
#include "common/array.h"
#include "common/str.h"
#include "sotc/scriptmachine.h"
#include "sotc/route.h"
namespace Sotc {
class ScriptMachine;

class StartupScene : public ScriptAnimation {
 struct ActorMove { bool active=false; int x=0,y=0; } _moves[11];
 bool moveOthers(ScriptMachine &machine);
 StartState _state;
 Common::Array<byte> _background, _scenery, _actorData[20];
 struct Animation { unsigned sequence, substep, mode, gate, base, direction; } _animations[11];
 Common::Array<byte> _textFile;
 Common::String _dialogueText;
 bool _talking=false, _portrait=false, _restoreTalk=false;
 unsigned _portraitHold=0, _portraitSequence=0;
 unsigned _speaker=0, _talkSequence=0, _talkStep=0, _talkGate=0;
 ScriptMachine *_machine=nullptr;
 Route _route; bool _following=false, _scriptRoute=false;
 unsigned collisionAt(int x,int y,unsigned direction,const ScriptMachine &machine,unsigned self=0) const;
 unsigned _inputLocks=0;
 unsigned _walkDirection; bool _walking;
 unsigned _slots[11], _clips[11][4], _types[20], _indices[20];
 bool _ready, _visible[11], _inDrawList[11], _collisionBypass[11];
public:
 StartupScene() : _walkDirection(1), _walking(false), _ready(false) {}
 bool load(Common::Array<byte> &pixels, Common::String &status, ScriptMachine &machine);
 bool render(Common::Array<byte> &pixels, Common::String &status, const ScriptMachine &machine);
 unsigned advance(ScriptMachine &machine);
 bool walk(unsigned direction, ScriptMachine &machine, bool scripted=false);
 bool destination(int x,int y,ScriptMachine &machine,bool scripted=false);
 bool move(unsigned actor,unsigned x,unsigned y,bool &pending) override;
 bool movementPending(unsigned actor,bool &pending) override;
 bool dialogue(unsigned actor,unsigned text,unsigned sequence,unsigned portrait) override;
 bool dialoguePending(bool &pending) override;
 const Common::String &dialogueText() const { return _dialogueText; }
 void finishDialogue() { _talking=false; if (_machine) _machine->stopSpeech(); }
 bool scriptWalking() const { return _scriptRoute; }
 void cancelRoute() { _following=false; _scriptRoute=false; _route.count=0; }
 bool movePlayer(ScriptMachine &machine);
 bool face(unsigned actor,unsigned direction) override;
 bool control(unsigned value,bool unlock,ScriptActor &actor) override;
 bool hide(unsigned actor) override;
 bool drawEnabled(unsigned actor,unsigned enabled) override;
 bool reset(unsigned actor, ScriptActor &state) override;
 bool resource(unsigned actor, unsigned index) override;
 bool sequence(unsigned actor, unsigned sequence, ScriptActor &state) override;
};
Common::String stepStartupScripts(ScriptMachine &machine, unsigned tick);
}
#endif
