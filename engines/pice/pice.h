// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PICE_ENGINE_H
#define PICE_ENGINE_H
#include "engines/engine.h"
#include "audio/mixer.h"
#include "common/ptr.h"
#include "common/random.h"
#include "pice/kro.h"
#include "pice/resource-names.h"
#include "pice/script.h"
#include "pice/sprite.h"
#include "pice/dialogue.h"
#include "pice/lipsync.h"
namespace Pice {
class PiceEngine : public Engine, public ScriptSceneSink {
public:
 explicit PiceEngine(OSystem *system) : Engine(system), _picture(0), _speech(0), _textColor(255) {}
 Common::Error run() override;
private:
 bool showPicture(uint position);
 bool requestSave(const Common::String &name,int16 local,const Common::String &key) override;
 bool snapshot(Common::Array<byte> &bytes,bool loading,bool apply=true);
 bool writeCheckpoint(bool automatic);
 bool readCheckpoint(bool automatic,bool apply=true);
 bool _savePending=false;
#ifdef PICE_TESTBENCH
 bool _testSavePending=false,_testClickDone=false,_testUseDone=false;
 uint32 _testSaveAt=0;
#endif
 bool background(const Common::String &name,int16 local,int16 flags) override;
 bool palette(const Common::String &name,int16 local) override;
 bool scenery(const Common::String &name,int16 local) override;
 bool volume(int16 handle,int16 value) override;
 bool createActor(const Common::String &name,int16 local,int16 sequence,int16 node,int16 &id) override;
 bool invoke(uint16 opcode,const Common::String &name,const Common::Array<int16> &args,const Common::String &key,int16 &result) override;
 struct Hotspot { bool active=true; int16 actor=-1,approach=-1; Common::Rect bounds; Common::String label; };
 struct AnimationSound { int16 actor=-1,sound=-1,volume=100,sequence=-1,step=-1; bool fired=false; };
 Common::Array<AnimationSound> _animationSounds;
 SpriteResource _inventoryPanel;
 int16 _inventorySelection=0;
 bool _palettePulse=false;
 int16 _pulsePeriod=5,_pulseRemaining=5,_pulseOffset=10,_pulseDelta=15;
 struct Action { int16 hotspot=-1, verb=-1, approach=-1, entry=-1, fallback=0; };
 Common::Array<Hotspot> _hotspots;
 Common::Array<Action> _actions;
 bool context(int16 index,int16 &value) const override;
 int hotspotAt(Common::Point point) const;
 bool clickRoom(Common::Point point,bool rightButton);
 void tickAction();
 int16 _actionContext=-1,_actionEntry=-1,_actionHotspot=-1;
 bool _actionWalking=false;
 int _hoverHotspot=-1;
 int16 _player=-1,_cameraActor=-1;
 bool createMenu(const Common::String &name,int16 local,int16 &id) override;
 Common::Array<Common::Array<Common::String> > _menus;
 bool createItem(const Common::String &name,int16 local,int16 sequence,int16 &id) override;
 struct RoomActor { bool active=true; SpriteResource sprite; int16 resource=0,sequence=0,node=0; Common::Point feet, labelPosition; bool routeEnabled=false; uint step=0; int32 frameTicks=0; int16 animationMode=0; Common::Point animationOrigin,walkTarget; int32 walkProgress=0,walkDistance=0; int16 walkDirection=0; Common::Array<Common::Point> path; uint pathIndex=0; bool animationPaused=false; };
 bool startWalkLeg(RoomActor &actor);
 int16 storeActor(const RoomActor &actor);
 bool runActorLifecycleTests();
 Common::Array<RoomActor> _actors;
 bool animatedDialogue(const Common::String &textName,const Common::String &key,const Common::String &animationName,const Common::Array<int16> &args) override;
 Common::RandomSource _random{"pice"};
 int16 _dialogueActor=-1,_dialogueMinSequence=0,_dialogueMaxSequence=0,_dialogueMinPause=0,_dialogueMaxPause=0;
 RoomActor _dialogueRestore;
 bool closeupDialogue(const Common::String &animationName,const Common::String &textName,const Common::String &key,const Common::Array<int16> &args,int16 &actor) override;
 bool loadDialogue(const Common::String &name,int16 local,const Common::String &key,DialogueLine &line,Common::Array<uint32> &voices);
 bool startDialoguePage();
 Common::Array<Common::Array<Common::String> > _dialoguePages;
 Common::Array<uint32> _dialogueVoices;
 Common::Array<VoiceAnimation> _closeupAnimations;
 SpriteResource _closeupSprite;
 Common::Point _closeupOrigin;
 uint _dialoguePage=0;
 uint32 _pageStart=0;
 bool _closeup=false;
 byte _textStyles[6][2]={{0,0},{0,0},{32,43},{0,0},{0,0},{0,0}};


 Common::Array<RoomActor> _items;
 Common::Array<byte> _ownedItems;
 int16 _selectedItem=-1;
 struct WorldItem { int16 item=-1,actor=-1; };
 Common::Array<WorldItem> _worldItems;
 ScriptVariables _worldVariables;
 RoomNavigation *_buildingNavigation=nullptr;
 bool showScriptScene();
 bool loadRoom(uint room);
 bool playMovie(const Common::String &name,int16 before,int16 after) override;
 bool changeRoom(int16 chapter,int16 room) override;
 int16 _pendingRoom=-1;
 uint _roomBankId=1140;
 bool renderRoom();
 void tickRoom();
 bool actorStepReached(int16 actor,int16 step) const override;
 bool actorBusy(int16 actor) const override;
 bool soundBusy(int16 sound) const override;
 ScriptResource _roomBank;
 Common::ScopedPtr<ScriptRunner> _roomRunner;
 uint32 _roomClock=0,_roomRenderClock=0;
 uint16 _inputLocks=0;

 Common::Array<byte> _scenePixels, _scenePalette, _roomCanvas;
 SpriteResource _scenery;
 RoomNavigation _navigation;
 Common::String _roomStatus;
 bool _scenePreview=false,_snapshotPending=false;
 uint32 _sceneBackground=0, _scenePaletteId=0;
 void playSpeech();
 bool playVoice(uint resource);
 bool playDialogueSample();
 ResourceNames _languageNames;
 Common::String _subtitle;
 void drawLabels();
 bool showAnimation(uint resource);
 void advanceAnimation();
 SpriteResource _animation;
 Common::Array<SpriteImage> _sprites;
 Common::Array<byte> _canvas;
 uint _animationId = 0, _sequence = 0, _step = 0;
 uint32 _nextFrame = 0;
 int _originX = 0, _originY = 0;
 bool _spriteMode = false;
 byte _backgroundColor = 0;
 struct RoomSound { bool active=false,persistent=false; uint resource=0,loops=1; uint32 elapsedBase=0; byte volume=255; Audio::SoundHandle handle; };
 RoomSound _music;
 bool startSample(RoomSound &sound,uint loops,uint32 elapsed,bool music);
 Common::Array<RoomSound> _sounds;
 bool startSound(uint id,uint loops,uint32 elapsed=0);
 KroArchive _graphics, _language, _soundArchive;
 Common::Array<uint> _pictures;
 uint _picture, _speech;
 byte _textColor;
 Audio::SoundHandle _speechHandle;
};
}
#endif
