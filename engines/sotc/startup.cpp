// SPDX-License-Identifier: GPL-3.0-or-later
#include "sotc/startup.h"
#include "sotc/startstate.h"
#include "sotc/animation.h"
#include "sotc/draworder.h"
#include "sotc/walk.h"
#include "sotc/collision.h"
#include "sotc/sequence.h"
#include "sotc/poserenderer.h"
#include "sotc/scriptmachine.h"
#include "sotc/pak.h"
#include "common/file.h"
#include "common/debug.h"
#include "common/textconsole.h"
namespace Sotc {
static bool readFile(const char *name, Common::Array<byte> &data) {
 Common::File file;
 if (!file.open(Common::Path(name)) || file.size() < 1 || file.size() > 4 * 1024 * 1024) return false;
 data.resize(file.size()); return file.read(data.data(), data.size()) == data.size();
}
Common::String stepStartupScripts(ScriptMachine &machine, unsigned tick) {
 machine.tick(); unsigned waiting = 0, stopped = 0;
 for (unsigned i = 0; i < machine.count(); ++i) {
  const ScriptTask &task = *machine.task(i);
  if (task.result >= kTimerWait && task.result <= kRegionWait) ++waiting;
  else if (task.result >= kUnknownWait) ++stopped;
  debug("[sotc-vm] tick=%u task=%u pc=%u op=%u flags=%02x result=%u executed=%u", tick, i, task.pc, task.opcode, task.flags, unsigned(task.result), task.executed);
 }
 return Common::String::format("Script tick %u: %u waiting, %u stopped", tick, waiting, stopped);
}
bool StartupScene::load(Common::Array<byte> &pixels, Common::String &status, ScriptMachine &machine) {
 pixels.clear(); status = "INIT.SAV unavailable";
 Common::Array<byte> save, scripts;
 _ready = false;
 StartState &state = _state;
 if (!readFile("INIT.SAV", save) || !state.load(Cc4View(save.data(), save.size()))) return false;
 unsigned bank = state.word(0x2df2), blockIndex = state.word(0x2df8), background = state.word(0x2e14);
 Common::String roomName = Common::String::format("R%02u.CC4", bank);
 Cc4View block;
 if (!readFile(roomName.c_str(), scripts) || !cc4Entry(Cc4View(scripts.data(), scripts.size()), 4, blockIndex, block) ||
     cc4Count(block, 2) != state.word(0x2e02)) return false;
 if (!machine.load(state, block)) return false;
 // Verify every rebased PC before rendering any of this snapshot.
 for (unsigned i = 0; i < state.word(0x2e02); ++i) {
  uint32_t pc;
  if (!state.taskPC(block, i, pc)) return false;
  debug("[sotc-init-task] task=%u pc=%u actor=%u flags=%02x", i, pc, state.byte(0x3118 + i * 22), state.byte(0x3119 + i * 22));
 }
 Pak rooms, actors, resources;
 Common::Array<byte> &scenery = _scenery;
 Common::Array<byte> *actorData = _actorData;
 for (unsigned i=0;i<20;++i) actorData[i].clear();
 if (!rooms.open(Common::String::format("D%02u.PAK", bank)) || !actors.open(Common::String::format("A%02u.PAK", bank)) ||
     !resources.open("RES.PAK") || !rooms.load(background, pixels) || pixels.size() != 64000 || !rooms.load(background + 1, scenery)) {
  pixels.clear(); return false;
 }
 for (unsigned i = 0; i < 20; ++i) {
  unsigned slot = 0x3286 + i * 6, type = state.byte(slot);
  _types[i]=type; _indices[i]=state.byte(slot+1);
  // Resource ID/type metadata is portable; the serialized far pointer is not.
  bool occupied = state.word(slot + 2) || state.word(slot + 4);
  if (!occupied) continue;
  if (type == 0) actors.load(state.byte(slot + 1), actorData[i]);
  else if (type == 1) resources.load(2, actorData[i]); // 0x6cb5 -> 0x609e, initialized at 0xc96.
  else if (type == 2) actorData[i] = scenery;
 }
 _background = pixels;
 for (unsigned i=0;i<11;++i) {
  unsigned a=0x2e54+i*52;
  _collisionBypass[i]=state.byte(a+23)==8;
  _inDrawList[i]=state.byte(a+50)!=0;
  _slots[i]=state.byte(a+11);
  _clips[i][0]=state.word(a+44); _clips[i][1]=state.byte(a+48);
  _clips[i][2]=state.word(a+46); _clips[i][3]=state.byte(a+49);
  _visible[i] = state.word(a+26)!=0;
  _animations[i] = {state.word(a+12), state.word(a+16), state.word(a+6), state.byte(a+20)};
 }
 _walkDirection=state.word(0x2e5c); _walking=false;
 _ready=true;
 return render(pixels,status,machine);
}
bool StartupScene::render(Common::Array<byte> &pixels, Common::String &status, const ScriptMachine &machine) {
 if (!_ready) return false;
 const StartState &state=_state;
 const Common::Array<byte> &scenery=_scenery;
 const Common::Array<byte> *actorData=_actorData;
 pixels=_background;
 PoseRenderer renderer(pixels);
 unsigned rendered = 0, unsupported = 0, sceneryLayer = 0;
 DrawOrder order;
 // Current room scenery depths come from its restored list. Original 0x7228
 // seeds those layers; 0x881d inserts eligible actors in ascending actor ID.
 for (unsigned i=0;i<state.byte(0x34de);++i) {
  unsigned id=state.byte(0x34e1+i*2);
  if ((id & 0xf0)==0x10) order.append(state.byte(0x34e0+i*2),id);
 }
 for (unsigned id=0;id<11;++id)
  if (_visible[id] && _inDrawList[id]) order.insert(machine.actor(id)->y,id);
 const uint8_t *sceneryLayers; unsigned sceneryCount;
 bool haveSceneryPose = va2Record(scenery.data(), scenery.size(), 0, 0, sceneryLayers, sceneryCount);
 for (unsigned i = 0; i < order.count; ++i) {
  unsigned id = order.entries[i].id;
  if ((id & 0xf0) == 0x10) {
   // Scene pose 0 is interleaved with actors by the serialized draw list.
   bool ok = false;
   if (haveSceneryPose && sceneryLayer < sceneryCount) {
    const uint8_t *layer = sceneryLayers + sceneryLayer * 8;
    if (layer[0] == 0 && layer[1] == 1) // Original 0xc806: nested pose.
     ok = renderer.pose(scenery, va2LE16(layer + 2), va2Signed16(layer + 4), va2Signed16(layer + 6));
    else if (layer[0] == 1 && layer[1] == 1)
     ok = renderer.pose(scenery, 0, 0, 0, 0, 0, 319, 199, sceneryLayer);
   }
   ++sceneryLayer;
   if (ok) ++rendered; else ++unsupported;
   continue;
  }
  if (id >= 11) { ++unsupported; continue; }
  if (!_visible[id]) continue;
  unsigned slot = _slots[id];
  if (slot >= 20 || actorData[slot].empty()) { ++unsupported; continue; }
  const Common::Array<byte> &data = actorData[slot];
  const ScriptActor &actor = *machine.actor(id);
  unsigned sequence = _animations[id].sequence, stepIndex = actor.step;
  SequenceStep step;
  int x = actor.x, y = actor.y, mx = 0, my = 0;
  int nextPose = -1; unsigned fraction = 0;
  bool ok = va2Step(data.data(), data.size(), sequence, stepIndex, step);
  if (ok && _animations[id].mode == 2) {
   unsigned mode = step.control >> 14;
   ok = mode <= 1 && va2Position(data.data(), data.size(), sequence, stepIndex, mx, my);
   if (ok && mode == 1) {
    SequenceStep next = step;
    if (stepIndex + 1 < actor.steps) ok = va2Step(data.data(), data.size(), sequence, stepIndex + 1, next);
    unsigned divisor = step.control & 0x3fff;
    fraction = ((_animations[id].substep << 8) & 65535) / (divisor ? divisor : 1);
    nextPose = next.pose;
   }
   x += mx; y += my;
  }
  ok = ok && renderer.pose(data, step.pose, x, y, _clips[id][0], _clips[id][1], _clips[id][2], _clips[id][3], -1, nextPose, fraction);

  if (ok) ++rendered; else ++unsupported;
 }
 status = Common::String::format("INIT scene: %u drawn, %u unsupported", rendered, unsupported);
 return unsupported == 0;
}
bool StartupScene::walk(unsigned direction, ScriptMachine &machine) {
 if (!_ready || direction>4 || _animations[0].mode==2) return false;
 bool moving=direction!=0;
 if (moving==_walking && (!moving || direction==_walkDirection)) return true;
 unsigned facing=moving ? direction : _walkDirection;
 unsigned sequenceIndex=(moving ? 4 : 0)+facing-1;
 unsigned slot=_slots[0];
 if (slot>=20) return false;
 const Common::Array<byte> &data=_actorData[slot];
 unsigned count=va2SequenceLength(data.data(),data.size(),sequenceIndex);
 SequenceStep step;
 if (!count || !va2Step(data.data(),data.size(),sequenceIndex,0,step)) return false;
 ScriptActor actor=*machine.actor(0); actor.step=0; actor.steps=count;
 _animations[0].sequence=sequenceIndex; _animations[0].mode=0; _animations[0].gate=255;
 _walkDirection=facing; _walking=moving;
 machine.setActor(0,actor);
 debug("[sotc-walk] direction=%u moving=%u sequence=%u",facing,unsigned(moving),sequenceIndex);
 return true;
}
unsigned StartupScene::collisionAt(int x,int y,unsigned direction,const ScriptMachine &machine) const {
 const ScriptActor &actor=*machine.actor(0);
 unsigned char terrain[320],obstacles[255*4];
 for (unsigned i=0;i<320;++i) terrain[i]=_state.byte(0x339e + i);
 unsigned count=_state.byte(0x3542);
 if (!_state.range(0x3544,count*4)) return 0xffff;
 for (unsigned i=0;i<count*4;++i) obstacles[i]=_state.byte(0x3544+i);
 Footprint candidate=footprint(x,y,actor.halfWidth,actor.footHeight);
 unsigned collision=terrainCollision(terrain,candidate,direction);
 if (!collision) collision=obstacleCollision(obstacles,count,candidate);
 if (!collision) {
  CollisionActor actors[11];
  for (unsigned i=0;i<11;++i) {
   const ScriptActor &a=*machine.actor(i);
   actors[i]={footprint(a.x,a.y,a.halfWidth,a.footHeight),_visible[i],_collisionBypass[i]};
  }
  collision=actorCollision(actors,0,candidate);
 }
 return collision;
}
bool StartupScene::destination(int x,int y,ScriptMachine &machine) {
 cancelRoute();
 if (!_ready || _animations[0].mode==2) return false;
 walk(0,machine);
 const ScriptActor &a=*machine.actor(0);
 bool found=_route.find(a.x,a.y,x,y,[&](int nx,int ny,unsigned d) { return collisionAt(nx,ny,d,machine)==0; });
 _following=found && _route.count;
 debug("[sotc-route] target=%d,%d found=%u steps=%u",x,y,unsigned(found),_route.count);
 return found;
}
bool StartupScene::movePlayer(ScriptMachine &machine) {
 if (_following) {
  if (!_route.count || _animations[0].mode==2) { cancelRoute(); return walk(0,machine); }
  const ScriptActor &a=*machine.actor(0); unsigned p=_route.points[_route.count-1];
  unsigned d=int(p%320)>a.x ? 2 : int(p%320)<a.x ? 4 : int(p/320)>a.y ? 3 : 1;
  if (!walk(d,machine)) { cancelRoute(); return false; }
 }
 if (!_walking) return true;
 if (_animations[0].mode==2) { _walking=false; return true; }
 unsigned slot=_slots[0]; if (slot>=20) return false;
 const Common::Array<byte> &data=_actorData[slot];
 ScriptActor actor=*machine.actor(0); SequenceStep step;
 if (!va2Step(data.data(),data.size(),_animations[0].sequence,actor.step,step)) return false;
 int dx=step.dx,dy=step.dy;
 int speed=_state.byte(0x2e2a); if (speed>=128) speed-=256;
 walkDelta(_walkDirection,speed,_state.signedWord(0x2e24),_state.signedWord(0x2e26),
           _state.word(0x2df8)==_state.word(0x2dfa),dx,dy);
 if (_following) {
  unsigned budget=unsigned(dx<0 ? -dx : dx)+unsigned(dy<0 ? -dy : dy);
  while (budget-- && _route.count) {
   unsigned p=_route.points[_route.count-1]; int x=p%320,y=p/320;
   unsigned d=x>actor.x ? 2 : x<actor.x ? 4 : y>actor.y ? 3 : 1;
   if (d!=_walkDirection) break;
   if (collisionAt(x,y,d,machine)) { cancelRoute(); return walk(0,machine); }
   actor.x=x; actor.y=y; machine.setActor(0,actor); --_route.count;
  }
  if (!_route.count) { debug("[sotc-route] arrived=%d,%d",actor.x,actor.y); cancelRoute(); return walk(0,machine); }
  return true;
 }
 int x=walkWord(actor.x+dx),y=walkWord(actor.y+dy);
 unsigned collision=collisionAt(x,y,_walkDirection,machine);
 // Boundary exits, sliding and automatic detours need the destination system.
 // For manual walking reject the candidate and retain the last valid position.
 if (collision || x<0 || x>319 || y<0 || y>199) {
  debug("[sotc-collision] blocked=%03x candidate=%d,%d retained=%d,%d",collision,x,y,actor.x,actor.y);
  return walk(0,machine);
 }
 actor.x=x; actor.y=y; machine.setActor(0,actor);
 debug("[sotc-walk] x=%d y=%d step=%u",x,y,actor.step);
 return true;
}
bool StartupScene::hide(unsigned id) {
 if (!_ready || id>=11) return false;
 if (id) _visible[id]=false;
 // Resource bytes may remain cached, but hidden actors neither draw nor advance.
 return true;
}
bool StartupScene::reset(unsigned id, ScriptActor &actor) {
 if (!_ready || id>=11) return false;
 if (!id) return true;
 // 0x78c8 clears the actor, selects sequence zero using the transient slot 0,
 // then installs resource slot 255. The following 91/87 bind and start it.
 unsigned count=va2SequenceLength(_actorData[0].data(),_actorData[0].size(),0);
 actor={0,0,4,2,0,count};
 _animations[id]={0,0,1,255}; _slots[id]=255; _collisionBypass[id]=false; _visible[id]=true; _inDrawList[id]=true;
 _clips[id][0]=_clips[id][1]=0; _clips[id][2]=319; _clips[id][3]=199;
 return true;
}
bool StartupScene::resource(unsigned id, unsigned index) {
 if (!_ready || id>=11) return false;
 for (unsigned slot=0;slot<20;++slot) {
  if (!_actorData[slot].empty() && _types[slot]==0 && _indices[slot]==index) {
   _slots[id]=slot; return true;
  }
 }
 // Keep immutable resource bytes cached; evict only slots without live users.
 for (unsigned slot=0;slot<20;++slot) {
  bool used=false;
  for (unsigned a=0;a<11;++a) if (_visible[a] && _slots[a]==slot) used=true;
  if (used || _types[slot]!=0) continue;
  Pak actors; Common::Array<byte> data;
  if (!actors.open(Common::String::format("A%02u.PAK",_state.word(0x2df2))) || !actors.load(index,data)) return false;
  _actorData[slot]=data; _types[slot]=0; _indices[slot]=index; _slots[id]=slot;
  return true;
 }
 return false;
}
bool StartupScene::sequence(unsigned id, unsigned sequenceIndex, ScriptActor &actor) {
 if (!_ready || id>=11) return false;
 unsigned slot=_slots[id];
 if (slot>=20) return false;
 const Common::Array<byte> &data=_actorData[slot];
 unsigned count=va2SequenceLength(data.data(),data.size(),sequenceIndex);
 SequenceStep first;
 if (!count || !va2Step(data.data(),data.size(),sequenceIndex,0,first)) return false;
 _animations[id].sequence=sequenceIndex; _animations[id].mode=2; _animations[id].gate=255;
 // Original 0x7857 preserves substep; it resets step/count and the advance gate.
 actor.step=0; actor.steps=count;
 debug("[sotc-sequence] actor=%u sequence=%u count=%u",id,sequenceIndex,count);
 return true;
}
unsigned StartupScene::advance(ScriptMachine &machine) {
 if (!_ready) return 0;
 unsigned unsupported=0;
 for (unsigned id=0;id<10;++id) {
  if (!_visible[id]) continue; // Original 0x7d3f update eligibility.
  unsigned slot=_slots[id];
  if (slot>=20) { ++unsupported; continue; }
  const Common::Array<byte> &data=_actorData[slot];
  Animation &animation=_animations[id];
  ScriptActor actor=*machine.actor(id);
  SequenceStep step;
  if (!va2Step(data.data(),data.size(),animation.sequence,actor.step,step) ||
      !advanceAnimation(step.control,animation.mode,animation.gate,actor.steps,actor.step,animation.substep)) {
   ++unsupported; continue;
  }
  machine.setActor(id,actor);
 }
 return unsupported;
}

}
