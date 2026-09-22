// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/pice.h"
#include "video/smk_decoder.h"
#include "common/config-manager.h"
#include "common/events.h"
#include "common/fs.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "graphics/paletteman.h"
namespace Pice {
#ifdef PICE_TESTBENCH
bool PiceEngine::auditWindowsAssets() {
 if(!_windows) return false;
 for(uint i=0;i<45;++i) {
  Common::Array<byte> data;
  if(!_soundArchive.read(i,data)) return false;
  Common::ScopedPtr<MidiParser> parser(MidiParser::createParser_SMF());
  if(!parser->loadMusic(data.data(),data.size())) return false;
 }
 Common::FSList files;
 if(!Common::FSNode(ConfMan.getPath("path")).getChild("VIDEO").getChildren(files,Common::FSNode::kListFilesOnly)) return false;
 uint count=0;
 for(uint i=0;i<files.size();++i) {
  Common::String name=files[i].getName(); name.toUppercase();
  if(!name.hasSuffix(".SMK")) continue;
  Video::SmackerDecoder decoder;
  if(!decoder.loadFile(Common::Path("VIDEO").join(files[i].getName())) || decoder.getFrameCount()<=0) return false;
  uint frames=decoder.getFrameCount();
  for(uint f=0;f<frames;++f) {
   if(!decoder.decodeNextFrame() || decoder.getCurFrame()!=int(f)) return false;
  }
  debug("[pice] Windows movie audit name=%s frames=%u PASS",name.c_str(),frames); ++count;
 }
 debug("[pice] Windows asset audit MIDI=45 movies=%u PASS",count);
 return count>0;
}
#endif
bool PiceEngine::playWindowsMovie(const Common::String &name,int16 before,int16 after) {
 Common::String movie=name; movie.toUppercase();
 if(movie.hasPrefix("VFINAL")) movie=Common::String::format("S%c%s",_languagePrefix,movie.c_str()+1);
 else if(movie.hasPrefix("V")) movie.setChar('S',0);
 if(movie.hasSuffix(".MUX")) { movie.erase(movie.size()-4); movie+=".SMK"; }
 Video::SmackerDecoder decoder;
 decoder.setSoundType(Audio::Mixer::kSFXSoundType);
 if(!decoder.loadFile(Common::Path("VIDEO").join(movie)) || decoder.getPixelFormat().bytesPerPixel!=1 ||
    !decoder.getWidth() || !decoder.getHeight()) {
  warning("[pice] Cannot load Windows movie %s",movie.c_str()); return false;
 }
 byte previous[768]; _system->getPaletteManager()->grabPalette(previous,0,256);
 Common::Array<Audio::SoundHandle> paused;
 auto pause=[&](Audio::SoundHandle h) { if(_mixer->isSoundHandleActive(h)) { _mixer->pauseHandle(h,true); paused.push_back(h); } };
 pause(_music.handle); pause(_speechHandle);
 for(uint i=0;i<_sounds.size();++i) if(_sounds[i].active) pause(_sounds[i].handle);
 bool midiPlaying=_midi.isPlaying(); if(midiPlaying) _midi.pause();
 _movieSkipped=false;
 if(before) _system->fillScreen(0);
 Common::Array<byte> screen; screen.resize(640*400,0);
 byte palette[768]={}; uint32 start=_system->getMillis(); uint frames=0;
 decoder.start();
 while(!shouldQuit() && !_movieSkipped && !decoder.endOfVideo()) {
  Common::Event event;
  while(_eventMan->pollEvent(event)) {
   if(event.type==Common::EVENT_QUIT) quitGame();
   if(_frontendMovies && (event.type==Common::EVENT_LBUTTONDOWN || (event.type==Common::EVENT_KEYDOWN &&
      (event.kbd.keycode==Common::KEYCODE_ESCAPE || event.kbd.keycode==Common::KEYCODE_SPACE)))) _movieSkipped=true;
  }
#ifdef PICE_TESTBENCH
  if(_frontendMovies && ConfMan.hasKey("pice_test_movie_skip_ms") &&
     _system->getMillis()-start>=uint32(ConfMan.getInt("pice_test_movie_skip_ms"))) _movieSkipped=true;
#endif
  if(shouldQuit() || _movieSkipped) break;
  if(decoder.needsUpdate()) {
   const Graphics::Surface *frame=decoder.decodeNextFrame();
   if(decoder.hasDirtyPalette()) { memcpy(palette,decoder.getPalette(),768); _system->getPaletteManager()->setPalette(palette,0,256); }
   if(frame) {
    ++frames; uint w=640,h=400;
    if(frame->w*h>frame->h*w) h=w*frame->h/frame->w; else w=h*frame->w/frame->h;
    uint black=0,brightness=766;
    for(uint i=0;i<256;++i) { uint sum=palette[i*3]+palette[i*3+1]+palette[i*3+2]; if(sum<brightness) { brightness=sum; black=i; } }
    for(uint i=0;i<screen.size();++i) screen[i]=black;
    for(uint y=0;y<h;++y) for(uint x=0;x<w;++x)
     screen[(y+(400-h)/2)*640+x+(640-w)/2]=*static_cast<const byte *>(frame->getBasePtr(x*frame->w/w,y*frame->h/h));
    _system->copyRectToScreen(screen.data(),640,0,0,640,400); presentFrame();
   }
  } else _system->delayMillis(5);
 }
 bool complete=frames>0 && decoder.getCurFrame()+1>=decoder.getFrameCount();
 decoder.close();
 for(uint i=0;i<paused.size();++i) _mixer->pauseHandle(paused[i],false);
 if(midiPlaying) { _midi.resume(); syncSoundSettings(); }
 if(after) _system->fillScreen(0);
 _system->getPaletteManager()->setPalette(previous,0,256);
 _roomClock=_system->getMillis(); _roomRenderClock=_roomClock;
#ifdef PICE_TESTBENCH
 _testMovieDone=complete && !shouldQuit();
#endif
 debug("[pice] Windows movie completed name=%s frames=%u complete=%d skipped=%d",movie.c_str(),frames,complete,_movieSkipped);
 return !shouldQuit() && (complete || _movieSkipped);
}
}
