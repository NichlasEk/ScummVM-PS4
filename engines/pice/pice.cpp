// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/pice.h"
#include "pice/resource-names.h"
#include "pice/script.h"
#include "pice/dialogue.h"
#include "pice/voice.h"
#include "pice/lipsync.h"
#include "pice/movie.h"
#include "pice/state.h"
#include "audio/decoders/raw.h"
#include "engines/util.h"
#include "common/system.h"
#include "common/config-manager.h"
#include "common/file.h"
#include "common/events.h"
#include "common/memstream.h"
#include "common/textconsole.h"
#include "common/endian.h"
#include "graphics/paletteman.h"
#include "graphics/cursorman.h"
#include "graphics/fontman.h"
#include "graphics/font.h"
#include "graphics/surface.h"
#include "audio/decoders/wave.h"
#include "audio/audiostream.h"
namespace Pice {
// Reproducible hash for comparing guest decoded bytes with the host oracle.
static uint32 hashBytes(const Common::Array<byte> &data) {
 uint32 value = 2166136261U;
 for (uint i = 0; i < data.size(); ++i) value = (value ^ data[i]) * 16777619U;
 return value;
}
void PiceEngine::drawLabels() {
 Graphics::Surface *screen = _system->lockScreen();
 const Graphics::Font *font = FontMan.getFontByUsage(Graphics::FontManager::kGUIFont);
 font->drawString(screen, "PICE ENGINE PREVIEW - PARTIAL GAMEPLAY", 8, 6, 624, _textColor);
 Common::String status = _spriteMode ? Common::String::format("Sprite %u | sequence %u | step %u/%u | provisional 20 ms/tick", _animationId, _sequence, _step + 1, _animation.sequences[_sequence].steps.size()) : Common::String::format("KSVGA resource %u | image %u/%u", _pictures[_picture], _picture + 1, _pictures.size());
 if (_scenePreview) status=_roomStatus;
 font->drawString(screen, status, 8, 24, 624, _textColor);
 bool hasInventory=false; for(uint i=0;i<_ownedItems.size();++i) if(_ownedItems[i]) hasInventory=true;
 if(!hasInventory && _scenePreview && _hoverHotspot>=0 && uint(_hoverHotspot)<_hotspots.size()) font->drawString(screen,_hotspots[_hoverHotspot].label,8,365,624,_textColor);
 else if(!hasInventory) font->drawString(screen, _scenePreview ? "F5: save checkpoint | F7: load checkpoint | Esc: exit" : "Click title: images/animation | Left: next | Right: previous", 8, 365, 624, _textColor);

 if(!_scenePreview || !hasInventory) font->drawString(screen, _scenePreview ? "Left: act / walk | Right: examine | F5 save | F7 load" : "[ Previous ]       [ Next ]        [ Repeat line ]        [ Exit ]", 8, 385, 624, _textColor);
 screen->fillRect(Common::Rect(0,338,640,360),0);
 if (!_subtitle.empty()) {
  if(_closeup) font->drawString(screen,_subtitle,9,342,624,_textStyles[2][1],Graphics::kTextAlignCenter);
  font->drawString(screen,_subtitle,8,341,624,_closeup ? _textStyles[2][0] : _textColor,Graphics::kTextAlignCenter);
 }
 _system->unlockScreen();
}
bool PiceEngine::showPicture(uint position) {
 Common::Array<byte> pixels, palette;
 uint16 w, h;
 uint resource = _pictures[position];
 if (!_graphics.read(resource, pixels) || !decodeImageHeader(pixels, w, h) ||
     !_graphics.read(resource + 1, palette) || palette.size() != 768) return false;
 _spriteMode = false;
 _scenePreview = false;
 _picture = position;
 _system->getPaletteManager()->setPalette(&palette[0], 0, 256);
 uint brightest = 0;
 for (uint i = 0; i < 256; ++i) {
  uint light = palette[i*3] + palette[i*3+1] + palette[i*3+2];
  if (light > brightest) { brightest = light; _textColor = i; }
 }
 _system->fillScreen(0);
 _system->copyRectToScreen(&pixels[4], w, (640 - w) / 2, 42, w, h);
 drawLabels(); _system->updateScreen();
 debug("[pice] image=%u width=%u height=%u pixel-fnv=%08x palette-fnv=%08x", resource, w, h, hashBytes(pixels), hashBytes(palette));
 return true;
}
bool PiceEngine::background(const Common::String &name,int16 local,int16 flags) {
 // Explicit English EU subset: original name resolution prefixes S; SB02
 // spans 815..881. Other groups and nonzero flags await implementation.
 if (!name.equalsIgnoreCase("B02") || local<0 || local>=67 || flags) return false;
 Common::Array<byte> pixels; uint16 w,h;
 uint resource=815+uint(local);
 if (!_graphics.read(resource,pixels) || !decodeImageHeader(pixels,w,h) || w!=640 || h>320) return false;
 _scenePixels=pixels; _sceneBackground=resource;
 debug("[pice] script-background name=%s local=%d flags=%d resource=%u fnv=%08x",name.c_str(),local,flags,resource,hashBytes(pixels));
 return true;
}
bool PiceEngine::palette(const Common::String &name,int16 local) {
 if (!name.equalsIgnoreCase("B02") || local<0 || local>=67) return false;
 Common::Array<byte> colors; uint resource=815+uint(local);
 if (!_graphics.read(resource,colors) || colors.size()!=768) return false;
 _scenePalette=colors; _scenePaletteId=resource;
 debug("[pice] script-palette name=%s local=%d resource=%u fnv=%08x",name.c_str(),local,resource,hashBytes(colors));
 return true;
}
bool PiceEngine::scenery(const Common::String &name,int16 local) {
 if (!name.equalsIgnoreCase("B02") || local<0 || local>=67) return false;
 Common::Array<byte> data; SpriteResource candidate;
 if (!_graphics.read(815+uint(local),data) || !candidate.load(data,815+local) || candidate.sequences.size()!=1 ||
     candidate.sequences[0].steps.size()!=1) return false;
 _scenery=candidate;
 debug("[pice] scenery resource=%u frame=%u parts=%u fnv=%08x",815+uint(local),candidate.sequences[0].steps[0].frame,
       candidate.frames[candidate.sequences[0].steps[0].frame].parts.size(),hashBytes(data));
 return true;
}
bool PiceEngine::startWalkLeg(RoomActor &actor) {
 if(actor.pathIndex>=actor.path.size()) { actor.animationMode=0; return true; }
 Common::Point target=actor.path[actor.pathIndex];
 int dx=target.x-actor.feet.x,dy=target.y-actor.feet.y,ax=ABS(dx),ay=ABS(dy);
 if(!ax && !ay) return false;
 int direction=ax*1000<ay*414 ? (dy>0 ? 0 : 4) : ay*1000<ax*414 ? (dx>0 ? 2 : 6) : dx>0 ? (dy>0 ? 1 : 3) : (dy>0 ? 7 : 5);
 if(!actor.sprite.hasSequence(direction) || !actor.sprite.hasSequence(direction+8)) return false;
 bool moves=false;
 const Common::Array<SequenceStep> &steps=actor.sprite.sequences[direction+8].steps;
 for(uint i=0;i<steps.size();++i) if(steps[i].x || steps[i].y) moves=true;
 if(!moves) return false;
 actor.walkTarget=target; actor.animationOrigin=actor.feet; actor.walkProgress=0; actor.walkDistance=MAX(ax,ay);
 actor.walkDirection=direction; actor.sequence=direction+8; actor.step=0; actor.frameTicks=MAX<uint16>(1,steps[0].ticks); actor.animationMode=4;
 return true;
}
bool PiceEngine::startSound(uint id,uint loops,uint32 elapsed) {
 if(id>=_sounds.size() || !_sounds[id].active) return false;
 if(!startSample(_sounds[id],loops,elapsed,false)) return false;
 debug("[pice] start-sound id=%u resource=%u loops=%u",id,_sounds[id].resource,loops); return true;
}
bool PiceEngine::startSample(RoomSound &sound,uint loops,uint32 elapsed,bool music) {
 Common::Array<byte> data; uint32 offset,size;
 if(!_soundArchive.read(sound.resource,data) || !voicePayload(data,offset,size)) return false;
 byte *owned=(byte *)malloc(size); if(!owned) return false;
 memcpy(owned,&data[offset],size);
 Audio::SeekableAudioStream *wave=Audio::makeWAVStream(new Common::MemoryReadStream(owned,size,DisposeAfterUse::YES),DisposeAfterUse::YES);
 if(!wave) return false;
 uint32 duration=wave->getLength().msecs();
 if(!duration) { delete wave; return false; }
 uint passed=elapsed/duration;
 _mixer->stopHandle(sound.handle); sound.loops=loops; sound.elapsedBase=elapsed;
 if(loops && passed>=loops) { delete wave; return true; }
 if(!wave->seek(elapsed%duration)) { delete wave; return false; }
 Audio::AudioStream *stream=new Audio::LoopingAudioStream(wave,loops ? loops-passed : 0,DisposeAfterUse::YES,false);
 _mixer->playStream(music ? Audio::Mixer::kMusicSoundType : Audio::Mixer::kSFXSoundType,&sound.handle,stream,-1,sound.volume);
 return true;
}
bool PiceEngine::volume(int16 handle,int16 value) {
 if(handle<0 || uint(handle)>=_sounds.size() || !_sounds[handle].active || value<0 || value>100) return false;
 _sounds[handle].volume=value*255/100;
 _mixer->setChannelVolume(_sounds[handle].handle,_sounds[handle].volume); return true;
}
int16 PiceEngine::storeActor(const RoomActor &actor) {
 for(uint i=0;i<_actors.size();++i) if(!_actors[i].active) { _actors[i]=actor; return i; }
 if(_actors.size()>=20) return -1;
 _actors.push_back(actor); return _actors.size()-1;
}
bool PiceEngine::runActorLifecycleTests() {
 // Removing a middle slot must preserve all remaining IDs, invalidate its
 // associations and permit reuse without increasing the 20-actor limit.
 for(uint i=0;i<20;++i) { RoomActor actor; actor.feet.x=i; if(storeActor(actor)!=int(i)) return false; }
 if(storeActor(RoomActor())!=-1) return false;
 Hotspot spot; spot.actor=7; _hotspots.push_back(spot);
 AnimationSound binding; binding.actor=7; _animationSounds.push_back(binding);
 binding.actor=8; _animationSounds.push_back(binding);
 WorldItem item; item.actor=7; _worldItems.push_back(item); _player=7;
 Common::Array<int16> args; args.push_back(7); int16 result;
 if(!invoke(83,Common::String(),args,Common::String(),result) || result!=7 || _actors.size()!=20 ||
    _actors[7].active || _actors[8].feet.x!=8 || _hotspots[0].active || _worldItems[0].actor!=-1 ||
    _animationSounds.size()!=1 || _animationSounds[0].actor!=8 || _player!=-1 || actorBusy(7) ||
    invoke(83,Common::String(),args,Common::String(),result) || storeActor(RoomActor())!=7) return false;
 _actors.clear(); _hotspots.clear(); _animationSounds.clear(); _worldItems.clear();
 _actors.push_back(RoomActor()); _items.push_back(RoomActor()); _ownedItems.resize(1,0);
 WorldItem pickup; pickup.actor=0; pickup.item=0; _worldItems.push_back(pickup);
 Hotspot pickupSpot; pickupSpot.actor=0; _hotspots.push_back(pickupSpot);
 args[0]=0;
 if(!invoke(67,Common::String(),args,Common::String(),result) || result!=0 || !_ownedItems[0] ||
    _actors[0].active || _hotspots[0].active || _worldItems[0].actor!=-1 ||
    !invoke(67,Common::String(),args,Common::String(),result)) return false;
 args[0]=1; if(invoke(67,Common::String(),args,Common::String(),result) || !_ownedItems[0]) return false;
 _actors.clear(); _items.clear(); _ownedItems.clear(); _hotspots.clear(); _worldItems.clear(); return true;
}
bool PiceEngine::invoke(uint16 op,const Common::String &name,const Common::Array<int16> &args,const Common::String &key,int16 &result) {
 result=-1;
 if(op==83 || op==147 || op==142 || op==98 || op==50 || op==123 || op==51 || op==96 || op==117 || op==97 || op==56 || op==92) {
  if(args.empty() || args[0]<0 || uint(args[0])>=_actors.size() || !_actors[args[0]].active) return false;
 }
 if(op==83) {
  if(args.size()!=1 || _dialogueActor==args[0]) return false;
  int16 id=args[0];
  for(uint i=_animationSounds.size();i>0;--i) if(_animationSounds[i-1].actor==id) _animationSounds.remove_at(i-1);
  for(uint i=0;i<_hotspots.size();++i) if(_hotspots[i].actor==id) _hotspots[i].active=false;
  for(uint i=0;i<_worldItems.size();++i) if(_worldItems[i].actor==id) _worldItems[i].actor=-1;
  _actors[id]=RoomActor(); _actors[id].active=false;
  if(_player==id) _player=-1;
  if(_cameraActor==id) _cameraActor=-1;
  result=id; debug("[pice] remove-actor id=%d",id); return true;
 }
 if(op==147) {
  if(args.size()!=4 || args[0]<0 || uint(args[0])>=_actors.size() || args[1]<0 || args[2]<0) return false;
  const RoomActor &actor=_actors[args[0]];
  if(!actor.sprite.hasSequence(args[1]) || uint(args[2])>=actor.sprite.sequences[args[1]].steps.size()) return false;
  Common::Array<int16> animation; animation.push_back(args[0]); animation.push_back(args[1]);
  if(!invoke(51,name,animation,key,result)) return false;
  debug("[pice] animation-trigger actor=%d sequence=%d step=%d task=%d",args[0],args[1],args[2],args[3]); return true;
 }
 if(op==105) {
  if(args.size()!=1 || args[0]<0 || uint(args[0])>=_sounds.size() || !_sounds[args[0]].active) return false;
  _mixer->stopHandle(_sounds[args[0]].handle); result=args[0]; return true;
 }
 if(op==90) {
  if(args.size()!=1 || args[0]<0 || uint(args[0])>=_hotspots.size()) return false;
  _hotspots[args[0]].active=false; result=args[0];
  debug("[pice] disabled hotspot=%d",args[0]); return true;
 }
 if(op==82) {
  if(args.size()!=1 || args[0]<0 || uint(args[0])>=_ownedItems.size()) return false;
  _ownedItems[args[0]]=0; if(_selectedItem==args[0]) _selectedItem=-1;
  result=args[0]; debug("[pice] inventory removed item=%d",args[0]); return true;
 }
 if(op==67) {
  // 0x31ea -> 0x87ac: mark owned once, then remove the world actor.
  if(args.size()!=1 || args[0]<0 || uint(args[0])>=_items.size()) return false;
  uint id=args[0];
  for(uint i=0;i<_worldItems.size();++i) if(_worldItems[i].item==int(id) && _worldItems[i].actor>=0) {
   Common::Array<int16> remove; remove.push_back(_worldItems[i].actor); int16 ignored;
   if(!invoke(83,Common::String(),remove,Common::String(),ignored)) return false;
  }
  _ownedItems.resize(_items.size(),0); _ownedItems[id]=1; result=id;
  debug("[pice] inventory pickup item=%u",id); return true;
 }
 if(op==104) {
  if(args.size()!=1 || (args[0]!=-2 && args[0]!=0)) return false;
  if(args[0]==0) { _mixer->stopHandle(_music.handle); _music=RoomSound(); }
  result=args[0]==-2 ? 0 : args[0]; return true;
 }
 if(op==32) {
  // Original PCM music path 0x1594e changes MUS02 to WUS02, base 308.
  // The CD includes rendered music as well as HMP tracks; use its PCM path.
  if(args.size()!=1 || !name.equalsIgnoreCase("MUS02") || args[0]<0 || args[0]>=6) return false;
  Common::Array<byte> data; uint32 offset,size;
  if(!_soundArchive.read(308+args[0],data) || !voicePayload(data,offset,size)) return false;
  _mixer->stopHandle(_music.handle); _music=RoomSound(); _music.active=true; _music.persistent=true; _music.resource=308+args[0];
  result=0; debug("[pice] load-music resource=%u",_music.resource); return true;
 }
 if(op==118 || op==33) {
  if(args.size()!=2 || args[1]<0) return false;
  if(op==33 && args[0]==-2) { result=0; return true; }
  if(args[0]!=0 || !_music.active || (op==33 && args[1]>100)) return false;
  if(op==118) {
   if(!startSample(_music,args[1],0,true)) return false;
   debug("[pice] start-music resource=%u loops=%d",_music.resource,args[1]);
#ifdef PICE_TESTBENCH
   if(ConfMan.getBool("pice_test_save_after_music")) { _testSavePending=true; _testSaveAt=_system->getMillis()+1500; }
#endif
  } else { _music.volume=(uint(args[1])*255+50)/100; _mixer->setChannelVolume(_music.handle,_music.volume); }
  result=op==33 ? args[1] : args[0]; return true;
 }
 if(op==133 || op==29) {
  if(args.size()!=1 || !name.equalsIgnoreCase("SMP02") || args[0]<0 || args[0]>=68) return false;
  uint slot=0; while(slot<_sounds.size() && _sounds[slot].active) ++slot;
  if(slot>=25) return false;
  Common::Array<byte> data; uint32 offset,size;
  if(!_soundArchive.read(45+args[0],data) || !voicePayload(data,offset,size)) return false;
  RoomSound sound; sound.active=true; sound.persistent=op==133; sound.resource=45+args[0]; result=slot;
  if(slot==_sounds.size()) _sounds.push_back(sound); else _sounds[slot]=sound;
  debug("[pice] load-sound id=%d resource=%u",result,sound.resource); return true;
 }
 if(op==101) {
  if(args.size()!=1 || args[0]<0 || uint(args[0])>=_sounds.size() || !_sounds[args[0]].active) return false;
  _mixer->stopHandle(_sounds[args[0]].handle); _sounds[args[0]]=RoomSound();
  for(uint i=_animationSounds.size();i>0;--i) if(_animationSounds[i-1].sound==args[0]) _animationSounds.remove_at(i-1);
  result=args[0]; debug("[pice] release-sound id=%d",args[0]); return true;
 }
 if(op==114 || op==100) {
  if(args.size()!=(op==114 ? 2U : 1U) || args[0]<0 || uint(args[0])>=_sounds.size() || (op==114 && args[1]<0)) return false;
  if(!startSound(args[0],op==100 ? 1 : uint(args[1]))) return false;
  result=args[0]; return true;
 }
 if(op==136) {
  if(args.size()!=3 || args[0]<0 || args[0]>=6 || args[1]<0 || args[1]>255 || args[2]<0 || args[2]>255) return false;
  _textStyles[args[0]][0]=args[1]; _textStyles[args[0]][1]=args[2]; result=0; return true;
 }
 if(op==142) {
  if(args.size()!=1 || args[0]<0 || uint(args[0])>=_actors.size()) return false;
  // Camera actor selection is inert for the current fixed-width backgrounds.
  _cameraActor=args[0]; result=0; return true;
 }
 if(op==98) {
  if(args.size()!=1 || args[0]<0 || uint(args[0])>=_actors.size() || _actors[args[0]].animationMode==4) return false;
  if(_actors[args[0]].animationMode) _actors[args[0]].animationMode=1;
  result=0; return true;
 }
 if(op==50 || op==123) {
  if(args.size()!=(op==50 ? 2U : 3U) || args[0]<0 || uint(args[0])>=_actors.size() || (op==50 && (args[1]<0 || uint(args[1])>=_navigation.nodes.size() ||
     !_navigation.nodes[args[1]].active || _navigation.nodes[args[1]].scale!=100))) return false;
  RoomActor actor=_actors[args[0]];
  if(!actor.routeEnabled) return false;
  Common::Point target=op==50 ? Common::Point(_navigation.nodes[args[1]].x,_navigation.nodes[args[1]].y) : Common::Point(args[1],args[2]);
  Common::Point floorStart;
  bool atNode=false;
  for(uint i=0;i<_navigation.nodes.size();++i) if(_navigation.nodes[i].active && actor.feet==Common::Point(_navigation.nodes[i].x,_navigation.nodes[i].y)) atNode=true;
  if(atNode) floorStart=actor.feet;
  else if(!_navigation.nearest(actor.feet,floorStart)) return false;
  if(!_navigation.route(floorStart,target,actor.path)) return false;
  if(actor.feet!=floorStart) actor.path.insert_at(0,floorStart);
  actor.pathIndex=0; actor.node=op==50 ? args[1] : -1;
  RoomActor validation=actor;
  for(uint i=0;i<actor.path.size();++i) {
   validation.pathIndex=i;
   if(!startWalkLeg(validation)) return false;
   validation.feet=actor.path[i];
  }
  if(!startWalkLeg(actor)) return false;
  _actors[args[0]]=actor;
  debug("[pice] walk actor=%d from=%d,%d node=%d to=%d,%d waypoints=%u",args[0],actor.feet.x,actor.feet.y,actor.node,target.x,target.y,actor.path.size());
  result=args[0]; return true;
 }
 if(op==93) {
  if(args.size()!=1 || (args[0]!=0 && args[0]!=1) || (!args[0] && _inputLocks==65535)) return false;
  if(args[0]) { if(_inputLocks) --_inputLocks; } else ++_inputLocks;
  result=0; return true;
 }
 if(op==51 || op==96 || op==117) {
  uint count=op==51 ? 2 : op==96 ? 4 : 5;
  if(args.size()!=count || args[0]<0 || uint(args[0])>=_actors.size()) return false;
  RoomActor actor=_actors[args[0]];
  if(op==51) {
   if(actor.animationMode==3 || args[1]<0 || !actor.sprite.hasSequence(args[1])) return false;
   actor.sequence=args[1];
  } else {
   if(!name.equalsIgnoreCase("A02") || args[1]<0 || args[1]>=179 || args[2]<0) return false;
   Common::Array<byte> data;
   if(!_graphics.read(args[1],data) || !actor.sprite.load(data,args[1]) || !actor.sprite.hasSequence(args[2])) return false;
   actor.resource=args[1]; actor.sequence=args[2];
   if(op==117) { actor.node=-1; actor.feet=Common::Point(args[3],args[4]); }
   else if(args[3]!=-1) {
    const RoomNavigation &nav=_roomRunner ? _roomRunner->navigation() : _navigation;
    if(args[3]<0 || uint(args[3])>=nav.nodes.size() || !nav.nodes[args[3]].active || nav.nodes[args[3]].scale!=100) return false;
    actor.node=args[3]; actor.feet=Common::Point(nav.nodes[args[3]].x,nav.nodes[args[3]].y);
   }
  }
  actor.step=0; actor.frameTicks=actor.sprite.sequences[actor.sequence].steps[0].ticks; actor.animationMode=1; actor.animationOrigin=actor.feet; actor.animationPaused=false;
  _actors[args[0]]=actor; result=op==51 ? args[0] : actor.sequence;
  debug("[pice] actor-animation actor=%d resource=%d sequence=%d at=%d,%d",args[0],actor.resource,actor.sequence,actor.feet.x,actor.feet.y); return true;
 }
 if(op==60) {
  const RoomNavigation *nav=_buildingNavigation ? _buildingNavigation : (_roomRunner ? &_roomRunner->navigation() : nullptr);
  if(args.size()!=2 || args[0]<0 || uint(args[0])>=_items.size() || !nav || _worldItems.size()>=10 ||
     args[1]<0 || uint(args[1])>=nav->nodes.size() || !nav->nodes[args[1]].active) return false;
  RoomActor actor=_items[args[0]]; actor.sequence+=2;
  const RoomNode &node=nav->nodes[args[1]];
  if(!actor.sprite.hasSequence(actor.sequence) || node.scale!=100) return false;
  actor.node=args[1]; actor.feet=Common::Point(node.x,node.y);
  WorldItem item; item.item=args[0];
  result=_worldItems.size(); actor.animationMode=1; actor.animationOrigin=actor.feet; actor.frameTicks=MAX<uint16>(1,actor.sprite.sequences[actor.sequence].steps[0].ticks); item.actor=storeActor(actor); if(item.actor<0) return false; _worldItems.push_back(item); return true;
 }
 if(op==61 && args.size()==3 && args[0]==-1) { result=-1; return true; }
 if(op==95) {
  // 0x755e modes 3/4 enable/disable the room's green/blue palette pulse.
  // Other palette transitions remain unsupported rather than being skipped.
  if(args.size()!=4 || (args[0]!=3 && args[0]!=4) || args[1]!=0 || args[2]!=0 || args[3]<=0) return false;
  _palettePulse=args[0]==3; _pulsePeriod=args[3]; _pulseRemaining=args[3]; result=args[0]; return true;
 }
 if(op==97) {
  if(args.size()!=2 || args[0]<0 || uint(args[0])>=_actors.size()) return false;
  RoomActor &actor=_actors[args[0]];
  if(args[1]<0 || !actor.sprite.hasSequence(args[1])) return false;
  // 0xc47 changes the sequence only for an idle actor, then sets mode 2.
  if(!actor.animationMode) { actor.sequence=args[1]; actor.step=0; actor.animationOrigin=actor.feet; actor.frameTicks=actor.sprite.sequences[actor.sequence].steps[0].ticks; }
  actor.animationMode=2; result=0; return true;
 }
 if(op==109) {
  // 0xd69 -> 0xb8ea: 30 bindings, each actor/sound/sequence/step/volume.
  if(args.size()!=5 || args[0]<0 || uint(args[0])>=_actors.size() || args[1]<0 || uint(args[1])>=_sounds.size() ||
     args[2]<0 || args[2]>100 || args[3]<-1 || args[4]<0 || _animationSounds.size()>=30) return false;
  AnimationSound binding; binding.actor=args[0]; binding.sound=args[1]; binding.volume=args[2]; binding.sequence=args[3]; binding.step=args[4];
  result=_animationSounds.size(); _animationSounds.push_back(binding); return true;
 }
 if(op==137) {
  if(args.size()!=2 || !name.equalsIgnoreCase("OBJECT") || args[0]<0 || args[0]>=9 || args[1]<0) return false;
  Common::Array<byte> data; SpriteResource panel;
  if(!_graphics.read(1306+args[0],data) || !panel.load(data,1306+args[0])) return false;
  _inventoryPanel=panel; _inventorySelection=args[1]; result=0; return true;
 }
 if(op==57 || op==59 || op==61) {
  if(name.size()<3 || _hotspots.size()>=50) return false;
  if(op==57 && (args.size()!=3 || args[0]<0 || uint(args[0])>=_actors.size())) return false;
  if(op==61 && (args.size()!=3 || args[0]<0 || uint(args[0])>=_worldItems.size())) return false;
  if(op==59 && (args.size()!=6 || args[2]<args[0] || args[3]<args[1])) return false;
  int16 textIndex=args[op==59 ? 5 : 2];
  Common::String group=name; group.setChar('E',0); group.setChar('T',2);
  uint32 resource; Common::Array<byte> data; DialogueLine line;
  if(textIndex<0 || !_languageNames.resolve(group,textIndex,resource) || !_language.read(resource,data) ||
     !decodeDialogue(data,key,line) || line.pages.empty() || line.pages[0].empty()) return false;
  Hotspot h; h.approach=args[op==59 ? 4 : 1]; if(op==57) h.actor=args[0]; else if(op==61) h.actor=_worldItems[args[0]].actor; else h.bounds=Common::Rect(args[0],args[1],args[2],args[3]); h.label=line.pages[0][0]; result=_hotspots.size(); _hotspots.push_back(h);
  debug("[pice] hotspot=%d actor=%d label=%s",result,h.actor,h.label.c_str()); return true;
 }
 if(op==58) {
  if(args.size()!=4 || args[0]<0 || uint(args[0])>=_hotspots.size() || args[3]<0 || args[3]>=50 || _actions.size()>=70) return false;
  Action action; action.hotspot=args[0]; action.verb=args[1]; action.approach=args[2]; action.entry=args[3];
  result=_actions.size(); _actions.push_back(action); return true;
 }
 if(op==110) {
  if(args.size()!=2 || args[0]<0 || uint(args[0])>=_actions.size()) return false;
  _actions[args[0]].fallback=args[1]; result=args[0]; return true;
 }
 if(op==56 || op==92 || op==135) {
  if(args.size()!=(op==135 ? 3U : 1U) || args[0]<0 || uint(args[0])>=_actors.size()) return false;
  if(op==56 || op==92) { _actors[args[0]].routeEnabled=true; if(op==56) _player=args[0]; }
  else _actors[args[0]].labelPosition=Common::Point(args[1],args[2]);
  result=args[0]; return true;
 }
 if(op==77) {
  if(args.size()!=4 || !name.equalsIgnoreCase("A02") || args[0]<0 || args[0]>=179 || args[1]<0) return false;
  Common::Array<byte> data; RoomActor actor;
  if(!_graphics.read(args[0],data) || !actor.sprite.load(data,args[0]) || !actor.sprite.hasSequence(args[1])) return false;
  actor.resource=args[0]; actor.sequence=args[1]; actor.node=-1; actor.feet=Common::Point(args[2],args[3]);
  actor.animationMode=1; actor.animationOrigin=actor.feet; actor.frameTicks=MAX<uint16>(1,actor.sprite.sequences[actor.sequence].steps[0].ticks); result=storeActor(actor); return result>=0;
 }
 return false;
}
bool PiceEngine::createMenu(const Common::String &name,int16 local,int16 &id) {
 if(name.size()<3 || local<0 || _menus.size()>=3) return false;
 Common::String group=name; group.setChar('E',0); group.setChar('T',2);
 uint32 resource; Common::Array<byte> data;
 if(!_languageNames.resolve(group,local,resource) || !_language.read(resource,data) || data.size()<21) return false;
 uint32 first=READ_LE_UINT32(&data[7]);
 if(first<21 || first>=data.size() || (first-10)%11) return false;
 uint count=(first-10)/11;
 if(count>50) return false;
 Common::Array<Common::String> choices;
 for(uint i=0;i<count;++i) {
  Common::String key;
  for(uint j=0;j<7 && data[i*11+j];++j) key+=char(data[i*11+j]);
  DialogueLine line;
  if(!decodeDialogue(data,key,line) || line.pages.empty() || line.pages[0].empty()) return false;
  choices.push_back(line.pages[0][0]);
 }
 id=_menus.size(); _menus.push_back(choices);
 debug("[pice] create-menu id=%d resource=%u entries=%u (initially disabled)",id,resource,count); return true;
}
bool PiceEngine::createItem(const Common::String &name,int16 local,int16 sequence,int16 &id) {
 if (!name.equalsIgnoreCase("OBJECT") || local<0 || local>=9 || _items.size()>=50) return false;
 RoomActor item; Common::Array<byte> data;
 if (!_graphics.read(1306+local,data) || !item.sprite.load(data,1306+local) || sequence<0 || !item.sprite.hasSequence(sequence)) return false;
 item.resource=1306+local; item.sequence=sequence; id=_items.size(); _items.push_back(item); _ownedItems.resize(_items.size(),0);
 debug("[pice] create-item id=%d resource=%d sequence=%d",id,item.resource,sequence); return true;
}
bool PiceEngine::createActor(const Common::String &name,int16 local,int16 sequence,int16 node,int16 &id) {
 const RoomNavigation *nav=_buildingNavigation ? _buildingNavigation : (_roomRunner ? &_roomRunner->navigation() : nullptr);
 if (!name.equalsIgnoreCase("A02") || local<0 || local>=179 || !nav ||
     node<0 || uint(node)>=nav->nodes.size() || !nav->nodes[node].active) return false;
 RoomActor actor; Common::Array<byte> data;
 if (!_graphics.read(local,data) || !actor.sprite.load(data,local) || sequence<0 || !actor.sprite.hasSequence(sequence)) return false;
 const RoomNode &position=nav->nodes[node];
 if (position.scale!=100) return false;
 actor.resource=local; actor.sequence=sequence; actor.node=node; actor.feet=Common::Point(position.x,position.y);
 actor.animationMode=1; actor.animationOrigin=actor.feet; actor.frameTicks=MAX<uint16>(1,actor.sprite.sequences[actor.sequence].steps[0].ticks); id=storeActor(actor); if(id<0) return false;
 debug("[pice] create-actor id=%d resource=%d sequence=%d node=%d x=%d y=%d",id,local,sequence,node,position.x,position.y); return true;
}
bool PiceEngine::showScriptScene() {
 Common::Array<byte> bytes;
 _roomRunner.reset(); _inputLocks=0; _dialogueActor=-1; _pulseOffset=10; _pulseDelta=15;
 _worldVariables=ScriptVariables(); _worldItems.clear(); _items.clear(); _ownedItems.clear(); _selectedItem=-1; _actors.clear(); _menus.clear(); _hotspots.clear(); _actions.clear(); _player=-1;
 for(uint i=0;i<_sounds.size();++i) _mixer->stopHandle(_sounds[i].handle);
 _sounds.clear(); _animationSounds.clear(); _mixer->stopHandle(_music.handle); _music=RoomSound();
 ScriptResource bootstrap;
 if (!_graphics.read(1139,bytes) || !bootstrap.load(bytes)) return false;
 ScriptRunner init(bootstrap,&_worldVariables,this); init.start(0);
 ScriptRunner::Result initResult=init.run(0,512);
 uint16 initOpcode=0; bootstrap.word(init.position(0),initOpcode);
 int16 entryMode; _worldVariables.read(true,113,entryMode);
 debug("[pice] bootstrap result=%d pc=%u opcode=%u items=%u entry-mode=%d",int(initResult),init.position(0),initOpcode,_items.size(),entryMode);
 return loadRoom(0);
}
bool PiceEngine::playMovie(const Common::String &name,int16 before,int16 after) {
 if((before!=0 && before!=1) || (after!=0 && after!=1) || name.empty()) return false;
 for(uint i=0;i<name.size();++i) if(name[i]=='/' || name[i]=='\\' || name[i]==':') return false;
 Common::File file;
 if(!file.open(Common::Path("E_VIDEO").join(name))) return false;
 if(file.size()<32 || file.size()>64*1024*1024) return false;
 Common::Array<byte> bytes; bytes.resize(file.size());
 if(file.read(&bytes[0],bytes.size())!=bytes.size()) return false;
 MovieResource movie;
 if(!movie.load(bytes)) return false;
 // Validate every compressed frame before playing any audio or advancing
 // the script. No malformed movie is mistaken for a normal end marker.
 while(movie.nextFrame()) {}
 if(movie.failed || !movie.frame) return false;
 debug("[pice] movie validated name=%s frames=%u audio=%u final-fnv=%08x",name.c_str(),movie.frame,movie.audio.size(),hashBytes(movie.pixels));
 if(!movie.load(bytes)) return false;
 Audio::SoundHandle handle;
 _mixer->playStream(Audio::Mixer::kSFXSoundType,&handle,Audio::makeRawStream(&movie.audio[0],movie.audio.size(),22050,Audio::FLAG_UNSIGNED,DisposeAfterUse::NO));
 Common::Array<byte> screen; screen.resize(640*400,0);
 uint32 start=_system->getMillis();
 auto waitForFrame=[&](uint frame) {
  // DOS PIT divisor, six interrupts per clock tick, two ticks per frame.
  uint32 target=uint64(frame)*movie.divisor*12*1000/1193182;
  do {
   Common::Event event;
   while(_eventMan->pollEvent(event)) if(event.type==Common::EVENT_QUIT) quitGame();
   if(shouldQuit() || _system->getMillis()-start>=target) break;
   _system->delayMillis(5);
  } while(true);
 };
 if(before) _system->fillScreen(0);
 while(!shouldQuit() && movie.nextFrame()) {
  waitForFrame(movie.frame-1);
  if(shouldQuit()) break;
  for(uint y=0;y<400;++y) for(uint x=0;x<640;++x) screen[y*640+x]=movie.pixels[(y/2)*320+x/2];
  _system->getPaletteManager()->setPalette(&movie.palette[0],0,256);
  _system->copyRectToScreen(&screen[0],640,0,0,640,400); _system->updateScreen();
 }
 if(!movie.failed && !shouldQuit()) waitForFrame(movie.frame);
 _mixer->stopHandle(handle);
 if(after) _system->fillScreen(0);
 _system->getPaletteManager()->setPalette(&_scenePalette[0],0,256);
 _roomClock=_system->getMillis(); _roomRenderClock=_roomClock;
 debug("[pice] movie completed name=%s frames=%u failed=%d",name.c_str(),movie.frame,movie.failed);
 return !movie.failed && !shouldQuit();
}
bool PiceEngine::changeRoom(int16 chapter,int16 room) {
 // 0x1c232 passes room+2 to the SMxx script group. SM02 begins at 1138;
 // bootstrap occupies slot 1, so room 0 is 1140 and room 24 is 1164.
 if(chapter!=2 || room<0 || room>=40 || _pendingRoom>=0) return false;
 Common::Array<byte> bytes; ScriptResource bank;
 if(!_graphics.read(1140+room,bytes) || !bank.load(bytes)) return false;
 _pendingRoom=room;
 debug("[pice] request-room chapter=%d room=%d bank=%d",chapter,room,1140+room); return true;
}
bool PiceEngine::loadRoom(uint room) {
 Common::Array<byte> bytes; ScriptResource bank;
 if(room>=40 || !_graphics.read(1140+room,bytes) || !bank.load(bytes)) return false;
 _roomRunner.reset(); _roomBank=bank; _roomBankId=1140+room;
 // Same-chapter teardown preserves inventory, dialogue menus, variables and
 // persistent SFX handles (0x1c4a9 -> 0x15286 with argument zero).
 for(uint i=0;i<_sounds.size();++i) if(_sounds[i].active && !_sounds[i].persistent) {
  _mixer->stopHandle(_sounds[i].handle); _sounds[i]=RoomSound();
 }
 _actors.clear(); _worldItems.clear(); _hotspots.clear(); _actions.clear(); _animationSounds.clear();
 _actionEntry=-1; _actionContext=-1; _actionHotspot=-1; _actionWalking=false; _hoverHotspot=-1;
 _scenery=SpriteResource(); _navigation.clear(); _player=-1; _cameraActor=-1; _inputLocks=0; _palettePulse=false; _dialogueActor=-1; _closeup=false;
 _mixer->stopHandle(_speechHandle); _subtitle.clear(); _pendingRoom=-1;

 _scenePixels.clear(); _scenePalette.clear();
  _roomRunner.reset(new ScriptRunner(_roomBank,&_worldVariables,this));
 ScriptRunner &runner=*_roomRunner;
 _buildingNavigation=&runner.navigation();
 if (!runner.start(0)) return false;
 ScriptRunner::Result result=runner.run(0,512); uint16 opcode=0;
 bank.word(runner.position(0),opcode);
 _buildingNavigation=nullptr;
 debug("[pice] scene-prefix bank=%u entry=0 result=%d pc=%u next-opcode=%u background=%u palette=%u",
       _roomBankId,int(result),runner.position(0),opcode,_sceneBackground,_scenePaletteId);
 _navigation=runner.navigation();
 uint nodes=0,edges=0,regions=0;
 for(uint i=0;i<_navigation.nodes.size();++i) if(_navigation.nodes[i].active) ++nodes;
 for(uint i=0;i<_navigation.edges.size();++i) if(_navigation.edges[i].active) ++edges;
 for(uint i=0;i<_navigation.triangles.size();++i) if(_navigation.triangles[i].active) ++regions;
 _roomStatus=Common::String::format("Room %u | %u nodes, %u regions | initialization result %d",_roomBankId,nodes,regions,int(result));
 debug("[pice] room-navigation nodes=%u edges=%u regions=%u",nodes,edges,regions);
 // A movie-only room (1176) starts a task without loading a background.
 if(_scenePixels.empty() && _scenePalette.empty() && result==ScriptRunner::kStopped) {
  _scenePixels.resize(4+640*320,0); WRITE_LE_UINT16(&_scenePixels[0],640); WRITE_LE_UINT16(&_scenePixels[2],320);
  _scenePalette.resize(768,0); _scenePalette[765]=_scenePalette[766]=_scenePalette[767]=255;
 }
 if (_scenePixels.empty() || _scenePalette.size()!=768) return false;
 _spriteMode=false; _scenePreview=true;
 _system->getPaletteManager()->setPalette(&_scenePalette[0],0,256);
 uint brightest=0;
 for (uint i=0;i<256;++i) {
  uint light=_scenePalette[i*3]+_scenePalette[i*3+1]+_scenePalette[i*3+2];
  if (light>brightest) { brightest=light; _textColor=i; }
 }
 _roomClock=_system->getMillis(); _roomRenderClock=_roomClock;
 return renderRoom();
}
bool PiceEngine::renderRoom() {
 _system->fillScreen(0);
 _roomCanvas.clear(); _roomCanvas.resize(640*400,0);
 uint height=READ_LE_UINT16(&_scenePixels[2]);
 memcpy(&_roomCanvas[((400-height)/2)*640],&_scenePixels[4],640*height);
 for(uint i=0;i<_actors.size();++i) {
  const RoomActor &actor=_actors[i];
  if(!actor.active) continue;
  uint frame=actor.sprite.sequences[actor.sequence].steps[actor.step].frame;
  if (!actor.sprite.paintFrame(frame,actor.feet.x,actor.feet.y-40,_roomCanvas,640,400)) return false;
 }
 if (!_scenery.sequences.empty()) {
  uint frame=_scenery.sequences[0].steps[0].frame;
  if (!_scenery.paintFrame(frame,0,-40,_roomCanvas,640,400)) return false;
 }
 if(_closeup && _dialogueActor>=0) {
  Common::Array<int16> sequences;
  if(!_closeupAnimations[_dialoguePage].sample((_system->getMillis()-_pageStart)/10,sequences) ||
     !_closeupSprite.paintFrame(_closeupSprite.sequences[0].steps[0].frame,_closeupOrigin.x,_closeupOrigin.y-40,_roomCanvas,640,400)) return false;
  for(uint i=0;i<sequences.size();++i) {
   if(!_closeupSprite.hasSequence(sequences[i]) || !_closeupSprite.paintFrame(_closeupSprite.sequences[sequences[i]].steps[0].frame,
       _closeupOrigin.x,_closeupOrigin.y-40,_roomCanvas,640,400)) return false;
  }
 }
 uint inventorySlot=0;
 for(uint i=0;i<_ownedItems.size() && inventorySlot<12;++i) if(_ownedItems[i]) {
  const RoomActor &item=_items[i];
  if(item.sprite.hasSequence(item.sequence)) {
   uint frame=item.sprite.sequences[item.sequence].steps[0].frame; Common::Rect bounds;
   if(item.sprite.frameBounds(frame,bounds) && bounds.width()>0 && bounds.height()>0 && bounds.width()<=512 && bounds.height()<=512) {
    Common::Array<byte> icon; icon.resize(bounds.width()*bounds.height(),0);
    if(item.sprite.paintFrame(frame,-bounds.left,-bounds.top,icon,bounds.width(),bounds.height())) {
     uint w=MIN<uint>(40,bounds.width()),h=MIN<uint>(34,bounds.height());
     for(uint y=0;y<h;++y) for(uint x=0;x<w;++x)
      _roomCanvas[(362+y)*640+inventorySlot*50+5+x]=icon[(y*bounds.height()/h)*bounds.width()+x*bounds.width()/w];
    }
   }
  }
  if(_selectedItem==int(i)) for(uint x=inventorySlot*50+4;x<inventorySlot*50+49;++x) _roomCanvas[399*640+x]=_textColor;
  ++inventorySlot;
 }
 _system->copyRectToScreen(&_roomCanvas[0],640,0,0,640,400);
 drawLabels(); _system->updateScreen(); return true;
}
bool PiceEngine::loadDialogue(const Common::String &name,int16 local,const Common::String &key,DialogueLine &line,Common::Array<uint32> &voices) {
 if(name.size()<3 || local<0) return false;
 Common::String group=name; group.setChar('E',0); group.setChar('T',2);
 uint32 resource; Common::Array<byte> data;
 if(!_languageNames.resolve(group,local,resource) || !_language.read(resource,data) || !decodeDialogue(data,key,line) || line.pages.empty()) return false;
 voices.clear();
 for(uint i=0;i<line.pages.size();++i) {
  if(line.voice<0) { voices.push_back(0xffffffff); continue; }
  uint32 voice,offset,size;
  if(!_languageNames.resolve(line.voiceGroup,uint(line.voice)+i,voice) || !_language.read(voice,data) || !voicePayload(data,offset,size)) return false;
  voices.push_back(voice);
 }
 return true;
}
bool PiceEngine::startDialoguePage() {
 if(_dialoguePage>=_dialogueVoices.size() || _dialoguePage>=_dialoguePages.size()) return false;
 if(_dialogueVoices[_dialoguePage]!=0xffffffff && !playVoice(_dialogueVoices[_dialoguePage])) return false;
 _pageStart=_system->getMillis(); _subtitle.clear();
 for(uint i=0;i<_dialoguePages[_dialoguePage].size();++i) { if(i) _subtitle+=' '; _subtitle+=_dialoguePages[_dialoguePage][i]; }
 debug("[pice] dialogue-page=%u/%u voice=%u closeup=%d",_dialoguePage+1,_dialoguePages.size(),_dialogueVoices[_dialoguePage],int(_closeup)); return true;
}
bool PiceEngine::closeupDialogue(const Common::String &animationName,const Common::String &textName,const Common::String &key,const Common::Array<int16> &args,int16 &actor) {
 if(args.size()!=6 || _dialogueActor>=0 || _cameraActor<0 || uint(_cameraActor)>=_actors.size() || !animationName.equalsIgnoreCase("A02") ||
    args[2]<0 || args[2]>=179 || args[3]!=320 || args[4]!=480) return false;
 DialogueLine line; Common::Array<uint32> voices; Common::Array<byte> data; SpriteResource sprite; Common::Array<VoiceAnimation> animations;
 if(!loadDialogue(textName,args[5],key,line,voices) || !_graphics.read(args[2],data) || !sprite.load(data,args[2]) || !sprite.hasSequence(0)) return false;
 for(uint i=0;i<voices.size();++i) {
  VoiceAnimation animation;
  if(!_language.read(voices[i],data) || !animation.load(data)) return false;
  animations.push_back(animation);
 }
 _dialoguePages=line.pages; _dialogueVoices=voices; _dialoguePage=0; _closeup=true;
 _closeupAnimations=animations; _closeupSprite=sprite; _closeupOrigin=Common::Point(args[0],args[1]);
 if(!startDialoguePage()) { _closeup=false; return false; }
 actor=_cameraActor; _dialogueActor=actor; _dialogueRestore=_actors[actor];
 debug("[pice] closeup-dialogue actor=%d animation=%d key=%s pages=%u",actor,args[2],key.c_str(),line.pages.size()); return true;
}
bool PiceEngine::objectDialogue(int16 actor,const Common::String &name,int16 local,const Common::String &base) {
 if(actor<0 || uint(actor)>=_actors.size() || !_actors[actor].active || _dialogueActor>=0) return false;
 if(name.size()<3 || local<0) return false;
 Common::String group=name,key; group.setChar('E',0); group.setChar('T',2);
 uint32 resource; Common::Array<byte> data;
 if(!_languageNames.resolve(group,local,resource) || !_language.read(resource,data) ||
    !selectObjectDialogueKey(data,base,_actionContext,_selectedItem,key)) return false;
 if(key.empty()) {
  // 0xee8 reads F_ICETXT.PAK from data[0x156a]; 0x3ffd replaces the
  // language letter and 0x4008 replaces character 2 with T, local index 1.
  group="E_TCETXT"; local=1;
  if(!_languageNames.resolve(group,local,resource) || !_language.read(resource,data) ||
     !genericDialogueKey(data,_actionContext,_random.getRandomNumber(0x7fffffff),key)) return false;
  debug("[pice] object-dialogue fallback group=%s key=%s",group.c_str(),key.c_str());
 }
 DialogueLine line; Common::Array<uint32> voices;
 if(!loadDialogue(group,local,key,line,voices)) return false;
 // 0x4579 attaches ordinary speech to the existing actor. Unlike opcode
 // 129 this does not replace its sprite with a scripted talking animation.
 _dialoguePages=line.pages; _dialogueVoices=voices; _dialoguePage=0; _closeup=false;
 if(!startDialoguePage()) return false;
 _dialogueRestore=_actors[actor]; _dialogueActor=actor;
 debug("[pice] object-dialogue actor=%d context=%d key=%s voice=%u",actor,_actionContext,key.c_str(),voices[0]);
 return true;
}
bool PiceEngine::animatedDialogue(const Common::String &textName,const Common::String &key,const Common::String &animationName,const Common::Array<int16> &args) {
 if(args.size()!=7 || _dialogueActor!=-1 || args[0]<0 || uint(args[0])>=_actors.size() || textName.size()<3 ||
    args[1]<0 || !animationName.equalsIgnoreCase("A02") || args[2]<0 || args[2]>=179 ||
    args[3]<0 || args[4]<args[3] || args[5]<0 || args[6]<args[5]) return false;
 DialogueLine line; Common::Array<uint32> voices; Common::Array<byte> data;
 if(!loadDialogue(textName,args[1],key,line,voices)) return false;
 RoomActor actor=_actors[args[0]];
 if(!_graphics.read(args[2],data) || !actor.sprite.load(data,args[2]) || uint(args[4])>=actor.sprite.sequences.size()) return false;
 for(int sequence=args[3];sequence<=args[4];++sequence) if(!actor.sprite.hasSequence(sequence)) return false;
 // 0x1704 / 0x1ab30: speech owns a temporary animation resource, choosing
 // sequences and pauses inside the script's inclusive ranges. State 10
 // restores the previous resource/sequence when the dialogue finishes.
 actor.resource=args[2]; actor.sequence=_random.getRandomNumberRng(args[3],args[4]); actor.step=0;
 actor.animationMode=3; actor.animationOrigin=actor.feet; actor.animationPaused=false;
 actor.frameTicks=MAX<uint16>(1,actor.sprite.sequences[actor.sequence].steps[0].ticks);
 _dialoguePages=line.pages; _dialogueVoices=voices; _dialoguePage=0; _closeup=false;
 if(!startDialoguePage()) return false;
 _dialogueActor=args[0]; _dialogueRestore=_actors[args[0]]; _actors[args[0]]=actor;
 _dialogueMinSequence=args[3]; _dialogueMaxSequence=args[4]; _dialogueMinPause=args[5]; _dialogueMaxPause=args[6];
 _subtitle.clear();
 for(uint i=0;i<line.pages[0].size();++i) { if(i) _subtitle+=' '; _subtitle+=line.pages[0][i]; }
 debug("[pice] scripted-dialogue actor=%d key=%s voice=%u",args[0],key.c_str(),voices[0]);
 return true;
}
bool PiceEngine::actorStepReached(int16 actor,int16 step) const {
 // Opcode 147 starts a non-looping sequence. Reaching a later step also
 // releases its task, so bounded catch-up cannot lose a crossed trigger.
 return !actorBusy(actor) || _actors[actor].step>=uint(step);
}
bool PiceEngine::soundBusy(int16 sound) const {
 return sound>=0 && uint(sound)<_sounds.size() && _sounds[sound].active && _mixer->isSoundHandleActive(_sounds[sound].handle);
}
bool PiceEngine::actorBusy(int16 actor) const {
 return actor>=0 && uint(actor)<_actors.size() && _actors[actor].active && (_dialogueActor==actor || _actors[actor].animationMode!=0);
}
void PiceEngine::tickRoom() {
 if(!_scenePreview || !_roomRunner) return;
 uint32 now=_system->getMillis(), ticks=(now-_roomClock)/10;
 if(!ticks) return;
 // Bounded catch-up retains elapsed time; the next visit handles any backlog.
 if(ticks>10) ticks=10;
 _roomClock+=ticks*10;
 // Text-only entries have no mixer completion signal. Native readable
 // pacing (not yet the original configurable text-speed formula).
 bool textPending=_dialogueActor>=0 && _dialogueVoices[_dialoguePage]==0xffffffff &&
  _system->getMillis()-_pageStart<MAX<uint32>(2000,MIN<uint32>(10000,_subtitle.size()*60));
 if(_dialogueActor>=0 && !textPending && !_mixer->isSoundHandleActive(_speechHandle)) {
  if(_dialoguePage+1<_dialoguePages.size()) {
   ++_dialoguePage;
   if(!startDialoguePage()) { warning("[pice] Cannot play next dialogue page"); _scenePreview=false; return; }
  } else {
   Common::Point feet=_actors[_dialogueActor].feet;
   _actors[_dialogueActor]=_dialogueRestore; _actors[_dialogueActor].feet=feet; _actors[_dialogueActor].animationMode=0;
   debug("[pice] scripted-dialogue complete actor=%d",_dialogueActor);
   _dialogueActor=-1; _subtitle.clear(); _closeup=false;
  }
 }
 for(uint i=0;i<_actors.size();++i) {
  RoomActor &actor=_actors[i];
  if(!actor.active || !actor.animationMode) continue;
  if(actor.animationPaused) {
   actor.frameTicks-=ticks;
   if(actor.frameTicks>0) continue;
   actor.sequence=_random.getRandomNumberRng(_dialogueMinSequence,_dialogueMaxSequence);
   actor.step=0; actor.animationPaused=false;
   actor.frameTicks=MAX<uint16>(1,actor.sprite.sequences[actor.sequence].steps[0].ticks);
   continue;
  }
  const Common::Array<SequenceStep> &steps=actor.sprite.sequences[actor.sequence].steps;
  actor.frameTicks-=ticks;
  uint budget=32;
  while(actor.frameTicks<=0 && actor.animationMode && budget--) {
   if(actor.animationMode==4) {
    int distance=MAX(ABS(int(steps[actor.step].x)),ABS(int(steps[actor.step].y)));
    actor.walkProgress=MIN(actor.walkDistance,actor.walkProgress+distance);
    actor.feet.x=actor.animationOrigin.x+(int64(actor.walkTarget.x)-actor.animationOrigin.x)*actor.walkProgress/actor.walkDistance;
    actor.feet.y=actor.animationOrigin.y+(int64(actor.walkTarget.y)-actor.animationOrigin.y)*actor.walkProgress/actor.walkDistance;
    if(actor.walkProgress==actor.walkDistance) {
     ++actor.pathIndex;
     if(actor.pathIndex<actor.path.size()) {
      if(!startWalkLeg(actor)) { warning("[pice] Invalid remaining walk leg"); _scenePreview=false; return; }
      break;
     }
     actor.animationMode=0; actor.sequence=actor.walkDirection; actor.step=0;
     debug("[pice] walk complete actor=%u node=%d at=%d,%d",i,actor.node,actor.feet.x,actor.feet.y); break;
    }
   } else { actor.feet.x+=steps[actor.step].x; actor.feet.y+=steps[actor.step].y; }
   if(actor.step+1==steps.size()) {
    if(actor.animationMode==1) {
     actor.animationMode=0;
     debug("[pice] actor-animation complete actor=%u resource=%d sequence=%d at=%d,%d",i,actor.resource,actor.sequence,actor.feet.x,actor.feet.y);
     break;
    }
    actor.step=0;
    if(actor.animationMode==3) {
     actor.animationPaused=true; actor.frameTicks=_random.getRandomNumberRng(_dialogueMinPause,_dialogueMaxPause); break;
    }
    if(actor.animationMode!=4) actor.feet=actor.animationOrigin;
   } else ++actor.step;
   actor.frameTicks+=MAX<uint16>(1,steps[actor.step].ticks);
  }
 }
 for(uint i=0;i<_animationSounds.size();++i) {
  AnimationSound &binding=_animationSounds[i];
  const RoomActor &actor=_actors[binding.actor];
  bool match=(binding.sequence==-1 || binding.sequence==actor.sequence) && binding.step==int(actor.step);
  if(match && !binding.fired) {
   if(volume(binding.sound,binding.volume)) startSound(binding.sound,1);
  }
  binding.fired=match;
 }
 tickAction();
 for(uint i=0;i<_roomBank.count();++i) {
  if(!_roomRunner->running(i)) continue;
  if(_roomRunner->exclusiveTask()>=0 && _roomRunner->exclusiveTask()!=int(i)) continue;
  for(uint budget=0;budget<64;++budget) {
   uint32 before=_roomRunner->position(i);
   ScriptRunner::Result result=_roomRunner->step(i,budget ? 0 : ticks);
   _navigation=_roomRunner->navigation();
   if(result==ScriptRunner::kInvalid || result==ScriptRunner::kUnsupported) {
    uint16 opcode=0; _roomBank.word(before,opcode);
    _roomStatus=Common::String::format("Room script %u stopped: opcode %u at %u",i,opcode,before);
    if(_actionEntry==int(i)) { _actionEntry=-1; _actionHotspot=-1; _actionContext=-1; _actionWalking=false; }
    _snapshotPending=true;
    warning("[pice] room-task=%u result=%d pc=%u opcode=%u",i,int(result),before,opcode);
   }
   if(_roomRunner->exclusiveTask()!=int(i) || result!=ScriptRunner::kAdvanced) break;
  }
 }
#ifdef PICE_TESTBENCH
 if(!_testClickDone && ConfMan.hasKey("pice_test_click_hotspot") && !_inputLocks && !actorBusy(_player)) {
  int target=ConfMan.getInt("pice_test_click_hotspot");
  if(target>=0 && uint(target)<_hotspots.size() && _hotspots[target].actor==-1) {
   const Common::Rect &bounds=_hotspots[target].bounds;
   Common::Point point((bounds.left+bounds.right)/2,(bounds.top+bounds.bottom)/2-40);
   if(hotspotAt(point)==target && clickRoom(point,ConfMan.getBool("pice_test_right_click"))) { _testClickDone=true; debug("[pice] test click hotspot=%d at=%d,%d",target,point.x,point.y); }
  }
  if(target>=0 && uint(target)<_hotspots.size() && _hotspots[target].actor>=0) {
   const RoomActor &actor=_actors[_hotspots[target].actor];
   Common::Array<byte> low,high; low.resize(640*400,0); high.resize(640*400,255);
   uint frame=actor.sprite.sequences[actor.sequence].steps[actor.step].frame;
   if(actor.sprite.paintFrame(frame,actor.feet.x,actor.feet.y-40,low,640,400) &&
      actor.sprite.paintFrame(frame,actor.feet.x,actor.feet.y-40,high,640,400)) {
    for(uint pixel=40*640;pixel<338*640 && !_testClickDone;++pixel) if(low[pixel]==high[pixel]) {
     Common::Point point(pixel%640,pixel/640);
     if(hotspotAt(point)==target && clickRoom(point,ConfMan.getBool("pice_test_right_click"))) { _testClickDone=true; debug("[pice] test click hotspot=%d at=%d,%d",target,point.x,point.y); }
    }
   }
  }
 }
 if(_testClickDone && !_testUseDone && ConfMan.hasKey("pice_test_use_hotspot") && _actionEntry<0 && !_inputLocks && !actorBusy(_player)) {
  int target=ConfMan.getInt("pice_test_use_hotspot");
  for(uint item=0;item<_ownedItems.size() && !_testUseDone;++item) if(_ownedItems[item]) {
   for(int y=40;y<338 && !_testUseDone;y+=2) for(int x=0;x<640 && !_testUseDone;x+=2) {
    Common::Point point(x,y);
    if(target>=0 && hotspotAt(point)==target) {
     _selectedItem=item;
     if(clickRoom(point,false)) { _testUseDone=true; debug("[pice] test use item=%u hotspot=%d",item,target); }
    }
   }
  }
 }
 if(_testClickDone && !_testUseDone && ConfMan.getBool("pice_test_save_after_fire") && _actionEntry<0 && !_inputLocks && !actorBusy(_player) &&
    _hotspots.size()>11 && !_hotspots[10].active && !_hotspots[11].active && _navigation.edges[51].enabled && _navigation.edges[52].enabled) {
  if(writeCheckpoint(false)) { _testUseDone=true; debug("[pice] fire solved checkpoint: edges51/52 enabled, hotspots10/11 disabled"); }
 }
 if(_testClickDone && !_testUseDone && ConfMan.getBool("pice_test_use_fire") && _actionEntry<0 && !_inputLocks && !actorBusy(_player)) {
  for(uint item=0;item<_ownedItems.size() && !_testUseDone;++item) if(_ownedItems[item] && _hotspots.size()>10) {
   _selectedItem=item;
   const Common::Rect &bounds=_hotspots[10].bounds;
   Common::Point point((bounds.left+bounds.right)/2,(bounds.top+bounds.bottom)/2-40);
   if(writeCheckpoint(false) && clickRoom(point,false)) { _testUseDone=true; debug("[pice] test use inventory item=%u on fire",item); }
  }
 }
 if(_testSavePending && int32(_system->getMillis()-_testSaveAt)>=0) {
  _testSavePending=false;
  if(!writeCheckpoint(false)) warning("[pice] Test music checkpoint failed");
 }
#endif
 if(_savePending) {
  _savePending=false;
  if(!writeCheckpoint(true)) { warning("[pice] Autosave failed; runtime paused"); _scenePreview=false; return; }
 }
 if(_pendingRoom>=0) {
  uint room=_pendingRoom;
  if(!loadRoom(room)) { warning("[pice] Cannot initialize room %u",room); _scenePreview=false; }
  return;
 }
 if(_palettePulse && _scenePalette.size()==768) {
  _pulseRemaining-=ticks;
  if(_pulseRemaining<=0) {
   _pulseRemaining=_pulsePeriod;
   Common::Array<byte> palette=_scenePalette;
   for(uint i=1;i<240;++i) {
    palette[i*3+1]=MAX<int>(0,int(palette[i*3+1])-_pulseOffset);
    palette[i*3+2]=MAX<int>(0,int(palette[i*3+2])-_pulseOffset);
   }
   _system->getPaletteManager()->setPalette(&palette[0],0,256);
   _pulseOffset+=_pulseDelta;
   if(_pulseOffset<=10 || _pulseOffset>=50) _pulseDelta=-_pulseDelta;
  }
 }
 if(now-_roomRenderClock>=33) {
  _roomRenderClock=now;
  if(!renderRoom()) { warning("[pice] Room composition failed"); _scenePreview=false; }
#ifdef PICE_TESTBENCH
  if(_snapshotPending && ConfMan.hasKey("pice_debug_snapshot_path")) {
   Common::DumpFile snapshot;
   if(snapshot.open(Common::Path(ConfMan.get("pice_debug_snapshot_path")))) {
    const char header[]="P6\n640 400\n255\n";
    snapshot.write(header,sizeof(header)-1);
    byte palette[768],row[640*3]; _system->getPaletteManager()->grabPalette(palette,0,256);
    for(uint y=0;y<400;++y) {
     for(uint x=0;x<640;++x) memcpy(row+x*3,palette+_roomCanvas[y*640+x]*3,3);
     snapshot.write(row,sizeof(row));
    }
    snapshot.finalize();
    debug("[pice] room snapshot written: %s",ConfMan.get("pice_debug_snapshot_path").c_str());
   } else warning("[pice] Cannot open debug snapshot path");
   _snapshotPending=false;
  }
#endif
 }
}
bool PiceEngine::showAnimation(uint resource) {
 _scenePreview = false;
 Common::Array<byte> data;
 if (!_graphics.read(resource, data) || !_animation.load(data,resource)) return false;
 _sprites.clear();
 uint maxImage = 0;
 for (uint f = 0; f < _animation.frames.size(); ++f)
  for (uint p = 0; p < _animation.frames[f].parts.size(); ++p) {
   const SpritePart &part = _animation.frames[f].parts[p];
   // Flag meanings beyond the observed untransformed case are not verified.
   if (part.flagsX != 1 || part.flagsY != 1) return false;
   if (part.image > maxImage) maxImage = part.image;
  }
 for (uint i = 0; i <= maxImage; ++i) {
  SpriteImage decoded;
  if (!_animation.image(i, decoded)) return false;
  if (i == 0) debug("[pice] sprite=%u image=0 w=%u h=%u pixel-fnv=%08x mask-fnv=%08x", resource, decoded.width, decoded.height, hashBytes(decoded.pixels), hashBytes(decoded.mask));
  _sprites.push_back(decoded);
 }
 _sequence = 0;
 for (uint i = 0; i < _animation.sequences.size(); ++i)
  if (_animation.sequences[i].steps.size() > _animation.sequences[_sequence].steps.size()) _sequence = i;
 if (!_animation.hasSequence(_sequence)) return false;
 int left = 32767, top = 32767, right = -32768, bottom = -32768;
 const SpriteSequence &sequence = _animation.sequences[_sequence];
 for (uint i = 0; i < sequence.steps.size(); ++i) {
  const SequenceStep &step = sequence.steps[i];
  const SpriteFrame &frame = _animation.frames[step.frame];
  for (uint j = 0; j < frame.parts.size(); ++j) {
   const SpritePart &part = frame.parts[j];
   const SpriteImage &img = _sprites[part.image];
   int x = part.x + step.x, y = part.y + step.y;
   left = MIN(left, x); top = MIN(top, y);
   right = MAX(right, x + img.width); bottom = MAX(bottom, y + img.height);
  }
 }
 if (right < left || bottom < top) return false;
 _originX = (640 - (right-left))/2 - left;
 _originY = 42 + (316 - (bottom-top))/2 - top;
 uint darkest = 766, brightest = 0;
 for (uint i = 0; i < 256; ++i) {
  uint light = _animation.palette[i*3] + _animation.palette[i*3+1] + _animation.palette[i*3+2];
  if (light < darkest) { darkest = light; _backgroundColor = i; }
  if (light > brightest) { brightest = light; _textColor = i; }
 }
 _system->getPaletteManager()->setPalette(&_animation.palette[0], 0, 256);
 _animationId = resource; _step = 0; _nextFrame = 0; _spriteMode = true;
 debug("[pice] animation=%u frames=%u sequences=%u selected=%u steps=%u palette-fnv=%08x", resource, _animation.frames.size(), _animation.sequences.size(), _sequence, sequence.steps.size(), hashBytes(_animation.palette));
 advanceAnimation();
 return true;
}
void PiceEngine::advanceAnimation() {
 if (!_spriteMode || int32(_system->getMillis() - _nextFrame) < 0) return;
 const SpriteSequence &sequence = _animation.sequences[_sequence];
 const SequenceStep &step = sequence.steps[_step];
 const SpriteFrame &frame = _animation.frames[step.frame];
 _canvas.resize(640*400); memset(&_canvas[0], _backgroundColor, _canvas.size());
 for (uint p = 0; p < frame.parts.size(); ++p) {
  const SpritePart &part = frame.parts[p];
  const SpriteImage &img = _sprites[part.image];
  int x0 = _originX + step.x + part.x, y0 = _originY + step.y + part.y;
  for (uint y = 0; y < img.height; ++y) {
   int dy = y0 + y;
   if (dy < 42 || dy >= 358) continue;
   for (uint x = 0; x < img.width; ++x) {
    int dx = x0 + x;
    if (dx >= 0 && dx < 640 && img.mask[y*img.width+x]) _canvas[dy*640+dx] = img.pixels[y*img.width+x];
   }
  }
 }
 _system->copyRectToScreen(&_canvas[0], 640, 0, 0, 640, 400);
 drawLabels();
 // Timing unit is provisional; original EXE timer semantics still need evidence.
 _nextFrame = _system->getMillis() + MAX<uint32>(1, step.ticks) * 20;
 _step = (_step + 1) % sequence.steps.size();
}
bool PiceEngine::playVoice(uint resource) {
 Common::Array<byte> data;
 uint32 offset,size;
 if (!_language.read(resource,data) || !voicePayload(data,offset,size)) return false;
 byte *owned=(byte *)malloc(size);
 if (!owned) return false;
 memcpy(owned,&data[offset],size);
 Common::MemoryReadStream *stream=new Common::MemoryReadStream(owned,size,DisposeAfterUse::YES);
 Audio::SeekableAudioStream *wave=Audio::makeWAVStream(stream,DisposeAfterUse::YES);
 if (!wave) { warning("[pice] Invalid WAV resource %u",resource); return false; }
 _mixer->stopHandle(_speechHandle);
 debug("[pice] speech=%u bytes=%u fnv=%08x rate=%d wave-offset=%u wave-bytes=%u",resource,data.size(),hashBytes(data),wave->getRate(),offset,size);
 _mixer->playStream(Audio::Mixer::kSpeechSoundType,&_speechHandle,wave);
 return true;
}
void PiceEngine::playSpeech() {
 _subtitle.clear();
 for (uint n=0;n<_language.count();++n) {
  uint resource=_speech++ % _language.count();
  if (playVoice(resource)) { drawLabels(); return; }
 }
}
bool PiceEngine::playDialogueSample() {
 // Explicit testbench selection, not an event triggered by a running scene.
 uint32 textResource,voiceResource;
 Common::Array<byte> data; DialogueLine line;
 if (!_languageNames.resolve("E_T02",6,textResource) || !_language.read(textResource,data) ||
     !decodeDialogue(data,"1070",line) || line.voice<0 ||
     !_languageNames.resolve(line.voiceGroup,uint(line.voice),voiceResource) ||
     line.pages.size()!=1 || line.pages[0].size()!=1 || !playVoice(voiceResource)) return false;
 _subtitle=line.pages[0][0];
 debug("[pice] dialogue bank=%u key=1070 group=%s local=%d voice=%u text-fnv=%08x subtitle=%s",
       textResource,line.voiceGroup.c_str(),line.voice,voiceResource,hashBytes(data),_subtitle.c_str());
 drawLabels();
 return true;
}
Common::Error PiceEngine::run() {
 if (!runStateTests() || !runActorLifecycleTests() || !runMovieTests() || !runLipSyncTests() || !runRoomTests() || !runSelfTests() || !runSpriteTests() || !runResourceNameTests() || !runScriptTests() || !runDialogueTests() || !runVoiceTests()) return Common::Error(Common::kUnknownError, "PICE parser self-tests failed");
 debug("[pice] parser self-tests PASS");
 if (!_graphics.open(Common::Path("KSVGA.KRO")) || !_language.open(Common::Path("E_KLANG.KRO")) || !_soundArchive.open(Common::Path("KSOUND.KRO")))
  return Common::Error(Common::kReadingFailed, "PICE preview requires the English European CD KSVGA.KRO, E_KLANG.KRO and KSOUND.KRO");
 debug("[pice] KSVGA=%u E_KLANG=%u entries", _graphics.count(), _language.count());
 Common::File nameFile;
 if (nameFile.open(Common::Path("E_KLANG.BIN"))) {
  if (nameFile.size()<9 || nameFile.size()>1024*1024) return Common::Error(Common::kReadingFailed,"Invalid language index size");
  Common::Array<byte> bytes; bytes.resize(nameFile.size());
  ResourceNames &names = _languageNames;
  if (nameFile.read(&bytes[0],bytes.size())!=bytes.size() || !names.load(bytes,_language.count()))
   return Common::Error(Common::kReadingFailed,"Invalid language resource index");
  debug("[pice] language-index groups=%u encoded-fnv=%08x",names.count(),hashBytes(bytes));
  for (uint i=0;i<names.count();++i) {
   uint32 id;
   if (!names.resolve(names.entry(i).name,0,id)) return Common::Error(Common::kReadingFailed,"Invalid resource name mapping");
   debug("[pice] language-group=%s base=%u",names.entry(i).name.c_str(),id);
  }
 } else warning("[pice] E_KLANG.BIN unavailable; named language resources disabled");
 // English EU graphics-name table identifies SM02..SM12 as 1138..1305.
 // This audits entry tables only; it does not run scene side effects.
 uint banks=0, entries=0, placeholders=0;
 uint prefixStopped=0, prefixUnsupported=0, prefixInvalid=0, prefixBudget=0, prefixWaiting=0, prefixMoved=0;
 for (uint i=1138;i<1306;++i) {
  Common::Array<byte> bytes;
  if (!_graphics.read(i,bytes)) return Common::Error(Common::kReadingFailed,"Cannot read script bank");
  if (bytes.size()==5 && memcmp(&bytes[0],"dummy",5)==0) { ++placeholders; continue; }
  ScriptResource scripts;
  if (!scripts.load(bytes)) return Common::Error(Common::kReadingFailed,"Invalid script entry table");
  ++banks; entries+=scripts.count();
  debug("[pice] script-bank=%u entries=%u fnv=%08x",i,scripts.count(),hashBytes(bytes));
  // Independent entry probes with fresh variable state, not a game scheduler.
  // Stop before unsupported scene commands and yield after 64 instructions.
  for(uint e=0;e<scripts.count();++e) {
   ScriptRunner probe(scripts); probe.start(e);
   ScriptRunner::Result result=probe.run(e,64);
   if(probe.position(e)!=scripts.entry(e)) ++prefixMoved;
   switch(result) {
   case ScriptRunner::kStopped: ++prefixStopped; break;
   case ScriptRunner::kUnsupported: ++prefixUnsupported; break;
   case ScriptRunner::kBudgetExhausted: ++prefixBudget; break;
   case ScriptRunner::kWaiting: ++prefixWaiting; break;
   default: ++prefixInvalid; break;
   }
  }
  if (i==1138) {
   uint16 opcode; scripts.word(scripts.entry(0),opcode);
   uint32 pc=scripts.entry(0)+2; int16 value;
   if (opcode!=31 || !scripts.expression(pc,value)) return Common::Error(Common::kReadingFailed,"Unexpected first script prefix");
   debug("[pice] script-prefix resource=1138 entry=0 opcode=%u argument=%d next-byte=%u",opcode,value,pc);
   ScriptRunner runner(scripts);
   if (!runner.start(0) || runner.step(0)!=ScriptRunner::kStopped || runner.running(0))
    return Common::Error(Common::kReadingFailed,"First script did not stop its own task");
   debug("[pice] script-runner resource=1138 entry=0 stop-self PASS next-byte=%u",runner.position(0));
  }
 }
 debug("[pice] script-index banks=%u entries=%u placeholders=%u",banks,entries,placeholders);
 debug("[pice] script-prefix-probes moved=%u stopped=%u unsupported=%u invalid-or-unsupported-expression=%u budget=%u waiting=%u",prefixMoved,prefixStopped,prefixUnsupported,prefixInvalid,prefixBudget,prefixWaiting);
 for (uint i = 0; i + 1 < _graphics.count(); ++i) {
  const KroEntry &e = _graphics.entry(i);
  if (e.size < 640*200 || e.size > 640*320+4 || _graphics.entry(i+1).size != 768) continue;
  Common::Array<byte> data;
  uint16 w, h;
  if (!_graphics.read(i, data)) return Common::Error(Common::kReadingFailed, "Invalid graphics resource");
  if (decodeImageHeader(data, w, h) && w <= 640 && h <= 320) _pictures.push_back(i);
 }
 if (_pictures.empty()) return Common::Error(Common::kReadingFailed, "No supported background images");
 initGraphics(640, 400);
 syncSoundSettings();
 CursorMan.setDefaultArrowCursor(true); CursorMan.showMouse(true);
 if (!showPicture(0)) { CursorMan.popCursor(); return Common::Error(Common::kReadingFailed); }
 // Exercise a text-to-voice mapping from original data; no gameplay trigger yet.
 bool restored=false;
#ifdef PICE_TESTBENCH
 if(ConfMan.getBool("pice_restore_slot0")) restored=readCheckpoint(false);
 else if(ConfMan.getBool("pice_restore_autosave")) restored=readCheckpoint(true);
#endif
 if (!restored && !showScriptScene()) { warning("[pice] Room initialization unsupported; staying in background preview"); if (!playDialogueSample()) playSpeech(); }
 bool done = false;
 while (!shouldQuit() && !done) {
  Common::Event event;
  while (_eventMan->pollEvent(event)) {
   int change = 0;
   if(_scenePreview && event.type==Common::EVENT_LBUTTONDOWN && event.mouse.y>=360) {
    uint slot=0; for(uint i=0;i<_ownedItems.size();++i) if(_ownedItems[i]) {
     if(int(slot)==event.mouse.x/50) { _selectedItem=_selectedItem==int(i) ? -1 : int(i); break; } ++slot;
    }
    continue;
   }
   if(_scenePreview && event.type==Common::EVENT_MOUSEMOVE) _hoverHotspot=hotspotAt(event.mouse);
   if(_scenePreview && (event.type==Common::EVENT_LBUTTONDOWN || event.type==Common::EVENT_RBUTTONDOWN) && event.mouse.y>=40 && event.mouse.y<338) {
    clickRoom(event.mouse,event.type==Common::EVENT_RBUTTONDOWN); continue;
   }
   if (event.type == Common::EVENT_RBUTTONDOWN && !_scenePreview) change = -1;
   if (event.type == Common::EVENT_MBUTTONDOWN && !_scenePreview) playSpeech();
   if (event.type == Common::EVENT_LBUTTONDOWN && !_scenePreview) {
    if (event.mouse.y < 40) {
     if (_spriteMode) showPicture(_picture);
     else if (!showAnimation(0)) warning("[pice] Cannot open animation 0");
    } else if (event.mouse.y >= 380) {
     if (event.mouse.x < 155) change = -1;
     else if (event.mouse.x < 310) change = 1;
     else if (event.mouse.x < 500) playDialogueSample();
     else done = true;
    } else if(!_scenePreview) change = 1;
   }
   if (event.type == Common::EVENT_KEYDOWN) {
    if (event.kbd.keycode == Common::KEYCODE_F5 && !writeCheckpoint(false)) warning("[pice] Save unavailable or failed");
    if (event.kbd.keycode == Common::KEYCODE_F7 && !readCheckpoint(false) && !readCheckpoint(true)) warning("[pice] No valid checkpoint to load");
    if (event.kbd.keycode == Common::KEYCODE_ESCAPE) done = true;
    if (!_scenePreview && event.kbd.keycode == Common::KEYCODE_RIGHT) change = 1;
    if (!_scenePreview && event.kbd.keycode == Common::KEYCODE_LEFT) change = -1;
    if (!_scenePreview && event.kbd.keycode == Common::KEYCODE_SPACE) playSpeech();
    if (!_scenePreview && event.kbd.keycode == Common::KEYCODE_c) playDialogueSample();
   }
   if (change && _spriteMode) {
    int next = int(_sequence) + change;
    if (next < 0) next = _animation.sequences.size() - 1;
    _sequence = uint(next) % _animation.sequences.size();
    // Keep the same anchor when browsing sequences from this resource.
    for(uint skip=0;skip<_animation.sequences.size() && !_animation.hasSequence(_sequence);++skip)
     _sequence=(_sequence+_animation.sequences.size()+change)%_animation.sequences.size();
    _step = 0; _nextFrame = 0;
   } else if (change) {
    uint next = (_picture + _pictures.size() + change) % _pictures.size();
    if (!showPicture(next)) { _mixer->stopHandle(_speechHandle); CursorMan.popCursor(); return Common::Error(Common::kReadingFailed); }
   }
  }
  tickRoom();
  advanceAnimation();
  _system->updateScreen(); _system->delayMillis(10);
 }
 for(uint i=0;i<_sounds.size();++i) _mixer->stopHandle(_sounds[i].handle);
 _mixer->stopHandle(_speechHandle); _mixer->stopHandle(_music.handle); CursorMan.popCursor();
 return Common::kNoError;
}
}
