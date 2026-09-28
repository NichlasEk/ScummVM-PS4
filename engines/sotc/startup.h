// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_STARTUP_H
#define SOTC_STARTUP_H
#include "common/array.h"
#include "common/str.h"
#include "sotc/scriptmachine.h"
#include "sotc/route.h"
#include "sotc/cutscene.h"
namespace Sotc {
class ScriptMachine;

class StartupScene : public ScriptAnimation {
 struct ActorMove {
  bool active=false; int x=0,y=0;
  // Detours are transient; saved destinations rebuild them after loading.
  bool replan=false;
  Common::Array<unsigned> route;
 } _moves[11];
 bool walkActor(unsigned id,unsigned direction);
 bool moveOthers(ScriptMachine &machine);
 Common::Array<int> _lightningRequests,_lightningLines;
 Cutscene _cinematic;
 StartState _state;
 unsigned _zoom=0,_paletteMode=0,_paletteAmount=0;
 unsigned _brightness=255, _fadeTarget=255, _fadeStep=0;
 Common::Array<byte> _background, _scenery, _actorData[20];
 struct Animation { unsigned sequence, substep, mode, gate, base, direction; } _animations[11];
 Common::Array<byte> _textFile;
 Common::String _dialogueText;
 bool _talking=false, _portrait=false, _restoreTalk=false;
 unsigned _portraitHold=0, _portraitSequence=0;
 unsigned _speechText=0;
 unsigned _speaker=0, _talkSequence=0, _talkStep=0, _talkGate=0;
 ScriptMachine *_machine=nullptr;
 Route _route; bool _following=false, _scriptRoute=false;
 unsigned exitCollisionAt(int x,int y,unsigned direction,const ScriptActor &actor) const;
 unsigned collisionAt(int x,int y,unsigned direction,const ScriptMachine &machine,unsigned self=0,bool scripted=false) const;
 unsigned _inputLocks=0;
 unsigned _walkDirection; bool _walking;
 unsigned _slots[11], _clips[11][4], _types[20], _indices[20];
 bool _ready, _visible[11], _inDrawList[11], _collisionBypass[11];
public:
 StartupScene() : _walkDirection(1), _walking(false), _ready(false) {}
 bool snapshot(Snapshot &s,bool reloadResources=true,unsigned version=7);
 bool roomSnapshot(Snapshot &s,ScriptMachine &machine);
 void adopt(const StartupScene &source,ScriptMachine &machine) { *this=source; _machine=&machine; }
 unsigned bank() const { return _state.word(0x2df2); }
 unsigned room() const { return _state.word(0x2df8); }
 bool cinematic(unsigned resource,unsigned sequence,unsigned backdrop,unsigned repeat,Cc4View cues) override;
 bool cinematicActive() const override {return _cinematic.active();}
 void skipCinematic(){_cinematic.skip();}
 bool lightning(const unsigned *rect) override;
 void completeTick() override;
 bool screenZoom(unsigned mode,int x,int y) override;
 bool palette(unsigned mode,unsigned amount) override;
 void applyPalette(byte *colors) const;
 bool textPosition(unsigned actor,unsigned x,unsigned y) override {
  return actor<11 && _state.setWord(0x2e54+actor*52+30,x) && _state.setWord(0x2e54+actor*52+32,y);
 }
 unsigned textX() const {return _state.word(0x2e54+_speaker*52+30);}
 bool portraitDialogue() const {return _portrait;}
 unsigned textY() const {return _state.word(0x2e54+_speaker*52+32);}
 unsigned brightness() const { return _brightness; }
 bool serviceRoom(ScriptMachine &machine,Common::String &status);
 bool background(unsigned index) override;
 int lookedObject(int &x,int &y) const;
 bool takeObject(Common::String &label);
 bool examine(bool talking=false);
 bool inventoryLabel(unsigned id,Common::String &label);
 bool objectLabel(unsigned slot,Common::String &label);
 unsigned exitDirection(int x,int y) const;
 unsigned facing() const {return _animations[0].direction;}
 unsigned objectCount() const {
  unsigned count=0;for(unsigned i=0;i<20;++i) if(_state.byte(0x3300+i*8)) ++count;
  return count;
 }
 bool object(unsigned id,unsigned x,unsigned y) override;
 bool removeObject(unsigned id) override;
 bool obstacle(const unsigned *rectangle) override;
 bool removeObstacle(unsigned x,unsigned y) override;
 bool actorLabel(unsigned actor,unsigned label) override {
  return actor<11 && _state.setByte(0x2e54+actor*52+28,label);
 }
 bool terrain(Cc4View data) override;
 bool exits(Cc4View data) override;
 bool restoreCollision(unsigned actor) override {if(actor>=11)return false;_collisionBypass[actor]=false;return true;}
 bool bypass(unsigned actor) override { if(actor>=11) return false; _collisionBypass[actor]=true; return true; }
 bool speechBank(unsigned bank) override { return _state.setWord(0x2e16,bank); }
 bool fade(bool in,unsigned speed) override { _fadeTarget=in ? 255 : 0; _fadeStep=speed ? speed : 255; return true; }
 bool fading() const override { return _brightness!=_fadeTarget; }
 void cancelFade() { _brightness=_fadeTarget=255; _fadeStep=0; }
 void resumeSpeech() { if(_talking && _machine) _machine->speech(_state.word(0x2e16),_speechText); }
 bool load(Common::Array<byte> &pixels, Common::String &status, ScriptMachine &machine);
 bool render(Common::Array<byte> &pixels, Common::String &status, const ScriptMachine &machine);
 unsigned advance(ScriptMachine &machine);
 bool walk(unsigned direction, ScriptMachine &machine, bool scripted=false);
 bool destination(int x,int y,ScriptMachine &machine,bool scripted=false);
 bool move(unsigned actor,unsigned x,unsigned y,bool &pending) override;
 bool approach(unsigned actor,int x,int y,bool &pending) override;
 bool movementPending(unsigned actor,bool &pending) override;
 bool dialogue(unsigned actor,unsigned text,unsigned sequence,unsigned portrait) override;
 bool dialoguePending(bool &pending) override;
 bool choiceText(unsigned index,Common::String &text);
 const Common::String &dialogueText() const { return _dialogueText; }
 void finishDialogue() { _talking=false; if (_machine) _machine->stopSpeech(); }
 bool scriptWalking() const { return _scriptRoute; }
 void cancelRoute() { _moves[0].active=false; _following=false; _scriptRoute=false; _route.count=0; _route.credit=0; }
 bool movePlayer(ScriptMachine &machine);
 bool face(unsigned actor,unsigned direction) override;
 bool control(unsigned value,bool unlock,ScriptActor &actor) override;
 bool hide(unsigned actor) override;
 bool clip(unsigned actor,bool horizontal,unsigned begin,unsigned end) override {
  if(actor>=11 || begin>end)return false;
  _clips[actor][horizontal ? 0 : 1]=begin;_clips[actor][horizontal ? 2 : 3]=end;return true;
 }
 bool drawEnabled(unsigned actor,unsigned enabled) override;
 bool reset(unsigned actor, ScriptActor &state) override;
 bool typedResource(unsigned actor,unsigned index,unsigned type) override;
 bool resource(unsigned actor, unsigned index) override;
 bool holdSequence(unsigned actor,unsigned index,unsigned step,ScriptActor &state) override;
 bool sequence(unsigned actor, unsigned sequence, ScriptActor &state) override;
};
Common::String stepStartupScripts(ScriptMachine &machine, unsigned tick);
}
#endif
