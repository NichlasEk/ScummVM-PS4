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
namespace Pice {
class PiceEngine : public Engine, public ScriptSceneSink {
public:
 explicit PiceEngine(OSystem *system) : Engine(system), _picture(0), _speech(0), _textColor(255) {}
 Common::Error run() override;
private:
 bool showPicture(uint position);
 bool background(const Common::String &name,int16 local,int16 flags) override;
 bool palette(const Common::String &name,int16 local) override;
 bool scenery(const Common::String &name,int16 local) override;
 bool volume(int16 handle,int16 value) override;
 bool createActor(const Common::String &name,int16 local,int16 sequence,int16 node,int16 &id) override;
 bool invoke(uint16 opcode,const Common::String &name,const Common::Array<int16> &args,const Common::String &key,int16 &result) override;
 struct Hotspot { int16 actor=-1,approach=-1; Common::Rect bounds; Common::String label; };
 struct AnimationSound { int16 actor=-1,sound=-1,volume=100,sequence=-1,step=-1; bool fired=false; };
 Common::Array<AnimationSound> _animationSounds;
 SpriteResource _inventoryPanel;
 int16 _inventorySelection=0;
 bool _palettePulse=false;
 int16 _pulsePeriod=5,_pulseRemaining=5,_pulseOffset=10,_pulseDelta=15;
 struct Action { int16 hotspot=-1, verb=-1, item=-1, entry=-1, fallback=0; };
 Common::Array<Hotspot> _hotspots;
 Common::Array<Action> _actions;
 int16 _player=-1;
 bool createMenu(const Common::String &name,int16 local,int16 &id) override;
 Common::Array<Common::Array<Common::String> > _menus;
 bool createItem(const Common::String &name,int16 local,int16 sequence,int16 &id) override;
 struct RoomActor { SpriteResource sprite; int16 resource=0,sequence=0,node=0; Common::Point feet, labelPosition; bool routeEnabled=false; uint step=0; int32 frameTicks=0; int16 animationMode=0; Common::Point animationOrigin,walkTarget; int32 walkProgress=0,walkDistance=0; int16 walkDirection=0; bool animationPaused=false; };
 Common::Array<RoomActor> _actors;
 bool animatedDialogue(const Common::String &textName,const Common::String &key,const Common::String &animationName,const Common::Array<int16> &args) override;
 Common::RandomSource _random{"pice"};
 int16 _dialogueActor=-1,_dialogueMinSequence=0,_dialogueMaxSequence=0,_dialogueMinPause=0,_dialogueMaxPause=0;
 RoomActor _dialogueRestore;

 Common::Array<RoomActor> _items;
 struct WorldItem { int16 item=-1,actor=-1; };
 Common::Array<WorldItem> _worldItems;
 ScriptVariables _worldVariables;
 RoomNavigation *_buildingNavigation=nullptr;
 bool showScriptScene();
 bool renderRoom();
 void tickRoom();
 bool actorBusy(int16 actor) const override;
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
 struct RoomSound { uint resource=0; byte volume=255; Audio::SoundHandle handle; };
 Common::Array<RoomSound> _sounds;
 bool startSound(uint id,uint loops);
 KroArchive _graphics, _language, _soundArchive;
 Common::Array<uint> _pictures;
 uint _picture, _speech;
 byte _textColor;
 Audio::SoundHandle _speechHandle;
};
}
#endif
