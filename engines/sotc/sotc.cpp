// SPDX-License-Identifier: GPL-3.0-or-later
#include "sotc/sotc.h"
#include "common/error.h"
#include "common/system.h"
#include "graphics/font.h"
#include "sotc/pak.h"
#include "sotc/va2.h"
#include "common/events.h"
#include "common/textconsole.h"
#include "graphics/paletteman.h"
#include "graphics/fontman.h"
#include "graphics/surface.h"
#include "engines/util.h"
namespace Sotc {
Common::Error SotcEngine::run() {
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

 Graphics::Surface surface; surface.create(320, 240, Graphics::PixelFormat::createFormatCLUT8());
 while (!shouldQuit()) {
  if (reload) {
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
   nextFrame = _system->getMillis() + 150; loadActor = false; redraw = true;
   if (!actorEntries.empty()) debug("[sotc-va2] %s", actors.entries[actorEntries[actor]].name.c_str());
  }
  unsigned actorFrames = actorData.empty() ? 0 : va2FrameCount(actorData.data(), actorData.size());
  if (showActor && actorFrames && int32(_system->getMillis() - nextFrame) >= 0) {
   actorFrame = (actorFrame + 1) % actorFrames; nextFrame = _system->getMillis() + 150; redraw = true;
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
   if (showActor && !actorEntries.empty()) {
    unsigned width, height;
    bool decoded = !actorData.empty() && va2Frame(actorData.data(), actorData.size(), actorFrame,
      actorPixels.data(), actorMask.data(), actorPixels.size(), width, height);
    actorTitle = Common::String::format("%s %u/%u %s", actors.entries[actorEntries[actor]].name.c_str(),
      actorFrames ? actorFrame + 1 : 0, actorFrames, decoded ? "" : "unsupported");
    if (decoded) {
     int left = (320 - int(width)) / 2, top = 170 - int(height);
     for (unsigned y = 0; y < height; ++y) for (unsigned x = 0; x < width; ++x) {
      int dx = left + x, dy = top + y;
      if (dx >= 0 && dx < 320 && dy >= 0 && dy < 200 && actorMask[y * width + x])
       *static_cast<byte *>(surface.getBasePtr(dx, dy)) = actorPixels[y * width + x];
     }
    }
   }
   const Graphics::Font *font = FontMan.getFontByUsage(Graphics::FontManager::kGUIFont);
   font->drawString(&surface, title, 2, 201, 316, 7);
   font->drawString(&surface, actorTitle, 2, 214, 316, 7);
   font->drawString(&surface, "L1/R1: figure  Triangle: toggle  []: pal", 2, 227, 316, 7);
   for (int y = 0; y < 480; ++y) for (int x = 0; x < 640; ++x)
    frame[y * 640 + x] = *static_cast<const byte *>(surface.getBasePtr(x / 2, y / 2));
   if (!showActor || actorFrame == 0) debug("[sotc] %s palette=%s", title.c_str(), resources.entries[palettes[pal]].name.c_str()); redraw = false;
  }
  Common::Event event;
  while (_eventMan->pollEvent(event)) {

   if (event.type == Common::EVENT_LBUTTONDOWN || (event.type == Common::EVENT_KEYDOWN && event.kbd.keycode == Common::KEYCODE_RIGHT)) { if (!backgrounds.empty()) scene = (scene + 1) % backgrounds.size(); redraw = true; }
   if (event.type == Common::EVENT_RBUTTONDOWN) { if (!backgrounds.empty()) scene = (scene + backgrounds.size() - 1) % backgrounds.size(); redraw = true; }
   if (event.type != Common::EVENT_KEYDOWN) continue;
   switch (event.kbd.keycode) {
   case Common::KEYCODE_LEFT: if (!backgrounds.empty()) scene = (scene + backgrounds.size() - 1) % backgrounds.size(); redraw = true; break;
   case Common::KEYCODE_UP: bank = (bank + 1) % 10; reload = true; break;
   case Common::KEYCODE_DOWN: bank = (bank + 9) % 10; reload = true; break;
   case Common::KEYCODE_p: pal = (pal + 1) % palettes.size(); redraw = true; break;
   case Common::KEYCODE_a: showActor = !showActor; redraw = true; break;
   case Common::KEYCODE_F5: if (!actorEntries.empty()) actor = (actor + actorEntries.size() - 1) % actorEntries.size(); loadActor = true; break;
   case Common::KEYCODE_F7: if (!actorEntries.empty()) actor = (actor + 1) % actorEntries.size(); loadActor = true; break;
   case Common::KEYCODE_ESCAPE: quitGame(); break;
   default: break;
   }
  }
  // Keep presenting even a static room: the PS4 backend needs regular flips.
  _system->copyRectToScreen(frame.data(), 640, 0, 0, 640, 480);
  _system->updateScreen();
  _system->delayMillis(10);
 }
 surface.free(); return Common::kNoError;
}
}
