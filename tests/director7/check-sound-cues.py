#!/usr/bin/env python3
"""Exercise actual cue dispatch against controlled mixer/movie adapters."""
import pathlib
import subprocess
import sys
import tempfile
s=(pathlib.Path(sys.argv[1])/'engines/director/sound.cpp').read_text()
a=s.index('void DirectorSound::processCuePoints() {')
b=s.index('\n\nSNDDecoder::SNDDecoder()',a)
function=s[a:b]
cpp=r'''
#include <cassert>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
using uint=unsigned; using uint32=uint32_t; using int32=int32_t;
namespace Common {template<class T> using Array=std::vector<T>; using String=std::string; using StringArray=Array<String>;}
#define warning(...) ((void)0)
bool isPS4Jonsson() {return true;}
const int kSoundCast=1,kCastSound=2,kEventCuePassed=3;
struct CastMemberID {CastMemberID(int,int){}};
struct CastMember {int _type=kCastSound;};
struct SoundCastMember:CastMember {Common::Array<int32> _cuePoints; Common::StringArray _cuePointNames;};
struct SoundID {int type=kSoundCast; struct {struct {int member=0,castLib=0;}cast;}u;};
struct SoundChannel {int handle=1,lastCuePointIndex=-1; SoundID lastPlayedSound; Common::Array<int32> fileCuePoints; Common::StringArray fileCueNames;};
struct Mixer {uint32 elapsed=0; bool active=true; uint32 getSoundElapsedTime(int){return elapsed;} bool isSoundHandleActive(int){return active;}};
struct Movie {
 SoundCastMember *cast=nullptr; Common::StringArray seen; std::vector<int> numbers;
 std::function<void()> callback;
 CastMember *getCastMember(CastMemberID){return cast;}
 void processSoundCue(int channel,int number,const Common::String &name) {
  assert(channel==5);seen.push_back(name);numbers.push_back(number);if(callback)callback();
 }
 void processEvent(int,uint){assert(false);}
};
struct Window {Movie movie; Movie *getCurrentMovie(){return &movie;}};
struct Entry {int _key; SoundChannel *_value;};
struct DirectorSound {std::vector<Entry> _channels; Window *_window; Mixer *_mixer; void processCuePoints();};
''' + function + r'''
int main() {
 Window window; Mixer mixer; SoundChannel channel; DirectorSound engine;
 engine._window=&window;engine._mixer=&mixer;engine._channels.push_back({5,&channel});
 channel.fileCuePoints={100,500};channel.fileCueNames={"first","slut"};
 // playFile has the default SoundID (cast member zero), but still owns cues.
 mixer.elapsed=99;engine.processCuePoints();assert(window.movie.seen.empty());
 mixer.elapsed=100;engine.processCuePoints();assert(window.movie.seen.size()==1 && channel.lastCuePointIndex==0);
 engine.processCuePoints();assert(window.movie.seen.size()==1);
 // Natural completion removes the handle before the next score poll.
 mixer.active=false;mixer.elapsed=0;engine.processCuePoints();
 assert(window.movie.seen.size()==2 && window.movie.seen.back()=="slut" && window.movie.numbers.back()==2);
 engine.processCuePoints();assert(window.movie.seen.size()==2);
 // Explicit stop clears pending file cues and must not fabricate callbacks.
 channel.fileCuePoints.clear();channel.fileCueNames.clear();channel.lastCuePointIndex=-1;
 engine.processCuePoints();assert(window.movie.seen.size()==2);
 // Cast-backed cues retain the same route. A callback may clear its source.
 SoundCastMember cast;cast._cuePoints={1,2};cast._cuePointNames={"castFirst","castLast"};
 window.movie.cast=&cast;channel.lastPlayedSound.u.cast.member=42;
 mixer.active=true;mixer.elapsed=2;
 window.movie.callback=[&](){assert(channel.lastCuePointIndex==0);cast._cuePoints.clear();cast._cuePointNames.clear();};
 engine.processCuePoints();assert(window.movie.seen.size()==3 && window.movie.seen.back()=="castFirst");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=pathlib.Path(tmp);(p/'test.cpp').write_text(cpp)
 subprocess.run(['clang++','-std=c++11','-O2',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Sound cues: file/cast delivery, timing, terminal cue, stop, once-only and callback mutation passed')
