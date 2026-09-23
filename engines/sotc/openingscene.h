// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_OPENINGSCENE_H
#define SOTC_OPENINGSCENE_H
#include "sotc/pak.h"
#include "sotc/cc4.h"
#include "sotc/sound.h"
namespace Sotc {
// Linear animated opening task. Parallel tasks/scene changes remain explicit
// boundaries, not no-ops. Uses the same VA2 renderer/advancer as gameplay.
class OpeningScene {
 struct Actor {
  Common::Array<byte> data;
  int x=0,y=0;
  unsigned sequence=0,step=0,count=0,substep=0;
  bool visible=false;
 } _actors[11];
 Pak _resources, _rooms;
 Common::Array<byte> _script,_background,_pixels;
 SoundEffects &_sound;
 unsigned _pc=0,_actor=0,_wait=0,_waitingActor=11,_brightness=0,_fade=0,_fadeStep=0,_fadeClock=0,_clock=0;
 bool _done=false,_failed=false;
 bool commands();
 bool render();
 bool advanceActors();
 bool fail(const char *reason);
public:
 explicit OpeningScene(SoundEffects &sound) : _sound(sound) {}
 bool start(Cc4View archive,unsigned pc,const Common::Array<byte> &background);
 void advance(unsigned elapsed);
 bool done() const { return _done; }
 bool failed() const { return _failed; }
 unsigned brightness() const { return _brightness; }
 const Common::Array<byte> &pixels() const { return _pixels; }
};
}
#endif
