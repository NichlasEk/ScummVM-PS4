// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/pice.h"
#include "common/config-manager.h"
#include "common/events.h"
#include "common/file.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "graphics/cursorman.h"
#include "graphics/font.h"
#include "graphics/fontman.h"
#include "graphics/paletteman.h"
#include "graphics/surface.h"

namespace Pice {
bool PiceEngine::startupMovies() {
 // Original startup 0x18df0/0x18e0a/0x18e24. ICE640 uses the
 // corresponding 640x480 S clips. Escape/click skips one clip at a time.
 _frontendMovies=true;
 bool ok=true;
 for(const char *name : {"SINTRO.MUX","SCREDITS.MUX","STITRE.MUX"}) {
  if(shouldQuit()) break;
  if(!playMovie(name,1,1)) { ok=false; break; }
 }
 _frontendMovies=false;
 return ok;
}
bool PiceEngine::newGame() {
 _mixer->stopHandle(_speechHandle);
 _gameOver=_gameCompleted=false; _savePending=false;
 _combinationRunner.reset(); _combinationFailed=false;
 _interruptedAction=InterruptedAction();
 _chapter=_pendingChapter=2; _pendingRoom=-1; _previousRoom=-1;
 _inventoryEnabled=_playerMovement=true; _advanceRequested=false;
 _spriteMode=false; _subtitle.clear();
 return showScriptScene();
}

bool PiceEngine::mainMenu(bool resumable) {
 Common::Array<byte> data;
 SpriteResource panel;
 // S_PANEL/1: original wooden crate, with no baked-in menu text.
 if(!_graphics.read(1321,data) || !panel.load(data,1321,true) || panel.palette.size()!=768 || panel.frames.empty()) {
  warning("[pice] Cannot decode original main-menu panel"); return false;
 }
 Common::Array<byte> backdrop; backdrop.resize(640*400,0);
 if(!panel.paintFrame(0,64,32,backdrop,640,400)) return false;
 byte oldPalette[768]; _system->getPaletteManager()->grabPalette(oldPalette,0,256);
 Common::Array<Audio::SoundHandle> paused;
 auto pause=[&](Audio::SoundHandle handle) {
  if(_mixer->isSoundHandleActive(handle)) { _mixer->pauseHandle(handle,true); paused.push_back(handle); }
 };
 pause(_music.handle); pause(_speechHandle);
 for(uint i=0;i<_sounds.size();++i) if(_sounds[i].active) pause(_sounds[i].handle);
 uint32 opened=_system->getMillis();
 bool restoreScene=true, running=false, finished=false;
 bool manual=readCheckpoint(false,false), automatic=readCheckpoint(true,false);
 enum Page { kMain,kLoad,kOptions,kExtras,kNewConfirm,kQuitConfirm };
 Page page=kMain;
 int selection=resumable ? 0 : 1;
 Common::String notice;
 const Graphics::Font *font=FontMan.getFontByUsage(Graphics::FontManager::kGUIFont);
 CursorMan.setDefaultArrowCursor(false); CursorMan.showMouse(true); _hotCursor=false; _cursorItem=-1;
 debug("[pice] main menu opened resumable=%d manual=%d autosave=%d",resumable,manual,automatic);
#ifdef PICE_TESTBENCH
 uint testKey=0; uint32 nextTestKey=opened+500; Common::String testKeys=ConfMan.get(resumable ? "pice_test_pause_keys" : "pice_test_frontend_keys");
 int capturedPage=-1;
#endif
 while(!shouldQuit() && !finished) {
  Common::Array<Common::String> rows; Common::Array<byte> enabled;
  const char *title="Main menu";
  auto add=[&](const Common::String &label,bool active=true) { rows.push_back(label); enabled.push_back(active); };
  if(page==kMain) {
   add("Return to game",resumable); add("New game"); add("Load a game",manual || automatic);
   add("Save a game",resumable && snapshotReady()); add("Options"); add("Exit game");
  } else if(page==kLoad) {
   title="Load a game"; add("Latest save",manual || automatic); add("Manual save (Q)",manual); add("Autosave",automatic); add("Back");
  } else if(page==kOptions) {
   title="Options";
   for(const char *key : {"speech_volume","music_volume","sfx_volume"}) {
    const char *label=key[0]=='s' && key[1]=='p' ? "Speech" : key[0]=='m' ? "Music" : "Sound effects";
    add(Common::String::format("%s  < %d%% >",label,(ConfMan.getInt(key)*100+127)/255));
   }
   add(Common::String::format("Stick sensitivity < %d/7 >",CLIP(ConfMan.getInt("kbdmouse_speed"),0,7)));
   add(_subtitles ? "Subtitles: On" : "Subtitles: Off"); add("Text language: English",false); add("Films and extras"); add("Back");
  } else if(page==kExtras) {
   title="Films and extras"; add("Replay introduction"); add("Credits"); add("Outtakes"); add("Back");
  } else {
   title=page==kNewConfirm ? "Start a new game?" : "Exit game?";
   add("No - go back"); add("Yes");
  }
  selection=CLIP(selection,0,int(rows.size())-1);
  const int top=page==kOptions ? 118 : 155, spacing=page==kOptions ? 29 : 32;
  Common::Array<byte> colors=panel.palette;
  const byte ink[]={0,0,0, 240,210,132, 255,245,201, 118,100,67};
  for(uint i=0;i<sizeof(ink);++i) colors[250*3+i]=ink[i];
  _system->getPaletteManager()->setPalette(&colors[0],0,256);
  _system->copyRectToScreen(&backdrop[0],640,0,0,640,400);
  Graphics::Surface *screen=_system->lockScreen();
  auto text=[&](const Common::String &label,int y,byte color) {
   Graphics::Surface line; line.create(226,12,Graphics::PixelFormat::createFormatCLUT8());
   line.fillRect(Common::Rect(0,0,226,12),250);
   font->drawString(&line,label,0,1,226,color,Graphics::kTextAlignCenter);
   for(int py=0;py<24;++py) for(int px=0;px<452;++px) {
    byte pixel=*static_cast<byte *>(line.getBasePtr(px/2,py/2));
    if(pixel!=250) *static_cast<byte *>(screen->getBasePtr(94+px,y+py))=pixel;
   }
   line.free();
  };
  text(title,43,251);
  for(uint i=0;i<rows.size();++i) {
   int y=top+i*spacing;
   if(int(i)==selection && enabled[i]) screen->frameRect(Common::Rect(140,y-2,506,y+25),252);
   text(rows[i],y,enabled[i] ? (int(i)==selection ? 252 : 251) : 253);
  }
  if(page==kNewConfirm && resumable) text("Unsaved progress will be lost",116,251);
  if(page==kQuitConfirm && resumable) text("Unsaved progress will be lost",116,251);
  if(!notice.empty()) text(notice,365,252);
  _system->unlockScreen(); _system->updateScreen();
#ifdef PICE_TESTBENCH
  // The test driver injects ordinary input; it cannot bypass menu guards.
  if(testKey<testKeys.size() && _system->getMillis()>=nextTestKey) {
   char key=testKeys[testKey++]; Common::Event e; e.type=Common::EVENT_KEYDOWN;
   e.kbd.keycode=key=='d' ? Common::KEYCODE_DOWN : key=='u' ? Common::KEYCODE_UP : key=='l' ? Common::KEYCODE_LEFT : key=='r' ? Common::KEYCODE_RIGHT : key=='b' ? Common::KEYCODE_ESCAPE : Common::KEYCODE_RETURN;
   _eventMan->pushEvent(e); nextTestKey=_system->getMillis()+250;
  }
  if(ConfMan.getBool("pice_test_frontend_capture") && capturedPage!=int(page)) {
   capturedPage=page; Common::DumpFile out;
   if(out.open(Common::Path(Common::String::format("/data/scummvm/saves/pice-menu-%d.ppm",int(page))))) {
    const char header[]="P6\n640 400\n255\n"; out.write(header,sizeof(header)-1);
    screen=_system->lockScreen();
    for(int y=0;y<400;++y) for(int x=0;x<640;++x) { byte c=*static_cast<byte *>(screen->getBasePtr(x,y)); out.write(&colors[c*3],3); }
    _system->unlockScreen(); out.finalize();
   }
  }
#endif
  Common::Event event;
  while(!finished && _eventMan->pollEvent(event)) {
   if(event.type==Common::EVENT_QUIT) { quitGame(); break; }
   bool activate=false,back=false; int adjust=0;
   if(event.type==Common::EVENT_MOUSEMOVE || event.type==Common::EVENT_LBUTTONDOWN) {
    int row=(event.mouse.y-top)/spacing;
    if(event.mouse.x>=140 && event.mouse.x<506 && event.mouse.y>=top && row<int(rows.size()) && (event.mouse.y-top)%spacing<24) {
     if(enabled[row]) selection=row;
     activate=event.type==Common::EVENT_LBUTTONDOWN && enabled[row];
     if(activate && page==kOptions && row<4) { adjust=event.mouse.x<320 ? -1 : 1; activate=false; }
    }
   }
   if(event.type==Common::EVENT_RBUTTONDOWN) back=true;
   if(event.type==Common::EVENT_KEYDOWN) {
    auto key=event.kbd.keycode;
    if(key==Common::KEYCODE_UP || key==Common::KEYCODE_DOWN) {
     int direction=key==Common::KEYCODE_UP ? -1 : 1;
     do { selection=(selection+rows.size()+direction)%rows.size(); } while(!enabled[selection]);
    }
    activate=key==Common::KEYCODE_RETURN || key==Common::KEYCODE_SPACE;
    back=key==Common::KEYCODE_ESCAPE;
    adjust=key==Common::KEYCODE_LEFT ? -1 : key==Common::KEYCODE_RIGHT ? 1 : 0;
   }
   if(back) {
    if(page==kMain) { if(resumable) { running=true; finished=true; } }
    else { page=kMain; selection=resumable ? 0 : 1; notice.clear(); }
    break;
   }
   if(page==kOptions && adjust && selection==3) {
    int speed=CLIP(ConfMan.getInt("kbdmouse_speed")+adjust,0,7);
    ConfMan.setInt("kbdmouse_speed",speed,Common::ConfigManager::kApplicationDomain);
    ConfMan.setInt("kbdmouse_speed",speed);
    ConfMan.flushToDisk(); break;
   }
   if(page==kOptions && adjust && selection<3) {
    const char *keys[]={"speech_volume","music_volume","sfx_volume"};
    int volume=CLIP(ConfMan.getInt(keys[selection])+adjust*26,0,255);
    ConfMan.setInt(keys[selection],volume,Common::ConfigManager::kApplicationDomain);
    ConfMan.setInt(keys[selection],volume);
    ConfMan.setBool("mute",false,Common::ConfigManager::kApplicationDomain);
    ConfMan.setBool("mute",false);
    if(selection==0) { ConfMan.setBool("speech_mute",false,Common::ConfigManager::kApplicationDomain); ConfMan.setBool("speech_mute",false); }
    syncSoundSettings(); ConfMan.flushToDisk(); break;
   }
   if(!activate || !enabled[selection]) continue;
   notice.clear();
   debug("[pice] frontend action page=%d row=%d",int(page),selection);
   if(page==kMain) {
    if(selection==0) { running=true; finished=true; }
    if(selection==1) { page=kNewConfirm; selection=0; }
    else if(selection==2) { page=kLoad; selection=0; }
    else if(selection==3) { bool saved=writeCheckpoint(false); manual|=saved; notice=saved ? "Manual save written" : "Saving unavailable right now"; }
    else if(selection==4) { page=kOptions; selection=0; }
    else if(selection==5) { page=kQuitConfirm; selection=0; }
   } else if(page==kOptions) {
    if(selection==4) { _subtitles=!_subtitles; ConfMan.setBool("pice_subtitles",_subtitles,Common::ConfigManager::kApplicationDomain); ConfMan.setBool("pice_subtitles",_subtitles); ConfMan.flushToDisk(); }
    else if(selection==6) { page=kExtras; selection=0; }
    else if(selection==7) { page=kMain; selection=4; }
   } else if(page==kExtras) {
    if(selection==3) { page=kOptions; selection=6; }
    else {
     bool ok;
     if(selection==0) ok=startupMovies();
     else { _frontendMovies=true; ok=playMovie(selection==1 ? "SCREDITS.MUX" : "SBESTOFF.MUX",1,1); _frontendMovies=false; }
     if(!ok) notice="Movie could not be played";
    }
   } else if(page==kLoad) {
    if(selection==3) { page=kMain; selection=2; }
    else {
     bool ok=selection==0 ? loadLatestSave() : readCheckpoint(selection==2);
     if(ok) { restoreScene=false; running=true; finished=true; } else notice="No valid save available";
    }
   } else if(page==kNewConfirm) {
    if(selection==0) { page=kMain; selection=1; }
    else {
     if(newGame()) { restoreScene=false; running=true; finished=true; }
     else { notice="Could not start the game"; page=kMain; selection=1; resumable=false; }
    }
   } else if(page==kQuitConfirm) {
    if(selection==0) { page=kMain; selection=5; }
    else finished=true;
   }
   break;
  }
  _system->delayMillis(10);
 }
 for(uint i=0;i<paused.size();++i) _mixer->pauseHandle(paused[i],false);
 if(restoreScene) {
  _system->getPaletteManager()->setPalette(oldPalette,0,256);
  _pageStart+=_system->getMillis()-opened;
 }
 _roomClock=_roomRenderClock=_system->getMillis(); _advanceRequested=false;
 if(running) renderRoom();
 debug("[pice] main menu closed running=%d restored=%d",running,!restoreScene);
 return running;
}
}
