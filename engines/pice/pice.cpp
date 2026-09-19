// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/pice.h"
#include "engines/util.h"
#include "common/system.h"
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
 font->drawString(screen, Common::String::format("KSVGA resource %u | image %u/%u", _pictures[_picture], _picture + 1, _pictures.size()), 8, 24, 624, _textColor);
 font->drawString(screen, "Left click: next | Right click: previous | middle button: voice", 8, 365, 624, _textColor);
 font->drawString(screen, "[ Previous ]       [ Next ]        [ Voice sample ]       [ Exit ]", 8, 385, 624, _textColor);
 _system->unlockScreen();
}
bool PiceEngine::showPicture(uint position) {
 Common::Array<byte> pixels, palette;
 uint16 w, h;
 uint resource = _pictures[position];
 if (!_graphics.read(resource, pixels) || !decodeImageHeader(pixels, w, h) ||
     !_graphics.read(resource + 1, palette) || palette.size() != 768) return false;
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
void PiceEngine::playSpeech() {
 _mixer->stopHandle(_speechHandle);
 Common::Array<byte> data;
 for (uint n = 0; n < _language.count(); ++n) {
  uint resource = _speech++ % _language.count();
  if (!_language.read(resource, data)) { warning("[pice] Cannot decode voice %u", resource); return; }
  if (data.size() < 12 || READ_BE_UINT32(&data[0]) != MKTAG('R','I','F','F')) continue;
  byte *owned = (byte *)malloc(data.size());
  if (!owned) return;
  memcpy(owned, &data[0], data.size());
  Common::MemoryReadStream *stream = new Common::MemoryReadStream(owned, data.size(), DisposeAfterUse::YES);
  Audio::SeekableAudioStream *wave = Audio::makeWAVStream(stream, DisposeAfterUse::YES);
  if (!wave) { warning("[pice] Invalid WAV resource %u", resource); return; }
  debug("[pice] speech=%u bytes=%u fnv=%08x rate=%d", resource, data.size(), hashBytes(data), wave->getRate());
  _mixer->playStream(Audio::Mixer::kSpeechSoundType, &_speechHandle, wave);
  return;
 }
}
Common::Error PiceEngine::run() {
 if (!runSelfTests()) return Common::Error(Common::kUnknownError, "PICE parser self-tests failed");
 debug("[pice] parser self-tests PASS");
 if (!_graphics.open(Common::Path("KSVGA.KRO")) || !_language.open(Common::Path("E_KLANG.KRO")))
  return Common::Error(Common::kReadingFailed, "PICE preview requires the English European CD KSVGA.KRO and E_KLANG.KRO");
 debug("[pice] KSVGA=%u E_KLANG=%u entries", _graphics.count(), _language.count());
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
 // Exercise the real WAV path on the first testbench run as well as by request.
 playSpeech();
 bool done = false;
 while (!shouldQuit() && !done) {
  Common::Event event;
  while (_eventMan->pollEvent(event)) {
   int change = 0;
   if (event.type == Common::EVENT_RBUTTONDOWN) change = -1;
   if (event.type == Common::EVENT_MBUTTONDOWN) playSpeech();
   if (event.type == Common::EVENT_LBUTTONDOWN) {
    if (event.mouse.y >= 380) {
     if (event.mouse.x < 155) change = -1;
     else if (event.mouse.x < 310) change = 1;
     else if (event.mouse.x < 500) playSpeech();
     else done = true;
    } else change = 1;
   }
   if (event.type == Common::EVENT_KEYDOWN) {
    if (event.kbd.keycode == Common::KEYCODE_ESCAPE) done = true;
    if (event.kbd.keycode == Common::KEYCODE_RIGHT) change = 1;
    if (event.kbd.keycode == Common::KEYCODE_LEFT) change = -1;
    if (event.kbd.keycode == Common::KEYCODE_SPACE) playSpeech();
   }
   if (change) {
    uint next = (_picture + _pictures.size() + change) % _pictures.size();
    if (!showPicture(next)) { _mixer->stopHandle(_speechHandle); CursorMan.popCursor(); return Common::Error(Common::kReadingFailed); }
   }
  }
  _system->updateScreen(); _system->delayMillis(10);
 }
 _mixer->stopHandle(_speechHandle); CursorMan.popCursor();
 return Common::kNoError;
}
}
