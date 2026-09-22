// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/pice.h"
#include "pice/film.h"
#include "engines/util.h"
#include "image/codecs/cinepak.h"
#include "common/config-manager.h"
#include "common/events.h"
#include "common/fs.h"
#include "common/memstream.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "graphics/cursorman.h"
#include "graphics/font.h"
#include "graphics/fontman.h"
#include "graphics/surface.h"
#include "audio/decoders/raw.h"
#include "audio/audiostream.h"
namespace Pice {
void PiceEngine::pauseEngineIntern(bool pause) {
 Engine::pauseEngineIntern(pause);
 if(_saturnMovieActive) {
  if(pause) _saturnPauseStart=_system->getMillis();
  else _saturnPausedMs+=_system->getMillis()-_saturnPauseStart;
 }
}
bool PiceEngine::playSaturnMovie(const Common::String &name,bool audit) {
 Common::File file; FilmIndex film;
 if(!file.open(Common::Path("MOVIE").join(name)) || !readFilmIndex(file,film)) {
  warning("[pice] Invalid Saturn FILM: %s",name.c_str()); return false;
 }
 Common::Array<byte> packet;
 Audio::SoundHandle sound;
 Common::ScopedPtr<Audio::QueuingAudioStream> audio(Audio::makeQueuingAudioStream(film.rate,false));
 if(!audit) for(uint i=0;i<film.samples.size();++i) if(film.samples[i].audio) {
  if(!readFilmSample(file,film.samples[i],packet)) return false;
  byte *copy=static_cast<byte *>(malloc(packet.size())); if(!copy) return false;
  memcpy(copy,packet.data(),packet.size());
  audio->queueBuffer(copy,packet.size(),DisposeAfterUse::YES,0); // signed 8-bit mono
 }
 audio->finish();
 Image::CinepakDecoder decoder;
 const Graphics::PixelFormat format=_system->getScreenFormat();
 if(!decoder.setOutputPixelFormat(format) || format.bytesPerPixel!=2) return false;
 Graphics::Surface canvas; canvas.create(640,480,format);
 if(!audit && film.audioBytes) _mixer->playStream(Audio::Mixer::kSFXSoundType,&sound,audio.release());
 _saturnMovieActive=true; _saturnPausedMs=0;
 uint32 start=_system->getMillis(); uint frames=0,strips=0; bool skipped=false,valid=true;
 auto waitUntil=[&](uint32 ms) {
  do {
   Common::Event event;
   while(_eventMan->pollEvent(event)) {
    if(event.type==Common::EVENT_QUIT) quitGame();
    if(!audit && (event.type==Common::EVENT_LBUTTONDOWN || (event.type==Common::EVENT_KEYDOWN &&
       (event.kbd.keycode==Common::KEYCODE_ESCAPE || event.kbd.keycode==Common::KEYCODE_SPACE)))) skipped=true;
   }
   if(shouldQuit() || skipped || audit || _system->getMillis()-start-_saturnPausedMs>=ms) break;
   _system->delayMillis(5);
  } while(true);
 };
 for(uint i=0;i<film.samples.size() && valid;++i) {
  const FilmSample &sample=film.samples[i];
  if(sample.audio) continue;
  waitUntil(uint64(sample.time)*1000/film.clock);
  if(shouldQuit() || skipped) break;
  if(!readFilmSample(file,sample,packet) || !validateFilmFrame(packet,film,strips)) { valid=false; break; }
  Common::MemoryReadStream stream(packet.data(),packet.size());
  const Graphics::Surface *frame=decoder.decodeFrame(stream);
  if(!frame || stream.err() || stream.eos() || frame->w!=film.width || frame->h!=film.height) { valid=false; break; }
  ++frames;
#ifdef PICE_TESTBENCH
  if(!audit && frames==10 && ConfMan.hasKey("pice_test_saturn_pause_ms")) {
   uint delay=CLIP<int>(ConfMan.getInt("pice_test_saturn_pause_ms"),0,5000);
   PauseToken token=pauseEngine(); _system->delayMillis(delay); token.clear();
  }
#endif
  if(!audit) {
   // Integer 2x scaling preserves the console pixels and reserves TV margins.
   canvas.fillRect(Common::Rect(640,480),0);
   uint scale=MIN<uint>(2,MIN<uint>(640/frame->w,480/frame->h));
   int left=(640-frame->w*scale)/2,top=(480-frame->h*scale)/2;
   for(int y=0;y<frame->h*scale;++y) for(int x=0;x<frame->w*scale;++x)
    *static_cast<uint16 *>(canvas.getBasePtr(left+x,top+y))=*static_cast<const uint16 *>(frame->getBasePtr(x/scale,y/scale));
   _system->copyRectToScreen(canvas.getPixels(),canvas.pitch,0,0,640,480); _system->updateScreen();
  }
 }
 if(valid && frames==film.videoCount && !skipped && !shouldQuit()) waitUntil(film.endMs);
 _saturnMovieActive=false;
 _mixer->stopHandle(sound); canvas.free();
 debug("[pice] Saturn movie name=%s frames=%u/%u audio=%u duration=%u valid=%d skipped=%d audit=%d",
       name.c_str(),frames,film.videoCount,film.audioBytes,film.endMs,valid,skipped,audit);
 debug("[pice] Saturn movie timing wall=%u paused=%u media=%u",_system->getMillis()-start,_saturnPausedMs,film.endMs);
 return valid && !shouldQuit() && (skipped || frames==film.videoCount);
}
Common::Error PiceEngine::runSaturnPreview() {
 if(!_graphics.open(Common::Path("KVGA.KRO")) || _graphics.isMac() || !runFilmTests())
  return Common::Error(Common::kReadingFailed,"Cannot read Saturn resources");
 // Separate preview path: never execute PC scripts or touch PC save slots.
 Graphics::PixelFormat format(2,5,6,5,0,11,5,0,0);
 initGraphics(640,480,&format);
 if(_system->getScreenFormat()!=format) return Common::Error(Common::kUnsupportedColorMode,"Saturn preview requires RGB565");
 CursorMan.showMouse(false); syncSoundSettings();
#ifdef PICE_TESTBENCH
 if(ConfMan.getBool("pice_test_saturn_assets")) {
  Common::FSList files;
  if(!Common::FSNode(ConfMan.getPath("path")).getChild("MOVIE").getChildren(files,Common::FSNode::kListFilesOnly))
   return Common::Error(Common::kReadingFailed,"Cannot list Saturn movies");
  uint movies=0;
  for(uint i=0;i<files.size();++i) {
   Common::String n=files[i].getName(); n.toUppercase();
   if(!n.hasSuffix(".CPK")) continue;
   if(!playSaturnMovie(files[i].getName(),true)) return Common::Error(Common::kReadingFailed,"Saturn movie audit failed");
   ++movies;
  }
  debug("[pice] Saturn FILM self-tests and full movie audit PASS movies=%u",movies);
 }
 if(ConfMan.hasKey("pice_test_saturn_movie")) {
  if(!playSaturnMovie(ConfMan.get("pice_test_saturn_movie"),false) && !shouldQuit())
   return Common::Error(Common::kReadingFailed,"Cannot play selected Saturn movie");
 } else
#endif
 {
  const char *startup[]={"INFOGRAM.CPK","XING.CPK","XINGCOPY.CPK","VINTRO.CPK"};
  for(uint i=0;i<4 && !shouldQuit();++i) if(!playSaturnMovie(startup[i],false) && !shouldQuit())
   return Common::Error(Common::kReadingFailed,"Cannot play Saturn startup movie");
 }
 Common::Array<uint> pictures; Common::Array<byte> data,palette;
 for(uint i=0;i+1<_graphics.count();++i) {
  const KroEntry &e=_graphics.entry(i); uint16 w,h;
  if(e.size<4+320*100 || e.size>4+640*240 || _graphics.entry(i+1).size!=768) continue;
  if(_graphics.read(i,data) && decodeImageHeader(data,w,h) && w>=320 && w<=640 && h<=240) pictures.push_back(i);
 }
 if(shouldQuit()) return Common::kNoError;
 if(pictures.empty()) return Common::Error(Common::kReadingFailed,"No Saturn room images");
 Graphics::Surface canvas; canvas.create(640,480,format);
 const Graphics::Font *font=FontMan.getFontByUsage(Graphics::FontManager::kGUIFont);
 uint index=0; bool redraw=true;
 while(!shouldQuit()) {
  if(redraw) {
   uint16 w,h;
   if(!_graphics.read(pictures[index],data) || !decodeImageHeader(data,w,h) || !_graphics.read(pictures[index]+1,palette)) {
    canvas.free(); return Common::Error(Common::kReadingFailed,"Cannot read Saturn room image");
   }
   canvas.fillRect(Common::Rect(640,480),0);
   uint scale=w<=320 ? 2 : 1, top=(480-h*scale)/2, left=(640-w*scale)/2;
   uint16 colors[256];
   for(uint c=0;c<256;++c) colors[c]=format.RGBToColor(palette[c*3],palette[c*3+1],palette[c*3+2]);
   for(uint y=0;y<h*scale;++y) for(uint x=0;x<w*scale;++x)
    *static_cast<uint16 *>(canvas.getBasePtr(left+x,top+y))=colors[data[4+(y/scale)*w+x/scale]];
   uint white=format.RGBToColor(220,230,240);
   font->drawString(&canvas,"Saturn preview - gameplay not available yet",0,16,640,white,Graphics::kTextAlignCenter);
   font->drawString(&canvas,Common::String::format("Background layer %u / %u",index+1,pictures.size()),0,430,640,white,Graphics::kTextAlignCenter);
   font->drawString(&canvas,"D-pad / Cross / Circle: browse   Square: intro   Triangle: quit",0,450,640,white,Graphics::kTextAlignCenter);
   _system->copyRectToScreen(canvas.getPixels(),canvas.pitch,0,0,640,480); _system->updateScreen(); redraw=false;
   debug("[pice] Saturn picture resource=%u size=%ux%u preview=%u/%u",pictures[index],w,h,index+1,pictures.size());
  }
  Common::Event e;
  while(_eventMan->pollEvent(e)) {
   if(e.type==Common::EVENT_QUIT) quitGame();
   bool key=e.type==Common::EVENT_KEYDOWN,joy=e.type==Common::EVENT_JOYBUTTON_DOWN;
   if(e.type==Common::EVENT_LBUTTONDOWN || (key && e.kbd.keycode==Common::KEYCODE_RIGHT) ||
      (joy && e.joystick.button==Common::JOYSTICK_BUTTON_DPAD_RIGHT)) { index=(index+1)%pictures.size(); redraw=true; }
   if(e.type==Common::EVENT_RBUTTONDOWN || (key && e.kbd.keycode==Common::KEYCODE_LEFT) ||
      (joy && e.joystick.button==Common::JOYSTICK_BUTTON_DPAD_LEFT)) { index=(index+pictures.size()-1)%pictures.size(); redraw=true; }
   if(key) {
    if(e.kbd.keycode==Common::KEYCODE_ESCAPE) quitGame();
    if(e.kbd.keycode==Common::KEYCODE_RETURN || e.kbd.keycode==Common::KEYCODE_PERIOD) {
     if(!playSaturnMovie("VINTRO.CPK",false) && !shouldQuit()) {
      canvas.free(); return Common::Error(Common::kReadingFailed,"Cannot replay Saturn intro");
     }
     redraw=true;
    }
   }
  }
  _system->delayMillis(10);
 }
 canvas.free(); return Common::kNoError;
}
}
