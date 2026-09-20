// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/pice.h"
#include "pice/state.h"
#include "pice/voice.h"
#include "common/savefile.h"
#include "common/system.h"
#include "common/config-manager.h"
#include "common/endian.h"
#include "common/textconsole.h"
#include "common/memstream.h"
#include "graphics/paletteman.h"
#include "audio/decoders/wave.h"
#include "audio/audiostream.h"
namespace Pice {
static uint32 checksum(const Common::Array<byte> &bytes) {
 uint32 hash=2166136261U; for(uint i=0;i<bytes.size();++i) hash=(hash^bytes[i])*16777619U; return hash;
}
bool PiceEngine::requestSave(const Common::String &name,int16 local,const Common::String &key) {
 // Original 0x20cf -> main loop 0x18f9c -> 0xe504. Queue the request;
 // serialize only after the requesting task has advanced its PC.
 if(_savePending || !name.equalsIgnoreCase("G_IceTxt") || local!=2 || key!="SAV00" || _dialogueActor>=0 || _pendingRoom>=0) return false;
 _savePending=true; debug("[pice] queued autosave key=%s",key.c_str()); return true;
}
bool PiceEngine::snapshot(Common::Array<byte> &bytes,bool loading,bool apply) {
 if(!loading && (_gameOver || _activeMenu>=0 || _actionEntry>=0 || !_scenePreview || !_roomRunner || _dialogueActor>=0 || _closeup || _pendingRoom>=0)) return false;
 // Parse into copies, reconstruct resource references and validate everything
 // before replacing live state. Saves contain resource IDs/hashes, not assets.
 int16 previousRoom=_previousRoom;
 auto owned=_ownedItems; int16 selectedItem=_selectedItem;
 auto actors=_actors; auto items=_items; auto worldItems=_worldItems;
 auto hotspots=_hotspots; auto actions=_actions; auto bindings=_animationSounds;
 auto menuEnabled=_menuEnabled; int16 lastMenuChoice=_lastMenuChoice;
 auto menus=_menus; auto sounds=_sounds; RoomSound music=_music; auto vars=_worldVariables;
 auto nav=_navigation; auto scenerySprite=_scenery; auto panel=_inventoryPanel;
 auto colors=_scenePalette; auto styles=Common::Array<byte>();
 for(uint i=0;i<12;++i) styles.push_back(_textStyles[i/2][i%2]);
 uint bankId=_roomBankId,backgroundId=_sceneBackground,paletteId=_scenePaletteId;
 int16 player=_player,camera=_cameraActor,selection=_inventorySelection;
 uint16 locks=_inputLocks;
 bool pulse=_palettePulse;
 int16 period=_pulsePeriod,remaining=_pulseRemaining,offset=_pulseOffset,delta=_pulseDelta;
 uint32 seed=_random.getSeed();
 Common::Array<byte> tasks,backgroundBytes,bankBytes;
 if(!loading) { StateIO out(tasks,false); if(!_roomRunner->syncState(out)) return false; }
 StateIO io(bytes,loading);
 uint32 version=7; io.number(version); if(version<1 || version>7) return false;
 io.number(bankId); io.number(backgroundId); io.number(paletteId); io.number(seed);
 if(!io.ok || bankId<1140 || bankId>=1180 || backgroundId>=_graphics.count() || paletteId>=_graphics.count() ||
    !_graphics.read(bankId,bankBytes) || !_graphics.read(backgroundId,backgroundBytes)) return false;
 uint32 bankHash=checksum(bankBytes),backgroundHash=checksum(backgroundBytes),expectedBank=bankHash,expectedBackground=backgroundHash;
 io.number(expectedBank); io.number(expectedBackground);
 uint16 width,height;
 if(!io.ok || bankHash!=expectedBank || backgroundHash!=expectedBackground || !decodeImageHeader(backgroundBytes,width,height) || width!=640 || height>320) return false;
 ScriptResource bank; if(!bank.load(bankBytes) || !vars.syncState(io)) return false;
 io.bytes(tasks,16384); if(!io.ok) return false;
 ScriptRunner candidate(bank,&vars,this); StateIO taskInput(tasks,true);
 if(!candidate.syncState(taskInput) || !taskInput.finished()) return false;
 auto sprite=[&](SpriteResource &resource) {
  int32 id=resource.archiveId; uint32 hash=resource.fingerprint; io.number(id); io.number(hash);
  if(!io.ok || id < -1 || id>=int(_graphics.count())) { io.ok=false; return; }
  if(id==-1) { if(hash) io.ok=false; if(loading) resource=SpriteResource(); return; }
  if(loading) {
   Common::Array<byte> raw;
   if(!_graphics.read(id,raw) || checksum(raw)!=hash || !resource.load(raw,id)) io.ok=false;
  }
 };
 auto actor=[&](RoomActor &a) {
  io.number(a.active); sprite(a.sprite); io.number(a.resource); io.number(a.sequence); io.number(a.node);
  io.point(a.feet); io.point(a.labelPosition); io.number(a.routeEnabled); io.number(a.step); io.number(a.frameTicks);
  io.number(a.animationMode); io.point(a.animationOrigin); io.point(a.walkTarget);
  io.number(a.walkProgress); io.number(a.walkDistance); io.number(a.walkDirection); io.number(a.pathIndex); io.number(a.animationPaused);
  if(io.count(a.path,52)) for(uint i=0;i<a.path.size();++i) io.point(a.path[i]);
  if(a.active && (!a.sprite.hasSequence(a.sequence) || a.step>=a.sprite.sequences[a.sequence].steps.size())) io.ok=false;
  if(a.animationMode<0 || a.animationMode>4 || a.animationMode==3 || a.animationPaused || a.pathIndex>a.path.size() ||
     (a.animationMode==4 && (a.walkDistance<=0 || a.walkProgress<0 || a.walkProgress>a.walkDistance || a.pathIndex>=a.path.size()))) io.ok=false;
 };
 if(io.count(actors,20)) for(uint i=0;i<actors.size();++i) actor(actors[i]);
 if(io.count(items,100)) for(uint i=0;i<items.size();++i) actor(items[i]);
 sprite(scenerySprite); sprite(panel);
 if(!io.ok) { warning("[pice] Invalid snapshot actors/resources"); return false; }
 auto validActor=[&](int id) { return id>=0 && uint(id)<actors.size() && actors[id].active; };
 if(io.count(worldItems,10)) for(uint i=0;i<worldItems.size();++i) {
  io.number(worldItems[i].actor); io.number(worldItems[i].item);
  if((worldItems[i].actor!=-1 && !validActor(worldItems[i].actor)) || worldItems[i].item<0 || uint(worldItems[i].item)>=items.size()) io.ok=false;
 }
 if(io.count(hotspots,50)) for(uint i=0;i<hotspots.size();++i) {
  Hotspot &h=hotspots[i]; io.number(h.active); io.number(h.actor); io.number(h.approach);
  io.number(h.bounds.left); io.number(h.bounds.top); io.number(h.bounds.right); io.number(h.bounds.bottom); io.string(h.label);
  if(h.active && h.actor!=-1 && !validActor(h.actor)) io.ok=false;
 }
 if(io.count(actions,70)) for(uint i=0;i<actions.size();++i) {
  Action &a=actions[i]; io.number(a.hotspot); io.number(a.verb); io.number(a.approach); io.number(a.entry); io.number(a.fallback);
  if(a.hotspot<0 || uint(a.hotspot)>=hotspots.size() || a.entry<0 || uint(a.entry)>=bank.count() || a.fallback<0 || uint(a.fallback)>=bank.count()) io.ok=false;
 }
 if(io.count(menus,20)) for(uint i=0;i<menus.size();++i) if(io.count(menus[i],200)) for(uint j=0;j<menus[i].size();++j) io.string(menus[i][j]);
 if(!io.ok) { warning("[pice] Invalid snapshot objects/actions"); return false; }
 RoomNavigation rebuilt; rebuilt.generatedEnabled=loading ? true : nav.generatedEnabled;
 for(uint i=0;i<nav.nodes.size();++i) {
  auto &n=nav.nodes[i]; io.number(n.active); io.number(n.x); io.number(n.y); io.number(n.scale);
  if(n.active && !rebuilt.node(i,n.x,n.y,n.scale)) io.ok=false;
 }
 for(uint i=0;i<nav.edges.size();++i) {
  auto &e=nav.edges[i]; io.number(e.active); io.number(e.a); io.number(e.b); io.number(e.enabled);
  if(e.active && !rebuilt.edge(i,e.a,e.b,e.enabled)) io.ok=false;
 }
 for(uint i=0;i<nav.triangles.size();++i) {
  auto &t=nav.triangles[i]; io.number(t.active); io.number(t.a); io.number(t.b); io.number(t.c); io.number(t.enabled);
  if(t.active && !rebuilt.triangle(i,t.a,t.b,t.c,t.enabled)) io.ok=false;
 }
 if(!io.ok) { warning("[pice] Invalid snapshot navigation"); return false; }
 Common::Array<byte> playing; playing.resize(sounds.size(),0);
 Common::Array<uint32> elapsed; elapsed.resize(sounds.size(),0);
 if(io.count(sounds,25)) {
  playing.resize(sounds.size(),0); elapsed.resize(sounds.size(),0);
  for(uint i=0;i<sounds.size();++i) {
   RoomSound &sound=sounds[i]; bool isPlaying=!loading && sound.active && _mixer->isSoundHandleActive(sound.handle);
   uint32 time=isPlaying ? sound.elapsedBase+_mixer->getSoundElapsedTime(sound.handle) : 0;
   io.number(sound.active); io.number(sound.persistent); io.number(sound.resource); io.number(sound.volume); io.number(sound.loops);
   io.number(isPlaying); io.number(time); playing[i]=isPlaying; elapsed[i]=time;
   if(sound.active) {
    Common::Array<byte> raw; uint32 start,size;
    if(sound.resource>=_soundArchive.count() || !_soundArchive.read(sound.resource,raw) || !voicePayload(raw,start,size)) io.ok=false;
    else if(loading && isPlaying) {
     Common::MemoryReadStream stream(&raw[start],size); Common::ScopedPtr<Audio::SeekableAudioStream> wave(Audio::makeWAVStream(&stream,DisposeAfterUse::NO));
     if(!wave || !wave->getLength().msecs()) io.ok=false;
    }
   } else if(isPlaying) io.ok=false;
  }
 }
 if(io.count(bindings,30)) for(uint i=0;i<bindings.size();++i) {
  AnimationSound &b=bindings[i]; io.number(b.actor); io.number(b.sound); io.number(b.volume); io.number(b.sequence); io.number(b.step); io.number(b.fired);
  if(!validActor(b.actor) || b.sound<0 || uint(b.sound)>=sounds.size() || !sounds[b.sound].active || b.volume<0 || b.volume>100 || b.sequence < -1 || b.step<0) io.ok=false;
 }
 if(!io.ok) { warning("[pice] Invalid snapshot sound bindings"); return false; }
 io.number(player); io.number(camera); io.number(selection); io.number(locks);
 io.number(pulse); io.number(period); io.number(remaining); io.number(offset); io.number(delta);
 io.bytes(colors,768); io.bytes(styles,12);
 bool musicPlaying=!loading && music.active && _mixer->isSoundHandleActive(music.handle);
 uint32 musicElapsed=musicPlaying ? music.elapsedBase+_mixer->getSoundElapsedTime(music.handle) : 0;
 if(version>=2) {
  io.number(music.active); io.number(music.persistent); io.number(music.resource); io.number(music.volume); io.number(music.loops);
  io.number(musicPlaying); io.number(musicElapsed);
  if(music.active) {
   Common::Array<byte> raw; uint32 start,size;
   if(music.resource<308 || music.resource>=314 || !_soundArchive.read(music.resource,raw) || !voicePayload(raw,start,size)) io.ok=false;
  } else if(musicPlaying) io.ok=false;
 } else { music=RoomSound(); musicPlaying=false; musicElapsed=0; }
 if(version>=3) { io.bytes(owned,100); io.number(selectedItem); }
 else { owned.clear(); owned.resize(items.size(),0); selectedItem=-1; }
 if(version>=4) io.number(rebuilt.generatedEnabled);
 for(uint i=0;i<actions.size();++i) {
  if(version>=5) io.number(actions[i].item); else actions[i].item=-1;
  if(actions[i].item < -1 || actions[i].item>=int(items.size()) || (actions[i].item>=0 && actions[i].verb!=9)) return false;
 }
 if(version>=6) io.number(previousRoom); else previousRoom=-1;
 if(previousRoom < -1 || previousRoom>=40) return false;
 if(version>=7) {
  if(!io.count(menuEnabled,20) || menuEnabled.size()!=menus.size()) return false;
  for(uint i=0;i<menuEnabled.size();++i) {
   io.bytes(menuEnabled[i],200);
   if(menuEnabled[i].size()!=menus[i].size()) return false;
   for(uint j=0;j<menuEnabled[i].size();++j) if(menuEnabled[i][j]>1) return false;
  }
  io.number(lastMenuChoice);
  if(lastMenuChoice < -1 || lastMenuChoice>=200) return false;
 } else {
  menuEnabled.clear(); menuEnabled.resize(menus.size());
  for(uint i=0;i<menus.size();++i) menuEnabled[i].resize(menus[i].size(),0);
  lastMenuChoice=-1;
 }
 if(owned.size()!=items.size() || selectedItem < -1 || selectedItem>=int(owned.size()) ||
    (selectedItem>=0 && !owned[selectedItem])) return false;
 for(uint i=0;i<owned.size();++i) if(owned[i]>1) return false;
 if(!io.finished() || colors.size()!=768 || styles.size()!=12 || (player!=-1 && !validActor(player)) ||
    (camera!=-1 && !validActor(camera)) || period<=0 || offset<0 || offset>255 || delta < -255 || delta>255) return false;
 if(!loading || !apply) return true;
 for(uint i=0;i<_sounds.size();++i) _mixer->stopHandle(_sounds[i].handle);
 _mixer->stopHandle(_speechHandle); _mixer->stopHandle(_music.handle); _music=music; _music.handle=Audio::SoundHandle(); _roomRunner.reset(); _roomBank=bank; _roomBankId=bankId; _worldVariables=vars;
 _roomRunner.reset(new ScriptRunner(_roomBank,&_worldVariables,this));
 StateIO taskRestore(tasks,true); if(!_roomRunner->syncState(taskRestore)) return false;
 _navigation=rebuilt; _roomRunner->navigation()=rebuilt;
 _gameOver=false;
 _previousRoom=previousRoom;
 _actors=actors; _items=items; _worldItems=worldItems; _hotspots=hotspots; _actions=actions; _animationSounds=bindings;
 _ownedItems=owned; _selectedItem=selectedItem;
 _menuEnabled=menuEnabled; _lastMenuChoice=lastMenuChoice; _activeMenu=_menuChoice=-1;
 _menus=menus; _sounds=sounds; _scenery=scenerySprite; _inventoryPanel=panel;
 _sceneBackground=backgroundId; _scenePaletteId=paletteId; _scenePixels=backgroundBytes; _scenePalette=colors;
 _player=player; _cameraActor=camera; _inventorySelection=selection; _inputLocks=locks;
 _palettePulse=pulse; _pulsePeriod=period; _pulseRemaining=remaining; _pulseOffset=offset; _pulseDelta=delta;
 for(uint i=0;i<12;++i) _textStyles[i/2][i%2]=styles[i];
 _actionEntry=-1; _actionContext=-1; _actionHotspot=-1; _actionWalking=false; _hoverHotspot=-1;
 _random.setSeed(seed); _dialogueActor=-1; _closeup=false; _subtitle.clear(); _savePending=false; _pendingRoom=-1;
 _buildingNavigation=nullptr; _spriteMode=false; _scenePreview=true; _snapshotPending=false;
 _roomClock=_roomRenderClock=_system->getMillis(); _roomStatus="Checkpoint restored | F5 save | F7 load";
 _system->getPaletteManager()->setPalette(&_scenePalette[0],0,256);
 uint brightest=0; for(uint i=0;i<256;++i) {
  uint light=_scenePalette[i*3]+_scenePalette[i*3+1]+_scenePalette[i*3+2];
  if(light>brightest) { brightest=light; _textColor=i; }
 }
 for(uint i=0;i<_sounds.size();++i) { _sounds[i].handle=Audio::SoundHandle(); if(playing[i] && !startSound(i,_sounds[i].loops,elapsed[i])) warning("[pice] Cannot restore sound %u",i); }
 uint ownedCount=0; for(uint i=0;i<_ownedItems.size();++i) if(_ownedItems[i]) ++ownedCount;
 debug("[pice] restored navigation generated=%d edge51=%d edge52=%d",_navigation.generatedEnabled,_navigation.edges[51].enabled,_navigation.edges[52].enabled);
 debug("[pice] restored inventory owned=%u selected=%d",ownedCount,_selectedItem);
 debug("[pice] restored music active=%d playing=%d elapsed=%u",_music.active,musicPlaying,musicElapsed);
 if(musicPlaying && !startSample(_music,_music.loops,musicElapsed,true)) warning("[pice] Cannot restore music");
 debug("[pice] checkpoint restored bank=%u actors=%u items=%u task20-pc=%u",bankId,actors.size(),items.size(),bank.count()>20 ? _roomRunner->position(20) : 0);
 return renderRoom();
}
static Common::String saveName(bool automatic) {
 return ConfMan.getActiveDomainName()+(automatic ? "-pice-autosave.sav" : "-pice-slot0.sav");
}
bool PiceEngine::loadLatestSave() {
 bool automatic=true; // Legacy saves have no ordering marker; prefer autosave.
 Common::ScopedPtr<Common::InSaveFile> latest(_system->getSavefileManager()->openForLoading(ConfMan.getActiveDomainName()+"-pice-latest"));
 if(latest && latest->size()==1) automatic=latest->readByte()!='M';
 return readCheckpoint(automatic) || readCheckpoint(!automatic);
}
bool PiceEngine::writeCheckpoint(bool automatic) {
 Common::Array<byte> payload; if(!snapshot(payload,false)) return false;
 // Reparse the full generated state before opening the output file.
 if(!snapshot(payload,true,false)) return false;
 Common::ScopedPtr<Common::OutSaveFile> file(_system->getSavefileManager()->openForSaving(saveName(automatic),false));
 if(!file) return false;
 file->write("PICESV01",8); file->writeUint32LE(payload.size()); file->writeUint32LE(checksum(payload));
 file->write(&payload[0],payload.size()); file->finalize(); if(file->err()) return false; file.reset();
 if(!readCheckpoint(automatic,false)) return false;
 Common::ScopedPtr<Common::OutSaveFile> latest(_system->getSavefileManager()->openForSaving(ConfMan.getActiveDomainName()+"-pice-latest",false));
 if(latest) { latest->writeByte(automatic ? 'A' : 'M'); latest->finalize(); }
 _roomStatus=automatic ? "Autosaved | F5 save | F7 load" : "Checkpoint saved | F5 save | F7 load";
 debug("[pice] checkpoint saved automatic=%d bytes=%u checksum=%08x",automatic,payload.size(),checksum(payload));
 return true;
}
bool PiceEngine::readCheckpoint(bool automatic,bool apply) {
 Common::ScopedPtr<Common::InSaveFile> file(_system->getSavefileManager()->openForLoading(saveName(automatic)));
 if(!file || file->size()<16 || file->size()>16*1024*1024) return false;
 char magic[8]; if(file->read(magic,8)!=8 || memcmp(magic,"PICESV01",8)) return false;
 uint32 size=file->readUint32LE(),hash=file->readUint32LE();
 if(size!=uint32(file->size()-16) || !size) return false;
 Common::Array<byte> payload; payload.resize(size);
 if(file->read(&payload[0],size)!=size || file->err() || checksum(payload)!=hash) return false;
 return snapshot(payload,true,apply);
}
}
