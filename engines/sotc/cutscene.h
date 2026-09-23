// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_CUTSCENE_H
#define SOTC_CUTSCENE_H
#include "sotc/pak.h"
#include "sotc/poserenderer.h"
#include "sotc/animation.h"
#include "sotc/scriptmachine.h"
namespace Sotc {
// Opcode 30's blocking VA2 playback (DOS 0xc410), using external Axx/Dxx.
class Cutscene {
 Common::Array<byte> _data,_base,_cues;
 unsigned _bank=0,_resource=0,_sequence=0,_backdrop=0,_repeat=0;
 unsigned _step=0,_substep=0,_loop=0,_cue=0;
 bool _active=false,_fresh=true;
 bool load() {
  Pak pak;if(!pak.open(Common::String::format("A%02u.PAK",_bank)) || !pak.load(_resource,_data))return false;
  return va2SequenceLength(_data.data(),_data.size(),_sequence)>0;
 }
public:
 bool active() const {return _active;}
 void skip(){_active=false;}
 bool start(unsigned bank,unsigned resource,unsigned sequence,unsigned backdrop,unsigned repeat,Cc4View cues,const Common::Array<byte> &scene,const Common::Array<byte> &scenery) {
  Cutscene next;next._bank=bank;next._resource=resource;next._sequence=sequence;next._backdrop=backdrop;next._repeat=repeat;
  if(cues.size>765 || cues.size%3 || !next.load())return false;
  next._cues.resize(cues.size);for(unsigned i=0;i<cues.size;++i)next._cues[i]=cues.data[i];
  next._base.resize(64000);memset(next._base.data(),0,64000);
  if(backdrop==32000){if(scene.size()!=64000)return false;next._base=scene;}
  else if(backdrop>=32768){PoseRenderer r(next._base);if(!r.pose(next._data,65536-backdrop,0,0))return false;}
  else if(backdrop){PoseRenderer r(next._base);if(!r.pose(scenery,backdrop,0,0))return false;}
  // Validate the entire selected animation before starting a modal screen.
  unsigned count=va2SequenceLength(next._data.data(),next._data.size(),sequence);
  for(unsigned i=0;i<count;++i){SequenceStep step;if(!va2Step(next._data.data(),next._data.size(),sequence,i,step) || (step.control>>14)>2)return false;}
  next._active=repeat!=0;*this=next;return true;
 }
 bool render(Common::Array<byte> &pixels) const {
  if(!_active)return false;
  pixels=_base;SequenceStep step;int x=0,y=0;
  if(!va2Step(_data.data(),_data.size(),_sequence,_step,step) || !va2Position(_data.data(),_data.size(),_sequence,_step,x,y))return false;
  unsigned count=va2SequenceLength(_data.data(),_data.size(),_sequence),fraction=0;int nextPose=-1;
  if((step.control>>14)==1){SequenceStep next=step;if(_step+1<count && !va2Step(_data.data(),_data.size(),_sequence,_step+1,next))return false;unsigned d=step.control&0x3fff;fraction=(_substep<<8)/(d ? d : 1);nextPose=next.pose;}
  PoseRenderer renderer(pixels);if((step.control>>14)==2)return renderer.scaledPose(_data,step.pose,x,y,step.control&0x3fff);return renderer.pose(_data,step.pose,x,y,0,0,319,199,-1,nextPose,fraction);
 }
 bool advance(ScriptMachine &machine) {
  if(!_active)return true;
  if(_fresh && _cue<_cues.size()/3 && _step==_cues[_cue*3+1]){
   if(!machine.playEffect(_cues[0]))return false;
   ++_cue;
  }
  _fresh=false;SequenceStep step;unsigned count=va2SequenceLength(_data.data(),_data.size(),_sequence);
  if(!va2Step(_data.data(),_data.size(),_sequence,_step,step))return false;
  unsigned previous=_step;bool wrapped=false;
  // The blocking loop uses the renderer's returned substep directly,
  // unlike the actor scheduler's additional frame-update test.
  if((step.control>>14)==1){unsigned duration=step.control&0x3fff;_substep=(_substep+1)&65535;if(_substep>=duration)_substep=0;}else _substep=0;
  if(!_substep){wrapped=_step+1==count;_step=(_step+1)%count;}
  if(wrapped){++_loop;_cue=0;_fresh=true;if(_loop>=_repeat)_active=false;}
  else if(previous!=_step)_fresh=true;
  return true;
 }
 bool snapshot(Snapshot &s) {
  s.value(_active);if(!_active)return s.ok();
  s.value(_bank,9);s.value(_resource,255);s.value(_sequence,255);s.value(_backdrop,65535);s.value(_repeat,255);
  s.value(_step,255);s.value(_substep,65535);s.value(_loop,255);s.value(_cue,255);s.value(_fresh);
  unsigned size=_cues.size();s.value(size,765);s.require(size%3==0);if(s.reading())_cues.resize(size);
  for(unsigned i=0;s.ok() && i<size;++i){unsigned v=_cues[i];s.value(v,255);_cues[i]=v;}
  if(s.reading())_base.resize(64000);
  for(unsigned i=0;s.ok() && i<64000;++i){unsigned v=_base[i];s.value(v,255);_base[i]=v;}
  s.require(_loop<_repeat && _cue<=size/3);
  if(!s.ok() || (s.reading() && !load()))return false;
  return _step<va2SequenceLength(_data.data(),_data.size(),_sequence);
 }
};
}
#endif
