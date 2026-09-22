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
 updateHover();
 Graphics::Surface *screen = _system->lockScreen();
 const Graphics::Font *font = FontMan.getFontByUsage(Graphics::FontManager::kGUIFont);
 if(!_scenePreview) font->drawString(screen, "PICE ENGINE PREVIEW - PARTIAL GAMEPLAY", 8, 6, 624, _textColor);
 Common::String status = _spriteMode ? Common::String::format("Sprite %u | sequence %u | step %u/%u | provisional 20 ms/tick", _animationId, _sequence, _step + 1, _animation.sequences[_sequence].steps.size()) : Common::String::format("KSVGA resource %u | image %u/%u", _pictures[_picture], _picture + 1, _pictures.size());
 if (_scenePreview) status=_roomStatus;
 if(!_scenePreview) font->drawString(screen, status, 8, 24, 624, _textColor);
 if(!_scenePreview) font->drawString(screen, _scenePreview ? "L1/Q: save | R1/U: load | Esc: exit" : "Click title: images/animation | Left: next | Right: previous", 8, 365, 624, _textColor);
 if(_hoverHotspot>=0 || _hoverItem>=0) {
  Graphics::Surface label; label.create(320,11,Graphics::PixelFormat::createFormatCLUT8());
  label.fillRect(Common::Rect(0,0,320,11),0);
  Common::String name=_hoverItem>=0 ? (uint(_hoverItem)<_itemLabels.size() && !_itemLabels[_hoverItem].empty() ? _itemLabels[_hoverItem] : "Inventory item") : _hotspots[_hoverHotspot].label;
  font->drawString(&label,displayText(name),4,1,312,_textColor,Graphics::kTextAlignCenter);
  for(int y=0;y<22;++y) for(int x=0;x<640;++x)
   *static_cast<byte *>(screen->getBasePtr(x,360+y))=*static_cast<const byte *>(label.getBasePtr(x/2,y/2));
  label.free();
 }

 if(_scenePreview && _hoverHotspot<0 && _hoverItem<0 && (_roomStatus.contains("stopped") || _roomStatus.contains("failed") || _roomStatus.contains("No valid")))
  font->drawString(screen,_roomStatus,8,365,624,_textColor);
 if(!_scenePreview) font->drawString(screen, _scenePreview ? "Left: act / walk | Right: examine | L1/Q save | R1/U load" : "[ Previous ]       [ Next ]        [ Repeat line ]        [ Exit ]", 8, 385, 624, _textColor);
 if (!_subtitle.empty()) {
  // Use a high-contrast, wrapped caption panel. The former one-line GUI
  // label clipped long replies and became tiny when the guest was scaled.
  Common::Array<Common::U32String> lines;
  const Common::U32String subtitle=displayText(_subtitle);
  int scale=2;
  font->wordWrapText(subtitle,304,lines);
  if(lines.size()>5) { scale=1; lines.clear(); font->wordWrapText(subtitle,608,lines); }
  int height=MIN<int>(320,int(lines.size())*font->getFontHeight()*scale+16);
  byte dark=0,light=_textColor; uint lowest=766,highest=0;
  byte palette[768]; _system->getPaletteManager()->grabPalette(palette,0,256);
  for(uint i=0;i<256;++i) {
   uint sum=palette[i*3]+palette[i*3+1]+palette[i*3+2];
   if(sum<lowest) { lowest=sum; dark=i; }
   if(sum>highest) { highest=sum; light=i; }
  }
  Graphics::Surface caption; caption.create(640/scale,height/scale,Graphics::PixelFormat::createFormatCLUT8());
  caption.fillRect(Common::Rect(0,0,caption.w,caption.h),dark);
  for(uint i=0;i<lines.size();++i)
   font->drawString(&caption,lines[i],8/scale,8/scale+i*font->getFontHeight(),caption.w-16/scale,light,Graphics::kTextAlignCenter);
  for(int y=0;y<height;++y) for(int x=0;x<640;++x)
   *static_cast<byte *>(screen->getBasePtr(x,400-height+y))=*static_cast<const byte *>(caption.getBasePtr(x/scale,y/scale));
  caption.free();
 }
 if(_activeMenu>=0 && _menuChoice==-1) {
  int rows=0; for(uint i=0;i<_menuEnabled[_activeMenu].size();++i) if(_menuEnabled[_activeMenu][i]) ++rows;
  Graphics::Surface menu; menu.create(136,rows*12,Graphics::PixelFormat::createFormatCLUT8()); menu.fillRect(Common::Rect(0,0,136,rows*12),0);
  int row=0;
  for(uint i=0;i<_menuEnabled[_activeMenu].size();++i) if(_menuEnabled[_activeMenu][i])
   font->drawString(&menu,displayText(_menus[_activeMenu][i]),4,row++*12+2,128,_textColor);
  for(int y=0;y<rows*24 && y+60<400;++y) for(int x=0;x<272;++x)
   *static_cast<byte *>(screen->getBasePtr(360+x,60+y))=*static_cast<const byte *>(menu.getBasePtr(x/2,y/2));
  menu.free();
  font->drawString(screen,"Click outside / Esc: close",364,MIN(384,64+rows*24),264,_textColor);
 }
 if(_gameOver) {
  screen->fillRect(Common::Rect(176,144,464,274),0);
  Graphics::Surface menu; menu.create(140,64,Graphics::PixelFormat::createFormatCLUT8());
  menu.fillRect(Common::Rect(0,0,140,64),0);
  font->drawString(&menu,"Game over",0,2,140,_textColor,Graphics::kTextAlignCenter);
  const char *options[]={"Load latest save", "Load autosave", "Load manual save", "Quit"};
  for(uint i=0;i<4;++i) font->drawString(&menu,options[i],0,17+i*12,140,_textColor,Graphics::kTextAlignCenter);
  for(int y=0;y<128;++y) for(int x=0;x<280;++x)
   *static_cast<byte *>(screen->getBasePtr(180+x,144+y))=*static_cast<const byte *>(menu.getBasePtr(x/2,y/2));
  menu.free();
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
bool PiceEngine::loadAnimation(const Common::String &name,int local,SpriteResource &sprite) {
 uint32 id; Common::Array<byte> bytes;
 return (matchesResourceGroup(name,"A02") || matchesResourceGroup(name,"A03") || matchesResourceGroup(name,"A06") || matchesResourceGroup(name,"A08") || matchesResourceGroup(name,"A09") || matchesResourceGroup(name,"A10") || matchesResourceGroup(name,"A11") || matchesResourceGroup(name,"A12")) &&
  resolveChapterGraphics(name,local,id) && _graphics.read(id,bytes) && sprite.load(bytes,id);
}
bool PiceEngine::background(const Common::String &name,int16 local,int16 flags) {
 // Explicit English EU subset: original name resolution prefixes S; SB02
 // spans 815..881. Other groups and nonzero flags await implementation.
 uint32 resource; if (!resolveChapterGraphics(name,local,resource) || flags) return false;
 Common::Array<byte> pixels; uint16 w,h;

 if (!_graphics.read(resource,pixels) || !decodeImageHeader(pixels,w,h) || w<640 || w>2048 || h>2048) return false;
 _scenePixels=pixels; _sceneBackground=resource;
 debug("[pice] script-background name=%s local=%d flags=%d resource=%u fnv=%08x",name.c_str(),local,flags,resource,hashBytes(pixels));
 return true;
}
bool PiceEngine::palette(const Common::String &name,int16 local) {
 uint32 resource; if (!resolveChapterGraphics(name,local,resource)) return false;
 Common::Array<byte> colors;
 if (!_graphics.read(resource,colors) || colors.size()!=768) return false;
 _scenePalette=colors; _scenePaletteId=resource;
 debug("[pice] script-palette name=%s local=%d resource=%u fnv=%08x",name.c_str(),local,resource,hashBytes(colors));
 return true;
}
bool PiceEngine::scenery(const Common::String &name,int16 local) {
 uint32 resource; if (!resolveChapterGraphics(name,local,resource)) return false;
 Common::Array<byte> data; SpriteResource candidate;
 if (!_graphics.read(resource,data) || !candidate.load(data,resource) || !candidate.hasSequence(0) || candidate.sequences.size()!=1 ||
     candidate.sequences[0].steps.size()!=1) return false;
 _scenery=candidate;
 debug("[pice] scenery resource=%u frame=%u parts=%u fnv=%08x",resource,candidate.sequences[0].steps[0].frame,
       candidate.frames[candidate.sequences[0].steps[0].frame].parts.size(),hashBytes(data));
 return true;
}
uint PiceEngine::actorScale(const RoomActor &actor) const {
 if(actor.animationMode==4 || actor.routeEnabled) return _navigation.scaleAt(actor.feet);
 if(actor.node>=0 && uint(actor.node)<_navigation.nodes.size() && _navigation.nodes[actor.node].active) return _navigation.nodes[actor.node].scale;
 return 100;
}
static int walkDirection(int dx,int dy) {
 // Original sprite order: N, NE, E, SE, S, SW, W, NW; screen Y grows down.
 int ax=ABS(dx),ay=ABS(dy);
 return ax*1000<ay*414 ? (dy>0 ? 4 : 0) : ay*1000<ax*414 ? (dx>0 ? 2 : 6) : dx>0 ? (dy>0 ? 3 : 1) : (dy>0 ? 5 : 7);
}
bool PiceEngine::startWalkLeg(RoomActor &actor) {
 if(actor.pathIndex>=actor.path.size()) { actor.animationMode=0; return true; }
 Common::Point target=actor.path[actor.pathIndex];
 int dx=target.x-actor.feet.x,dy=target.y-actor.feet.y,ax=ABS(dx),ay=ABS(dy);
 if(!ax && !ay) return false;
 int direction=walkDirection(dx,dy);
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
 if(!requestSave("f_IceTxt",2,"SAV00") || !_savePending) return false;
 _savePending=false;
 if(requestSave("f_IceTxt",2,"INVALID") || _savePending) return false;
 const int directions[][2]={{0,-10},{10,-10},{10,0},{10,10},{0,10},{-10,10},{-10,0},{-10,-10}};
 for(int i=0;i<8;++i) if(walkDirection(directions[i][0],directions[i][1])!=i) return false;
 if(walkDirection(2,12)!=4 || walkDirection(12,2)!=2) return false;

 ScriptVariables shared,local(&shared); int16 v;
 if(!shared.write(false,96,77) || !shared.write(true,17,16) || !local.read(true,17,v) || v!=16 ||
    !local.write(false,96,2) || !shared.read(false,96,v) || v!=77 ||
    !local.write(true,18,17) || !shared.read(true,18,v) || v!=17) return false;

 Common::Array<int16> closeupEntry;
 closeupEntry.push_back(_chapter); closeupEntry.push_back(_previousRoom); closeupEntry.push_back(3);
 closeupEntry.push_back(0); closeupEntry.push_back(2); closeupEntry.push_back(-1);
 int16 absentActor=42;
 if(_player!=-1 || !enterRoom("A03",closeupEntry,absentActor) || absentActor!=-1 || !_actors.empty()) return false;
 _player=0;
 if(enterRoom("A03",closeupEntry,absentActor)) return false;
 _player=-1;

 // Only the owning action's final unlock releases foreground input.
 _actionEntry=12; _actionWalking=false; _actionExitChapter=-1; _inputLocks=1;
 releaseActionInput(12); if(_actionEntry!=12) return false;
 _inputLocks=0; releaseActionInput(11); if(_actionEntry!=12) return false;
 _actionExitChapter=3; releaseActionInput(12); if(_actionEntry!=12) return false;
 _actionExitChapter=-1; releaseActionInput(12); if(_actionEntry!=-1) return false;
 Common::Array<int16> interruptionArgs; int16 interruptionResult;
 _actionContext=9; _actionEntry=7; _actionHotspot=3;
 if(!invoke(140,Common::String(),interruptionArgs,Common::String(),interruptionResult)) return false;
 _actionContext=-1; _actionEntry=-1; _actionHotspot=-1;
 if(!invoke(141,Common::String(),interruptionArgs,Common::String(),interruptionResult) || _actionContext!=9 || _actionEntry!=7 || _actionHotspot!=3 ||
    invoke(141,Common::String(),interruptionArgs,Common::String(),interruptionResult)) return false;
 _actionContext=_actionEntry=_actionHotspot=-1;
 _selectedItem=10; Common::Array<int16> noArgs; int16 deselectResult=-1;
 if(!invoke(124,Common::String(),noArgs,Common::String(),deselectResult) || _selectedItem!=-1 || deselectResult!=0) return false;
 Action itemAction; itemAction.verb=9; itemAction.item=2;
 if(!itemAction.matches(9,2) || itemAction.matches(9,3) || itemAction.matches(6,2)) return false;
 itemAction.verb=6; if(!itemAction.matches(6,-1) || itemAction.matches(1,-1)) return false;

 // Removing a middle slot must preserve all remaining IDs, invalidate its
 // associations and permit reuse without increasing the 20-actor limit.
 for(uint i=0;i<20;++i) { RoomActor actor; actor.feet.x=i; if(storeActor(actor)!=int(i)) return false; }
 if(storeActor(RoomActor())!=-1) return false;
 int16 coordinate;
 _actors[0].feet=Common::Point(-12,345);
 if(!actorCoordinate(0,false,coordinate) || coordinate!=-12 || !actorCoordinate(0,true,coordinate) || coordinate!=345 ||
    actorCoordinate(-1,false,coordinate) || actorCoordinate(20,false,coordinate)) return false;
 _actors[0].active=false; if(actorCoordinate(0,false,coordinate)) return false; _actors[0].active=true;
 const uint16 coordinateWords[]={1,9,73,20,0,20,300,9,74,20,0,20,300};
 Common::Array<byte> coordinateBytes; coordinateBytes.resize(sizeof(coordinateWords));
 for(uint i=0;i<sizeof(coordinateWords)/sizeof(coordinateWords[0]);++i) WRITE_LE_UINT16(&coordinateBytes[i*2],coordinateWords[i]);
 ScriptResource coordinateScript; uint32 coordinatePC=2;
 if(!coordinateScript.load(coordinateBytes) || !coordinateScript.expression(coordinatePC,coordinate,nullptr,this) || coordinate || coordinatePC!=14 ||
    !coordinateScript.expression(coordinatePC,coordinate,nullptr,this) || coordinate!=1 || coordinatePC!=sizeof(coordinateWords)) return false;

 Hotspot spot; spot.actor=7; _hotspots.push_back(spot);
 AnimationSound binding; binding.actor=7; _animationSounds.push_back(binding);
 binding.actor=8; _animationSounds.push_back(binding);
 WorldItem item; item.actor=7; _worldItems.push_back(item); _player=7;
 Common::Array<int16> args; args.push_back(7); int16 result;
 if(!invoke(83,Common::String(),args,Common::String(),result) || result!=7 || _actors.size()!=20 ||
    _actors[7].active || _actors[8].feet.x!=8 || _hotspots[0].active || _worldItems[0].actor!=-1 ||
    _animationSounds.size()!=2 || _animationSounds[0].actor!=-1 || _animationSounds[1].actor!=8 || _player!=-1 || actorBusy(7) ||
    invoke(83,Common::String(),args,Common::String(),result) || storeActor(RoomActor())!=7) return false;
 args[0]=0;
 if(!invoke(115,Common::String(),args,Common::String(),result) || result!=0 || _animationSounds[1].actor!=8) return false;
 args[0]=30; if(invoke(115,Common::String(),args,Common::String(),result)) return false;
 args[0]=1;
 if(!invoke(115,Common::String(),args,Common::String(),result) || _animationSounds.size()!=2 || _animationSounds[1].actor!=-1) return false;
 _sounds.push_back(RoomSound());
 args.clear(); args.push_back(8); args.push_back(0); args.push_back(100); args.push_back(-1); args.push_back(0);
 if(!invoke(109,Common::String(),args,Common::String(),result) || result!=0 || _animationSounds.size()!=2) return false;
 if(!invoke(109,Common::String(),args,Common::String(),result) || result!=1 || _animationSounds.size()!=2) return false;
 _sounds.push_back(RoomSound()); _animationSounds[0].fired=true;
 args.clear(); args.push_back(0); args.push_back(1); args.push_back(50);
 if(!invoke(116,Common::String(),args,Common::String(),result) || result!=0 ||
    _animationSounds[0].sound!=1 || _animationSounds[0].volume!=50 || !_animationSounds[0].fired ||
    _animationSounds[0].actor!=8 || _animationSounds[0].sequence!=-1 || _animationSounds[0].step!=0 ||
    _animationSounds[1].sound!=0) return false;
 args[1]=-1; args[2]=-1;
 if(!invoke(116,Common::String(),args,Common::String(),result) || _animationSounds[0].sound!=1 || _animationSounds[0].volume!=50) return false;
 args[1]=0; args[2]=101;
 if(invoke(116,Common::String(),args,Common::String(),result) || _animationSounds[0].sound!=1) return false;
 args[1]=2; args[2]=25;
 if(invoke(116,Common::String(),args,Common::String(),result) || _animationSounds[0].volume!=50) return false;
 _sounds.clear();
 _actors.clear(); _hotspots.clear(); _animationSounds.clear(); _worldItems.clear();
 _actors.push_back(RoomActor()); _items.push_back(RoomActor()); _ownedItems.resize(1,0);
 args.clear(); args.push_back(0);
 WorldItem pickup; pickup.actor=0; pickup.item=0; _worldItems.push_back(pickup);
 Hotspot pickupSpot; pickupSpot.actor=0; pickupSpot.label="Test key"; _hotspots.push_back(pickupSpot);
 args[0]=0;
 if(!invoke(67,Common::String(),args,Common::String(),result) || result!=0 || !_ownedItems[0] ||
    _actors[0].active || _hotspots[0].active || _worldItems[0].actor!=-1 ||
    !invoke(67,Common::String(),args,Common::String(),result)) return false;
 if(_itemLabels.size()!=1 || _itemLabels[0]!="Test key") return false;
 args[0]=1; if(invoke(67,Common::String(),args,Common::String(),result) || !_ownedItems[0]) return false;
 _hotspots[0].active=true;
 args.clear(); args.push_back(0); args.push_back(0); args.push_back(-1); args.push_back(11);
 if(!invoke(81,Common::String(),args,Common::String(),result) || result!=0 || _actions.size()!=1 ||
    !_actions[0].matches(9,0) || _actions[0].matches(9,1) || _actions[0].entry!=11 || _actions[0].approach!=-1) return false;
 args[1]=1; if(invoke(81,Common::String(),args,Common::String(),result) || _actions.size()!=1) return false;
 args[0]=-1;
 if(!invoke(81,Common::String(),args,Common::String(),result) || result!=-1 || _actions.size()!=1) return false;
 args.clear(); args.push_back(-1); args.push_back(10);
 if(!invoke(110,Common::String(),args,Common::String(),result) || result!=-1 || _actions.size()!=1) return false;
 // Fixed-capacity hotspot references may be dormant, but never out of range.
 args.clear(); args.push_back(49); args.push_back(6); args.push_back(-1); args.push_back(1);
 if(!invoke(58,Common::String(),args,Common::String(),result) || result!=1 ||
    _actions.size()!=2 || _hotspots.size()!=1 || _actions[1].hotspot!=49) return false;
 args[0]=50; if(invoke(58,Common::String(),args,Common::String(),result) || _actions.size()!=2) return false;
 args[0]=-2; if(invoke(58,Common::String(),args,Common::String(),result) || _actions.size()!=2) return false;
 args.clear(); args.push_back(0);
 if(!invoke(84,Common::String(),args,Common::String(),result) || result!=0 || _actions.size()!=2 ||
    _actions[0].hotspot!=-1 || _actions[1].hotspot!=49) return false;
 args[0]=2; if(invoke(84,Common::String(),args,Common::String(),result)) return false;
 args.clear(); args.push_back(1); args.push_back(6); args.push_back(-1); args.push_back(13);
 if(!invoke(58,Common::String(),args,Common::String(),result) || result!=0 || _actions.size()!=2 ||
    _actions[0].entry!=13 || _actions[1].hotspot!=49) return false;
 _actions.clear();
 Common::Array<Common::String> choices; choices.push_back("A"); choices.push_back("hidden"); choices.push_back("C");
 _menus.push_back(choices); Common::Array<byte> enabled; enabled.resize(3,0); _menuEnabled.push_back(enabled);
 args.clear(); args.push_back(0); args.push_back(0);
 if(!invoke(68,Common::String(),args,Common::String(),result)) return false;
 args[1]=2; if(!invoke(68,Common::String(),args,Common::String(),result)) return false;
 bool ready=false;
 if(!pollMenu(0,ready) || ready || clickMenu(Common::Point(359,61)) || clickMenu(Common::Point(370,120)) ||
    !clickMenu(Common::Point(370,85)) || !pollMenu(0,ready) || !ready || _lastMenuChoice!=2) return false;
 if(!pollMenu(0,ready) || ready) return false;
 _menuChoice=-2; if(!pollMenu(0,ready) || !ready || _lastMenuChoice!=-1 || _activeMenu!=-1) return false;
 if(!invoke(69,Common::String(),args,Common::String(),result) || _menuEnabled[0][2]) return false;
 args[1]=3; if(invoke(68,Common::String(),args,Common::String(),result)) return false;
 // Scripted close must cancel the next menu poll instead of reopening it.
 if(!invoke(99,Common::String(),noArgs,Common::String(),result) || _activeMenu!=-1 || _lastMenuChoice!=-1 ||
    !pollMenu(0,ready) || !ready || _activeMenu!=-1 || _lastMenuChoice!=-1) return false;
 if(!pollMenu(0,ready) || ready || _activeMenu!=0) return false;
 _menus.clear(); _menuEnabled.clear(); _activeMenu=_menuChoice=_lastMenuChoice=-1;
 int16 ownedValue;
 if(!stateValue(2,0,ownedValue) || !ownedValue || stateValue(2,-1,ownedValue) || stateValue(2,1,ownedValue) || stateValue(3,0,ownedValue)) return false;
 _ownedItems[0]=0; if(!stateValue(2,0,ownedValue) || ownedValue) return false;
 // Task-state predicates distinguish inactive, waiting and finished tasks.
 const uint16 taskWords[]={1,22,20,1,31,20,0};
 Common::Array<byte> taskBytes; taskBytes.resize(sizeof(taskWords));
 for(uint i=0;i<sizeof(taskWords)/sizeof(taskWords[0]);++i) WRITE_LE_UINT16(&taskBytes[i*2],taskWords[i]);
 if(!_roomBank.load(taskBytes)) return false;
 _roomRunner.reset(new ScriptRunner(_roomBank,&_worldVariables,this));
 if(!stateValue(3,0,ownedValue) || ownedValue || stateValue(3,-1,ownedValue) || stateValue(3,1,ownedValue) ||
    !_roomRunner->start(0) || !stateValue(3,0,ownedValue) || ownedValue!=1 ||
    _roomRunner->step(0)!=ScriptRunner::kWaiting || !stateValue(3,0,ownedValue) || ownedValue!=1) return false;
 _roomRunner->step(0,2);
 if(_roomRunner->step(0)!=ScriptRunner::kStopped || !stateValue(3,0,ownedValue) || ownedValue) return false;
 _roomRunner.reset(); _roomBank=ScriptResource();

 args.clear(); args.push_back(0); args.push_back(26); args.push_back(20); args.push_back(2); args.push_back(8);
 if(!invoke(80,Common::String(),args,Common::String(),result) || result!=0 || _actions.back().exitChapter!=2 ||
    _actions.back().exitRoom!=8 || _actions.back().entry!=20 || _actions.back().approach!=26 || _actions.back().verb!=3) return false;
 uint count=_actions.size(); args[4]=40;
 if(invoke(80,Common::String(),args,Common::String(),result) || _actions.size()!=count) return false;
 args[2]=-1; args[4]=7;
 if(!invoke(80,Common::String(),args,Common::String(),result) || _actions.back().entry!=-1) return false;
 // Item -1 is a valid fallback-only action, not a wildcard item match.
 args.clear(); args.push_back(0); args.push_back(-1); args.push_back(-1); args.push_back(10);
 if(!invoke(81,Common::String(),args,Common::String(),result) || _actions.back().item!=-1 ||
    _actions.back().matches(9,0) || _actions.back().matches(6,-1)) return false;
 uint dormantCount=_actions.size(); args[1]=-2;
 if(invoke(81,Common::String(),args,Common::String(),result) || _actions.size()!=dormantCount) return false;
 // Releasing a background must not reset an independently running fade.
 _scenePixels.resize(16,1); _fadeLevel=27;
 args.clear(); args.push_back(10); args.push_back(0); args.push_back(0); args.push_back(0);
 if(!invoke(95,Common::String(),args,Common::String(),result) || result!=10 ||
    !_scenePixels.empty() || _fadeLevel!=27) return false;
 _fadeLevel=-1;
 _ownedItems.resize(4,0); _ownedItems[0]=1; _ownedItems[3]=1;
 if(inventoryAt(Common::Point(10,10))!=0 || inventoryAt(Common::Point(60,10))!=3 ||
    inventoryAt(Common::Point(110,10))!=-1 || inventoryAt(Common::Point(10,40))!=-1 || inventoryAt(Common::Point(-1,10))!=-1) return false;
 _itemLabels.clear();
 // Tall-room camera clamps at both ends; hit testing uses world Y.
 _scenePixels.resize(4); WRITE_LE_UINT16(&_scenePixels[0],640); WRITE_LE_UINT16(&_scenePixels[2],632);
 _actors.resize(1); _cameraActor=0; _actors[0].feet.y=80;
 if(cameraY()!=0) return false;
 _actors[0].feet.y=396; if(cameraY()!=156) return false;
 _actors[0].feet.y=654; if(cameraY()!=312) return false;
 _hotspots.clear(); Hotspot tall; tall.label="test"; tall.bounds=Common::Rect(100,650,120,670); _hotspots.push_back(tall);
 if(hotspotAt(Common::Point(110,308))!=0 || hotspotAt(Common::Point(110,20))!=-1) return false;
 _actions.clear(); Hotspot trigger; trigger.bounds=tall.bounds; _hotspots.insert_at(0,trigger);
 if(hotspotAt(Common::Point(110,308))!=1) return false;
 Action triggerAction; triggerAction.hotspot=0; _actions.push_back(triggerAction);
 if(hotspotAt(Common::Point(110,308))!=0) return false;
 _actions.clear(); _hotspots.remove_at(0);
 // The entire visible scene ends at y360; its bottom 22 pixels contain
 // pickable objects such as the weapon butt. Captions begin below it.
 int savedPlayer=_player; _player=0;
 _hotspots[0].bounds=Common::Rect(100,690,120,720);
 if(hotspotAt(Common::Point(110,338))!=0 || hotspotAt(Common::Point(110,359))!=0 ||
    hotspotAt(Common::Point(110,360))!=-1) return false;
 _hotspots[0].bounds=tall.bounds; _player=savedPlayer;
 // Hold the viewport, let explicit recenter override the freeze, then release.
 _cameraRecenter=false; _actors[0].active=true;
 args.clear(); args.push_back(1);
 if(!invoke(119,Common::String(),args,Common::String(),result)) return false;
 _actors[0].feet.y=396; if(cameraY()!=312) return false;
 args[0]=0;
 if(!invoke(139,Common::String(),args,Common::String(),result) || cameraY()!=156) return false;
 _cameraPosition=Common::Point(cameraX(),cameraY()); _cameraRecenter=false;
 _actors[0].feet.y=80; if(cameraY()!=156) return false;
 if(!invoke(119,Common::String(),args,Common::String(),result) || cameraY()!=0) return false;
 _cameraActor=-1;
 if(!invoke(56,Common::String(),args,Common::String(),result) || _cameraActor!=0 || !_cameraRecenter) return false;
 _cameraRecenter=false; _player=-1;
 _actors.resize(2); _actors[1].active=true; args[0]=1;
 if(!invoke(142,Common::String(),args,Common::String(),result) || _cameraActor!=0 || _cameraRecenter) return false;
 // Sequence -1 cancels movement while retaining the actor and facing.
 _actors[0].animationMode=4; _actors[0].walkDirection=2; _actors[0].sequence=10;
 _actors[0].path.push_back(Common::Point(100,100)); _actors[0].walkDistance=20;
 args.clear(); args.push_back(0); args.push_back(-1);
 _player=0; _actionWalking=true; _actionEntry=15; _actionHotspot=13;
 if(!invoke(51,Common::String(),args,Common::String(),result) || actorBusy(0) ||
    !_actors[0].active || _actors[0].sequence!=2 || !_actors[0].path.empty() || _actors[0].walkDistance) return false;
 if(_actionWalking || _actionEntry!=-1 || _actionHotspot!=-1) return false;
 _actionEntry=15;
 if(!invoke(51,Common::String(),args,Common::String(),result) || _actors[0].sequence!=2 || _actionEntry!=15) return false;
 _actionEntry=-1; _player=-1;
 // Preloaded animations exchange ownership, preserve the old resource in
 // the slot, reject bad sequences without mutation and can swap back.
 _actors[0].sprite.archiveId=7; _actors[0].resource=7;
 _actors[0].sprite.sequences.resize(1); _actors[0].sprite.sequences[0].steps.resize(1);
 SpriteResource preload; preload.archiveId=8; preload.sequences.resize(1); preload.sequences[0].steps.resize(1);
 _preloadedAnimations.push_back(preload);
 args.clear(); args.push_back(0); args.push_back(0); args.push_back(1); args.push_back(12); args.push_back(34);
 if(invoke(146,Common::String(),args,Common::String(),result) || _actors[0].resource!=7 || _preloadedAnimations[0].archiveId!=8) return false;
 args[2]=0;
 if(!invoke(146,Common::String(),args,Common::String(),result) || _actors[0].resource!=8 ||
    _preloadedAnimations[0].archiveId!=7 || _actors[0].feet!=Common::Point(12,34) || !actorBusy(0)) return false;
 if(!invoke(146,Common::String(),args,Common::String(),result) || _actors[0].resource!=7 || _preloadedAnimations[0].archiveId!=8) return false;
 _preloadedAnimations.clear(); args.resize(2);
 // A disconnected but valid destination completes without crossing a wall,
 // cancels an old walk, and leaves malformed node references as errors.
 _navigation.clear(); _navigation.node(0,0,0,100); _navigation.node(1,100,0,100);
 _navigation.edge(0,0,1,0);
 _actors[0].feet=Common::Point(0,0); _actors[0].routeEnabled=true;
 _actors[0].animationMode=4; _actors[0].sequence=10; _actors[0].walkDirection=2;
 _actors[0].path.push_back(Common::Point(20,0)); _actors[0].walkDistance=20;
 args[1]=1;
 if(!invoke(50,Common::String(),args,Common::String(),result) || result!=0 || actorBusy(0) ||
    _actors[0].feet!=Common::Point(0,0) || !_actors[0].path.empty() || _actors[0].sequence!=2 || _navigation.edges[0].enabled) return false;
 args[1]=2; if(invoke(50,Common::String(),args,Common::String(),result)) return false;
 _navigation.clear();
 WRITE_LE_UINT16(&_scenePixels[2],316); if(cameraY()!=0) return false;
 _cameraActor=-1; _scenePixels.clear();
 _actions.clear(); _actors.clear(); _items.clear(); _ownedItems.clear(); _hotspots.clear(); _worldItems.clear(); return true;
}
int PiceEngine::storeAction(const Action &action) {
 // Original 0x83cb reuses the first inactive slot, preserving all other IDs.
 for(uint i=0;i<_actions.size();++i) if(_actions[i].hotspot==-1) { _actions[i]=action; return i; }
 if(_actions.size()>=70) return -1;
 _actions.push_back(action); return _actions.size()-1;
}
bool PiceEngine::invoke(uint16 op,const Common::String &name,const Common::Array<int16> &args,const Common::String &key,int16 &result) {
 if(op==99) {
  // 0xd0f -> 0x94da: close menu, clear context 6, wake its waiter.
  if(!args.empty()) return false;
  _activeMenu=-1; _menuChoice=-2; _lastMenuChoice=-1; result=0; return true;
 }
 if(op==124) {
  // ICE640 0x1379 clears the selected inventory ID at 0x15c4.
  if(!args.empty()) return false;
  _selectedItem=-1; result=0; return true;
 }
 result=-1;
 if(op==83 || op==147 || op==142 || op==98 || op==50 || op==123 || op==51 || op==96 || op==117 || op==97 || op==56 || op==92) {
  if(args.empty() || args[0]<0 || uint(args[0])>=_actors.size() || !_actors[args[0]].active) return false;
 }
 if(op==83) {
  if(args.size()!=1 || _dialogueActor==args[0]) return false;
  int16 id=args[0];
  for(uint i=_animationSounds.size();i>0;--i) if(_animationSounds[i-1].actor==id) _animationSounds[i-1]=AnimationSound();
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
  _itemLabels.resize(_items.size());
  for(uint h=0;h<_hotspots.size();++h) for(uint w=0;w<_worldItems.size();++w)
   if(_itemLabels[id].empty() && _worldItems[w].item==int(id) && _worldItems[w].actor>=0 && _hotspots[h].actor==_worldItems[w].actor) _itemLabels[id]=_hotspots[h].label;
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
  int base=name.equalsIgnoreCase("MUS02") ? 308 : name.equalsIgnoreCase("MUS03") ? 314 : name.equalsIgnoreCase("MUS06") ? 331 : name.equalsIgnoreCase("MUS08") ? 340 : name.equalsIgnoreCase("MUS09") ? 342 : name.equalsIgnoreCase("MUS10") ? 346 : name.equalsIgnoreCase("MUS11") ? 349 : name.equalsIgnoreCase("MUS12") ? 350 : -1;
  if(args.size()!=1 || base<0 || args[0]<0 || args[0]>=(base==308 ? 6 : base==314 ? 17 : base==331 ? 9 : base==340 ? 2 : base==342 ? 4 : base==346 ? 3 : base==349 ? 1 : 3)) return false;
  Common::Array<byte> data; uint32 offset,size;
  if(!_soundArchive.read(base+args[0],data) || !voicePayload(data,offset,size)) return false;
  _mixer->stopHandle(_music.handle); _music=RoomSound(); _music.active=true; _music.persistent=true; _music.resource=base+args[0];
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
 if(op==149) {
  if(args.size()!=1 || (args[0]!=0 && args[0]!=1)) return false;
  _inventoryEnabled=args[0]!=0; result=0; return true;
 }
 if(op==64) {
  if(args.size()!=3 || args[0]<0 || args[1]<0 || uint(args[0])>=_items.size() || uint(args[1])>=_items.size() || args[2]<0 || _itemPairs.size()>=10) return false;
  ItemPair pair; pair.first=args[0]; pair.second=args[1]; pair.entry=args[2];
  result=_itemPairs.size(); _itemPairs.push_back(pair); return true;
 }
 if(op==30) {
  // 0x2615 -> 0x14f6a: update sample volume and return the percentage.
  if(args.size()!=2 || args[0]<0 || uint(args[0])>=_sounds.size() || !_sounds[args[0]].active || args[1]<0 || args[1]>100) return false;
  RoomSound &sound=_sounds[args[0]]; sound.volume=(uint(args[1])*255+50)/100;
  _mixer->setChannelVolume(sound.handle,sound.volume); result=args[1]; return true;
 }
 if(op==133 || op==29) {
  int base=name.equalsIgnoreCase("SMP02") ? 45 : name.equalsIgnoreCase("SMP03") ? 113 : name.equalsIgnoreCase("SMP06") ? 165 : name.equalsIgnoreCase("SMP08") ? 196 : name.equalsIgnoreCase("SMP09") ? 222 : name.equalsIgnoreCase("SMP10") ? 244 : name.equalsIgnoreCase("SMP11") ? 261 : name.equalsIgnoreCase("SMP12") ? 277 : -1;
  if(args.size()!=1 || base<0 || args[0]<0 || args[0]>=(base==45 ? 68 : base==113 ? 52 : base==165 ? 31 : base==196 ? 26 : base==222 ? 22 : base==244 ? 17 : base==261 ? 16 : 28)) return false;
  uint slot=0; while(slot<_sounds.size() && _sounds[slot].active) ++slot;
  if(slot>=25) return false;
  Common::Array<byte> data; uint32 offset,size;
  if(!_soundArchive.read(base+args[0],data) || !voicePayload(data,offset,size)) return false;
  RoomSound sound; sound.active=true; sound.persistent=op==133; sound.resource=base+args[0]; result=slot;
  if(slot==_sounds.size()) _sounds.push_back(sound); else _sounds[slot]=sound;
  debug("[pice] load-sound id=%d resource=%u",result,sound.resource); return true;
 }
 if(op==101) {
  if(args.size()!=1 || args[0]<0 || uint(args[0])>=_sounds.size() || !_sounds[args[0]].active) return false;
  _mixer->stopHandle(_sounds[args[0]].handle); _sounds[args[0]]=RoomSound();
  for(uint i=_animationSounds.size();i>0;--i) if(_animationSounds[i-1].sound==args[0]) _animationSounds[i-1]=AnimationSound();
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
 if(op==153) {
  if(args.size()!=1 || (args[0]!=0 && args[0]!=1)) return false;
  _playerMovement=args[0]!=0; result=0; return true;
 }
 if(op==140) {
  if(!args.empty()) return false;
  InterruptedAction saved;
  saved.valid=true; saved.context=_actionContext; saved.entry=_actionEntry; saved.hotspot=_actionHotspot;
  saved.walking=_actionWalking; saved.exitChapter=_actionExitChapter; saved.exitRoom=_actionExitRoom; saved.actor=_player;
  if(_player>=0 && uint(_player)<_actors.size()) {
   const RoomActor &a=_actors[_player];
   saved.resumeWalk=a.active && a.animationMode==4 && !a.path.empty();
   if(saved.resumeWalk) saved.destination=a.path.back();
  }
  _interruptedAction=saved; result=0; return true;
 }
 if(op==141) {
  if(!args.empty() || !_interruptedAction.valid) return false;
  const InterruptedAction saved=_interruptedAction;
  if(saved.resumeWalk) {
   Common::Array<int16> walk; walk.push_back(saved.actor); walk.push_back(saved.destination.x); walk.push_back(saved.destination.y);
   int16 ignored; if(!invoke(123,Common::String(),walk,Common::String(),ignored)) return false;
  }
  _actionContext=saved.context; _actionEntry=saved.entry; _actionHotspot=saved.hotspot; _actionWalking=saved.walking;
  _actionExitChapter=saved.exitChapter; _actionExitRoom=saved.exitRoom;
  _interruptedAction=InterruptedAction(); result=0;
  debug("[pice] restored interrupted action entry=%d resume-walk=%d",_actionEntry,int(saved.resumeWalk)); return true;
 }
 if(op==119) {
  if(args.size()!=1) return false;
  _cameraPosition=Common::Point(cameraX(),cameraY());
  _cameraFrozen=args[0]==1; result=0; return true;
 }
 if(op==139 || op==142) {
  if(args.size()!=1 || args[0]<0 || uint(args[0])>=_actors.size() || !_actors[args[0]].active) return false;
  // 139 requests an immediate recenter (0x1c50 -> 0x3775). Our
  // actor-centered camera has no easing, so selection recenters immediately.
  // 142 only waits for this actor; it does not select a camera target.
  if(op==139) { _cameraActor=args[0]; _cameraRecenter=true; }
  result=0; return true;
 }
 if(op==98) {
  if(args.size()!=1 || args[0]<0 || uint(args[0])>=_actors.size() || _actors[args[0]].animationMode==4) return false;
  if(_actors[args[0]].animationMode) _actors[args[0]].animationMode=1;
  result=0; return true;
 }
 if((op==50 || op==123) && args.size()==(op==50 ? 2U : 3U) && _player>=0 && args[0]==_player && !_playerMovement) { result=args[0]; return true; }
 if(op==50 || op==123) {
  if(args.size()!=(op==50 ? 2U : 3U) || args[0]<0 || uint(args[0])>=_actors.size() || (op==50 && (args[1]<0 || uint(args[1])>=_navigation.nodes.size() ||
     !_navigation.nodes[args[1]].active || !_navigation.nodes[args[1]].scale || _navigation.nodes[args[1]].scale>400))) return false;
  RoomActor actor=_actors[args[0]];
  if(!actor.routeEnabled) { warning("[pice] walk routing disabled actor=%d",args[0]); return false; }
  Common::Point target=op==50 ? Common::Point(_navigation.nodes[args[1]].x,_navigation.nodes[args[1]].y) : Common::Point(args[1],args[2]);
  if(op==123) { Common::Point projected; if(!_navigation.nearest(target,projected)) return false; target=projected; }
  Common::Point floorStart;
  bool atNode=false;
  for(uint i=0;i<_navigation.nodes.size();++i) if(_navigation.nodes[i].active && actor.feet==Common::Point(_navigation.nodes[i].x,_navigation.nodes[i].y)) atNode=true;
  if(atNode) floorStart=actor.feet;
  else if(!_navigation.nearest(actor.feet,floorStart)) return false;
  bool projectedTarget=false;
  if(!_navigation.route(floorStart,target,actor.path)) {
   // Original node walks resolve coordinates through the same floor planner
   // as coordinate walks (0x2bbc -> 0x1b687 -> 0x6a52). Off-floor door nodes
   // may only have disabled links; approach their nearest walkable point.
   Common::Point projected;
   if(op!=50 || _navigation.region(target)>=0 || !_navigation.nearest(target,projected) || projected==target ||
      !_navigation.route(floorStart,projected,actor.path)) {
    // Original 0x1b794..0x1b808 clears movement when the planner has no
    // route; opcode50/123 still completes. Do not turn a blocked walk into
    // a script failure or bypass disabled links (e.g. prison cell walls).
    if(actor.animationMode==4) { actor.sequence=actor.walkDirection; actor.step=0; }
    actor.path.clear(); actor.pathIndex=0; actor.walkProgress=actor.walkDistance=0;
    actor.animationMode=0; actor.animationPaused=false; actor.animationOrigin=actor.feet;
    _actors[args[0]]=actor; result=args[0];
    debug("[pice] walk blocked actor=%d from=%d,%d target=%d,%d; standing",args[0],actor.feet.x,actor.feet.y,target.x,target.y);
    return true;
   }
   debug("[pice] walk target projected node=%d from=%d,%d to=%d,%d",args[1],target.x,target.y,projected.x,projected.y);
   target=projected; projectedTarget=true;
  }
  if(actor.feet!=floorStart) actor.path.insert_at(0,floorStart);
  actor.pathIndex=0; actor.node=op==50 && !projectedTarget ? args[1] : -1;
  RoomActor validation=actor;
  for(uint i=0;i<actor.path.size();++i) {
   validation.pathIndex=i;
   if(!startWalkLeg(validation)) { warning("[pice] walk leg invalid actor=%d resource=%d from=%d,%d to=%d,%d",args[0],actor.resource,validation.feet.x,validation.feet.y,actor.path[i].x,actor.path[i].y); return false; }
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
 if(op==87) {
  // Original 0x93e: assign absolute coordinates, clear the node, refresh
  // the current sequence with its existing frame, and wait for the actor.
  if(args.size()!=3 || args[0]<0 || uint(args[0])>=_actors.size() || !_actors[args[0]].active) return false;
  RoomActor &actor=_actors[args[0]];
  if(actor.animationMode==3 || !actor.sprite.hasSequence(actor.sequence)) return false;
  actor.feet=Common::Point(args[1],args[2]); actor.node=-1;
  if(actor.step>=actor.sprite.sequences[actor.sequence].steps.size()) actor.step=0;
  actor.frameTicks=MAX<uint16>(1,actor.sprite.sequences[actor.sequence].steps[actor.step].ticks);
  actor.animationMode=1; actor.animationOrigin=actor.feet; actor.animationPaused=false;
  actor.path.clear(); actor.pathIndex=0; actor.walkDistance=actor.walkProgress=0;
  result=-1; return true;
 }
 if(op==88) {
  // 0x9ab: relocate to a navigation node and restart the current sequence.
  const RoomNavigation &nav=_roomRunner ? _roomRunner->navigation() : _navigation;
  if(args.size()!=2 || args[0]<0 || uint(args[0])>=_actors.size() || !_actors[args[0]].active ||
     args[1]<0 || uint(args[1])>=nav.nodes.size() || !nav.nodes[args[1]].active ||
     !nav.nodes[args[1]].scale || nav.nodes[args[1]].scale>400) return false;
  RoomActor &actor=_actors[args[0]];
  if(actor.animationMode==3 || !actor.sprite.hasSequence(actor.sequence)) return false;
  actor.node=args[1]; actor.feet=Common::Point(nav.nodes[args[1]].x,nav.nodes[args[1]].y);
  actor.step=0; actor.frameTicks=MAX<uint16>(1,actor.sprite.sequences[actor.sequence].steps[0].ticks);
  actor.animationMode=1; actor.animationOrigin=actor.feet; actor.animationPaused=false;
  debug("[pice] relocate actor=%d node=%d at=%d,%d",args[0],actor.node,actor.feet.x,actor.feet.y);
  result=-1; return true;
 }
 if(op==51 || op==96 || op==117) {
  uint count=op==51 ? 2 : op==96 ? 4 : 5;
  if(args.size()!=count || args[0]<0 || uint(args[0])>=_actors.size()) return false;
  RoomActor actor=_actors[args[0]];
  if(op==51 && args[1]==-1) {
   // Original 0x2c85 -> 0x1b654 cancels walking rather than selecting -1.
   if(actor.animationMode==3) return false;
   if(actor.animationMode==4) { actor.sequence=actor.walkDirection; actor.step=0; }
   actor.path.clear(); actor.pathIndex=0; actor.walkProgress=actor.walkDistance=0;
   actor.animationMode=0; actor.animationPaused=false; actor.animationOrigin=actor.feet;
   _actors[args[0]]=actor; cancelPendingApproach(args[0]); result=args[0];
   debug("[pice] actor movement stopped actor=%d sequence=%d at=%d,%d",args[0],actor.sequence,actor.feet.x,actor.feet.y);
   return true;
  }
  if(op==51) {
   if(actor.animationMode==3 || args[1]<0 || !actor.sprite.hasSequence(args[1])) return false;
   actor.sequence=args[1];
  } else {
   if(args[1]<0 || args[2]<0) return false;
   Common::Array<byte> data;
   if(!loadAnimation(name,args[1],actor.sprite) || !actor.sprite.hasSequence(args[2])) return false;
   actor.resource=actor.sprite.archiveId; actor.sequence=args[2];
   if(op==117) { actor.node=-1; actor.feet=Common::Point(args[3],args[4]); }
   else if(args[3]!=-1) {
    const RoomNavigation &nav=_roomRunner ? _roomRunner->navigation() : _navigation;
    if(args[3]<0 || uint(args[3])>=nav.nodes.size() || !nav.nodes[args[3]].active || !nav.nodes[args[3]].scale || nav.nodes[args[3]].scale>400) return false;
    actor.node=args[3]; actor.feet=Common::Point(nav.nodes[args[3]].x,nav.nodes[args[3]].y);
   }
  }
  actor.step=0; actor.frameTicks=actor.sprite.sequences[actor.sequence].steps[0].ticks; actor.animationMode=1; actor.animationOrigin=actor.feet; actor.animationPaused=false;
  _actors[args[0]]=actor; if(op==51) cancelPendingApproach(args[0]); result=op==51 ? args[0] : actor.sequence;
  debug("[pice] actor-animation actor=%d resource=%d sequence=%d at=%d,%d",args[0],actor.resource,actor.sequence,actor.feet.x,actor.feet.y); return true;
 }
 if(op==60) {
  const RoomNavigation *nav=_buildingNavigation ? _buildingNavigation : (_roomRunner ? &_roomRunner->navigation() : nullptr);
  if(args.size()!=2 || args[0]<0 || uint(args[0])>=_items.size() || !nav || _worldItems.size()>=10 ||
     args[1]<0 || uint(args[1])>=nav->nodes.size() || !nav->nodes[args[1]].active) return false;
  if(uint(args[0])<_ownedItems.size() && _ownedItems[args[0]]) { result=-1; return true; } // Original 0x3032 skips owned objects.
  RoomActor actor=_items[args[0]]; actor.sequence+=2;
  const RoomNode &node=nav->nodes[args[1]];
  if(!actor.sprite.hasSequence(actor.sequence) || !node.scale || node.scale>400) return false;
  actor.node=args[1]; actor.feet=Common::Point(node.x,node.y);
  WorldItem item; item.item=args[0];
  result=_worldItems.size(); actor.animationMode=1; actor.animationOrigin=actor.feet; actor.frameTicks=MAX<uint16>(1,actor.sprite.sequences[actor.sequence].steps[0].ticks); item.actor=storeActor(actor); if(item.actor<0) return false; _worldItems.push_back(item); return true;
 }
 if(op==61 && args.size()==3 && args[0]==-1) { result=-1; return true; }
 if(op==143) {
  // Original 0x1cbe -> 0x1a907 allocates one of five off-screen resources.
  if(args.size()!=1) return false;
  uint slot=0;
  while(slot<_preloadedAnimations.size() && _preloadedAnimations[slot].archiveId>=0) ++slot;
  SpriteResource sprite;
  if(slot>=5 || !loadAnimation(name,args[0],sprite)) return false;
  if(slot==_preloadedAnimations.size()) _preloadedAnimations.push_back(sprite);
  else _preloadedAnimations[slot]=sprite;
  result=slot; debug("[pice] animation preload slot=%u resource=%d",slot,sprite.archiveId); return true;
 }
 if(op==146) {
  // Original 0x1d9b -> 0x1ac83 swaps the actor resource with the preload
  // slot and starts a finite sequence at absolute coordinates. Scheduler
  // state 12 (0x4d6) waits for completion; it does not swap back.
  if(args.size()!=5 || args[0]<0 || uint(args[0])>=_actors.size() || !_actors[args[0]].active ||
     args[1]<0 || uint(args[1])>=_preloadedAnimations.size()) return false;
  RoomActor &actor=_actors[args[0]];
  SpriteResource &preload=_preloadedAnimations[args[1]];
  if(actor.animationMode==3 || preload.archiveId<0 || !preload.hasSequence(args[2])) return false;
  SpriteResource previous=actor.sprite;
  actor.sprite=preload; preload=previous; actor.resource=actor.sprite.archiveId;
  actor.sequence=args[2]; actor.step=0; actor.node=-1;
  actor.feet=Common::Point(args[3],args[4]); actor.animationOrigin=actor.feet;
  actor.frameTicks=MAX<uint16>(1,actor.sprite.sequences[actor.sequence].steps[0].ticks);
  actor.animationMode=1; actor.animationPaused=false;
  actor.path.clear(); actor.pathIndex=0; actor.walkDistance=actor.walkProgress=0;
  result=actor.sequence;
  debug("[pice] preload-animation actor=%d slot=%d resource=%d sequence=%d",args[0],args[1],actor.resource,actor.sequence);
  return true;
 }
 if(op==144) {
  // 0x1a943 releases a preload handle without touching room actors.
  if(args.size()!=1 || args[0]<0 || uint(args[0])>=_preloadedAnimations.size() ||
     _preloadedAnimations[args[0]].archiveId<0) return false;
  _preloadedAnimations[args[0]]=SpriteResource(); result=args[0]; return true;
 }
 if(op==95) {
  // Original 0x7536 dispatch entries 1,2,5,6 return directly at 0x76bf.
  if(args.size()==4 && (args[0]==1 || args[0]==2 || args[0]==5 || args[0]==6)) { result=args[0]; return true; }
  // Mode 10 calls 0x3e39: release the background resource handle (0x1444).
  // It does not cancel the palette effect; the next movie/room replaces it.
  if(args.size()==4 && args[0]==10) {
   _scenePixels.clear(); result=10; debug("[pice] background released"); return true;
  }
  // 0x75bf/0x7873: asynchronous palette interpolation. This scene uses
  // mode 7, RGB target black, duration 200; update only palette entries 0..239.
  if(args.size()==4 && args[0]==7 && args[1]==0 && (args[2]==0 || args[2]==1) && args[3]>0) {
   _fadeIn=args[2]==1; _fadeLevel=0; _fadeStep=args[3]>255 ? 1 : 255/args[3];
   _fadePeriod=args[3]>255 ? args[3]/255 : 1; _fadeRemaining=_fadePeriod;
   _palettePulse=false; result=7; debug("[pice] palette fade-to-black started ticks=%d",args[3]); return true;
  }
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
     args[2]<0 || args[2]>100 || args[3]<-1 || args[4]<0) return false;
  AnimationSound binding; binding.actor=args[0]; binding.sound=args[1]; binding.volume=args[2]; binding.sequence=args[3]; binding.step=args[4];
  uint slot=0; while(slot<_animationSounds.size() && _animationSounds[slot].actor>=0) ++slot;
  if(slot>=30) return false;
  if(slot==_animationSounds.size()) _animationSounds.push_back(binding); else _animationSounds[slot]=binding;
  result=slot; return true;
 }
 if(op==116) {
  // 0x1114 -> 0xb99e updates sound (+2) and volume (+8) only;
  // -1 preserves either field, including the existing fired latch.
  if(args.size()!=3 || args[0]<0 || uint(args[0])>=_animationSounds.size() ||
     _animationSounds[args[0]].actor<0 || args[1]<-1 ||
     (args[1]>=0 && uint(args[1])>=_sounds.size()) || args[2]<-1 || args[2]>100) return false;
  AnimationSound &binding=_animationSounds[args[0]];
  if(args[1]!=-1) binding.sound=args[1];
  if(args[2]!=-1) binding.volume=args[2];
  result=0; debug("[pice] update-animation-sound id=%d sound=%d volume=%d",args[0],binding.sound,binding.volume); return true;
 }
 if(op==115) {
  // 0x10fd -> 0xb940: free a binding slot without renumbering other IDs.
  if(args.size()!=1 || args[0]<0 || args[0]>=30) return false;
  // Original table has 30 slots, including never-used/free slots.
  if(uint(args[0])<_animationSounds.size()) _animationSounds[args[0]]=AnimationSound();
  result=0; return true;
 }
 if(op==137) {
  if(args.size()!=2 || !name.equalsIgnoreCase("OBJECT") || args[0]<0 || args[0]>=9 || args[1]<0) return false;
  Common::Array<byte> data; SpriteResource panel;
  if(!_graphics.read(1306+args[0],data) || !panel.load(data,1306+args[0])) return false;
  _inventoryPanel=panel; _inventorySelection=args[1]; result=0; return true;
 }
 if(op==89) {
  // 0x2fc4 -> 0x817e: unnamed rectangular hotspot, four evaluated coordinates.
  if(args.size()!=4 || args[2]<args[0] || args[3]<args[1] || _hotspots.size()>=50) return false;
  Hotspot h; h.bounds=Common::Rect(args[0],args[1],args[2],args[3]);
  result=_hotspots.size(); _hotspots.push_back(h); return true;
 }
 if(op==130) {
  // 0x18a1: independent random idle animation, no dialogue or task wait.
  if(args.size()!=6 || args[0]<0 || uint(args[0])>=_actors.size() || !_actors[args[0]].active ||
     args[1]<0 || args[2]<0 || args[3]<args[2] || args[4]<0 || args[5]<args[4]) return false;
  Common::Array<byte> data; RoomActor actor=_actors[args[0]];
  if(!loadAnimation(name,args[1],actor.sprite)) return false;
  for(int i=args[2];i<=args[3];++i) if(!actor.sprite.hasSequence(i)) return false;
  actor.resource=actor.sprite.archiveId; actor.idleMin=args[2]; actor.idleMax=args[3]; actor.idlePauseMin=args[4]; actor.idlePauseMax=args[5];
  actor.sequence=_random.getRandomNumberRng(actor.idleMin,actor.idleMax); actor.step=0; actor.animationMode=5;
  actor.animationPaused=false; actor.animationOrigin=actor.feet;
  actor.frameTicks=MAX<uint16>(1,actor.sprite.sequences[actor.sequence].steps[0].ticks);
  _actors[args[0]]=actor; result=1;
  debug("[pice] idle animation actor=%d resource=%d sequences=%d..%d",args[0],args[1],args[2],args[3]); return true;
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
 if(op==68 || op==69) {
  if(args.size()!=2 || args[0]<0 || uint(args[0])>=_menuEnabled.size() || args[1]<0 || uint(args[1])>=_menuEnabled[args[0]].size()) return false;
  _menuEnabled[args[0]][args[1]]=op==68; result=args[0]; return true;
 }
 if(op==80) {
  // 0x34e8 -> 0x83cb: exit hotspot, approach, script, chapter, room.
  if(args.size()!=5 || args[0]<0 || uint(args[0])>=_hotspots.size() || args[1]<-1 || args[1]>=50 ||
     args[2]<-1 || args[2]>=50 || (chapterRoomBase(args[3])<0) || args[4]<0 || args[4]>=chapterRoomCount(args[3])) return false;
  Action action; action.hotspot=args[0]; action.verb=3; action.approach=args[1]; action.entry=args[2];
  action.exitChapter=args[3]; action.exitRoom=args[4]; if(storeAction(action)<0) return false; result=0; return true;
 }
 if(op==58 || op==81) {
  if(args.size()!=4) return false;
  if(args[0]==-1) { result=-1; return true; } // Original optional absent hotspot.
  // Original 0x2ee5 stores item -1 as well: a dormant item action can
  // supply a fallback for other verbs (0x7f64), without matching any item.
  if(op==81 && (args[1]<-1 || args[1]>=int(_items.size()))) return false;
  // 0x83cb stores a slot in the fixed 50-hotspot table, even when the
  // slot is currently inactive. Bridge re-entry registers Wayne's action
  // without recreating his hotspot; no hit test can select that empty slot.
  if(args[0]<0 || args[0]>=50 || args[3]<-1 || args[3]>=50) return false;
  Action action; action.hotspot=args[0]; action.verb=op==81 ? 9 : args[1]; action.item=op==81 ? args[1] : -1; action.approach=args[2]; action.entry=args[3];
  result=storeAction(action); return result>=0;
 }
 if(op==84) {
  // 0x7cb -> 0x84d7 clears one action slot without compacting the table.
  if(args.size()!=1 || args[0]<0 || uint(args[0])>=_actions.size()) return false;
  _actions[args[0]]=Action(); result=args[0];
  debug("[pice] remove-action id=%d",result); return true;
 }
 if(op==110) {
  if(args.size()!=2) return false;
  // 0xddc: absent optional action -1 is a successful no-op.
  if(args[0]==-1) { result=-1; return true; }
  if(args[0]<0 || uint(args[0])>=_actions.size()) return false;
  _actions[args[0]].fallback=args[1]; result=args[0]; return true;
 }
 if(op==56 || op==92 || op==135) {
  if(args.size()!=(op==135 ? 3U : 1U) || args[0]<0 || uint(args[0])>=_actors.size()) return false;
  if(op==56 || op==92) { _actors[args[0]].routeEnabled=true; if(op==56) {
   _player=args[0];
   // Original 0x1aafe selects and recenters the first player camera.
   if(_cameraActor==-1) { _cameraActor=args[0]; _cameraRecenter=true; }
  } }
  else _actors[args[0]].labelPosition=Common::Point(args[1],args[2]);
  result=args[0]; return true;
 }
 if(op==77) {
  if(args.size()!=4 || args[0]<0 || args[1]<0) return false;
  Common::Array<byte> data; RoomActor actor;
  if(!loadAnimation(name,args[0],actor.sprite) || !actor.sprite.hasSequence(args[1])) return false;
  actor.resource=actor.sprite.archiveId; actor.sequence=args[1]; actor.node=-1; actor.feet=Common::Point(args[2],args[3]);
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
 id=_menus.size(); _menus.push_back(choices); Common::Array<byte> enabled; enabled.resize(choices.size(),0); _menuEnabled.push_back(enabled);
 debug("[pice] create-menu id=%d resource=%u entries=%u (initially disabled)",id,resource,count); return true;
}
void PiceEngine::refreshInventoryLabels() {
 uint32 resource; Common::Array<byte> data;
 if(!_languageNames.resolve(Common::String::format("E_T%02d",_chapter),0,resource) || !_language.read(resource,data)) return;
 _itemLabels.resize(_items.size());
 for(uint i=0;i<_items.size();++i) {
  DialogueLine line;
  if(decodeDialogue(data,Common::String::format("%02u",i),line) && !line.pages.empty() && !line.pages[0].empty())
   _itemLabels[i]=line.pages[0][0];
 }
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
 if (local<0 || !nav ||
     node<0 || uint(node)>=nav->nodes.size() || !nav->nodes[node].active) return false;
 RoomActor actor; Common::Array<byte> data;
 if (!loadAnimation(name,local,actor.sprite) || sequence<0 || !actor.sprite.hasSequence(sequence)) return false;
 const RoomNode &position=nav->nodes[node];
 if (!position.scale || position.scale>400) return false;
 actor.resource=actor.sprite.archiveId; actor.sequence=sequence; actor.node=node; actor.feet=Common::Point(position.x,position.y);
 actor.animationMode=1; actor.animationOrigin=actor.feet; actor.frameTicks=MAX<uint16>(1,actor.sprite.sequences[actor.sequence].steps[0].ticks); id=storeActor(actor); if(id<0) return false;
 debug("[pice] create-actor id=%d resource=%d sequence=%d node=%d x=%d y=%d",id,local,sequence,node,position.x,position.y); return true;
}
bool PiceEngine::showScriptScene() {
 Common::Array<byte> bytes;
 _roomRunner.reset(); _inputLocks=0; _dialogueActor=-1; _narration=false; _pulseOffset=10; _pulseDelta=15;
 _worldVariables=ScriptVariables(); _worldItems.clear(); _items.clear(); _ownedItems.clear(); _itemLabels.clear(); _selectedItem=-1; _actors.clear(); _menus.clear(); _menuEnabled.clear(); _activeMenu=_menuChoice=_lastMenuChoice=-1; _hotspots.clear(); _actions.clear(); _player=-1;
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
 if(!file.open(Common::Path("E_VIDEO").join(name))) { warning("[pice] movie open failed: %s",name.c_str()); return false; }
 if(file.size()<32 || file.size()>64*1024*1024) return false;
 Common::Array<byte> bytes; bytes.resize(file.size());
 if(file.read(&bytes[0],bytes.size())!=bytes.size()) return false;
 MovieResource movie;
 if(!movie.load(bytes)) { warning("[pice] movie container failed: %s",name.c_str()); return false; }
 // Validate every compressed frame before playing any audio or advancing
 // the script. No malformed movie is mistaken for a normal end marker.
 while(movie.nextFrame()) {}
 if(movie.failed || !movie.frame) { warning("[pice] movie frame failed: %s after %u frames",name.c_str(),movie.frame); return false; }
 debug("[pice] movie validated name=%s frames=%u audio=%u final-fnv=%08x",name.c_str(),movie.frame,movie.audio.size(),hashBytes(movie.pixels));
 if(!movie.load(bytes)) { warning("[pice] movie container failed: %s",name.c_str()); return false; }
 // Original movie entry 0x1644b/0x16450 suspends music and room sounds;
 // 0x16548/0x1654d restores only the previously playing channels afterward.
 Common::Array<Audio::SoundHandle> paused;
 auto pause=[&](Audio::SoundHandle sound) {
  if(_mixer->isSoundHandleActive(sound)) { _mixer->pauseHandle(sound,true); paused.push_back(sound); }
 };
 pause(_music.handle); pause(_speechHandle);
 for(uint i=0;i<_sounds.size();++i) if(_sounds[i].active) pause(_sounds[i].handle);
 debug("[pice] movie background audio paused channels=%u",paused.size());
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
  // Fit the decoded image to the preview canvas, preserving its aspect ratio.
  uint width=640,height=400;
  if(movie.width*height>movie.height*width) height=width*movie.height/movie.width;
  else width=height*movie.width/movie.height;
  uint black=0,brightness=766;
  for(uint i=0;i<256;++i) {
   uint sum=movie.palette[i*3]+movie.palette[i*3+1]+movie.palette[i*3+2];
   if(sum<brightness) { brightness=sum; black=i; }
  }
  for(uint i=0;i<screen.size();++i) screen[i]=black;
  for(uint y=0;y<height;++y) for(uint x=0;x<width;++x)
   screen[(y+(400-height)/2)*640+x+(640-width)/2]=movie.pixels[(y*movie.height/height)*movie.width+x*movie.width/width];
  _system->getPaletteManager()->setPalette(&movie.palette[0],0,256);
  _system->copyRectToScreen(&screen[0],640,0,0,640,400); _system->updateScreen();
 }
 if(!movie.failed && !shouldQuit()) waitForFrame(movie.frame);
 _mixer->stopHandle(handle);
 for(uint i=0;i<paused.size();++i) _mixer->pauseHandle(paused[i],false);
 debug("[pice] movie background audio resumed channels=%u",paused.size());
 if(after) _system->fillScreen(0);
 _system->getPaletteManager()->setPalette(&_scenePalette[0],0,256);
 _roomClock=_system->getMillis(); _roomRenderClock=_roomClock;
#ifdef PICE_TESTBENCH
 _testMovieDone=!movie.failed && !shouldQuit();
#endif
 debug("[pice] movie completed name=%s frames=%u failed=%d",name.c_str(),movie.frame,movie.failed);
 return !movie.failed && !shouldQuit();
}
bool PiceEngine::loadChapter(int16 chapter,uint room) {
 if((chapter!=3 && chapter!=6 && chapter!=8 && chapter!=9 && chapter!=10 && chapter!=11 && chapter!=12) || room>=uint(chapterRoomCount(chapter))) return false;
 // Chapter-local definitions must be recreated before any destination room.
 _roomRunner.reset(); _actors.clear(); _hotspots.clear(); _actions.clear(); _worldItems.clear();
 _items.clear(); _ownedItems.clear(); _itemLabels.clear(); _menus.clear(); _menuEnabled.clear();
 _animationSounds.clear(); _selectedItem=-1; _activeMenu=_menuChoice=-1; _inputLocks=0;
 for(uint i=0;i<_sounds.size();++i) _mixer->stopHandle(_sounds[i].handle);
 _sounds.clear(); _mixer->stopHandle(_music.handle); _music=RoomSound();
 _chapter=chapter; _roomBankId=chapterRoomBase(chapter)+room; _itemPairs.clear(); _pendingChapter=chapter; _pendingRoom=-1; _previousRoom=-1;
 Common::Array<byte> bytes; ScriptResource bootstrap;
 if(!_graphics.read(chapterRoomBase(chapter)-1,bytes) || !bootstrap.load(bytes)) return false;
 ScriptRunner init(bootstrap,&_worldVariables,this); init.start(0);
 ScriptRunner::Result result=init.run(0,4096); uint16 op=0; bootstrap.word(init.position(0),op);
 debug("[pice] chapter bootstrap chapter=%d result=%d pc=%u opcode=%u",chapter,int(result),init.position(0),op);
 if(result!=ScriptRunner::kStopped) return false;
 return loadRoom(room);
}
bool PiceEngine::changeRoom(int16 chapter,int16 room) {
 // 0x1c232 passes room+2 to the SMxx script group. SM02 begins at 1138;
 // bootstrap occupies slot 1, so room 0 is 1140 and room 24 is 1164.
 if((chapterRoomBase(chapter)<0) || room<0 || room>=chapterRoomCount(chapter) || _pendingRoom>=0) return false;
 Common::Array<byte> bytes; ScriptResource bank;
 if(!_graphics.read(chapterRoomBase(chapter)+room,bytes) || !bank.load(bytes)) return false;
 _pendingRoom=room; _pendingChapter=chapter;
 debug("[pice] request-room chapter=%d room=%d bank=%d",chapter,room,chapterRoomBase(chapter)+room); return true;
}
bool PiceEngine::gameOver(int16 reason) {
 if(reason<0 || reason>=50) return false;
 _menuOpenedAt=_system->getMillis();
 _gameOver=true; _activeMenu=_menuChoice=-1; _dialogueActor=-1; _narration=false; _subtitle.clear();
 _actionEntry=-1; _actionWalking=false;
 _mixer->stopHandle(_speechHandle);
 for(uint i=0;i<_sounds.size();++i) _mixer->stopHandle(_sounds[i].handle);
 debug("[pice] game over reason=%d; recovery menu opened",reason);
 return true;
}
bool PiceEngine::enterRoom(const Common::String &name,const Common::Array<int16> &args,int16 &actor) {
 actor=-1;
 if(args.size()!=6) return false;
 if(args[0]!=_chapter || args[1]!=_previousRoom) return true;
 // Original 0x1c683 skips actor placement when the player slot is -1.
 // Actorless closeups without an entry continuation need no animation.
 if(_player==-1 && args[5]==-1) return true;
 if(_player<0 || uint(_player)>=_actors.size() || !_actors[_player].active) return false;
 Common::Array<int16> animation;
 animation.push_back(_player); animation.push_back(args[3]); animation.push_back(args[4]); animation.push_back(args[2]);
 int16 sequence;
 if(!invoke(96,name,animation,Common::String(),sequence)) return false;
 actor=_player;
 debug("[pice] room-entry from=%d node=%d actor=%d animation=%d sequence=%d task=%d",_previousRoom,args[2],actor,args[3],args[4],args[5]);
 return true;
}
bool PiceEngine::loadRoom(uint room) {
 Common::Array<byte> bytes; ScriptResource bank;
 if(room>=uint(chapterRoomCount(_chapter)) || !_graphics.read(chapterRoomBase(_chapter)+room,bytes) || !bank.load(bytes)) return false;
 _previousRoom=_roomRunner ? int(_roomBankId)-chapterRoomBase(_chapter) : -1;
 _roomRunner.reset(); _roomBank=bank; _roomBankId=chapterRoomBase(_chapter)+room;
 // Same-chapter teardown preserves inventory, dialogue menus, variables and
 // persistent SFX handles (0x1c4a9 -> 0x15286 with argument zero).
 for(uint i=0;i<_sounds.size();++i) if(_sounds[i].active && !_sounds[i].persistent) {
  _mixer->stopHandle(_sounds[i].handle); _sounds[i]=RoomSound();
 }
 _combinationRunner.reset(); _combinationFailed=false;
 _interruptedAction=InterruptedAction();
 _actors.clear(); _worldItems.clear(); _hotspots.clear(); _actions.clear(); _animationSounds.clear(); _inventoryEnabled=true; _advanceRequested=false;
 _actionEntry=-1; _actionContext=-1; _actionHotspot=-1; _actionWalking=false; _hoverHotspot=-1;
 _playerMovement=true; _cursorItem=-2;
 _preloadedAnimations.clear(); _scenery=SpriteResource(); _navigation.clear(); _player=-1; _cameraActor=-1; _cameraFrozen=false; _cameraRecenter=false; _cameraPosition=Common::Point(); _inputLocks=0; _palettePulse=false; _fadeLevel=-1; _dialogueActor=-1; _narration=false; _closeup=false;
 _mixer->stopHandle(_speechHandle); _subtitle.clear(); _pendingRoom=-1;

 _scenePixels.clear(); _scenePalette.clear();
  _roomRunner.reset(new ScriptRunner(_roomBank,&_worldVariables,this));
 ScriptRunner &runner=*_roomRunner;
 _buildingNavigation=&runner.navigation();
 runner.prioritizeInitialization();
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
 if(result==ScriptRunner::kUnsupported || result==ScriptRunner::kInvalid) {
  _roomStatus=Common::String::format("Room script 0 stopped: opcode %u at %u",opcode,runner.position(0));
  warning("[pice] room initialization failed bank=%u pc=%u opcode=%u",_roomBankId,runner.position(0),opcode);
 }
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
 refreshInventoryLabels();
 return renderRoom();
}
int PiceEngine::cameraX() const {
 if(_cameraFrozen && !_cameraRecenter) return _cameraPosition.x;
 if(_scenePixels.size()<4 || _cameraActor<0 || uint(_cameraActor)>=_actors.size()) return 0;
 return MAX(0,MIN(int(READ_LE_UINT16(&_scenePixels[0]))-640,int(_actors[_cameraActor].feet.x)-320));
}
int PiceEngine::cameraY() const {
 if(_cameraFrozen && !_cameraRecenter) return _cameraPosition.y;
 if(_scenePixels.size()<4 || _cameraActor<0 || uint(_cameraActor)>=_actors.size()) return 0;
 return MAX(0,MIN(int(READ_LE_UINT16(&_scenePixels[2]))-320,int(_actors[_cameraActor].feet.y)-240));
}
bool PiceEngine::renderRoom() {
 _system->fillScreen(0);
 _roomCanvas.clear(); _roomCanvas.resize(640*400,0);
 int viewX=cameraX(),viewY=cameraY();
 _cameraPosition=Common::Point(viewX,viewY); _cameraRecenter=false;
 if(!_scenePixels.empty()) {
  uint height=MIN<uint>(320,READ_LE_UINT16(&_scenePixels[2]));
  uint width=READ_LE_UINT16(&_scenePixels[0]);
  for(uint y=0;y<height;++y) memcpy(&_roomCanvas[((400-height)/2+y)*640],&_scenePixels[4+(y+viewY)*width+viewX],640);
 }
 // Original 0xb3b6 merges actors (Y at +5) and scenery (Y at +3).
 // Equal anchors paint scenery first, allowing an actor to stand in front.
 struct Layer { int y; uint index; bool actor; };
 Common::Array<Layer> layers;
 uint sceneryFrame=0;
 if(_scenery.hasSequence(0)) {
  sceneryFrame=_scenery.sequences[0].steps[0].frame;
  if(sceneryFrame>=_scenery.frames.size()) return false;
  for(uint i=0;i<_scenery.frames[sceneryFrame].parts.size();++i) {
   Layer layer={_scenery.frames[sceneryFrame].parts[i].y,i,false}; layers.push_back(layer);
  }
 }
 for(uint i=0;i<_actors.size();++i) if(_actors[i].active) {
  Layer layer={_actors[i].feet.y,i,true}; layers.push_back(layer);
 }
 // Stable insertion preserves resource/actor order at equal anchors.
 for(uint i=1;i<layers.size();++i) {
  Layer layer=layers[i]; uint j=i;
  while(j && layers[j-1].y>layer.y) { layers[j]=layers[j-1]; --j; }
  layers[j]=layer;
 }
 for(uint i=0;i<layers.size();++i) {
  const Layer &layer=layers[i];
  if(layer.actor) {
   const RoomActor &actor=_actors[layer.index];
   uint frame=actor.sprite.sequences[actor.sequence].steps[actor.step].frame;
   if(!actor.sprite.paintScaledFrame(frame,actor.feet.x-viewX,actor.feet.y-40-viewY,actorScale(actor),_roomCanvas,640,400)) return false;
  } else if(!_scenery.paintPart(sceneryFrame,layer.index,-viewX,-40-viewY,_roomCanvas,640,400)) return false;
 }
 if(_closeup && dialogueActive()) {
  Common::Array<int16> sequences;
  if(!_closeupAnimations[_dialoguePage].sample((_system->getMillis()-_pageStart)/10,sequences) ||
     !_closeupSprite.paintFrame(_closeupSprite.sequences[0].steps[0].frame,_closeupOrigin.x,_closeupOrigin.y-40,_roomCanvas,640,400)) return false;
  for(uint i=0;i<sequences.size();++i) {
   if(!_closeupSprite.hasSequence(sequences[i]) || !_closeupSprite.paintFrame(_closeupSprite.sequences[sequences[i]].steps[0].frame,
       _closeupOrigin.x,_closeupOrigin.y-40,_roomCanvas,640,400)) return false;
  }
 }
 uint inventorySlot=0;
 for(uint i=0;inventoryVisible() && i<_ownedItems.size() && inventorySlot<12;++i) if(_ownedItems[i]) {
  const RoomActor &item=_items[i];
  if(item.sprite.hasSequence(item.sequence)) {
   uint frame=item.sprite.sequences[item.sequence].steps[0].frame; Common::Rect bounds;
   if(item.sprite.frameBounds(frame,bounds) && bounds.width()>0 && bounds.height()>0 && bounds.width()<=512 && bounds.height()<=512) {
    Common::Array<byte> icon; icon.resize(bounds.width()*bounds.height(),0);
    if(item.sprite.paintFrame(frame,-bounds.left,-bounds.top,icon,bounds.width(),bounds.height())) {
     uint w=MIN<uint>(40,bounds.width()),h=MIN<uint>(34,bounds.height());
     for(uint y=0;y<h;++y) for(uint x=0;x<w;++x)
      _roomCanvas[(2+y)*640+inventorySlot*50+5+x]=icon[(y*bounds.height()/h)*bounds.width()+x*bounds.width()/w];
    }
   }
  }
  if(_selectedItem==int(i)) for(uint x=inventorySlot*50+4;x<inventorySlot*50+49;++x) _roomCanvas[38*640+x]=_textColor;
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
 if(args.size()!=6 || dialogueActive() ||
    args[2]<0) return false;
 DialogueLine line; Common::Array<uint32> voices; Common::Array<byte> data; SpriteResource sprite; Common::Array<VoiceAnimation> animations;
 if(!loadDialogue(textName,args[5],key,line,voices) || !loadAnimation(animationName,args[2],sprite) || !sprite.hasSequence(0)) return false;
 for(uint i=0;i<voices.size();++i) {
  VoiceAnimation animation;
  if(!_language.read(voices[i],data) || !animation.load(data)) return false;
  animations.push_back(animation);
 }
 _dialoguePages=line.pages; _dialogueVoices=voices; _dialoguePage=0; _closeup=true;
 _closeupAnimations=animations; _closeupSprite=sprite; _closeupOrigin=Common::Point(args[0],args[1]);
 if(!startDialoguePage()) { _closeup=false; return false; }
 // Original 0x16bb uses the independent text channel (state 5), not
 // a camera actor. args[3:5] are text anchors, not required dimensions.
 actor=-27; _narration=true;
 debug("[pice] closeup-dialogue actor=%d animation=%d key=%s pages=%u",actor,args[2],key.c_str(),line.pages.size()); return true;
}
bool PiceEngine::objectDialogue(int16 actor,const Common::String &name,int16 local,const Common::String &base) {
 if(actor<0 || uint(actor)>=_actors.size() || !_actors[actor].active || dialogueActive()) return false;
 Common::String group=name,key;
 if(!resolveObjectDescription(group,local,base,key)) return false;
 return speakDialogue(actor,group,local,key);
}
bool PiceEngine::resolveObjectDescription(Common::String &group,int16 &local,const Common::String &base,Common::String &key) {
 if(group.size()<3 || local<0) return false;
 group.setChar('E',0); group.setChar('T',2);
 uint32 resource; Common::Array<byte> data;
 if(!_languageNames.resolve(group,local,resource) || !_language.read(resource,data) ||
    !selectObjectDialogueKey(data,base,_actionContext,_selectedItem,key)) return false;
 if(key.empty()) {
  group="E_TCETXT"; local=1;
  if(!_languageNames.resolve(group,local,resource) || !_language.read(resource,data) ||
     !genericDialogueKey(data,_actionContext,_random.getRandomNumber(0x7fffffff),key)) return false;
  debug("[pice] object-dialogue fallback group=%s key=%s",group.c_str(),key.c_str());
 }
 return true;
}
bool PiceEngine::describeObject(int16 x,int16 y,const Common::String &name,int16 local,const Common::String &base) {
 // 0x138e selects the same LOOK/USE/item-dependent key as actor object speech,
 // then 0x45e7 uses independent narration (script state 5).
 if(dialogueActive()) return false;
 Common::String group=name,key;
 if(!resolveObjectDescription(group,local,base,key)) return false;
 debug("[pice] describe-object base=%s key=%s context=%d",base.c_str(),key.c_str(),_actionContext);
 return narrate(x,y,group,local,key);
}

bool PiceEngine::narrate(int16 x,int16 y,const Common::String &name,int16 local,const Common::String &key) {
 if(dialogueActive()) return false;
 DialogueLine line; Common::Array<uint32> voices;
 if(!loadDialogue(name,local,key,line,voices)) return false;
 _dialoguePages=line.pages; _dialogueVoices=voices; _dialoguePage=0; _closeup=false;
 if(!startDialoguePage()) return false;
 _narration=true;
 // Coordinates are original text anchors; the native caption panel currently
 // uses the same readable placement for actor speech and recorded voices.
 debug("[pice] narration key=%s voice=%u anchor=%d,%d",key.c_str(),voices[0],x,y);
 return true;
}

bool PiceEngine::speakDialogue(int16 actor,const Common::String &name,int16 local,const Common::String &key) {
 if(actor<0 || uint(actor)>=_actors.size() || !_actors[actor].active || dialogueActive()) return false;
 DialogueLine line; Common::Array<uint32> voices;
 if(!loadDialogue(name,local,key,line,voices)) return false;
 // Both 52 and 111 use 0x4579: ordinary speech on the existing actor.
 _dialoguePages=line.pages; _dialogueVoices=voices; _dialoguePage=0; _closeup=false;
 if(!startDialoguePage()) return false;
 _dialogueRestore=_actors[actor]; _dialogueActor=actor;
 debug("[pice] actor-dialogue actor=%d key=%s voice=%u",actor,key.c_str(),voices[0]);
 return true;
}
bool PiceEngine::animatedDialogue(const Common::String &textName,const Common::String &key,const Common::String &animationName,const Common::Array<int16> &args) {
 if(args.size()!=7 || dialogueActive() || args[0]<0 || uint(args[0])>=_actors.size() || textName.size()<3 ||
    args[1]<0 || args[2]<0 ||
    args[3]<0 || args[4]<args[3] || args[5]<0 || args[6]<args[5]) return false;
 DialogueLine line; Common::Array<uint32> voices; Common::Array<byte> data;
 if(!loadDialogue(textName,args[1],key,line,voices)) return false;
 RoomActor actor=_actors[args[0]];
 if(!loadAnimation(animationName,args[2],actor.sprite) || uint(args[4])>=actor.sprite.sequences.size()) return false;
 for(int sequence=args[3];sequence<=args[4];++sequence) if(!actor.sprite.hasSequence(sequence)) return false;
 // 0x1704 / 0x1ab30: speech owns a temporary animation resource, choosing
 // sequences and pauses inside the script's inclusive ranges. State 10
 // restores the previous resource/sequence when the dialogue finishes.
 actor.resource=actor.sprite.archiveId; actor.sequence=_random.getRandomNumberRng(args[3],args[4]); actor.step=0;
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
 if(_gameOver) {
#ifdef PICE_TESTBENCH
  if(!_testUseDone && ConfMan.getBool("pice_test_recover_latest") && _system->getMillis()-_menuOpenedAt>=2000) {
   _testUseDone=true;
   Common::Event event; event.type=Common::EVENT_LBUTTONDOWN; event.mouse=Common::Point(320,186);
   _eventMan->pushEvent(event);
  }
#endif
  renderRoom(); return;
 }
 if(!_scenePreview || !_roomRunner) return;
 uint32 now=_system->getMillis(), ticks=(now-_roomClock)/10;
 if(!ticks) return;
 // Bounded catch-up retains elapsed time; the next visit handles any backlog.
 if(ticks>10) ticks=10;
 _roomClock+=ticks*10;
 // Text-only entries have no mixer completion signal. Native readable
 // pacing (not yet the original configurable text-speed formula).
 bool textPending=dialogueActive() && _dialogueVoices[_dialoguePage]==0xffffffff &&
  _system->getMillis()-_pageStart<MAX<uint32>(2000,MIN<uint32>(10000,_subtitle.size()*60));
 if(dialogueActive() && !textPending && !_mixer->isSoundHandleActive(_speechHandle)) {
  if(_dialoguePage+1<_dialoguePages.size()) {
   ++_dialoguePage;
   if(!startDialoguePage()) { warning("[pice] Cannot play next dialogue page"); _scenePreview=false; return; }
  } else {
   finishDialogue();
  }
 }
 for(uint i=0;i<_actors.size();++i) {
  RoomActor &actor=_actors[i];
  if(!actor.active || !actor.animationMode) continue;
  if(actor.animationPaused) {
   actor.frameTicks-=ticks;
   if(actor.frameTicks>0) continue;
   actor.sequence=actor.animationMode==5 ? _random.getRandomNumberRng(actor.idleMin,actor.idleMax) : _random.getRandomNumberRng(_dialogueMinSequence,_dialogueMaxSequence);
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
    if(actor.animationMode==3 || actor.animationMode==5) {
     actor.animationPaused=true; actor.frameTicks=actor.animationMode==5 ? _random.getRandomNumberRng(actor.idlePauseMin,actor.idlePauseMax) : _random.getRandomNumberRng(_dialogueMinPause,_dialogueMaxPause); break;
    }
    if(actor.animationMode!=4) actor.feet=actor.animationOrigin;
   } else ++actor.step;
   actor.frameTicks+=MAX<uint16>(1,steps[actor.step].ticks);
  }
 }
 for(uint i=0;i<_animationSounds.size();++i) {
  AnimationSound &binding=_animationSounds[i];
  if(binding.actor<0) continue;
  const RoomActor &actor=_actors[binding.actor];
  bool match=(binding.sequence==-1 || binding.sequence==actor.sequence) && binding.step==int(actor.step);
  if(match && !binding.fired) {
   if(volume(binding.sound,binding.volume)) startSound(binding.sound,1);
  }
  binding.fired=match;
 }
 tickCombination(ticks);
 tickAction();
 for(uint i=0;!_combinationRunner && !_combinationFailed && i<_roomBank.count();++i) {
  if(!_roomRunner->running(i)) continue;
  if(_roomRunner->exclusiveTask()>=0 && _roomRunner->exclusiveTask()!=int(i)) continue;
  for(uint budget=0;budget<64;++budget) {
   uint32 before=_roomRunner->position(i);
   uint previousLocks=_inputLocks;
   ScriptRunner::Result result=_roomRunner->step(i,budget ? 0 : ticks);
   if(previousLocks && !_inputLocks && result==ScriptRunner::kAdvanced) releaseActionInput(i);
   _navigation=_roomRunner->navigation();
   if(result==ScriptRunner::kInvalid || result==ScriptRunner::kUnsupported) {
    uint16 opcode=0; _roomBank.word(before,opcode);
    _roomStatus=Common::String::format("Room script %u stopped: opcode %u at %u",i,opcode,before);
    if(_actionEntry==int(i)) { _actionEntry=-1; _actionHotspot=-1; _actionContext=-1; _actionWalking=false; }
    _snapshotPending=true;
    warning("[pice] room-task=%u result=%d pc=%u opcode=%u bank=%u",i,int(result),before,opcode,_roomBankId);
   }
   if(_roomRunner->exclusiveTask()!=int(i) || result!=ScriptRunner::kAdvanced) break;
  }
 }
 // Keep clicks until a script tick actually runs, not merely a render loop.
 _advanceRequested=false;
#ifdef PICE_TESTBENCH
 if(ConfMan.getBool("pice_test_death_continue") && !_testUseDone && _roomBankId==1261 &&
    _roomRunner->running(4) && _roomRunner->position(4)==486 && !_roomRunner->waiting(4)) {
  Common::Event click; click.type=Common::EVENT_LBUTTONDOWN; click.mouse=Common::Point(320,200);
  _eventMan->pushEvent(click); _testUseDone=true;
  debug("[pice] test death-screen continue click queued");
 }
 if(ConfMan.getBool("pice_test_trace_waits")) {
  static uint32 lastTrace=0;
  if(now-lastTrace>=5000) {
   lastTrace=now; _snapshotPending=true;
   debug("[pice] waits bank=%u locks=%u action=%d walking=%d exclusive=%d dialogue=%d",_roomBankId,_inputLocks,_actionEntry,_actionWalking,_roomRunner->exclusiveTask(),_dialogueActor);
   for(uint t=0;t<50;++t) if(_roomRunner->running(t)) debug("[pice] waits task=%u pc=%u waiting=%d",t,_roomRunner->position(t),_roomRunner->waiting(t));
   for(uint a=0;a<_actors.size();++a) if(_actors[a].active) debug("[pice] waits actor=%u mode=%d seq=%d step=%u feet=%d,%d",a,_actors[a].animationMode,_actors[a].sequence,_actors[a].step,_actors[a].feet.x,_actors[a].feet.y);

  }
 }
 if(ConfMan.getBool("pice_test_inventory") && !_testUseDone) {
  Common::Event event; event.type=Common::EVENT_MOUSEMOVE; event.mouse=Common::Point(125,20);
  _eventMan->pushEvent(event);
  _system->warpMouse(125,20);
  if(inventoryVisible()) {
   int item=inventoryAt(Common::Point(125,20));
   bool selected=item>=0 && selectInventoryItem(item);
   debug("[pice] inventory hover test bank=%u native-enabled=%d visible=1 item=%d selected=%d",_roomBankId,_inventoryEnabled,item,selected);
   _snapshotPending=true; _testUseDone=true;
  }
 }
 if(ConfMan.getBool("pice_test_combine") && !_testUseDone && !_itemPairs.empty() && !_inputLocks && _actionEntry<0 && !dialogueActive()) {
  // Isolated fixture: give the registered components, then use normal UI selection.
  const ItemPair pair=_itemPairs[0]; Common::Array<int16> a; int16 result;
  a.push_back(pair.first); bool ok=invoke(67,Common::String(),a,Common::String(),result);
  a[0]=pair.second; ok=ok && invoke(67,Common::String(),a,Common::String(),result);
  _testUseDone=true; _selectedItem=-1;
  if(ok && selectInventoryItem(pair.first) && selectInventoryItem(pair.second)) debug("[pice] test combination through inventory selection started");
 }
 if(ConfMan.getBool("pice_test_medal_ambient")) {
  if(ConfMan.getBool("pice_test_hamsun_full_inventory") && _activeMenu>=0 && _menuChoice==-1 && now-_menuOpenedAt>=1000 && _testConversationChoices<2) {
   int choice=_testConversationChoices ? 6 : 2, row=0;
   for(uint i=0;i<_menuEnabled[_activeMenu].size();++i) if(_menuEnabled[_activeMenu][i]) {
    if(int(i)==choice) { if(clickMenu(Common::Point(370,60+row*24+12))) ++_testConversationChoices; break; }
    ++row;
   }
  }
  if(ConfMan.getBool("pice_test_hamsun_full_inventory") && _testAmbientStep==4 && _testAmbientItem==10 && _roomBankId==1143 && ambientDialogue()) {
   _testAmbientItem=9; _testAmbientStep=1;
  }
  if(ConfMan.getBool("pice_test_hamsun_full_inventory") && _testAmbientStep==4 && _testAmbientItem==9 && _roomBankId==1142 && !_savePending && !_inputLocks && _actionEntry<0 && _roomRunner->exclusiveTask()<0 && !actorBusy(_player)) {
   if(writeCheckpoint(false)) {
    _testAmbientStep=5; debug("[pice] test recorder complete and checkpoint saved");
    _selectedItem=-1;
    if(clickRoom(Common::Point(30,195),false)) debug("[pice] test exit to bridge clicked");
   }
  }
  if(_testAmbientStep==0 && _ownedItems.size()>10 && _ownedItems[10] && _actionEntry<0 && !_inputLocks) {
   if(loadRoom(3)) { _testAmbientStep=1; debug("[pice] test medal collected, entering bunk"); }
  } else if(_testAmbientStep==1 && ambientDialogue() && now-_pageStart>=1000) {
   int slot=0; for(int i=0;i<_testAmbientItem;++i) if(_ownedItems[i]) ++slot;
   Common::Event event; event.type=Common::EVENT_MOUSEMOVE; event.mouse=Common::Point(slot*50+25,20);
   _eventMan->pushEvent(event); _testAmbientStep=2;
  } else if(_testAmbientStep==2 && ambientDialogue() && inventoryVisible()) {
   Common::Event event; event.type=Common::EVENT_LBUTTONDOWN; event.mouse=_eventMan->getMousePos();
   _eventMan->pushEvent(event); _testAmbientStep=3;
   debug("[pice] test inventory visible during ambient speech, selecting item=%d",_testAmbientItem);
  } else if(_testAmbientStep==3 && _selectedItem==_testAmbientItem) {
   // Exercise item use against Hamsun's actual sprite hotspot.
   for(int y=40;y<338 && _testAmbientStep==3;y+=2) for(int x=0;x<640 && _testAmbientStep==3;x+=2) {
    Common::Point point(x,y);
    if(hotspotAt(point)==1 && clickRoom(point,false)) {
     _testAmbientStep=4; debug("[pice] test item used on Hamsun item=%d context=%d",_testAmbientItem,_actionContext);
    }
   }
  }
 }
 if(!_testClickDone && ConfMan.getBool("pice_test_exit_ambient") && ambientDialogue() && now-_pageStart>=1500) {
  _testClickDone=true;
  Common::Event event; event.type=Common::EVENT_LBUTTONDOWN; event.mouse=Common::Point(320,380);
  _eventMan->pushEvent(event);
  debug("[pice] test exit during ambient dialogue");
 }
 if(!_testMenuClosed && _activeMenu>=0 && ConfMan.getBool("pice_test_close_menu") && _system->getMillis()-_menuOpenedAt>=2000) {
  _testMenuClosed=true;
  Common::Event event; event.type=Common::EVENT_LBUTTONDOWN; event.mouse=Common::Point(0,0); _eventMan->pushEvent(event);
 }
 if(!_testUseDone && _activeMenu>=0 && ConfMan.hasKey("pice_test_menu_option") && _system->getMillis()-_menuOpenedAt>=2000) {
  int wanted=ConfMan.getInt("pice_test_menu_option"),row=0;
  for(uint i=0;i<_menuEnabled[_activeMenu].size();++i) if(_menuEnabled[_activeMenu][i]) {
   if(int(i)==wanted) { _testUseDone=clickMenu(Common::Point(370,60+row*24+12)); break; }
   ++row;
  }
 }
 if(!_testClickDone && (ConfMan.hasKey("pice_test_click_hotspot") || ConfMan.hasKey("pice_test_click_label")) && !_inputLocks && !actorBusy(_player)) {
  int target=ConfMan.hasKey("pice_test_click_hotspot") ? ConfMan.getInt("pice_test_click_hotspot") : -1;
  if(ConfMan.hasKey("pice_test_click_label")) {
   target=-1;
   for(uint i=0;i<_hotspots.size();++i) if(_hotspots[i].active && _hotspots[i].label.equalsIgnoreCase(ConfMan.get("pice_test_click_label"))) { target=i; break; }
  }
  // Probe the actual screen hit test, including scrolling and scaled actors.
  if(target>=0 && uint(target)<_hotspots.size() && _hotspots[target].active) {
   int probeX=ConfMan.hasKey("pice_test_click_x") ? ConfMan.getInt("pice_test_click_x") : -1;
   int probeY=ConfMan.hasKey("pice_test_click_y") ? ConfMan.getInt("pice_test_click_y") : -1;
   for(int y=probeY>=0 ? probeY : 40;y<(probeY>=0 ? probeY+1 : 360) && !_testClickDone;++y) for(int x=probeX>=0 ? probeX : 0;x<(probeX>=0 ? probeX+1 : 640) && !_testClickDone;++x) {
    Common::Point point(x,y);
    if(hotspotAt(point)!=target) continue;
    if(ConfMan.hasKey("pice_test_click_item")) _selectedItem=ConfMan.getInt("pice_test_click_item");
    if(clickRoom(point,ConfMan.getBool("pice_test_right_click"))) {
     _testClickDone=true; _snapshotPending=true;
     debug("[pice] test click hotspot=%d at=%d,%d",target,point.x,point.y);
    }
   }
  }
 }
 if(_testClickDone && !_testUseDone && ConfMan.hasKey("pice_test_walk_node") && _actionEntry<0 && !_inputLocks && !actorBusy(_player) && !dialogueActive()) {
  Common::Array<int16> walk; walk.push_back(_player); walk.push_back(ConfMan.getInt("pice_test_walk_node")); int16 ignored;
  if(invoke(50,Common::String(),walk,Common::String(),ignored)) { _testUseDone=true; debug("[pice] test walk node=%d",walk[1]); }
 }
 if(_testClickDone && !_testUseDone && ConfMan.hasKey("pice_test_followup_hotspot") && _actionEntry<0 && !_inputLocks && !actorBusy(_player) && !dialogueActive()) {
  int target=ConfMan.getInt("pice_test_followup_hotspot");
  for(int y=40;y<338 && !_testUseDone;++y) for(int x=0;x<640 && !_testUseDone;++x) {
   Common::Point point(x,y); if(hotspotAt(point)!=target) continue;
   _selectedItem=-1;
   if(clickRoom(point,false)) { _testUseDone=true; debug("[pice] test followup clicked hotspot=%d",target); }
  }
 }
 if(_testClickDone && !_testUseDone && ConfMan.hasKey("pice_test_use_hotspot") && (!ConfMan.hasKey("pice_test_use_room") || _roomBankId==uint(1140+ConfMan.getInt("pice_test_use_room"))) && _actionEntry<0 && !_inputLocks && !actorBusy(_player)) {
  int readySpot=ConfMan.hasKey("pice_test_use_ready_hotspot") ? ConfMan.getInt("pice_test_use_ready_hotspot") : -1;
  bool ready=readySpot<0 || (uint(readySpot)<_hotspots.size() && _hotspots[readySpot].active);
  int target=ConfMan.getInt("pice_test_use_hotspot");
  for(uint item=0;ready && item<_ownedItems.size() && !_testUseDone;++item) if(_ownedItems[item] && (!ConfMan.hasKey("pice_test_use_item") || item==uint(ConfMan.getInt("pice_test_use_item")))) {
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
 if(!_testMenuChoiceDone && ConfMan.hasKey("pice_test_menu_choice") && _activeMenu>=0 && _menuChoice==-1 && now-_menuOpenedAt>1000) {
  int choice=ConfMan.getInt("pice_test_menu_choice");
  if(choice>=0 && uint(_activeMenu)<_menuEnabled.size() && uint(choice)<_menuEnabled[_activeMenu].size() && _menuEnabled[_activeMenu][choice]) {
   _menuChoice=choice; _testMenuChoiceDone=true; debug("[pice] test menu choice=%d",choice);
  }
 }
 if(ConfMan.getBool("pice_test_dismiss_menu") && _activeMenu>=0 && _menuChoice==-1 && now-_menuOpenedAt>1000) _menuChoice=-2;
 if(((_testMovieDone && ConfMan.hasKey("pice_test_checkpoint_room")) || ConfMan.hasKey("pice_test_checkpoint_bank")) && !_testCheckpointDone &&
    _roomBankId==uint(ConfMan.hasKey("pice_test_checkpoint_bank") ? ConfMan.getInt("pice_test_checkpoint_bank") : 1140+ConfMan.getInt("pice_test_checkpoint_room")) && !_roomRunner->running(0) &&
    !_savePending && !_inputLocks && _actionEntry<0 && !dialogueActive() && _pendingRoom<0 &&
    _fadeLevel<0 && _roomRunner->exclusiveTask()<0 && !actorBusy(_player)) {
  if(writeCheckpoint(false)) { _testCheckpointDone=true; debug("[pice] test progression checkpoint bank=%u",_roomBankId); }
 }
 if(ConfMan.getBool("pice_test_book_placement") && !_testCheckpointDone && _roomBankId==1247 &&
    !_roomRunner->running(10) && !_roomRunner->running(11) && !dialogueActive()) {
  int16 placed,book;
  if(_worldVariables.read(true,59,placed) && placed==1 && _worldVariables.read(true,3,book) &&
     book>=0 && uint(book)<_ownedItems.size() && !_ownedItems[book] && writeCheckpoint(false)) {
   _testCheckpointDone=true; _snapshotPending=true;
   debug("[pice] book placement verified slot=1 value=%d item=%d removed=1",placed,book);
  }
 }
 if(ConfMan.getBool("pice_test_preload_roundtrip") && !_testCheckpointDone && !_preloadedAnimations.empty()) {
  Common::Array<byte> payload;
  auto expected=_preloadedAnimations;
  if(snapshot(payload,false)) {
   bool ok=snapshot(payload,true) && _preloadedAnimations.size()==expected.size();
   for(uint i=0;ok && i<expected.size();++i)
    ok=_preloadedAnimations[i].archiveId==expected[i].archiveId && _preloadedAnimations[i].fingerprint==expected[i].fingerprint;
   if(ok) ok=writeCheckpoint(false);
   _testCheckpointDone=true;
   debug("[pice] preload snapshot roundtrip bank=%u slots=%u pass=%d",_roomBankId,expected.size(),ok);
  }
 }
 if(_testSavePending && int32(_system->getMillis()-_testSaveAt)>=0) {
  _testSavePending=false;
  if(!writeCheckpoint(false)) warning("[pice] Test music checkpoint failed");
 }
#endif
 // Keep scripted requests queued through fades, dialogue and scene changes.
 if(_savePending && snapshotReady()) {
  _savePending=false;
  if(!writeCheckpoint(true)) {
   warning("[pice] Autosave failed; retaining gameplay view");
   _roomStatus="Autosave failed | L1/Q: retry saving";
  }
 }
 if(_pendingRoom>=0) {
  uint room=_pendingRoom;
  if(!(_pendingChapter==_chapter ? loadRoom(room) : loadChapter(_pendingChapter,room))) { warning("[pice] Cannot initialize room %u",room); _scenePreview=false; }
  return;
 }
 if(_fadeLevel>=0 && _fadeLevel<255 && _scenePalette.size()==768) {
  _fadeRemaining-=ticks;
  while(_fadeRemaining<=0 && _fadeLevel<255) {
   _fadeRemaining+=_fadePeriod; _fadeLevel=MIN(255,_fadeLevel+_fadeStep);
  }
  Common::Array<byte> colors=_scenePalette;
  for(uint i=0;i<240*3;++i) colors[i]=int(colors[i])*(_fadeIn ? _fadeLevel : 256-_fadeLevel)/256;
  _system->getPaletteManager()->setPalette(&colors[0],0,256);
  if(_fadeLevel==255) { if(_fadeIn) { _system->getPaletteManager()->setPalette(&_scenePalette[0],0,256); _fadeLevel=-1; } debug("[pice] palette fade complete"); }
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
 // Standalone preview has no room palette to inherit.
 if (!_graphics.read(resource, data) || !_animation.load(data,resource) || _animation.palette.size()!=768) return false;
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
 if(restored && ConfMan.hasKey("pice_test_chapter") && !loadChapter(ConfMan.getInt("pice_test_chapter"),ConfMan.hasKey("pice_test_room") ? ConfMan.getInt("pice_test_room") : 0)) return Common::Error(Common::kUnknownError,"Chapter test initialization failed");
 if(restored && ConfMan.getBool("pice_test_curator_entry")) _worldVariables.write(true,50,1);
 if(restored && ConfMan.hasKey("pice_test_room") && !ConfMan.hasKey("pice_test_chapter")) restored=loadRoom(ConfMan.getInt("pice_test_room"));
 if(restored && ConfMan.getBool("pice_test_venus_walk") && _roomBankId==1249) {
  _testClickDone=clickRoom(Common::Point(350-cameraX(),240-cameraY()),false);
  debug("[pice] Venus floor click accepted=%d",int(_testClickDone));
 }
 if(restored && ConfMan.getBool("pice_test_book_placement") && _roomBankId==1247) {
  int16 book,result; Common::Array<int16> args;
  if(!_worldVariables.read(true,3,book)) return Common::Error(Common::kUnknownError,"Missing book variable");
  args.push_back(book);
  if(!invoke(67,Common::String(),args,Common::String(),result)) return Common::Error(Common::kUnknownError,"Book fixture pickup failed");
  ConfMan.setInt("pice_test_click_item",book);
 }
 if(restored && ConfMan.getBool("pice_test_snapshot_roundtrip")) {
  Common::Array<byte> payload;
  uint bank=_roomBankId; int chapter=_chapter;
  Common::Point camera(cameraX(),cameraY()); bool frozen=_cameraFrozen;
  bool ok=snapshot(payload,false) && snapshot(payload,true) && _roomBankId==bank && _chapter==chapter &&
   camera==Common::Point(cameraX(),cameraY()) && _cameraFrozen==frozen;
  debug("[pice] snapshot roundtrip bank=%u chapter=%d bytes=%u pass=%d",bank,chapter,payload.size(),ok);
  if(!ok) return Common::Error(Common::kUnknownError,"Snapshot roundtrip test failed");
 }
 if(restored && ConfMan.getBool("pice_test_curator_threat") && _roomBankId==1241) {
  _roomRunner.reset(new ScriptRunner(_roomBank,&_worldVariables,this));
  _roomRunner->navigation()=_navigation;
  _roomRunner->start(21);
 }
 if(restored && ConfMan.getBool("pice_test_ward_walk") && _roomBankId==1199) {
  // Isolate the reported movement from unrelated entry cutscenes.
  _roomRunner.reset(new ScriptRunner(_roomBank,&_worldVariables,this));
  _roomRunner->navigation()=_navigation;
  _actors[_player].feet=Common::Point(_navigation.nodes[21].x,_navigation.nodes[21].y);
  Common::Array<int16> args; args.push_back(_player); args.push_back(18); int16 result;
  debug("[pice] ward walk fixture success=%d",invoke(50,Common::String(),args,Common::String(),result));
  debug("[pice] item names 13=%s 23=%s",_itemLabels[13].c_str(),_itemLabels[23].c_str());
 }
 if(restored && ConfMan.getBool("pice_test_hamsun_full_inventory")) {
  // Original pickup commands on room objects; only this isolated fixture
  // skips walking/clicking. Inventory selection/use below uses real events.
  if(!loadRoom(26)) return Common::Error(Common::kUnknownError,"Drawer test fixture failed");
  Common::Array<int16> pickupIds;
  for(uint i=0;i<_worldItems.size();++i) pickupIds.push_back(_worldItems[i].item);
  for(uint i=0;i<pickupIds.size();++i) { Common::Array<int16> args; args.push_back(pickupIds[i]); int16 value;
   if(!invoke(67,Common::String(),args,Common::String(),value)) return Common::Error(Common::kUnknownError,"Drawer pickup failed");
  }
  if(!loadRoom(2)) return Common::Error(Common::kUnknownError,"Quarters test fixture failed");
  for(int id : {10,13}) { Common::Array<int16> args; args.push_back(id); int16 value;
   if(!invoke(67,Common::String(),args,Common::String(),value)) return Common::Error(Common::kUnknownError,"Quarters pickup failed");
  }
  if(!loadRoom(3)) return Common::Error(Common::kUnknownError,"Bunk test fixture failed");
  _testAmbientStep=1; _testClickDone=true;
 }

#endif
 if (!restored && !showScriptScene()) { warning("[pice] Room initialization unsupported; staying in background preview"); if (!playDialogueSample()) playSpeech(); }
#ifdef PICE_TESTBENCH
 if(ConfMan.hasKey("pice_test_movie") && !playMovie(ConfMan.get("pice_test_movie"),1,1))
  return Common::Error(Common::kUnknownError,"Movie test failed");
#endif
 bool done = false;
 while (!shouldQuit() && !done) {
  Common::Event event;
  while (_eventMan->pollEvent(event)) {
   if(event.type==Common::EVENT_LBUTTONDOWN || event.type==Common::EVENT_RBUTTONDOWN || event.type==Common::EVENT_KEYDOWN) _advanceRequested=true;
   int change = 0;
   if(_gameOver) {
    int choice=-1;
    if(event.type==Common::EVENT_LBUTTONDOWN && event.mouse.x>=180 && event.mouse.x<460 && event.mouse.y>=174 && event.mouse.y<270) choice=(event.mouse.y-174)/24;
    if(event.type==Common::EVENT_KEYDOWN && event.kbd.keycode==Common::KEYCODE_RETURN) choice=0;
    if(choice==0 && !loadLatestSave()) _roomStatus="No valid save available";
    if(choice==1 && !readCheckpoint(true)) _roomStatus="No valid autosave available";
    if(choice==2 && !readCheckpoint(false)) _roomStatus="No valid manual save available";
    if(choice==3) done=true;
    continue;
   }
   if(_activeMenu>=0 && event.type==Common::EVENT_KEYDOWN && event.kbd.keycode==Common::KEYCODE_ESCAPE) { _menuChoice=-2; continue; }
   if(_activeMenu>=0 && (event.type==Common::EVENT_LBUTTONDOWN || event.type==Common::EVENT_RBUTTONDOWN)) {
    if(event.type==Common::EVENT_RBUTTONDOWN || !clickMenu(event.mouse)) _menuChoice=-2;
    continue;
   }
   if(_scenePreview && ambientDialogue() &&
      (event.type==Common::EVENT_LBUTTONDOWN || event.type==Common::EVENT_RBUTTONDOWN)) finishDialogue();
   if(_scenePreview && event.type==Common::EVENT_LBUTTONDOWN && event.mouse.y>=0 && event.mouse.y<40) {
    int item=inventoryVisible() ? inventoryAt(event.mouse) : -1;
    if(item>=0) selectInventoryItem(item);
    continue;
   }

   if(_scenePreview && (event.type==Common::EVENT_LBUTTONDOWN || event.type==Common::EVENT_RBUTTONDOWN) && event.mouse.y>=(!_playerMovement || _player<0 ? 0 : 40) && event.mouse.y<(!_playerMovement || _player<0 ? 400 : 360)) {
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
