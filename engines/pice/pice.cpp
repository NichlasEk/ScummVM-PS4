// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/pice.h"
#include "pice/resource-names.h"
#include "pice/script.h"
#include "pice/dialogue.h"
#include "pice/voice.h"
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
 font->drawString(screen, "PICE ENGINE PREVIEW - GAMEPLAY NOT IMPLEMENTED", 8, 6, 624, _textColor);
 Common::String status = _spriteMode ? Common::String::format("Sprite %u | sequence %u | step %u/%u | provisional 20 ms/tick", _animationId, _sequence, _step + 1, _animation.sequences[_sequence].steps.size()) : Common::String::format("KSVGA resource %u | image %u/%u", _pictures[_picture], _picture + 1, _pictures.size());
 if (_scenePreview) status=_roomStatus;
 font->drawString(screen, status, 8, 24, 624, _textColor);
 font->drawString(screen, "Click title: images/animation | Left: next | Right: previous", 8, 365, 624, _textColor);
 font->drawString(screen, "[ Previous ]       [ Next ]        [ Repeat line ]        [ Exit ]", 8, 385, 624, _textColor);
 screen->fillRect(Common::Rect(0,338,640,360),0);
 if (!_subtitle.empty()) font->drawString(screen,_subtitle,8,341,624,_textColor,Graphics::kTextAlignCenter);
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
 if (!_graphics.read(resource,pixels) || !decodeImageHeader(pixels,w,h) || w!=640 || h!=316) return false;
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
 if (!_graphics.read(815+uint(local),data) || !candidate.load(data) || candidate.sequences.size()!=1 ||
     candidate.sequences[0].steps.size()!=1) return false;
 _scenery=candidate;
 debug("[pice] scenery resource=%u frame=%u parts=%u fnv=%08x",815+uint(local),candidate.sequences[0].steps[0].frame,
       candidate.frames[candidate.sequences[0].steps[0].frame].parts.size(),hashBytes(data));
 return true;
}
bool PiceEngine::startSound(uint id,uint loops) {
 if(id>=_sounds.size()) return false;
 RoomSound &sound=_sounds[id]; Common::Array<byte> data; uint32 offset,size;
 if(!_soundArchive.read(sound.resource,data) || !voicePayload(data,offset,size)) return false;
 byte *owned=(byte *)malloc(size); if(!owned) return false;
 memcpy(owned,&data[offset],size);
 Audio::SeekableAudioStream *wave=Audio::makeWAVStream(new Common::MemoryReadStream(owned,size,DisposeAfterUse::YES),DisposeAfterUse::YES);
 if(!wave) return false;
 _mixer->stopHandle(sound.handle);
 _mixer->playStream(Audio::Mixer::kSFXSoundType,&sound.handle,Audio::makeLoopingAudioStream(wave,loops),-1,sound.volume);
 debug("[pice] start-sound id=%u resource=%u loops=%u",id,sound.resource,loops); return true;
}
bool PiceEngine::volume(int16 handle,int16 value) {
 if(handle<0 || uint(handle)>=_sounds.size() || value<0 || value>100) return false;
 _sounds[handle].volume=value*255/100;
 _mixer->setChannelVolume(_sounds[handle].handle,_sounds[handle].volume); return true;
}
bool PiceEngine::invoke(uint16 op,const Common::String &name,const Common::Array<int16> &args,const Common::String &key,int16 &result) {
 result=-1;
 if(op==133 || op==29) {
  if(args.size()!=1 || !name.equalsIgnoreCase("SMP02") || args[0]<0 || args[0]>=68 || _sounds.size()>=25) return false;
  Common::Array<byte> data; uint32 offset,size;
  if(!_soundArchive.read(45+args[0],data) || !voicePayload(data,offset,size)) return false;
  RoomSound sound; sound.resource=45+args[0]; result=_sounds.size(); _sounds.push_back(sound);
  debug("[pice] load-sound id=%d resource=%u",result,sound.resource); return true;
 }
 if(op==114 || op==100) {
  if(args.size()!=(op==114 ? 2U : 1U) || args[0]<0 || uint(args[0])>=_sounds.size() || (op==114 && args[1]<0)) return false;
  if(!startSound(args[0],op==100 ? 1 : uint(args[1]))) return false;
  result=args[0]; return true;
 }
 if(op==50) {
  if(args.size()!=2 || args[0]<0 || uint(args[0])>=_actors.size() || args[1]<0 || uint(args[1])>=_navigation.nodes.size() ||
     !_navigation.nodes[args[1]].active || _navigation.nodes[args[1]].scale!=100) return false;
  RoomActor &actor=_actors[args[0]];
  if(!actor.routeEnabled) return false;
  Common::Point target(_navigation.nodes[args[1]].x,_navigation.nodes[args[1]].y), floorStart;
  if(!_navigation.nearest(actor.feet,floorStart) || !_navigation.segment(floorStart,target)) return false;
  int dx=target.x-actor.feet.x,dy=target.y-actor.feet.y,ax=ABS(dx),ay=ABS(dy);
  if(!ax && !ay) { actor.animationMode=0; result=args[0]; return true; }
  int direction=ax*1000<ay*414 ? (dy>0 ? 0 : 4) : ay*1000<ax*414 ? (dx>0 ? 2 : 6) : dx>0 ? (dy>0 ? 1 : 3) : (dy>0 ? 7 : 5);
  if(!actor.sprite.hasSequence(direction) || !actor.sprite.hasSequence(direction+8)) return false;
  bool moves=false;
  const Common::Array<SequenceStep> &walkSteps=actor.sprite.sequences[direction+8].steps;
  for(uint i=0;i<walkSteps.size();++i) if(walkSteps[i].x || walkSteps[i].y) moves=true;
  if(!moves) return false;
  actor.node=args[1]; actor.walkTarget=target; actor.animationOrigin=actor.feet; actor.walkProgress=0;
  actor.walkDistance=MAX(ax,ay); actor.walkDirection=direction; actor.sequence=direction+8;
  actor.step=0; actor.frameTicks=MAX<uint16>(1,actor.sprite.sequences[actor.sequence].steps[0].ticks); actor.animationMode=4;
  debug("[pice] walk actor=%d from=%d,%d target-node=%d to=%d,%d direction=%d",args[0],actor.feet.x,actor.feet.y,args[1],target.x,target.y,direction);
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
   if(!_graphics.read(args[1],data) || !actor.sprite.load(data) || !actor.sprite.hasSequence(args[2])) return false;
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
  if(args.size()!=2 || args[0]<0 || uint(args[0])>=_items.size() || !_buildingNavigation || _worldItems.size()>=10 || _actors.size()>=20 ||
     args[1]<0 || uint(args[1])>=_buildingNavigation->nodes.size() || !_buildingNavigation->nodes[args[1]].active) return false;
  RoomActor actor=_items[args[0]]; actor.sequence+=2;
  const RoomNode &node=_buildingNavigation->nodes[args[1]];
  if(!actor.sprite.hasSequence(actor.sequence) || node.scale!=100) return false;
  actor.node=args[1]; actor.feet=Common::Point(node.x,node.y);
  WorldItem item; item.item=args[0]; item.actor=_actors.size();
  result=_worldItems.size(); _actors.push_back(actor); _worldItems.push_back(item); return true;
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
  if(!_graphics.read(1306+args[0],data) || !panel.load(data)) return false;
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
  Action action; action.hotspot=args[0]; action.verb=args[1]; action.item=args[2]; action.entry=args[3];
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
  if(args.size()!=4 || !name.equalsIgnoreCase("A02") || args[0]<0 || args[0]>=179 || args[1]<0 || _actors.size()>=20) return false;
  Common::Array<byte> data; RoomActor actor;
  if(!_graphics.read(args[0],data) || !actor.sprite.load(data) || !actor.sprite.hasSequence(args[1])) return false;
  actor.resource=args[0]; actor.sequence=args[1]; actor.node=-1; actor.feet=Common::Point(args[2],args[3]);
  result=_actors.size(); _actors.push_back(actor); return true;
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
 if (!_graphics.read(1306+local,data) || !item.sprite.load(data) || sequence<0 || !item.sprite.hasSequence(sequence)) return false;
 item.resource=1306+local; item.sequence=sequence; id=_items.size(); _items.push_back(item);
 debug("[pice] create-item id=%d resource=%d sequence=%d",id,item.resource,sequence); return true;
}
bool PiceEngine::createActor(const Common::String &name,int16 local,int16 sequence,int16 node,int16 &id) {
 if (!name.equalsIgnoreCase("A02") || local<0 || local>=179 || _actors.size()>=20 || !_buildingNavigation ||
     node<0 || uint(node)>=_buildingNavigation->nodes.size() || !_buildingNavigation->nodes[node].active) return false;
 RoomActor actor; Common::Array<byte> data;
 if (!_graphics.read(local,data) || !actor.sprite.load(data) || sequence<0 || !actor.sprite.hasSequence(sequence)) return false;
 const RoomNode &position=_buildingNavigation->nodes[node];
 if (position.scale!=100) return false;
 actor.resource=local; actor.sequence=sequence; actor.node=node; actor.feet=Common::Point(position.x,position.y);
 id=_actors.size(); _actors.push_back(actor);
 debug("[pice] create-actor id=%d resource=%d sequence=%d node=%d x=%d y=%d",id,local,sequence,node,position.x,position.y); return true;
}
bool PiceEngine::showScriptScene() {
 Common::Array<byte> bytes; ScriptResource &bank=_roomBank;
 _roomRunner.reset(); _inputLocks=0; _dialogueActor=-1; _pulseOffset=10; _pulseDelta=15;
 _worldVariables=ScriptVariables(); _worldItems.clear(); _items.clear(); _actors.clear(); _menus.clear(); _hotspots.clear(); _actions.clear(); _player=-1;
 for(uint i=0;i<_sounds.size();++i) _mixer->stopHandle(_sounds[i].handle);
 _sounds.clear(); _animationSounds.clear();
 ScriptResource bootstrap;
 if (!_graphics.read(1139,bytes) || !bootstrap.load(bytes)) return false;
 ScriptRunner init(bootstrap,&_worldVariables,this); init.start(0);
 ScriptRunner::Result initResult=init.run(0,512);
 uint16 initOpcode=0; bootstrap.word(init.position(0),initOpcode);
 int16 entryMode; _worldVariables.read(true,113,entryMode);
 debug("[pice] bootstrap result=%d pc=%u opcode=%u items=%u entry-mode=%d",int(initResult),init.position(0),initOpcode,_items.size(),entryMode);
 if (!_graphics.read(1140,bytes) || !bank.load(bytes)) return false;
 _scenePixels.clear(); _scenePalette.clear();
  _roomRunner.reset(new ScriptRunner(bank,&_worldVariables,this));
 ScriptRunner &runner=*_roomRunner;
 _buildingNavigation=&runner.navigation();
 if (!runner.start(0)) return false;
 ScriptRunner::Result result=runner.run(0,512); uint16 opcode=0;
 bank.word(runner.position(0),opcode);
 _buildingNavigation=nullptr;
 debug("[pice] scene-prefix bank=1140 entry=0 result=%d pc=%u next-opcode=%u background=%u palette=%u",
       int(result),runner.position(0),opcode,_sceneBackground,_scenePaletteId);
 _navigation=runner.navigation();
 uint nodes=0,edges=0,regions=0;
 for(uint i=0;i<_navigation.nodes.size();++i) if(_navigation.nodes[i].active) ++nodes;
 for(uint i=0;i<_navigation.edges.size();++i) if(_navigation.edges[i].active) ++edges;
 for(uint i=0;i<_navigation.triangles.size();++i) if(_navigation.triangles[i].active) ++regions;
 _roomStatus=Common::String::format("Room 1140 | %u nodes, %u regions | initialization result %d",nodes,regions,int(result));
 debug("[pice] room-navigation nodes=%u edges=%u regions=%u",nodes,edges,regions);
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
 memcpy(&_roomCanvas[42*640],&_scenePixels[4],640*316);
 for(uint i=0;i<_actors.size();++i) {
  const RoomActor &actor=_actors[i];
  uint frame=actor.sprite.sequences[actor.sequence].steps[actor.step].frame;
  if (!actor.sprite.paintFrame(frame,actor.feet.x,actor.feet.y-40,_roomCanvas,640,400)) return false;
 }
 if (!_scenery.sequences.empty()) {
  uint frame=_scenery.sequences[0].steps[0].frame;
  if (!_scenery.paintFrame(frame,0,-40,_roomCanvas,640,400)) return false;
 }
 _system->copyRectToScreen(&_roomCanvas[0],640,0,0,640,400);
 drawLabels(); _system->updateScreen(); return true;
}
bool PiceEngine::animatedDialogue(const Common::String &textName,const Common::String &key,const Common::String &animationName,const Common::Array<int16> &args) {
 if(args.size()!=7 || _dialogueActor!=-1 || args[0]<0 || uint(args[0])>=_actors.size() || textName.size()<3 ||
    args[1]<0 || !animationName.equalsIgnoreCase("A02") || args[2]<0 || args[2]>=179 ||
    args[3]<0 || args[4]<args[3] || args[5]<0 || args[6]<args[5]) return false;
 Common::String group=textName; group.setChar('E',0); group.setChar('T',2);
 uint32 resource,voice; Common::Array<byte> data; DialogueLine line;
 if(!_languageNames.resolve(group,args[1],resource) || !_language.read(resource,data) || !decodeDialogue(data,key,line) ||
    line.pages.size()!=1 || line.pages[0].empty() || line.voice<0 || !_languageNames.resolve(line.voiceGroup,line.voice,voice)) return false;
 RoomActor actor=_actors[args[0]];
 if(!_graphics.read(args[2],data) || !actor.sprite.load(data) || uint(args[4])>=actor.sprite.sequences.size()) return false;
 for(int sequence=args[3];sequence<=args[4];++sequence) if(!actor.sprite.hasSequence(sequence)) return false;
 // 0x1704 / 0x1ab30: speech owns a temporary animation resource, choosing
 // sequences and pauses inside the script's inclusive ranges. State 10
 // restores the previous resource/sequence when the dialogue finishes.
 actor.resource=args[2]; actor.sequence=_random.getRandomNumberRng(args[3],args[4]); actor.step=0;
 actor.animationMode=3; actor.animationOrigin=actor.feet; actor.animationPaused=false;
 actor.frameTicks=MAX<uint16>(1,actor.sprite.sequences[actor.sequence].steps[0].ticks);
 if(!playVoice(voice)) return false;
 _dialogueActor=args[0]; _dialogueRestore=_actors[args[0]]; _actors[args[0]]=actor;
 _dialogueMinSequence=args[3]; _dialogueMaxSequence=args[4]; _dialogueMinPause=args[5]; _dialogueMaxPause=args[6];
 _subtitle.clear();
 for(uint i=0;i<line.pages[0].size();++i) { if(i) _subtitle+=' '; _subtitle+=line.pages[0][i]; }
 debug("[pice] scripted-dialogue actor=%d text=%u key=%s voice=%u",args[0],resource,key.c_str(),voice);
 return true;
}
bool PiceEngine::actorBusy(int16 actor) const {
 return actor>=0 && uint(actor)<_actors.size() && _actors[actor].animationMode!=0;
}
void PiceEngine::tickRoom() {
 if(!_scenePreview || !_roomRunner) return;
 uint32 now=_system->getMillis(), ticks=(now-_roomClock)/10;
 if(!ticks) return;
 // Bounded catch-up retains elapsed time; the next visit handles any backlog.
 if(ticks>10) ticks=10;
 _roomClock+=ticks*10;
 if(_dialogueActor>=0 && !_mixer->isSoundHandleActive(_speechHandle)) {
  Common::Point feet=_actors[_dialogueActor].feet;
  _actors[_dialogueActor]=_dialogueRestore;
  _actors[_dialogueActor].feet=feet;
  _actors[_dialogueActor].animationMode=0;
  debug("[pice] scripted-dialogue complete actor=%d",_dialogueActor);
  _dialogueActor=-1; _subtitle.clear();
 }
 for(uint i=0;i<_actors.size();++i) {
  RoomActor &actor=_actors[i];
  if(!actor.animationMode) continue;
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
 for(uint i=0;i<_roomBank.count();++i) {
  if(!_roomRunner->running(i)) continue;
  uint32 before=_roomRunner->position(i);
  ScriptRunner::Result result=_roomRunner->step(i,ticks);
  if(result==ScriptRunner::kInvalid || result==ScriptRunner::kUnsupported) {
   uint16 opcode=0; _roomBank.word(before,opcode);
   _roomStatus=Common::String::format("Room script %u stopped: opcode %u at %u",i,opcode,before);
   _snapshotPending=true;
   warning("[pice] room-task=%u result=%d pc=%u opcode=%u",i,int(result),before,opcode);
  }
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
 if (!_graphics.read(resource, data) || !_animation.load(data)) return false;
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
 if (!runRoomTests() || !runSelfTests() || !runSpriteTests() || !runResourceNameTests() || !runScriptTests() || !runDialogueTests() || !runVoiceTests()) return Common::Error(Common::kUnknownError, "PICE parser self-tests failed");
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
 if (!showScriptScene()) { warning("[pice] Room initialization unsupported; staying in background preview"); if (!playDialogueSample()) playSpeech(); }
 bool done = false;
 while (!shouldQuit() && !done) {
  Common::Event event;
  while (_eventMan->pollEvent(event)) {
   int change = 0;
   if (event.type == Common::EVENT_RBUTTONDOWN && !_scenePreview) change = -1;
   if (event.type == Common::EVENT_MBUTTONDOWN) playSpeech();
   if (event.type == Common::EVENT_LBUTTONDOWN) {
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
    if (event.kbd.keycode == Common::KEYCODE_ESCAPE) done = true;
    if (event.kbd.keycode == Common::KEYCODE_RIGHT) change = 1;
    if (event.kbd.keycode == Common::KEYCODE_LEFT) change = -1;
    if (event.kbd.keycode == Common::KEYCODE_SPACE) playSpeech();
    if (event.kbd.keycode == Common::KEYCODE_c) playDialogueSample();
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
 _mixer->stopHandle(_speechHandle); CursorMan.popCursor();
 return Common::kNoError;
}
}
