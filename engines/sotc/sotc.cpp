// SPDX-License-Identifier: GPL-3.0-or-later
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
 SoundEffects sound(_mixer);
 ScriptMachine startupMachine; startupMachine.setSound(&sound); unsigned scriptTicks = 0; Common::String scriptStatus;
 StartupScene startupScene; startupMachine.setAnimation(&startupScene);
 bool startupAvailable = false, showStartup = false;
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
 OpeningScene animatedOpening(sound); bool openingAnimated=false;
 uint32 openingLast=_system->getMillis(); unsigned openingMusicClock=0;
 if (showOpening) debug("[sotc-opening] logos from R05/D05 followed by supported animated prefix");
 else warning("[sotc-opening] Logo prefix unavailable; opening menu directly");
 auto finishOpening=[&]() {
  showOpening=false; sound.music(255); sound.effect(255); redraw=true;
  debug("[sotc-menu] opening finished or skipped; waiting for New game");
 };

 Common::Array<Common::String> dialogueLines; Common::String shownDialogue; unsigned dialoguePage=0;
 int pointerX=0,pointerY=0;
 bool runScene = true, sceneFault = false; unsigned heldDirection=0;
 uint32 nextSceneFrame = _system->getMillis() + 100;
 auto sceneStep = [&]() {
  if (!startupScene.movePlayer(startupMachine)) { runScene=false; sceneFault=true; redraw=true; return; }
  scriptStatus = stepStartupScripts(startupMachine, ++scriptTicks);
  bool rendered = startupScene.render(startupPixels,startupStatus,startupMachine);
  unsigned blockedAnimations = rendered ? startupScene.advance(startupMachine) : 1;
  if (!rendered || blockedAnimations) runScene = false;
  for (unsigned i=0;i<startupMachine.count();++i)
   if (startupMachine.task(i)->result >= kUnknownWait) runScene = false;
  sceneFault=!runScene;
  if (!runScene) debug("[sotc-animation] paused tick=%u unsupported=%u",scriptTicks,blockedAnimations);
  redraw = true;
 };
 auto resumeScene = [&]() {
  if (!sceneFault) { runScene=true; nextSceneFrame=_system->getMillis()+100; }
 };
 Graphics::Surface surface; surface.create(320, 240, Graphics::PixelFormat::createFormatCLUT8());
 while (!shouldQuit()) {
  if (showOpening) {
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
  // Diagnostic 10 Hz preview cadence; original scene pacing is not yet bound.
  if (!mainMenu && showStartup && runScene && int32(_system->getMillis()-nextSceneFrame)>=0) {
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
    title = runScene ? "Walk test - collision stops" : "Scene paused - not gameplay"; actorTitle = scriptStatus;
   }
   const Graphics::Font *font = FontMan.getFontByUsage(Graphics::FontManager::kGUIFont);
   if (showStartup && !startupScene.dialogueText().empty()) {
    if (shownDialogue!=startupScene.dialogueText()) {
     shownDialogue=startupScene.dialogueText(); dialoguePage=0; dialogueLines.clear();
     font->wordWrapText(shownDialogue,306,dialogueLines);
    }
    unsigned end=MIN<unsigned>(dialoguePage+8,dialogueLines.size());
    surface.fillRect(Common::Rect(2,2,318,8+(end-dialoguePage)*font->getFontHeight()),0);
    for (unsigned i=dialoguePage;i<end;++i)
     font->drawString(&surface,dialogueLines[i],7,5+(i-dialoguePage)*font->getFontHeight(),306,7);
   } else { shownDialogue.clear(); dialogueLines.clear(); dialoguePage=0; }
   font->drawString(&surface, title, 2, 201, 316, 7);
   font->drawString(&surface, actorTitle, 2, 214, 316, 7);
   font->drawString(&surface, showStartup ? (!startupScene.dialogueText().empty() ? "Cross / click: next dialogue page" : "Click / Cross: walk  D-pad: steer") : "F8: start scene  L1/R1: figure", 2, 227, 316, 7);
   if (mainMenu) {
    surface.fillRect(Common::Rect(320,240),0);
    if (!menuBackground.empty()) memcpy(surface.getBasePtr(0,0),menuBackground.data(),64000);
    surface.fillRect(Common::Rect(32,26,288,58),0);
    font->drawString(&surface,"Shadow of the Comet",32,36,256,7,Graphics::kTextAlignCenter);
    const char *labels[]={"Continue", "New game", "Settings", "Quit"};
    for (int i=0;i<4;++i) {
     const int y=78+i*26;
     surface.fillRect(Common::Rect(64,y,256,y+22),0);
     if (i==menuSelection) surface.frameRect(Common::Rect(64,y,256,y+22),7);
     font->drawString(&surface,labels[i],68,y+6,184,i==0 && !startupAvailable ? 8 : 7,Graphics::kTextAlignCenter);
    }
    font->drawString(&surface,"Experimental engine - opening scene",0,207,320,7,Graphics::kTextAlignCenter);
    font->drawString(&surface,"D-pad / click: select   Cross: confirm",0,226,320,7,Graphics::kTextAlignCenter);
   }
   if ((mainMenu || showStartup) && pointerY<480 && pointerX<640 && pointerX>=0 && pointerY>=0) {
    int px=pointerX/2,py=pointerY/2;
    for (int d=-3;d<=3;++d) {
     if (px+d>=0 && px+d<320) *static_cast<byte *>(surface.getBasePtr(px+d,py))=7;
     if (py+d>=0 && py+d<240) *static_cast<byte *>(surface.getBasePtr(px,py+d))=7;
    }
   }
   if (showOpening) {
    surface.fillRect(Common::Rect(320,240),0);
    memcpy(surface.getBasePtr(0,20),openingAnimated ? animatedOpening.pixels().data() : openingImages[openingClock.logo()].data(),64000);
    byte faded[768]; unsigned brightness=openingAnimated ? animatedOpening.brightness() : openingClock.brightness();
    for (unsigned i=0;i<768;++i) faded[i]=palette[i]*brightness/255;
    _system->getPaletteManager()->setPalette(faded,0,256);
   }
   for (int y = 0; y < 480; ++y) for (int x = 0; x < 640; ++x)
    frame[y * 640 + x] = *static_cast<const byte *>(surface.getBasePtr(x / 2, y / 2));
   if (!showActor || actorFrame == 0) debug("[sotc] %s palette=%s", title.c_str(), resources.entries[palettes[pal]].name.c_str()); redraw = false;
  }
  Common::Event event;
  while (_eventMan->pollEvent(event)) {
   if (event.type==Common::EVENT_MOUSEMOVE) { pointerX=event.mouse.x; pointerY=event.mouse.y; if (showStartup || mainMenu) redraw=true; }
   if (event.type==Common::EVENT_KEYDOWN || event.type==Common::EVENT_KEYUP) debug("[sotc-input] type=%u key=%u",unsigned(event.type),unsigned(event.kbd.keycode));
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
     do { menuSelection=(menuSelection+movement+4)%4; } while (!startupAvailable && menuSelection==0);
     redraw=true;
    }
    if (event.type==Common::EVENT_MOUSEMOVE || event.type==Common::EVENT_LBUTTONDOWN) {
     int x=event.mouse.x/2,y=event.mouse.y/2;
     if (x>=64 && x<256 && y>=78 && y<178 && (y-78)%26<22) {
      int selection=(y-78)/26;
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
       shownDialogue.clear(); dialogueLines.clear(); dialoguePage=0; pal=0;
       scriptStatus=stepStartupScripts(startupMachine,++scriptTicks);
       mainMenu=false; _mixer->pauseAll(false); nextSceneFrame=_system->getMillis()+100;
       debug("[sotc-menu] New game: opening scene reset");
      }
     } else if (menuSelection==2) {
      GUI::ConfigDialog settings; runDialog(settings); syncSoundSettings();
      _mixer->pauseAll(true);
     } else if (menuSelection==3) quitGame();
     redraw=true;
    }
    continue;
   }
   if (event.type==Common::EVENT_KEYDOWN && event.kbd.keycode==Common::KEYCODE_ESCAPE) {
    heldDirection=0;
    if (showStartup && !startupScene.scriptWalking()) { startupScene.cancelRoute(); startupScene.walk(0,startupMachine); }
    mainMenu=true; menuSelection=startupAvailable ? 0 : 1; _mixer->pauseAll(true); redraw=true;
    debug("[sotc-menu] opened; simulation and sound paused");
    continue;
   }
   if (event.type==Common::EVENT_KEYDOWN && event.kbd.keycode==Common::KEYCODE_RETURN) {
    event.type=Common::EVENT_LBUTTONDOWN; event.mouse.x=pointerX; event.mouse.y=pointerY;
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
      if (dialoguePage+8<dialogueLines.size()) dialoguePage+=8;
      else startupScene.finishDialogue();
      resumeScene(); redraw=true;
     }
     continue;
    }
    if (startupScene.scriptWalking()) continue;
    if (event.mouse.y>=0 && event.mouse.y<400 && event.mouse.x>=0 && event.mouse.x<640) {
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
