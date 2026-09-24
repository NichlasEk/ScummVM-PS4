// SPDX-License-Identifier: GPL-3.0-or-later
#include "sotc/dialoguebox.h"
#include "sotc/sotc.h"
#include "common/error.h"
#include "common/system.h"
#include "graphics/font.h"
#include "sotc/pak.h"
#include "sotc/va2.h"
#include "sotc/sequence.h"
#include "sotc/script.h"
#include "sotc/startup.h"
#include "sotc/scriptmachine.h"
#include "sotc/sound.h"
#include "sotc/opening.h"
#include "sotc/openingscene.h"
#include "sotc/toolbar.h"
#include "sotc/journal.h"
#include "sotc/inventory.h"
#include "sotc/save.h"
#include "common/config-manager.h"
#include "common/file.h"
#include "common/events.h"
#include "common/textconsole.h"
#include "graphics/paletteman.h"
#include "graphics/fontman.h"
#include "graphics/surface.h"
#include "engines/util.h"
#include "engines/dialogs.h"
#include "gui/message.h"
namespace Sotc {
static void inspectRoomScripts(int bank) {
 Common::File file;
 if (!file.open(Common::Path(Common::String::format("R%02d.CC4", bank)))) {
  warning("[sotc-cc4] Cannot open room scripts for bank %d", bank); return;
 }
 if (file.size() < 4 || file.size() > 4 * 1024 * 1024) {
  warning("[sotc-cc4] Invalid bank size"); return;
 }
 Common::Array<byte> bytes; bytes.resize(file.size());
 if (file.read(bytes.data(), bytes.size()) != bytes.size()) {
  warning("[sotc-cc4] Short read"); return;
 }
 Cc4View archive(bytes.data(), bytes.size());
 unsigned blocks = cc4Count(archive, 4), scripts = 0;
 for (unsigned i = 0; i < blocks; ++i) {
  Cc4View block;
  if (!cc4Entry(archive, 4, i, block) || !cc4Count(block, 2)) {
   warning("[sotc-cc4] Invalid block %u", i); return;
  }
  scripts += cc4Count(block, 2);
 }
 debug("[sotc-cc4] R%02d blocks=%u scripts=%u (inspection only)", bank, blocks, scripts);
 if (!blocks) { warning("[sotc-cc4] Invalid directory"); return; }
 Cc4View block, script;
 if (!cc4Entry(archive, 4, 0, block) || !cc4Entry(block, 2, 0, script)) return;
 uint32 pc = 0;
 for (unsigned limit = 0; limit < 128; ++limit) {
  ScriptInstruction op;
  ScriptDecode status = decodeScript(script, pc, op);
  debug("[sotc-script] block=0 script=0 pc=%u opcode=%u status=%d next=%u target=%d",
        pc, op.opcode, int(status), op.next, int(op.target));
  // Do not invent initial variables, follow branches or run game effects.
  if (status != kInstruction || op.opcode == 2 || op.opcode == 3 || op.opcode == 10 || op.opcode == 80) break;
  pc = op.next;
 }
}
Common::Error SotcEngine::run() {
 syncSoundSettings();
 initGraphics(640, 480);
 Common::Array<byte> frame; frame.resize(640 * 480);
 Pak resources; Common::Array<byte> palette, image;
 if (!resources.open("RES.PAK")) return Common::Error(Common::kReadingFailed, "Cannot read RES.PAK");
 int pal = 0; Common::Array<uint> palettes;
 for (uint i = 0; i < resources.entries.size(); ++i)
  if (resources.entries[i].name.hasSuffix(".PAL") && resources.entries[i].size == 768) palettes.push_back(i);
 if (palettes.empty()) return Common::Error(Common::kReadingFailed, "No palettes in RES.PAK");
 Pak rooms; int bank = 0, scene = 0; bool reload = true, redraw = true;
 Common::Array<uint> backgrounds;
 Pak actors; Common::Array<byte> actorData, actorPixels, actorMask;
 Common::Array<uint> actorEntries;
 actorPixels.resize(640 * 256); actorMask.resize(640 * 256);
 bool showActor = true, loadActor = true; int actor = 0; unsigned actorFrame = 0;
 uint32 nextFrame = 0;
 unsigned sequence = 0;
 Common::Array<byte> startupPixels; Common::String startupStatus;
 Toolbar toolbar; Common::Array<byte> toolbarData;
 if (!resources.load(3,toolbarData) || !toolbar.load(toolbarData))
  return Common::Error(Common::kReadingFailed,"Cannot load original toolbar");
 Inventory inventory;
 Journal journal; bool showJournal=false,showInventory=false;
 Common::Array<unsigned> inventoryIds;Common::Array<Common::String> inventoryNames;unsigned inventorySelection=0;
 Common::Array<byte> journalFont,journalText,inventoryIcons;unsigned inventoryIcon=0;
 if(!resources.load(4,inventoryIcons))return Common::Error(Common::kReadingFailed,"Cannot load inventory icons");
 Common::File journalFile;
 if(!resources.load(0,journalFont) || !journalFile.open(Common::Path("E.CC4")) || journalFile.size()>4*1024*1024)
  return Common::Error(Common::kReadingFailed,"Cannot load diary resources");
 journalText.resize(journalFile.size());
 if(journalFile.read(journalText.data(),journalText.size())!=journalText.size() || !journal.load(toolbarData,journalFont,journalText))
  return Common::Error(Common::kReadingFailed,"Invalid diary resources");
 if(!inventory.load(toolbarData,inventoryIcons,journalFont))
  return Common::Error(Common::kReadingFailed,"Invalid inventory resources");
 DialogueBox dialogueBox,pickupBox; unsigned pickupRemaining=0,pickupBank=0,pickupRoom=0;
 Common::Array<byte> bubbleData;
 if(!resources.load(1,bubbleData) || !dialogueBox.load(journalFont,bubbleData) || !pickupBox.load(journalFont,bubbleData))
  return Common::Error(Common::kReadingFailed,"Cannot load original dialogue resources");
 SoundEffects sound(_mixer);
 ScriptMachine startupMachine; startupMachine.setSound(&sound); unsigned scriptTicks = 0; Common::String scriptStatus;
 StartupScene startupScene; startupMachine.setAnimation(&startupScene);
 bool startupAvailable = false, showStartup = false;
 Common::SaveFileManager &savefiles=*_system->getSavefileManager();
 const Common::String savePrefix=ConfMan.getActiveDomainName()+"-sotc";
 bool savedGame=hasQuickSave(savefiles,savePrefix);
 bool mainMenu = true; int menuSelection = 1;
 Common::Array<byte> menuBackground;
 Pak intro;
 if (intro.open("D05.PAK")) {
  for (unsigned i=0;i<intro.entries.size();++i)
   if (intro.entries[i].name == "INTRO1.RAW") intro.load(i,menuBackground);
 }
 if (menuBackground.size()!=64000) menuBackground.clear();
 OpeningPlan openingPlan = {};
 Common::Array<byte> openingImages[2], openingScript;
 Common::File openingFile;
 bool showOpening=false;
 if (openingFile.open(Common::Path("R05.CC4")) && openingFile.size()>0 && openingFile.size()<4*1024*1024) {
  openingScript.resize(openingFile.size());
  if (openingFile.read(openingScript.data(),openingScript.size())==openingScript.size() &&
      openingPlan.load(Cc4View(openingScript.data(),openingScript.size()))) {
   showOpening=intro.load(openingPlan.logos[0].background,openingImages[0]) && openingImages[0].size()==64000 &&
               intro.load(openingPlan.logos[1].background,openingImages[1]) && openingImages[1].size()==64000;
  }
 }
 OpeningClock openingClock(openingPlan);
 OpeningScene animatedOpening(sound), cdOpening(sound); bool openingAnimated=false, showStory=false, showCd=false;
 Common::Array<byte> cdScript,cdText,cdPalettes[2];
 auto readCinematicFile=[](const char *name,Common::Array<byte> &out) {
  Common::File file;
  if (!file.open(Common::Path(name)) || file.size()<1 || file.size()>4*1024*1024) return false;
  out.resize(file.size()); return file.read(out.data(),out.size())==out.size();
 };
 if (showOpening && readCinematicFile("R09.CC4",cdScript)) {
  if (!readCinematicFile("E.CC4",cdText) || !resources.load(8,cdPalettes[0]) || !resources.load(7,cdPalettes[1]) ||
      cdPalettes[0].size()!=768 || cdPalettes[1].size()!=768 ||
      !cdOpening.startCd(Cc4View(cdScript.data(),cdScript.size()),Cc4View(cdText.data(),cdText.size())))
   return Common::Error(Common::kReadingFailed,"Cannot initialize CD opening");
  showCd=true; debug("[sotc-cd] starting before R05 logos");
 }
 if(savedGame) { showCd=false; showOpening=false; sound.stopSpeech(); sound.effect(255); sound.music(255); menuSelection=2; }
 uint32 openingLast=_system->getMillis(); unsigned openingMusicClock=0;
 if (showOpening) debug("[sotc-opening] logos from R05/D05 followed by supported animated prefix");
 else warning("[sotc-opening] Logo prefix unavailable; opening menu directly");
 auto finishOpening=[&]() {
  showOpening=false; sound.music(255); sound.effect(255); redraw=true;
  debug("[sotc-menu] opening finished or skipped; waiting for New game");
 };

 Common::String shownDialogue; unsigned dialoguePage=0;
 DialogueTimer dialogueTimer; uint32 dialogueLastTime=_system->getMillis();
 Common::Array<Common::String> storyLines; Common::String storyText; unsigned storyPage=0;
 Common::Rect choiceRects[6];
 Common::Array<byte> cursorData;
 resources.load(9,cursorData); // Original CURSOR.VA2, never bundled with the engine.
 byte cursorPixels[4][256]={},cursorMasks[4][256]={};bool cursorReady[4]={};
 const unsigned cursorFrames[4]={1,4,0,3}; // up, right, down, left
 for(unsigned i=0;i<4;++i){unsigned w=0,h=0;cursorReady[i]=va2Frame(cursorData.data(),cursorData.size(),cursorFrames[i],cursorPixels[i],cursorMasks[i],256,w,h) && w==16 && h==16;}
 int pointerX=0,pointerY=0;
 bool runScene = true, sceneFault = false; unsigned heldDirection=0;
 uint32 nextSceneFrame = _system->getMillis() + 100;
 auto sceneStep = [&]() {
  if(startupMachine.waitingForInput())return;
  if (!startupScene.fading() && !startupScene.movePlayer(startupMachine)) { runScene=false; sceneFault=true; redraw=true; return; }
  scriptStatus = stepStartupScripts(startupMachine, ++scriptTicks);
  if(startupScene.cinematicActive())heldDirection=0;
  if(startupMachine.menuRequested()){startupMachine.dismissMenuRequest();mainMenu=true;menuSelection=0;_mixer->pauseAll(true);redraw=true;return;}
  if (!startupScene.serviceRoom(startupMachine,scriptStatus)) {
   runScene=false; sceneFault=true; redraw=true; return;
  }
  bool rendered = startupScene.render(startupPixels,startupStatus,startupMachine);
  unsigned blockedAnimations = rendered ? startupScene.advance(startupMachine) : 1;
  if(!rendered)scriptStatus=startupStatus;
  else if(blockedAnimations)scriptStatus=Common::String::format("Room %u: %u animation update(s) failed",startupScene.room(),blockedAnimations);
  if (!rendered || blockedAnimations) runScene = false;
  for (unsigned i=0;i<startupMachine.count();++i)
   if (startupMachine.task(i)->result >= kUnknownWait) runScene = false;
  sceneFault=!runScene;
  if(sceneFault) startupScene.cancelFade();
  if (!runScene) debug("[sotc-animation] paused tick=%u unsupported=%u",scriptTicks,blockedAnimations);
  redraw = true;
 };
 auto resumeScene = [&]() {
  if (!sceneFault) { runScene=true; nextSceneFrame=_system->getMillis()+100; }
 };
 auto quickSave=[&]() {
  if(!startupAvailable || showStory || showOpening || showCd) {
   _system->displayMessageOnOSD(Common::U32String("Saving is available during gameplay")); return;
  }
  SaveState state; state.ticks=scriptTicks; state.page=dialoguePage; state.music=sound.musicTrack(); state.running=runScene; state.fault=sceneFault;
  bool ok=writeQuickSave(savefiles,savePrefix,startupMachine,startupScene,state);
  if(ok) savedGame=true;
  debug("[sotc-save] save ok=%u tick=%u actor=%d,%d",unsigned(ok),scriptTicks,startupMachine.actor(0)->x,startupMachine.actor(0)->y);
  _system->displayMessageOnOSD(Common::U32String(ok ? "Quick save complete (Q / L1)" : "Quick save failed"));
 };
 auto quickLoad=[&]() {
  SaveState state;
  if(!readQuickSave(savefiles,savePrefix,startupMachine,startupScene,state,startupPixels,startupStatus)) {
   _system->displayMessageOnOSD(Common::U32String("No compatible quick save could be loaded")); return;
  }
  sound.stopSpeech(); sound.effect(255); sound.music(255);
  sound.music(state.music); startupScene.resumeSpeech();
  scriptTicks=state.ticks; runScene=state.running; sceneFault=state.fault; heldDirection=0;
  pickupRemaining=0;
  startupAvailable=showStartup=true; mainMenu=showStory=showOpening=showCd=false; pal=0; toolbar.hide();
  shownDialogue=startupScene.dialogueText(); dialogueBox.text(shownDialogue);
  dialoguePage=state.page<dialogueBox.count() ? state.page : 0;
  dialogueTimer.reset(dialogueBox.pageDelay(dialoguePage,sound.speechLengthMs()));dialogueLastTime=_system->getMillis();
  scriptStatus=Common::String::format("Restored at script tick %u",scriptTicks);
  _mixer->pauseAll(false); nextSceneFrame=_system->getMillis()+100; redraw=true;
  debug("[sotc-save] load ok=1 tick=%u actor=%d,%d",scriptTicks,startupMachine.actor(0)->x,startupMachine.actor(0)->y);
  _system->displayMessageOnOSD(Common::U32String("Quick save loaded (U / R1)"));
 };
 auto finishStory=[&]() {
  showStory=false; sound.stopSpeech(); sound.music(255); sound.effect(255);
  scriptStatus=stepStartupScripts(startupMachine,++scriptTicks);
  nextSceneFrame=_system->getMillis()+100; redraw=true;
  if(!savedGame) quickSave();
  debug("[sotc-story] returning to opening gameplay scene");
 };
 auto finishCd=[&]() {
  showCd=false; sound.stopSpeech(); sound.music(255); sound.effect(255);
  storyText.clear(); storyLines.clear(); storyPage=0; openingLast=_system->getMillis(); redraw=true;
  debug("[sotc-cd] finished or skipped; starting R05 logos");
 };
 Graphics::Surface surface; surface.create(320, 240, Graphics::PixelFormat::createFormatCLUT8());
 while (!shouldQuit()) {
  if (showCd) {
   uint32 now=_system->getMillis(); cdOpening.advance(now-openingLast); openingLast=now; redraw=true;
   if (cdOpening.done()) {
    if (cdOpening.failed()) { surface.free(); return Common::Error(Common::kUnknownError,"CD opening script could not continue"); }
    finishCd();
   }
  }
  if (showOpening && !showCd) {
   uint32 now=_system->getMillis(); unsigned elapsed=now-openingLast; openingLast=now;
   if (openingAnimated) animatedOpening.advance(elapsed); else openingClock.advance(elapsed);
   openingMusicClock+=elapsed;
   if (openingMusicClock>=MAX(1U,openingPlan.musicDelay)*100) {
    sound.music(openingPlan.music); openingMusicClock=0; // Original task 3 retries while music is idle.
   }
   redraw=true;
   if (openingAnimated) { if (animatedOpening.done()) finishOpening(); }
   else if (openingClock.done()) {
    openingAnimated=animatedOpening.start(Cc4View(openingScript.data(),openingScript.size()),openingPlan.continuation,openingImages[1]);
    if (!openingAnimated) finishOpening();
   }
  }
  if (showStory) {
   uint32 now=_system->getMillis(); animatedOpening.advance(now-openingLast); openingLast=now; redraw=true;
   if (animatedOpening.done()) {
    if (animatedOpening.failed()) { surface.free(); return Common::Error(Common::kUnknownError,"Story script could not continue"); }
    finishStory();
   }
  }
  uint32 dialogueNow=_system->getMillis(),dialogueElapsed=dialogueNow-dialogueLastTime;
  dialogueLastTime=dialogueNow;
  if(pickupRemaining) {
   if(pickupBank!=startupScene.bank() || pickupRoom!=startupScene.room() || !startupScene.dialogueText().empty())pickupRemaining=0;
   else if(showStartup && !mainMenu && !showInventory && !showJournal && !showStory)
    pickupRemaining=dialogueElapsed>=pickupRemaining ? 0 : pickupRemaining-dialogueElapsed;
   if(!pickupRemaining)redraw=true;
  }
  if(shownDialogue!=startupScene.dialogueText()) {
   shownDialogue=startupScene.dialogueText();dialoguePage=0;dialogueBox.text(shownDialogue);
   dialogueTimer.reset(dialogueBox.pageDelay(0,sound.speechLengthMs()));dialogueElapsed=0;
  }
  bool dialogueActive=showStartup && runScene && !sceneFault && !mainMenu && !showInventory && !showJournal && !showStory && !shownDialogue.empty();
  if(dialogueTimer.advance(dialogueElapsed,dialogueActive,!dialogueBox.more(dialoguePage),sound.speechActive())) {
   if(dialogueBox.more(dialoguePage)) {
    dialoguePage=dialogueBox.next(dialoguePage);
    dialogueTimer.reset(dialogueBox.pageDelay(dialoguePage,sound.speechLengthMs()));
   } else startupScene.finishDialogue();
   redraw=true;
  }
  // Diagnostic 10 Hz preview cadence; original scene pacing is not yet bound.
  if (!showInventory && !showJournal && !showStory && !mainMenu && showStartup && runScene && int32(_system->getMillis()-nextSceneFrame)>=0) {
   sceneStep(); nextSceneFrame=_system->getMillis()+100;
  }
  if (reload) {
   inspectRoomScripts(bank);
   backgrounds.clear();
   if (!rooms.open(Common::String::format("D%02d.PAK", bank))) { surface.free(); return Common::Error(Common::kReadingFailed, "Cannot read scenery archive"); }
   for (uint i = 0; i < rooms.entries.size(); ++i)
    if (rooms.entries[i].name.hasSuffix(".RAW") && rooms.entries[i].size == 64000) backgrounds.push_back(i);
   actorEntries.clear();
   if (actors.open(Common::String::format("A%02d.PAK", bank)))
    for (uint i = 0; i < actors.entries.size(); ++i)
     if (actors.entries[i].name.hasSuffix(".VA2")) actorEntries.push_back(i);
   actor = 0; loadActor = true;
   scene = 0; reload = false; redraw = true;
  }
  if (loadActor) {
   actorData.clear(); actorFrame = 0;
   if (!actorEntries.empty()) actors.load(actorEntries[actor], actorData);
   sequence = 0;
   if (!actorData.empty()) {
    unsigned sequences = va2Records(actorData.data(), actorData.size(), 2);
    for (unsigned i = 0; i < sequences; ++i)
     if (va2SequenceLength(actorData.data(), actorData.size(), i) > 1) { sequence = i; break; }
   }
   debug("[sotc-sequence] selected=%u", sequence);
   nextFrame = _system->getMillis() + 150; loadActor = false; redraw = true;
   if (!actorEntries.empty()) debug("[sotc-va2] %s", actors.entries[actorEntries[actor]].name.c_str());
  }
  unsigned actorFrames = actorData.empty() ? 0 : va2FrameCount(actorData.data(), actorData.size());
  unsigned sequences = actorData.empty() ? 0 : va2Records(actorData.data(), actorData.size(), 2);
  unsigned steps = actorData.empty() ? 0 : va2SequenceLength(actorData.data(), actorData.size(), sequence);
  unsigned cycle = steps ? steps : actorFrames;
  if (!mainMenu && !showStartup && showActor && cycle && actorFrame + 1 < cycle && int32(_system->getMillis() - nextFrame) >= 0) {
   actorFrame = (actorFrame + 1) % cycle; nextFrame = _system->getMillis() + 150; redraw = true;
  }
  if (redraw) {
   surface.fillRect(Common::Rect(320,240), 0);
   if (!resources.load(palettes[pal], palette) || palette.size() != 768) { surface.free(); return Common::kReadingFailed; }
   if(showStartup && !mainMenu && !showStory && !showOpening && !showCd) {
    startupScene.applyPalette(palette.data());
    for(unsigned i=0;i<768;++i) palette[i]=byte(unsigned(palette[i])*startupScene.brightness()/255);
   }
   _system->getPaletteManager()->setPalette(palette.data(), 0, 256);
   Common::String title = "No 320x200 background";
   if (!backgrounds.empty()) {
    if (!rooms.load(backgrounds[scene], image) || image.size() != 64000) { surface.free(); return Common::kReadingFailed; }
    memcpy(surface.getBasePtr(0,0), image.data(), 64000);
    title = Common::String::format("D%02d: %s  %d/%u", bank, rooms.entries[backgrounds[scene]].name.c_str(), scene + 1, backgrounds.size());
   }
   Common::String actorTitle = "Triangle: show frames";
   if (!showStartup && showActor && !actorEntries.empty()) {
    bool decoded = true;
    SequenceStep step; int motionX = 0, motionY = 0;
    const uint8_t *poseRecord = nullptr; unsigned layers = 1;
    if (steps) decoded = va2Step(actorData.data(), actorData.size(), sequence, actorFrame, step)
      && va2Record(actorData.data(), actorData.size(), 0, step.pose, poseRecord, layers)
      && va2Position(actorData.data(), actorData.size(), sequence, actorFrame, motionX, motionY)
      && (step.control >> 14) == 0; // Special render modes need their own implementation.
    // Validate every layer before drawing, so unsupported poses never appear partial.
    for (unsigned pass = 0; decoded && pass < 2; ++pass) for (unsigned i = 0; decoded && i < layers; ++i) {
     PoseLayer layer; layer.frame = actorFrame; layer.mirror = false; layer.x = 0; layer.y = 0;
     if (steps && !va2Layer(actorData.data(), actorData.size(), step.pose, i, layer)) { decoded = false; break; }
     unsigned width, height;
     if (!va2Frame(actorData.data(), actorData.size(), layer.frame, actorPixels.data(), actorMask.data(), actorPixels.size(), width, height)) { decoded = false; break; }
     if (!pass) continue;
     // Preview anchor is fixed; bottom-relative placement is inferred from poses.
     int left = steps ? 160 + motionX + layer.x : (320 - int(width)) / 2;
     int top = 170 + motionY + layer.y - int(height) + 1;
     for (unsigned y = 0; y < height; ++y) for (unsigned x = 0; x < width; ++x) {
      int dx = left + x, dy = top + y;
      unsigned sx = layer.mirror ? width - 1 - x : x;
      if (dx >= 0 && dx < 320 && dy >= 0 && dy < 200 && actorMask[y * width + sx])
       *static_cast<byte *>(surface.getBasePtr(dx, dy)) = actorPixels[y * width + sx];
     }
    }
    actorTitle = Common::String::format("%s %s%u %u/%u%s", actors.entries[actorEntries[actor]].name.c_str(),
      steps ? "S" : "raw", steps ? sequence : 0, cycle ? actorFrame + 1 : 0, cycle, decoded ? "" : " unsupported");
   }
   if (showStartup) {
    memcpy(surface.getBasePtr(0, 0), startupPixels.data(), 64000);
    int objectX=0,objectY=0;
    int object=startupScene.lookedObject(objectX,objectY);
    if(object>=0 && !showJournal && !startupMachine.choiceCount()) {
     const ScriptActor &a=*startupMachine.actor(0);unsigned direction=startupScene.facing();
     int eyeX=a.x+(direction==2 ? 4 : direction==4 ? -5 : 0);
     int eyeY=a.y-35+(direction==1 ? -1 : direction==3 ? 1 : 0);
     // Original 0xc25d uses color 7 and a one-pixel random endpoint jitter.
     int x=CLIP(eyeX,0,319),y=CLIP(eyeY,0,199),endX=CLIP(objectX,0,319),endY=CLIP(objectY,0,199);
     int dx=ABS(endX-x),dy=-ABS(endY-y),sx=x<endX ? 1 : -1,sy=y<endY ? 1 : -1,error=dx+dy;
     for(unsigned pixel=0;;++pixel) {
      if(pixel%6<3)*static_cast<byte *>(surface.getBasePtr(x,y))=7;
      if(x==endX && y==endY)break;
      int twice=error*2;if(twice>=dy){error+=dy;x+=sx;}if(twice<=dx){error+=dx;y+=sy;}
     }
    }
    title = runScene ? "Walk test - collision stops" : "Scene paused - not gameplay"; actorTitle = scriptStatus;
   }
   if(showStartup && !mainMenu && !showInventory && !showJournal && !showStory && pickupRemaining) {
    Common::Array<byte> labelPixels;labelPixels.resize(64000);
    memcpy(labelPixels.data(),surface.getBasePtr(0,0),64000);
    const ScriptActor &actor=*startupMachine.actor(0);
    if(pickupBox.render(labelPixels,0,actor.x,actor.y-65,false,22))
     memcpy(surface.getBasePtr(0,0),labelPixels.data(),64000);
   }
   const Graphics::Font *font = FontMan.getFontByUsage(Graphics::FontManager::kGUIFont);
   if (showStartup && !startupScene.dialogueText().empty()) {
    if (shownDialogue!=startupScene.dialogueText()) {
     shownDialogue=startupScene.dialogueText(); dialoguePage=0; dialogueBox.text(shownDialogue);
     dialogueTimer.reset(dialogueBox.pageDelay(0,sound.speechLengthMs()));dialogueLastTime=_system->getMillis();
    }
    Common::Array<byte> dialogPixels;dialogPixels.resize(64000);
    memcpy(dialogPixels.data(),surface.getBasePtr(0,0),64000);
    if(dialogueBox.render(dialogPixels,dialoguePage,startupScene.textX(),startupScene.textY(),startupScene.portraitDialogue()))
     memcpy(surface.getBasePtr(0,0),dialogPixels.data(),64000);
   } else { shownDialogue.clear(); dialoguePage=0; }
   font->drawString(&surface, title, 2, 201, 316, 7);
   font->drawString(&surface, actorTitle, 2, 214, 316, 7);
   font->drawString(&surface, showStartup ? (startupMachine.waitingForInput() ? "Click / Cross: continue" : startupScene.cinematicActive() ? "Cross: skip  Options: menu" : !startupScene.dialogueText().empty() ? "Cross / click: next dialogue page" : "Click / Cross: walk  D-pad: steer") : "F8: start scene  L1/R1: figure", 2, 227, 316, 7);
   if (mainMenu) {
    surface.fillRect(Common::Rect(320,240),0);
    if (!menuBackground.empty()) memcpy(surface.getBasePtr(0,0),menuBackground.data(),64000);
    surface.fillRect(Common::Rect(32,26,288,58),0);
    font->drawString(&surface,"Shadow of the Comet",32,36,256,7,Graphics::kTextAlignCenter);
    const char *labels[]={"Continue", "New game", "Load quick save", "Replay opening", "Settings", "Quit"};
    for (int i=0;i<6;++i) {
     const int y=66+i*22;
     surface.fillRect(Common::Rect(64,y,256,y+20),0);
     if (i==menuSelection) surface.frameRect(Common::Rect(64,y,256,y+20),7);
     font->drawString(&surface,labels[i],68,y+6,184,i==0 && !startupAvailable ? 8 : 7,Graphics::kTextAlignCenter);
    }
    font->drawString(&surface,"Experimental engine - opening scene",0,207,320,7,Graphics::kTextAlignCenter);
    font->drawString(&surface,"D-pad / click: select   Cross: confirm",0,226,320,7,Graphics::kTextAlignCenter);
   }
   if (!mainMenu && showStartup && !showStory && toolbar.visible() && startupScene.dialogueText().empty()) {
    if (!toolbar.render()) { surface.free(); return Common::Error(Common::kUnknownError,"Cannot draw toolbar"); }
    Common::Array<byte> barPixels=toolbar.pixels();PoseRenderer barRenderer(barPixels);
    if(!barRenderer.pose(inventoryIcons,inventoryIcon,194,11,180,0,224,39)) {
     surface.free();return Common::Error(Common::kReadingFailed,"Cannot draw inventory icon");
    }
    memcpy(surface.getBasePtr(0,0),barPixels.data(),320*40);
    surface.fillRect(Common::Rect(0,225,320,240),0);
    font->drawString(&surface,toolbar.hover()==6 ? "Menu / settings" : toolbar.hover()==0 ? "Talk to someone nearby" : toolbar.hover()==3 ? "Look around here" : toolbar.hover()==1 ? "Take the object in view" : toolbar.hover()==4 ? "Inventory / notebook" : "This action is not available in the preview yet",0,227,320,7,Graphics::kTextAlignCenter);
   }
   if (showOpening || showStory || showCd) {
    OpeningScene &cinematic=showCd ? cdOpening : animatedOpening;
    surface.fillRect(Common::Rect(320,240),0);
    memcpy(surface.getBasePtr(0,20),(showCd || openingAnimated || showStory) ? cinematic.pixels().data() : openingImages[openingClock.logo()].data(),64000);
    byte faded[768]; unsigned brightness=(showCd || openingAnimated || showStory) ? cinematic.brightness() : openingClock.brightness();
    const Common::Array<byte> &scenePalette=showCd ? cdPalettes[cdOpening.cdBlock()==0 ? 0 : 1] : palette;
    for (unsigned i=0;i<768;++i) faded[i]=scenePalette[i]*brightness/255;
    _system->getPaletteManager()->setPalette(faded,0,256);
    if ((showStory || showCd) && !cinematic.dialogueText().empty()) {
     if (storyText!=cinematic.dialogueText()) {
      storyText=cinematic.dialogueText(); storyPage=0; storyLines.clear(); font->wordWrapText(storyText,300,storyLines);
     }
     unsigned end=MIN<unsigned>(storyPage+8,storyLines.size());
     int left=CLIP<int>(int(cinematic.textX())-150,0,20),top=CLIP<int>(20+cinematic.textY(),20,120);
     surface.fillRect(Common::Rect(left,top,left+300,top+4+(end-storyPage)*font->getFontHeight()),0);
     for (unsigned i=storyPage;i<end;++i) font->drawString(&surface,storyLines[i],left,top+2+(i-storyPage)*font->getFontHeight(),300,cinematic.textColor(),Graphics::kTextAlignCenter);
    } else { storyText.clear(); storyLines.clear(); storyPage=0; }

   }
   if(showStartup && !mainMenu && !showStory && startupMachine.choiceCount()) {
    toolbar.hide();
    unsigned count=startupMachine.choiceCount(),selected=startupMachine.choiceSelection();
    DialogueBox answers[6],heading;Common::String text;
    if(!startupScene.choiceText(startupMachine.choiceHeader(),text)) {
     surface.free();return Common::Error(Common::kReadingFailed,"Cannot load dialogue choices");
    }
    heading.load(journalFont,bubbleData);heading.text(text);
    int widths[6],heights[6],headingWidth=0,headingHeight=0,total=0;
    if(heading.count()){heading.size(0,heading.count(),headingWidth,headingHeight);total+=headingHeight;}
    for(unsigned i=0;i<count;++i) {
     if(!startupScene.choiceText(startupMachine.choiceText(i),text)) {
      surface.free();return Common::Error(Common::kReadingFailed,"Cannot load dialogue choice text");
     }
     answers[i].load(journalFont,bubbleData);answers[i].text(text.empty() ? " " : text);
     answers[i].size(0,answers[i].count(),widths[i],heights[i]);total+=heights[i];
    }
    int left=CLIP<int>(startupMachine.choiceX(),0,18),top=CLIP<int>(startupMachine.choiceY(),0,MAX<int>(0,199-MIN<int>(total,199)));
    unsigned first=0;int available=199-top;
    // Keep complete answer boxes visible when a long list needs scrolling.
    if(total>available){headingHeight=0;int throughSelected=0;for(unsigned i=0;i<=selected;++i)throughSelected+=heights[i];while(first<selected && throughSelected>available)throughSelected-=heights[first++];}
    Common::Array<byte> choicePixels;choicePixels.resize(64000);memcpy(choicePixels.data(),surface.getBasePtr(0,0),64000);
    int y=top;
    if(headingHeight){heading.render(choicePixels,0,left,y,false,0,true,heading.count());y+=headingHeight;}
    for(unsigned i=0;i<6;++i)choiceRects[i]=Common::Rect();
    for(unsigned i=first;i<count;++i) {
     if(y+heights[i]>199)break;
     int x=MIN<int>(left,319-widths[i]);
     if(!answers[i].render(choicePixels,0,x,y,false,i==selected ? 159 : 22,true,answers[i].count())) {
      surface.free();return Common::Error(Common::kReadingFailed,"Cannot render original answer box");
     }
     choiceRects[i]=Common::Rect(x,y,x+widths[i],y+heights[i]);y+=heights[i];
    }
    memcpy(surface.getBasePtr(0,0),choicePixels.data(),64000);
    surface.fillRect(Common::Rect(0,225,320,240),0);
    font->drawString(&surface,"D-pad: choose  Cross / click: confirm",2,227,316,7);
   }
   if(showInventory) {
    Common::Array<byte> panel=startupPixels;
    if(!inventory.render(panel,inventoryIds,inventoryNames,inventorySelection)) {
     surface.free();return Common::Error(Common::kReadingFailed,"Cannot render original inventory");
    }
    memcpy(surface.getBasePtr(0,0),panel.data(),64000);
    surface.fillRect(Common::Rect(0,200,320,240),0);
    font->drawString(&surface,"D-pad: select  Cross: read/use  Circle: close",0,218,320,7,Graphics::kTextAlignCenter);
   }
   if(showJournal) {
    Common::Array<byte> book=startupPixels;
    if(!journal.render(book)) {surface.free();return Common::Error(Common::kReadingFailed,"Cannot render diary");}
    memcpy(surface.getBasePtr(0,0),book.data(),64000);
    surface.fillRect(Common::Rect(0,200,320,240),0);
    font->drawString(&surface,"Left / Right: pages   Circle / Esc: close",0,217,320,7,Graphics::kTextAlignCenter);
   }
   if ((mainMenu || showStartup) && pointerY<480 && pointerX<640 && pointerX>=0 && pointerY>=0) {
    int px=pointerX/2,py=pointerY/2;
    unsigned direction=showStartup && !mainMenu && !showInventory && !showJournal && !showStory && !showCd &&
     !toolbar.visible() && startupScene.dialogueText().empty() && !startupMachine.choiceCount() ? startupScene.exitDirection(px,py) : showInventory ? 2 : 0;
    if(direction && cursorReady[direction-1]) {
     for(int y=0;y<16;++y)for(int x=0;x<16;++x) {
      int dx=px+x-7,dy=py+y-7;
      if(dx>=0 && dx<320 && dy>=0 && dy<240 && cursorMasks[direction-1][y*16+x])
       *static_cast<byte *>(surface.getBasePtr(dx,dy))=cursorPixels[direction-1][y*16+x];
     }
    } else for (int d=-3;d<=3;++d) {
     if (px+d>=0 && px+d<320) *static_cast<byte *>(surface.getBasePtr(px+d,py))=7;
     if (py+d>=0 && py+d<240) *static_cast<byte *>(surface.getBasePtr(px,py+d))=7;
    }
   }
   for (int y = 0; y < 480; ++y) for (int x = 0; x < 640; ++x)
    frame[y * 640 + x] = *static_cast<const byte *>(surface.getBasePtr(x / 2, y / 2));
   if (!showActor || actorFrame == 0) debug("[sotc] %s palette=%s", title.c_str(), resources.entries[palettes[pal]].name.c_str()); redraw = false;
  }
  Common::Event event;
  while (_eventMan->pollEvent(event)) {
   if(event.type==Common::EVENT_QUIT){quitGame();continue;}
   if(!showInventory && !showJournal && event.type==Common::EVENT_KEYDOWN && !event.kbdRepeat && (event.kbd.keycode==Common::KEYCODE_q || event.kbd.keycode==Common::KEYCODE_u)) {
    if(event.kbd.keycode==Common::KEYCODE_q) quickSave(); else quickLoad();
    continue;
   }
   if(event.type==Common::EVENT_MOUSEMOVE) {pointerX=event.mouse.x;pointerY=event.mouse.y;redraw=true;}
   // Normalize controller events before modal menus consume them.
   if((showInventory || showJournal || startupScene.cinematicActive() || startupMachine.waitingForInput()) && event.type==Common::EVENT_JOYBUTTON_DOWN) {
    Common::KeyCode key=Common::KEYCODE_INVALID;
    switch(event.joystick.button) {
    case Common::JOYSTICK_BUTTON_A:key=Common::KEYCODE_RETURN;break;
    case Common::JOYSTICK_BUTTON_B:key=Common::KEYCODE_ESCAPE;break;
    case Common::JOYSTICK_BUTTON_DPAD_UP:key=Common::KEYCODE_UP;break;
    case Common::JOYSTICK_BUTTON_DPAD_DOWN:key=Common::KEYCODE_DOWN;break;
    case Common::JOYSTICK_BUTTON_DPAD_LEFT:key=Common::KEYCODE_LEFT;break;
    case Common::JOYSTICK_BUTTON_DPAD_RIGHT:key=Common::KEYCODE_RIGHT;break;
    default:break;
    }
    if(key!=Common::KEYCODE_INVALID){event.type=Common::EVENT_KEYDOWN;event.kbd.keycode=key;event.kbdRepeat=false;}
   }
   if(showStartup && !mainMenu && startupScene.cinematicActive()) {
    if(event.type==Common::EVENT_KEYDOWN && event.kbd.keycode==Common::KEYCODE_ESCAPE){mainMenu=true;menuSelection=0;_mixer->pauseAll(true);redraw=true;continue;}
    if(event.type==Common::EVENT_LBUTTONDOWN || (event.type==Common::EVENT_KEYDOWN && !event.kbdRepeat && event.kbd.keycode==Common::KEYCODE_RETURN)){startupScene.skipCinematic();redraw=true;}
    continue;
   }
   if(showStartup && !mainMenu && startupMachine.waitingForInput()) {
    if(event.type==Common::EVENT_LBUTTONDOWN || event.type==Common::EVENT_RBUTTONDOWN ||
       (event.type==Common::EVENT_KEYDOWN && !event.kbdRepeat)) {startupMachine.acknowledgeInput();heldDirection=0;redraw=true;}
    continue;
   }
   if(showInventory) {
    // The PS4 backend maps Cross to a left click. Confirm the highlighted
    // row even when the pointer is still over the toolbar after D-pad selection.
    bool close=event.type==Common::EVENT_RBUTTONDOWN,confirm=event.type==Common::EVENT_LBUTTONDOWN;int delta=0;
    if(event.type==Common::EVENT_KEYDOWN && !event.kbdRepeat) {
     close=event.kbd.keycode==Common::KEYCODE_ESCAPE;
     if(event.kbd.keycode==Common::KEYCODE_UP)delta=-1;
     if(event.kbd.keycode==Common::KEYCODE_DOWN)delta=1;
     confirm=event.kbd.keycode==Common::KEYCODE_RETURN || event.kbd.keycode==Common::KEYCODE_SPACE;
    }
    if(event.type==Common::EVENT_MOUSEMOVE || event.type==Common::EVENT_LBUTTONDOWN) {
     int x=event.mouse.x/2,y=event.mouse.y/2;
     pointerX=event.mouse.x;pointerY=event.mouse.y;
     int i=inventory.itemAt(x,y,inventorySelection,inventoryIds.size());
     if(i>=0){inventorySelection=unsigned(i);redraw=true;}
     if(event.type==Common::EVENT_LBUTTONDOWN && x>=159 && x<167) {
      unsigned first=inventorySelection/Inventory::kRows*Inventory::kRows;
      if(y>=52 && y<59 && first) {inventorySelection=first-Inventory::kRows;confirm=false;redraw=true;}
      if(y>=183 && y<191 && first+Inventory::kRows<inventoryIds.size()) {inventorySelection=first+Inventory::kRows;confirm=false;redraw=true;}
     }
     if(event.type==Common::EVENT_MOUSEMOVE)redraw=true;
    }
    if(delta && !inventoryIds.empty()){inventorySelection=(inventorySelection+inventoryIds.size()+delta)%inventoryIds.size();redraw=true;}
    if(confirm && inventorySelection<inventoryIds.size()) {
     unsigned id=inventoryIds[inventorySelection];inventoryIcon=id;
     if(id==0) {showJournal=journal.open(startupMachine.journalProgress());close=true;}
     else if(startupMachine.useObject(id)){close=true;resumeScene();}
    }
    if(close){showInventory=false;if(!showJournal)_mixer->pauseAll(false);nextSceneFrame=_system->getMillis()+100;redraw=true;}
    if(event.type==Common::EVENT_QUIT)quitGame();
    continue;
   }
   if(showJournal) {
    bool close=event.type==Common::EVENT_RBUTTONDOWN;
    int turn=0;
    if(event.type==Common::EVENT_KEYDOWN && !event.kbdRepeat) {
     close=event.kbd.keycode==Common::KEYCODE_ESCAPE || event.kbd.keycode==Common::KEYCODE_j;
     if(event.kbd.keycode==Common::KEYCODE_LEFT)turn=-1;
     if(event.kbd.keycode==Common::KEYCODE_RIGHT)turn=1;
    }
    if(event.type==Common::EVENT_MOUSEMOVE) {pointerX=event.mouse.x;pointerY=event.mouse.y;}
    if(event.type==Common::EVENT_LBUTTONDOWN || (event.type==Common::EVENT_KEYDOWN && event.kbd.keycode==Common::KEYCODE_RETURN)) {
     int x=event.type==Common::EVENT_LBUTTONDOWN ? event.mouse.x/2 : pointerX/2;
     int y=event.type==Common::EVENT_LBUTTONDOWN ? event.mouse.y/2 : pointerY/2;
     if(x<50 || x>=284 || y<42 || y>=193)close=true;else turn=x<167 ? -1 : 1;
    }
    if(event.type==Common::EVENT_QUIT)quitGame();
    if(close) {showJournal=false;_mixer->pauseAll(false);nextSceneFrame=_system->getMillis()+100;redraw=true;}
    else if(turn && journal.turn(turn))redraw=true;
    continue;
   }
   if(showStartup && !mainMenu && !showStory && startupScene.dialogueText().empty() && !startupMachine.choiceCount() &&
      event.type==Common::EVENT_KEYDOWN && !event.kbdRepeat && event.kbd.keycode==Common::KEYCODE_j) {
    if(journal.open(startupMachine.journalProgress())) {
     showJournal=true;toolbar.hide();heldDirection=0;if(!startupScene.scriptWalking()){startupScene.cancelRoute();startupScene.walk(0,startupMachine);}_mixer->pauseAll(true);redraw=true;
    }
    continue;
   }
   if (event.type==Common::EVENT_MOUSEMOVE) { pointerX=event.mouse.x; pointerY=event.mouse.y; if (showStartup || mainMenu) redraw=true;
    if (showStartup && !mainMenu && !showStory && startupScene.dialogueText().empty() && !startupMachine.choiceCount()) toolbar.move(pointerX/2,pointerY/2);
    else toolbar.hide();
   }
   if (event.type==Common::EVENT_KEYDOWN || event.type==Common::EVENT_KEYUP) debug("[sotc-input] type=%u key=%u",unsigned(event.type),unsigned(event.kbd.keycode));
   if (showCd) {
    if (event.type==Common::EVENT_KEYDOWN && event.kbd.keycode==Common::KEYCODE_ESCAPE) finishCd();
    else if (!cdOpening.dialogueText().empty() && (event.type==Common::EVENT_LBUTTONDOWN ||
      (event.type==Common::EVENT_KEYDOWN && !event.kbdRepeat && event.kbd.keycode==Common::KEYCODE_RETURN))) {
     if (storyPage+8<storyLines.size()) storyPage+=8;
     else cdOpening.finishDialogue();
     redraw=true;
    }
    continue;
   }
   if (showStory) {
    if (event.type==Common::EVENT_KEYDOWN && event.kbd.keycode==Common::KEYCODE_ESCAPE) finishStory();
    else if (!animatedOpening.dialogueText().empty() &&
      (event.type==Common::EVENT_LBUTTONDOWN || (event.type==Common::EVENT_KEYDOWN && event.kbd.keycode==Common::KEYCODE_RETURN))) {
     if (storyPage+8<storyLines.size()) storyPage+=8; else animatedOpening.finishDialogue();
     redraw=true;
    }
    continue;
   }
   if (showOpening) {
    if (event.type==Common::EVENT_LBUTTONDOWN || event.type==Common::EVENT_RBUTTONDOWN ||
        (event.type==Common::EVENT_KEYDOWN && (event.kbd.keycode==Common::KEYCODE_RETURN || event.kbd.keycode==Common::KEYCODE_ESCAPE)) ||
        (event.type==Common::EVENT_JOYBUTTON_DOWN && event.joystick.button==Common::JOYSTICK_BUTTON_A)) finishOpening();
    continue;
   }
   // Menu owns input and freezes simulation; no scene is loaded until New game.
   if (mainMenu) {
    int movement=0; bool confirm=false;
    if (event.type==Common::EVENT_KEYDOWN) {
     if (event.kbd.keycode==Common::KEYCODE_UP) movement=-1;
     if (event.kbd.keycode==Common::KEYCODE_DOWN) movement=1;
     confirm=event.kbd.keycode==Common::KEYCODE_RETURN || event.kbd.keycode==Common::KEYCODE_SPACE;
     if (event.kbd.keycode==Common::KEYCODE_ESCAPE && startupAvailable) {
      mainMenu=false; _mixer->pauseAll(false); nextSceneFrame=_system->getMillis()+100; redraw=true;
     }
    }
    if (event.type==Common::EVENT_JOYBUTTON_DOWN) {
     if (event.joystick.button==Common::JOYSTICK_BUTTON_DPAD_UP) movement=-1;
     if (event.joystick.button==Common::JOYSTICK_BUTTON_DPAD_DOWN) movement=1;
     confirm=event.joystick.button==Common::JOYSTICK_BUTTON_A;
    }
    if (movement) {
     do { menuSelection=(menuSelection+movement+6)%6; } while (!startupAvailable && menuSelection==0);
     redraw=true;
    }
    if (event.type==Common::EVENT_MOUSEMOVE || event.type==Common::EVENT_LBUTTONDOWN) {
     int x=event.mouse.x/2,y=event.mouse.y/2;
     if (x>=64 && x<256 && y>=66 && y<198 && (y-66)%22<20) {
      int selection=(y-66)/22;
      if (selection!=0 || startupAvailable) { menuSelection=selection; redraw=true; confirm=event.type==Common::EVENT_LBUTTONDOWN; }
     }
    }
    if (confirm) {
     debug("[sotc-menu] selection=%d",menuSelection);
     if (menuSelection==0 && startupAvailable) {
      mainMenu=false; _mixer->pauseAll(false); nextSceneFrame=_system->getMillis()+100;
     } else if (menuSelection==1) {
      bool restart=true;
      if (startupAvailable) {
       GUI::MessageDialog question(Common::String("Start a new game? Current progress will be lost."),Common::String("No"),Common::String("Yes"));
       restart=runDialog(question)==GUI::kMessageAlt;
      }
      if (restart) {
       sound.stopSpeech(); sound.effect(255); sound.music(255);
       startupAvailable=startupScene.load(startupPixels,startupStatus,startupMachine);
       if (!startupAvailable) { surface.free(); _mixer->pauseAll(false); return Common::Error(Common::kReadingFailed,"Cannot load opening scene"); }
       showStartup=true; scriptTicks=0; heldDirection=0; sceneFault=false; runScene=true;
       shownDialogue.clear(); dialoguePage=0; pal=0;
       Common::File storyFile; Common::Array<byte> storyBytes;
       if (!storyFile.open(Common::Path("E.CC4")) || storyFile.size()<1 || storyFile.size()>4*1024*1024) {
        surface.free(); return Common::Error(Common::kReadingFailed,"Cannot read story subtitles");
       }
       storyBytes.resize(storyFile.size());
       if (storyFile.read(storyBytes.data(),storyBytes.size())!=storyBytes.size() ||
           !animatedOpening.startStory(Cc4View(openingScript.data(),openingScript.size()),Cc4View(storyBytes.data(),storyBytes.size()))) {
        surface.free(); return Common::Error(Common::kReadingFailed,"Cannot initialize story scene");
       }
       showStory=true; openingLast=_system->getMillis(); storyText.clear(); storyLines.clear(); storyPage=0;
       mainMenu=false; _mixer->pauseAll(false); nextSceneFrame=_system->getMillis()+100;
       debug("[sotc-menu] New game: opening scene reset");
      }
     } else if (menuSelection==2) {
      quickLoad();
     } else if (menuSelection==3) {
      sound.stopSpeech(); sound.music(255); sound.effect(255);
      openingClock=OpeningClock(openingPlan); openingAnimated=false; openingMusicClock=0;
      showOpening=openingImages[0].size()==64000 && openingImages[1].size()==64000;
      showCd=!cdScript.empty() && cdOpening.startCd(Cc4View(cdScript.data(),cdScript.size()),Cc4View(cdText.data(),cdText.size()));
      openingLast=_system->getMillis(); storyText.clear(); storyLines.clear(); storyPage=0;
      _mixer->pauseAll(false);
      if(!showOpening && !showCd) _system->displayMessageOnOSD(Common::U32String("Opening resources are unavailable"));
      debug("[sotc-menu] replay opening cd=%u",unsigned(showCd));
     } else if (menuSelection==4) {
      GUI::ConfigDialog settings; runDialog(settings); syncSoundSettings();
      _mixer->pauseAll(true);
     } else if (menuSelection==5) quitGame();
     redraw=true;
    }
    continue;
   }
   if (event.type==Common::EVENT_KEYDOWN && event.kbd.keycode==Common::KEYCODE_ESCAPE) {
    toolbar.hide(); heldDirection=0;
    if (showStartup && !startupScene.scriptWalking()) { startupScene.cancelRoute(); startupScene.walk(0,startupMachine); }
    mainMenu=true; menuSelection=startupAvailable ? 0 : 1; _mixer->pauseAll(true); redraw=true;
    debug("[sotc-menu] opened; simulation and sound paused");
    continue;
   }
   if(showStartup && startupMachine.choiceCount()) {
    int movement=0;bool confirm=false;
    if(event.type==Common::EVENT_KEYDOWN && !event.kbdRepeat) {
     if(event.kbd.keycode==Common::KEYCODE_UP) movement=-1;
     if(event.kbd.keycode==Common::KEYCODE_DOWN) movement=1;
     confirm=event.kbd.keycode==Common::KEYCODE_RETURN || event.kbd.keycode==Common::KEYCODE_SPACE;
    }
    if(event.type==Common::EVENT_JOYBUTTON_DOWN) {
     if(event.joystick.button==Common::JOYSTICK_BUTTON_DPAD_UP) movement=-1;
     if(event.joystick.button==Common::JOYSTICK_BUTTON_DPAD_DOWN) movement=1;
     confirm=event.joystick.button==Common::JOYSTICK_BUTTON_A;
    }
    if(movement) {
     int selection=int(startupMachine.choiceSelection())+movement+startupMachine.choiceCount();
     startupMachine.highlightChoice(selection%startupMachine.choiceCount());redraw=true;
    }
    if(event.type==Common::EVENT_MOUSEMOVE || event.type==Common::EVENT_LBUTTONDOWN) {
     for(unsigned i=0;i<startupMachine.choiceCount();++i) if(choiceRects[i].contains(event.mouse.x/2,event.mouse.y/2)) {
      startupMachine.highlightChoice(i);confirm=event.type==Common::EVENT_LBUTTONDOWN;redraw=true;break;
     }
    }
    if(confirm) {
     unsigned selected=startupMachine.choiceSelection();
     if(startupMachine.choose(selected)) { debug("[sotc-choice] selected=%u",selected);heldDirection=0;resumeScene();redraw=true; }
    }
    continue; // Choosing a reply must not also move the player or skip dialogue.
   }
   if (event.type==Common::EVENT_KEYDOWN && event.kbd.keycode==Common::KEYCODE_RETURN) {
    event.type=Common::EVENT_LBUTTONDOWN; event.mouse.x=pointerX; event.mouse.y=pointerY;
   }
   if (showStartup && toolbar.visible() && startupScene.dialogueText().empty() &&
       (event.type==Common::EVENT_LBUTTONDOWN || event.type==Common::EVENT_RBUTTONDOWN)) {
    int x=event.mouse.x/2,y=event.mouse.y/2;
    if (y>=0 && y<40 && x>=0 && x<320) {
     if (event.type==Common::EVENT_LBUTTONDOWN && toolbar.menuAt(x,y)) {
      toolbar.hide(); heldDirection=0;
      if (!startupScene.scriptWalking()) { startupScene.cancelRoute(); startupScene.walk(0,startupMachine); }
      mainMenu=true; menuSelection=0; _mixer->pauseAll(true);
      debug("[sotc-toolbar] main menu opened");
     }
     else if(event.type==Common::EVENT_LBUTTONDOWN && (x<45 || (x>=135 && x<180))) {
      heldDirection=0;toolbar.hide();if(startupScene.examine(x<45))resumeScene();
     }
     else if(event.type==Common::EVENT_LBUTTONDOWN && x>=45 && x<90) {
      Common::String label;
      bool taken=startupScene.takeObject(label);
      if(taken) {
       pickupBox.text(label);pickupRemaining=2000;
       pickupBank=startupScene.bank();pickupRoom=startupScene.room();
      } else _system->displayMessageOnOSD(Common::U32String("Face a nearby object to take it"));
      toolbar.hide();if(taken)resumeScene();
     }
     else if(event.type==Common::EVENT_LBUTTONDOWN && x>=180 && x<225) {
      inventoryIds.clear();inventoryNames.clear();inventorySelection=0;
      for(unsigned i=0;i<256;++i) {
       Common::String label;unsigned state=startupMachine.inventoryState(i);
       if((state==1 || state==2) && startupScene.inventoryLabel(i,label)){inventoryIds.push_back(i);inventoryNames.push_back(label);}
      }
      showInventory=true;toolbar.hide();heldDirection=0;
      if(!startupScene.scriptWalking()) {startupScene.cancelRoute();startupScene.walk(0,startupMachine);}
      _mixer->pauseAll(true);
     }
     redraw=true; continue; // Never turn a toolbar click into a walk destination.
    }
   }
   if(showStartup && event.type==Common::EVENT_KEYDOWN && !event.kbdRepeat && event.kbd.keycode==Common::KEYCODE_SPACE) {
    heldDirection=0;toolbar.hide();if(startupScene.examine())resumeScene();redraw=true;continue;
   }
   if (event.type == Common::EVENT_KEYDOWN && event.kbd.keycode == Common::KEYCODE_F8) {
    if (startupAvailable) { if (!startupScene.scriptWalking()) { startupScene.cancelRoute(); startupScene.walk(0,startupMachine); } heldDirection=0; showStartup = !showStartup; pal = 0; redraw = true; }
    continue;
   }
   if (showStartup && event.type == Common::EVENT_KEYDOWN && event.kbd.keycode == Common::KEYCODE_a) {
    runScene = !runScene; nextSceneFrame=_system->getMillis()+100; redraw=true; continue;
   }
   if (showStartup) {
    unsigned direction=0; bool pressed=false, released=false;
    if (event.type==Common::EVENT_KEYDOWN || event.type==Common::EVENT_KEYUP) {
     pressed=event.type==Common::EVENT_KEYDOWN; released=!pressed;
     switch (event.kbd.keycode) {
     case Common::KEYCODE_UP: direction=1; break;
     case Common::KEYCODE_RIGHT: direction=2; break;
     case Common::KEYCODE_DOWN: direction=3; break;
     case Common::KEYCODE_LEFT: direction=4; break;
     default: break;
     }
    } else if (event.type==Common::EVENT_JOYBUTTON_DOWN || event.type==Common::EVENT_JOYBUTTON_UP) {
     // SDL hat events are queued directly and can reach us without keymapping.
     pressed=event.type==Common::EVENT_JOYBUTTON_DOWN; released=!pressed;
     switch (event.joystick.button) {
     case Common::JOYSTICK_BUTTON_DPAD_UP: direction=1; break;
     case Common::JOYSTICK_BUTTON_DPAD_RIGHT: direction=2; break;
     case Common::JOYSTICK_BUTTON_DPAD_DOWN: direction=3; break;
     case Common::JOYSTICK_BUTTON_DPAD_LEFT: direction=4; break;
     default: break;
     }
    }
    if (direction && (pressed || released)) {
     if (sceneFault || startupScene.scriptWalking() || !startupScene.dialogueText().empty()) continue;
     if (released && heldDirection!=direction) continue;
     startupScene.cancelRoute();
     heldDirection=pressed ? direction : 0;
     if (!startupScene.walk(pressed ? direction : 0,startupMachine)) { runScene=false; sceneFault=true; }
     else if (pressed) resumeScene();
     redraw=true; continue;
    }
   }
   if (showStartup && (event.type==Common::EVENT_LBUTTONDOWN || event.type==Common::EVENT_RBUTTONDOWN)) {
    if (sceneFault) continue;
    if (!startupScene.dialogueText().empty()) {
     if (event.type==Common::EVENT_LBUTTONDOWN) {
      if (dialogueBox.more(dialoguePage)) {
       dialoguePage=dialogueBox.next(dialoguePage);
       dialogueTimer.reset(dialogueBox.pageDelay(dialoguePage,sound.speechLengthMs()));dialogueLastTime=_system->getMillis();
      }
      else startupScene.finishDialogue();
      resumeScene(); redraw=true;
     }
     continue;
    }
    if (startupScene.scriptWalking()) continue;
    if (event.mouse.y>=0 && (event.mouse.y<400 || startupScene.exitDirection(event.mouse.x/2,event.mouse.y/2)) && event.mouse.x>=0 && event.mouse.x<640) {
     if (startupScene.destination(event.mouse.x/2,event.mouse.y/2,startupMachine)) resumeScene();
     heldDirection=0; redraw=true;
    }
    continue;
   }
   if (showStartup && !(event.type == Common::EVENT_KEYDOWN && event.kbd.keycode == Common::KEYCODE_ESCAPE)) continue;

   if (event.type == Common::EVENT_LBUTTONDOWN || (event.type == Common::EVENT_KEYDOWN && event.kbd.keycode == Common::KEYCODE_RIGHT)) { if (!backgrounds.empty()) scene = (scene + 1) % backgrounds.size(); redraw = true; }
   if (event.type == Common::EVENT_RBUTTONDOWN) { if (!backgrounds.empty()) scene = (scene + backgrounds.size() - 1) % backgrounds.size(); redraw = true; }
   if (event.type != Common::EVENT_KEYDOWN) continue;
   switch (event.kbd.keycode) {
   case Common::KEYCODE_LEFT: if (!backgrounds.empty()) scene = (scene + backgrounds.size() - 1) % backgrounds.size(); redraw = true; break;
   case Common::KEYCODE_UP: bank = (bank + 1) % 10; reload = true; break;
   case Common::KEYCODE_DOWN: bank = (bank + 9) % 10; reload = true; break;
   case Common::KEYCODE_p: pal = (pal + 1) % palettes.size(); redraw = true; break;
   case Common::KEYCODE_a: showActor = !showActor; if (showActor) { actorFrame = 0; nextFrame = _system->getMillis() + 150; } redraw = true; break;
   case Common::KEYCODE_F5: if (!actorEntries.empty()) actor = (actor + actorEntries.size() - 1) % actorEntries.size(); loadActor = true; break;
   case Common::KEYCODE_F7: if (!actorEntries.empty()) actor = (actor + 1) % actorEntries.size(); loadActor = true; break;
   case Common::KEYCODE_F9: if (sequences) sequence = (sequence + sequences - 1) % sequences; actorFrame = 0; redraw = true; debug("[sotc-sequence] selected=%u", sequence); break;
   case Common::KEYCODE_F10: if (sequences) sequence = (sequence + 1) % sequences; actorFrame = 0; redraw = true; debug("[sotc-sequence] selected=%u", sequence); break;
   case Common::KEYCODE_ESCAPE: quitGame(); break;
   default: break;
   }
  }
  // Keep presenting even a static room: the PS4 backend needs regular flips.
  _system->copyRectToScreen(frame.data(), 640, 0, 0, 640, 480);
  _system->updateScreen();
  _system->delayMillis(10);
 }
 _mixer->pauseAll(false); surface.free(); return Common::kNoError;
}
}
