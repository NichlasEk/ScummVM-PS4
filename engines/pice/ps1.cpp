// SPDX-License-Identifier: GPL-3.0-or-later
#include "pice/pice.h"
#include "video/psx_decoder.h"
#include "common/file.h"
#include "common/memstream.h"
#include "common/events.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "graphics/surface.h"
namespace Pice {
// STR files retain their 2352-byte Mode 2 sectors, including XA subheaders.
// Validate the supported stream layout before entering the shared MDEC decoder.
static bool inspectPS1Movie(Common::SeekableReadStream &file,uint &frames) {
 frames=0;
 if(file.size()<=0 || file.size()%2352) return false;
 int audioFormat=-1;
 byte s[2352]; uint chunk=0,chunks=0,frame=0,size=0,width=0,height=0;
 while(file.pos()<file.size()) {
  if(file.read(s,sizeof(s))!=sizeof(s) || s[0] || s[11] || s[15]!=2 || memcmp(s+16,s+20,4)) return false;
  for(uint i=1;i<11;++i) if(s[i]!=255) return false;
  uint type=s[18]&14;
  if(type==4) {
   if(s[17]!=1 || (s[19]&~5) || (audioFormat>=0 && audioFormat!=s[19])) return false;
   audioFormat=s[19]; continue;
  }
  if(type!=2 && type!=8) continue;
  if(s[17]!=1 || READ_LE_UINT32(s+24)!=0x80010160) return false;
  uint c=READ_LE_UINT16(s+28),n=READ_LE_UINT16(s+30),f=READ_LE_UINT32(s+32),bytes=READ_LE_UINT32(s+36);
  uint w=READ_LE_UINT16(s+40),h=READ_LE_UINT16(s+42),version=READ_LE_UINT16(s+50);
  if(!n || n>64 || c>=n || bytes<8 || bytes>65535 || bytes>n*2016 || !w || !h || w>640 || h>480 || (version!=2 && version!=3)) return false;
  if(!c) {
   if(chunk!=chunks || (frames && (f!=frame+1 || w!=width || h!=height))) return false;
   frame=f; chunks=n; chunk=0; size=bytes; width=w; height=h; ++frames;
  }
  if(c!=chunk || n!=chunks || f!=frame || bytes!=size || w!=width || h!=height) return false;
  ++chunk;
 }
 return frames && chunk==chunks && file.seek(0);
}
bool PiceEngine::runPS1MovieTests() {
 Common::Array<byte> bytes; bytes.resize(2352,0);
 for(uint i=1;i<11;++i) bytes[i]=255;
 bytes[15]=2; bytes[17]=bytes[21]=1; bytes[18]=bytes[22]=8;
 WRITE_LE_UINT32(bytes.data()+24,0x80010160); WRITE_LE_UINT16(bytes.data()+30,1);
 WRITE_LE_UINT32(bytes.data()+32,1); WRITE_LE_UINT32(bytes.data()+36,8);
 WRITE_LE_UINT16(bytes.data()+40,16); WRITE_LE_UINT16(bytes.data()+42,16); WRITE_LE_UINT16(bytes.data()+50,3);
 auto valid=[&]() { Common::MemoryReadStream stream(bytes.data(),bytes.size()); uint frames; return inspectPS1Movie(stream,frames) && frames==1; };
 if(!valid()) return false;
 bytes[28]=1; if(valid()) return false; bytes[28]=0;
 bytes[17]=bytes[21]=2; if(valid()) return false; bytes[17]=bytes[21]=1;
 bytes[50]=4; if(valid()) return false; bytes[50]=3;
 bytes.pop_back(); return !valid();
}
bool PiceEngine::playPS1Movie(const Common::String &name,bool skippable,bool audit) {
 Common::String movie=name; movie.toUppercase();
 if(movie.hasSuffix(".CPK")) movie.erase(movie.size()-4);
 if(!movie.hasSuffix(".STR")) movie+=".STR";
 Common::Path path=Common::Path("MOVIE").join(movie);
 Common::File file; uint expected=0;
 if(!file.open(path) || !inspectPS1Movie(file,expected)) {
  warning("[pice] Unsupported or malformed PS1 STR: %s",movie.c_str()); return false;
 }
 Video::PSXStreamDecoder decoder(Video::PSXStreamDecoder::kCD2x);
 decoder.setSoundType(Audio::Mixer::kSFXSoundType);
 if(!decoder.loadFile(path)) return false;
 Graphics::PixelFormat rgb(2,5,6,5,0,11,5,0,0);
 if(!decoder.setOutputPixelFormat(rgb)) return false;
 if(audit) {
  for(uint i=0;i<expected;++i) if(!decoder.decodeNextFrame()) return false;
  debug("[pice] PS1 movie audit name=%s frames=%u PASS",movie.c_str(),expected);
  return true;
 }
 _ps1Movie=&decoder; bool skipped=false; uint frames=0;
 Common::Array<uint16> canvas; canvas.resize(640*480,0);
 decoder.start();
 while(!shouldQuit() && !skipped && !decoder.endOfVideo()) {
  Common::Event event;
  while(_eventMan->pollEvent(event)) {
   if(event.type==Common::EVENT_QUIT) quitGame();
   if(skippable && (event.type==Common::EVENT_LBUTTONDOWN || (event.type==Common::EVENT_KEYDOWN &&
      (event.kbd.keycode==Common::KEYCODE_ESCAPE || event.kbd.keycode==Common::KEYCODE_SPACE)))) skipped=true;
  }
  if(skipped || shouldQuit()) break;
  if(decoder.needsUpdate()) {
   const Graphics::Surface *surface=decoder.decodeNextFrame();
   if(surface) {
    ++frames;
    uint scale=MIN<uint>(640/surface->w,400/surface->h),w=surface->w*scale,h=surface->h*scale;
    if(!scale || surface->format.bytesPerPixel!=2) break;
    for(uint y=0;y<h;++y) for(uint x=0;x<w;++x)
     canvas[((480-h)/2+y)*640+(640-w)/2+x]=*static_cast<const uint16 *>(surface->getBasePtr(x/scale,y/scale));
    _system->copyRectToScreen(canvas.data(),1280,0,0,640,480); _system->updateScreen();
   }
  } else _system->delayMillis(5);
 }
 _ps1Movie=nullptr; decoder.close();
 debug("[pice] PS1 movie name=%s frames=%u/%u skipped=%d",movie.c_str(),frames,expected,skipped);
 return !shouldQuit() && (skipped || frames==expected);
}
}
