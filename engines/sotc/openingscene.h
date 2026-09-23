// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_OPENINGSCENE_H
#define SOTC_OPENINGSCENE_H
#include "sotc/pak.h"
#include "sotc/cc4.h"
#include "sotc/sound.h"
namespace Sotc {
// Animated opening tasks. Unsupported commands/scene changes remain explicit
// boundaries, not no-ops. Uses the same VA2 renderer/advancer as gameplay.
class OpeningScene {
 struct Actor {
  Common::Array<byte> data;
  int x=0,y=0;
  unsigned sequence=0,step=0,count=0,substep=0,gate=255,mode=2;
  int clipLeft=0,clipRight=319;
  bool moving=false; int targetX=0,targetY=0;
  bool visible=false;
 } _actors[11];
 Pak _resources, _rooms;
 struct Task {
  Common::Array<byte> script;
  unsigned pc=0,actor=0,wait=0,waitingActor=11;
  bool enabled=false;
 } _tasks[3];
 Common::Array<byte> _background,_pixels;
 SoundEffects &_sound;
 unsigned _activeTask=2,_randomValue=0; uint32 _randomState=1;
 unsigned _brightness=0,_fade=0,_fadeStep=0,_fadeClock=0,_clock=0;
 bool _done=false,_failed=false,_story=false,_talking=false;
 unsigned _speechBank=0,_textX=0,_textY=0,_textColor=7;
 Common::Array<byte> _textFile;
 Common::String _dialogue;
 bool _portrait=false,_speakerVisible=false; unsigned _speaker=0;
 bool selectSequence(unsigned id,unsigned sequence,unsigned mode=2);
 bool commands(unsigned id);
 bool render();
 bool advanceActors();
 bool fail(const char *reason);
public:
 explicit OpeningScene(SoundEffects &sound) : _sound(sound) {}
 bool start(Cc4View archive,unsigned pc,const Common::Array<byte> &background);
 bool startStory(Cc4View archive,Cc4View text);
 const Common::String &dialogueText() const { return _dialogue; }
 unsigned textX() const { return _textX; }
 unsigned textY() const { return _textY; }
 unsigned textColor() const { return _textColor; }
 void finishDialogue();
 void advance(unsigned elapsed);
 bool done() const { return _done; }
 bool failed() const { return _failed; }
 unsigned brightness() const { return _brightness; }
 const Common::Array<byte> &pixels() const { return _pixels; }
};
}
#endif
