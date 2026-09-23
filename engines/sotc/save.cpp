// SPDX-License-Identifier: GPL-3.0-or-later
#include "sotc/save.h"
namespace Sotc {
static const unsigned kMaxSave=400000;
static Common::String filename(const Common::String &prefix,unsigned slot) { return prefix+Common::String::format(".quick%u",slot); }
static unsigned readSlot(Common::SaveFileManager &files,const Common::String &prefix,unsigned slot,Common::Array<byte> &data) {
 Common::InSaveFile *in=files.openForLoading(filename(prefix,slot));
 if(!in) return 0;
 const int64 size=in->size();
 bool ok=size>=16 && size<=kMaxSave;
 if(ok) { data.resize(size); ok=in->read(data.data(),data.size())==data.size() && !in->err(); }
 delete in;
 if(!ok) return 0;
 Snapshot s(data.data(),data.size(),true);
 s.identity(0x43544f53); s.identity(1);
 unsigned generation=0,hash=0; s.value(generation); s.value(hash);
 return s.ok() && hash==Snapshot::hash(data.data()+16,data.size()-16) ? generation : 0;
}
static unsigned latest(Common::SaveFileManager &files,const Common::String &prefix,Common::Array<byte> &data,unsigned &slot) {
 Common::Array<byte> other;
 unsigned a=readSlot(files,prefix,0,data),b=readSlot(files,prefix,1,other);
 slot=b>a ? 1 : 0; if(b>a) data=other; return b>a ? b : a;
}
bool hasQuickSave(Common::SaveFileManager &files,const Common::String &prefix) {
 Common::Array<byte> data; unsigned slot; return latest(files,prefix,data,slot)!=0;
}
bool writeQuickSave(Common::SaveFileManager &files,const Common::String &prefix,ScriptMachine &machine,StartupScene &scene,SaveState &state) {
 Common::Array<byte> data; unsigned slot;
 unsigned generation=latest(files,prefix,data,slot);
 if(generation==0xffffffffU) return false;
 ++generation; data.resize(kMaxSave);
 Snapshot s(data.data(),data.size(),false);
 s.identity(0x43544f53); s.identity(1); s.value(generation); unsigned checksum=0; s.value(checksum);
 state.sync(s);
 if(!machine.snapshot(s) || !scene.snapshot(s) || !s.ok()) return false;
 data.resize(s.size()); checksum=Snapshot::hash(data.data()+16,data.size()-16);
 for(unsigned i=0;i<4;++i) data[12+i]=byte(checksum>>(i*8));
 // Alternate generations: interrupted writes never overwrite the latest valid save.
 Common::OutSaveFile *out=files.openForSaving(filename(prefix,1-slot));
 if(!out) return false;
 bool ok=out->write(data.data(),data.size())==data.size(); out->finalize(); ok=ok && !out->err(); delete out;
 Common::Array<byte> verify;
 return ok && readSlot(files,prefix,1-slot,verify)==generation;
}
bool readQuickSave(Common::SaveFileManager &files,const Common::String &prefix,ScriptMachine &machine,StartupScene &scene,SaveState &state,Common::Array<byte> &pixels,Common::String &status) {
 Common::Array<byte> data; unsigned slot;
 if(!latest(files,prefix,data,slot)) return false;
 // Decode and reload every external resource into candidates before changing live state.
 ScriptMachine candidate; StartupScene candidateScene;
 Common::Array<byte> candidatePixels; Common::String candidateStatus;
 if(!candidateScene.load(candidatePixels,candidateStatus,candidate)) return false;
 Snapshot s(data.data()+16,data.size()-16,true); SaveState restored; restored.sync(s);
 if(!candidate.snapshot(s) || !candidateScene.snapshot(s) || !s.ok() || s.size()!=data.size()-16 ||
    !candidateScene.render(candidatePixels,candidateStatus,candidate)) return false;
 if(!machine.count() && !scene.load(pixels,status,machine)) return false;
 Snapshot commit(data.data()+16,data.size()-16,true); restored.sync(commit);
 if(!machine.snapshot(commit) || !scene.snapshot(commit,false)) return false;
 scene.adoptResources(candidateScene); state=restored; pixels=candidatePixels; status=candidateStatus;
 return true;
}
}
