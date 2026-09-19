// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/pice.h"
#include "pice/resource-names.h"
#include "pice/script.h"
#include "pice/dialogue.h"
#include "pice/voice.h"
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
 Common::String status = _spriteMode ? Common::String::format("Sprite %u | sequence %u | step %u/%u | provisional 20 ms/tick", _animationId, _sequence, _step + 1, _animation.sequences[_sequence].steps.size()) : Common::String::format("KSVGA resource %u | image %u/%u", _pictures[_picture], _picture + 1, _pictures.size());
 font->drawString(screen, status, 8, 24, 624, _textColor);
 font->drawString(screen, "Click title: images/animation | Left: next | Right: previous", 8, 365, 624, _textColor);
 font->drawString(screen, "[ Previous ]       [ Next ]        [ Repeat line ]        [ Exit ]", 8, 385, 624, _textColor);
 screen->fillRect(Common::Rect(0,338,640,360),0);
 if (!_subtitle.empty()) font->drawString(screen,_subtitle,8,341,624,_textColor,Graphics::kTextAlignCenter);
 _system->unlockScreen();
}
bool PiceEngine::showPicture(uint position) {
 Common::Array<byte> pixels, palette;
 uint16 w, h;
 uint resource = _pictures[position];
 if (!_graphics.read(resource, pixels) || !decodeImageHeader(pixels, w, h) ||
     !_graphics.read(resource + 1, palette) || palette.size() != 768) return false;
 _spriteMode = false;
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
bool PiceEngine::showAnimation(uint resource) {
 Common::Array<byte> data;
 if (!_graphics.read(resource, data) || !_animation.load(data)) return false;
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
 if (_animation.sequences.empty() || _animation.sequences[_sequence].steps.empty()) return false;
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
 if (!runSelfTests() || !runSpriteTests() || !runResourceNameTests() || !runScriptTests() || !runDialogueTests() || !runVoiceTests()) return Common::Error(Common::kUnknownError, "PICE parser self-tests failed");
 debug("[pice] parser self-tests PASS");
 if (!_graphics.open(Common::Path("KSVGA.KRO")) || !_language.open(Common::Path("E_KLANG.KRO")))
  return Common::Error(Common::kReadingFailed, "PICE preview requires the English European CD KSVGA.KRO and E_KLANG.KRO");
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
 uint prefixStopped=0, prefixUnsupported=0, prefixInvalid=0, prefixBudget=0, prefixMoved=0;
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
 debug("[pice] script-prefix-probes moved=%u stopped=%u unsupported=%u invalid-or-unsupported-expression=%u budget=%u",prefixMoved,prefixStopped,prefixUnsupported,prefixInvalid,prefixBudget);
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
 if (!playDialogueSample()) playSpeech();
 if (!showAnimation(0)) warning("[pice] Animation 0 unsupported; staying in background preview");
 bool done = false;
 while (!shouldQuit() && !done) {
  Common::Event event;
  while (_eventMan->pollEvent(event)) {
   int change = 0;
   if (event.type == Common::EVENT_RBUTTONDOWN) change = -1;
   if (event.type == Common::EVENT_MBUTTONDOWN) playSpeech();
   if (event.type == Common::EVENT_LBUTTONDOWN) {
    if (event.mouse.y < 40) {
     if (_spriteMode) showPicture(_picture);
     else if (!showAnimation(0)) warning("[pice] Cannot open animation 0");
    } else if (event.mouse.y >= 380) {
     if (event.mouse.x < 155) change = -1;
     else if (event.mouse.x < 310) change = 1;
     else if (event.mouse.x < 500) playDialogueSample();
     else done = true;
    } else change = 1;
   }
   if (event.type == Common::EVENT_KEYDOWN) {
    if (event.kbd.keycode == Common::KEYCODE_ESCAPE) done = true;
    if (event.kbd.keycode == Common::KEYCODE_RIGHT) change = 1;
    if (event.kbd.keycode == Common::KEYCODE_LEFT) change = -1;
    if (event.kbd.keycode == Common::KEYCODE_SPACE) playSpeech();
    if (event.kbd.keycode == Common::KEYCODE_c) playDialogueSample();
   }
   if (change && _spriteMode) {
    int next = int(_sequence) + change;
    if (next < 0) next = _animation.sequences.size() - 1;
    _sequence = uint(next) % _animation.sequences.size();
    // Keep the same anchor when browsing sequences from this resource.
    if (_animation.sequences[_sequence].steps.empty()) _sequence = 0;
    _step = 0; _nextFrame = 0;
   } else if (change) {
    uint next = (_picture + _pictures.size() + change) % _pictures.size();
    if (!showPicture(next)) { _mixer->stopHandle(_speechHandle); CursorMan.popCursor(); return Common::Error(Common::kReadingFailed); }
   }
  }
  advanceAnimation();
  _system->updateScreen(); _system->delayMillis(10);
 }
 _mixer->stopHandle(_speechHandle); CursorMan.popCursor();
 return Common::kNoError;
}
}
