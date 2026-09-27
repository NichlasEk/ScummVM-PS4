#!/usr/bin/env python3
"""Run production Watchmaker sound control against an observable mixer fixture."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
src=root/'.tools/scummvm-vulkan-src/engines/watchmaker'
body=(src/'ll/ll_sound.cpp').read_text();body=body[body.index('namespace Watchmaker {'):]
header=(src/'ll/ll_sound.h').read_text();header=header[header.index('namespace Watchmaker {'):header.rindex('#endif')]
fixture=r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <algorithm>
#include <map>
using byte=unsigned char;using uint=unsigned;using uint32=unsigned;using int32=int;using int8=signed char;
template<class T>T MIN(T a,T b){return std::min(a,b);}template<class T>T MAX(T a,T b){return std::max(a,b);}
template<class T>T CLIP(T x,T a,T b){return std::max(a,std::min(b,x));}
#define USE_MAD
#define warning(...) ((void)0)
#define debug(...) ((void)0)
namespace DisposeAfterUse{enum Flag{NO,YES};}
namespace Common{
struct String:std::string{
 using std::string::string;String(const std::string&s):std::string(s){}
 void setChar(char c,uint i){at(i)=c;}bool hasPrefix(const char*p)const{return find(p)==0;}
 bool hasSuffixIgnoreCase(const char*p)const{std::string s=*this;for(auto &c:s)c=tolower(c);return s.size()>=std::string(p).size() && s.compare(s.size()-std::string(p).size(),std::string(p).size(),p)==0;}
 uint findLastOf(char c)const{return rfind(c)==npos ? (uint)npos : (uint)rfind(c);}
};
struct Path{String name;Path(const String&s):name(s){}};
struct SeekableReadStream{virtual ~SeekableReadStream(){}};
}
struct Search{
 std::string last;
 bool hasFile(Common::Path p){last=p.name;return p.name!="missing.wav";}
 Common::SeekableReadStream *createReadStreamForMember(Common::Path p){return hasFile(p)?new Common::SeekableReadStream:nullptr;}
}SearchMan;
namespace Audio{
int live=0;
struct AudioStream{AudioStream(){++live;}virtual ~AudioStream(){--live;}};
struct SeekableAudioStream:AudioStream{};
SeekableAudioStream* makeWAVStream(Common::SeekableReadStream*f,DisposeAfterUse::Flag){delete f;return new SeekableAudioStream;}
SeekableAudioStream* makeMP3Stream(Common::SeekableReadStream*f,DisposeAfterUse::Flag){delete f;return new SeekableAudioStream;}
int loops=0;AudioStream* makeLoopingAudioStream(SeekableAudioStream*s,int){++loops;return s;}
struct SoundHandle{unsigned id=0;};
struct Mixer{
 enum SoundType{kMusicSoundType,kSFXSoundType,kSpeechSoundType};
 struct State{AudioStream *stream;int volume,balance;SoundType type;};
 std::map<unsigned,State> active;unsigned next=0;
 bool isReady(){return true;}bool isSoundHandleActive(SoundHandle h){return active.count(h.id);}
 void stopHandle(SoundHandle h){auto i=active.find(h.id);if(i!=active.end()){delete i->second.stream;active.erase(i);}}
 void playStream(SoundType t,SoundHandle*h,AudioStream*s,int=-1,byte v=255){h->id=++next;active[h->id]={s,v,0,t};}
 void setChannelVolume(SoundHandle h,byte v){if(active.count(h.id))active[h.id].volume=v;}
 void setChannelBalance(SoundHandle h,int v){if(active.count(h.id))active[h.id].balance=v;}
};
}
struct System{Audio::Mixer mixer;Audio::Mixer*getMixer(){return &mixer;}} systemFixture,*g_system=&systemFixture;
'''
main=r'''
int main(){using namespace Watchmaker;
 sSound s={};snprintf(s.name,sizeof(s.name),"./Wav/test.wav");s.lIndex=9;
 s.flMinDistance=1;s.flMaxDistance=11;s.v3flPosition.x=6;
 sListener l={};l.v3flFrontOrientation.z=1;l.v3flTopOrientation.y=1;sSetListener(&l);
 assert(sStartSound(&s,false));assert(sIsPlaying(9));assert(Audio::live==1);
 auto &mix=systemFixture.mixer;assert(mix.active.begin()->second.volume==127);assert(mix.active.begin()->second.balance==127);
 assert(sStartSound(&s,false));assert(Audio::live==1); // same id restarts, old stream freed
 sStopSound(9);assert(!sIsPlaying(9)&&Audio::live==0);
 s.dwLooped=1;sStartSound(&s,false);auto next=mix.next;sStartSound(&s,false);assert(mix.next==next&&Audio::loops==1);
 sStopAllSounds();s.dwLooped=0;s.dwFlags=SOUND_SPEECH;sSetAllSpeechVolume(50);
 sStartSoundDiffuse(&s);assert(mix.active.begin()->second.type==Audio::Mixer::kSpeechSoundType);
 assert(mix.active.begin()->second.volume==127&&mix.active.begin()->second.balance==0);
 mix.stopHandle(channels[0].handle);assert(!sIsPlaying(9)); // natural decoder completion
 snprintf(s.name,sizeof(s.name),"missing.wav");assert(!sStartSound(&s,false));assert(!sIsPlaying(9));
 assert(mPlayMusic(".\\Midi\\intro.mid"));assert(SearchMan.last=="Music/intro.mp3");
 assert(Audio::live==1);mCloseMusicSystem();assert(Audio::live==0&&mix.active.empty());
 assert(volumeFromPercent(100)==255&&volumeFromPercent(255)==255);
 puts("PASS production audio: path mapping, restart/loop, lifetime, position, volume, speech completion and cleanup");
}
'''
# Match size_type for npos in the small Common::String adapter.
fixture=fixture.replace('uint findLastOf(char c)const{return rfind(c)==npos ? (uint)npos : (uint)rfind(c);}', 'uint findLastOf(char c)const{return (uint)rfind(c);}\n static constexpr uint npos=(uint)-1;')
work=root/'build/watchmaker-data-test';work.mkdir(parents=True,exist_ok=True)
p=work/'audio.cpp';p.write_text(fixture+header+body+main)
subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p),'-o',str(work/'audio')],check=True)
subprocess.run([str(work/'audio')],check=True)
