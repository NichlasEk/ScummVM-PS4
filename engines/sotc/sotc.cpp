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
#include "common/file.h"
#include "common/events.h"
#include "common/textconsole.h"
#include "graphics/paletteman.h"
#include "graphics/fontman.h"
#include "graphics/surface.h"
#include "engines/util.h"
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
 ScriptMachine startupMachine; unsigned scriptTicks = 0; Common::String scriptStatus;
 bool startupAvailable = loadStartupScene(startupPixels, startupStatus, startupMachine), showStartup = startupAvailable;
 if (startupAvailable) scriptStatus = stepStartupScripts(startupMachine, ++scriptTicks);

 Graphics::Surface surface; surface.create(320, 240, Graphics::PixelFormat::createFormatCLUT8());
 while (!shouldQuit()) {
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
  if (!showStartup && showActor && cycle && actorFrame + 1 < cycle && int32(_system->getMillis() - nextFrame) >= 0) {
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
    title = "Start snapshot - not gameplay"; actorTitle = scriptStatus;
   }
   const Graphics::Font *font = FontMan.getFontByUsage(Graphics::FontManager::kGUIFont);
   font->drawString(&surface, title, 2, 201, 316, 7);
   font->drawString(&surface, actorTitle, 2, 214, 316, 7);
   font->drawString(&surface, showStartup ? "Cross/click: script step  Options: view" : "Options: start scene  L1/R1: figure", 2, 227, 316, 7);
   for (int y = 0; y < 480; ++y) for (int x = 0; x < 640; ++x)
    frame[y * 640 + x] = *static_cast<const byte *>(surface.getBasePtr(x / 2, y / 2));
   if (!showActor || actorFrame == 0) debug("[sotc] %s palette=%s", title.c_str(), resources.entries[palettes[pal]].name.c_str()); redraw = false;
  }
  Common::Event event;
  while (_eventMan->pollEvent(event)) {
   if (event.type == Common::EVENT_KEYDOWN && event.kbd.keycode == Common::KEYCODE_F8) {
    if (startupAvailable) { showStartup = !showStartup; pal = 0; redraw = true; }
    continue;
   }
   if (showStartup && event.type == Common::EVENT_LBUTTONDOWN) {
    scriptStatus = stepStartupScripts(startupMachine, ++scriptTicks); redraw = true; continue;
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
 surface.free(); return Common::kNoError;
}
}
