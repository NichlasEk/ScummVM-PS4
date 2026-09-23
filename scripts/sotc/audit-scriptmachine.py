#!/usr/bin/env python3
"""Exercise the shared SOTC scheduler with fixtures and the supplied INIT.SAV."""
import argparse
import ctypes as c
import json
from pathlib import Path
import struct
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('directory', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='sotc-vm-') as temp:
    temp = Path(temp)
    (temp / 'test.cpp').write_text('''
#include "engines/sotc/scriptmachine.h"
static Sotc::ScriptMachine vm;
struct TestSound : Sotc::ScriptSound {
 unsigned last = 0, calls = 0; bool accept = true;
 bool effect(unsigned i) override { last = i; ++calls; return accept; }
 bool music(unsigned i) override { last = i; ++calls; return accept; }
} sound;
struct TestAnimation : Sotc::ScriptAnimation {
 bool accept=false; unsigned calls=0;
 bool moving=false;
 bool move(unsigned actor,unsigned x,unsigned y,bool &pending) override {if (!accept || actor!=0 || x!=104 || y!=171) return false; pending=moving=true; return true;}
 bool movementPending(unsigned actor,bool &pending) override {if (!accept || actor!=0) return false; pending=moving; return true;}
 unsigned lock=0;
 bool control(unsigned value,bool unlock,Sotc::ScriptActor &) override {if (!accept) return false; lock=unlock ? 0 : value ? lock|(1u<<value) : 15; return true;}
 bool hide(unsigned) override {return accept;}
 bool reset(unsigned,Sotc::ScriptActor &actor) override {if (!accept) return false; actor={0,0,4,2,0,1}; return true;}
 bool resource(unsigned,unsigned index) override {return accept && index==13;}
 bool sequence(unsigned,unsigned seq,Sotc::ScriptActor &actor) override {
  ++calls; if (!accept || seq!=3) return false;
  actor.step=0; actor.steps=2; return true;
 }
} animation;
extern "C" void finishMove() { animation.moving=false; }
extern "C" unsigned controlLock() {return animation.lock;}
extern "C" void animationHost(bool enabled,bool accept) {
 animation.calls=0; animation.accept=accept; vm.setAnimation(enabled ? &animation : nullptr);
}
extern "C" void soundHost(bool enabled,bool accept) {
 sound.calls=0; sound.accept=accept; vm.setSound(enabled ? &sound : nullptr);
}
extern "C" unsigned soundLast() {return sound.last;}
extern "C" unsigned soundCalls() {return sound.calls;}
extern "C" bool load(const uint8_t *s,unsigned n,const uint8_t *b,unsigned m) {
 Sotc::StartState state;
 return state.load(Sotc::Cc4View(s,n)) && vm.load(state,Sotc::Cc4View(b,m));
}
extern "C" int visit(unsigned i) {return vm.visit(i);}
extern "C" void seed(unsigned s) {vm.seedRandom(s);}
extern "C" unsigned rng() {return vm.randomState();}
extern "C" unsigned randomValue() {unsigned v; return vm.randomValue(v) ? v : 65536;}
extern "C" void event(bool p) {vm.setEventPending(p);}
extern "C" bool pending() {return vm.eventPending();}
extern "C" unsigned rect(unsigned j) {return vm.task(1)->rectangle[j];}
extern "C" void tick() {vm.tick();}
extern "C" bool actor(unsigned i,int x,int y,unsigned step,unsigned count) {
 Sotc::ScriptActor a={x,y,4,2,step,count}; return vm.setActor(i,a);
}
extern "C" bool info(unsigned i,unsigned *out) {
 const Sotc::ScriptTask *t=vm.task(i); if(!t) return false;
 out[0]=t->pc; out[1]=t->flags; out[2]=t->timer; out[3]=t->actor;
 out[4]=t->opcode; out[5]=t->result; out[6]=t->executed; return true;
}
''')
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
                    '-I', str(root), str(temp / 'test.cpp'), '-o', str(temp / 'test.so')], check=True)
    lib = c.CDLL(str(temp / 'test.so'))
    lib.load.restype = lib.actor.restype = lib.info.restype = c.c_bool
    lib.load.argtypes = [c.c_char_p, c.c_uint, c.c_char_p, c.c_uint]
    lib.rng.restype = c.c_uint
    def info(i=1):
        out = (c.c_uint * 7)()
        assert lib.info(i, out)
        return list(out)
    def fixture(script, flags=0, rectangle=(0, 0, 0, 0), extra=None):
        scripts = [b'\x02', bytes(script)] + ([] if extra is None else [bytes(extra)])
        offsets, pos = [], len(scripts) * 2
        for data in scripts:
            offsets.append(pos)
            pos += len(data)
        block = struct.pack('<' + 'H' * len(scripts), *offsets) + b''.join(scripts)
        state = bytearray(3902)
        def word(address, value):
            struct.pack_into('<H', state, address - 0x2dee, value)
        word(0x2e02, len(scripts))
        for i in range(len(scripts)):
            state[0x3119 - 0x2dee + i * 22] = flags if i == 1 else 32
        for i, v in enumerate(rectangle):
            word(0x3110 + 22 + 14 + i * 2, v)
        save = b'NED\0' + bytes(60) + bytes(v ^ ((i + 1) * 5 & 255) for i, v in enumerate(state))
        assert lib.load(save, len(save), block, len(block))
        return save, block
    fixture([9, 2, 21, 3, 2])
    assert lib.visit(1) == 2 and info()[:3] == [2, 2, 2]
    assert lib.visit(1) == 2 and info()[2] == 1
    assert lib.visit(1) == 1 and info()[0] == 4 and info()[3] == 3
    fixture([40, 55, 154, 63, 199, 2])
    assert lib.actor(0, 220, 151, 0, 1)
    assert lib.visit(1) == 4 and info()[0] == 0
    assert lib.actor(0, 106, 154, 0, 1)  # Footprint edge touches region edge.
    assert lib.visit(1) == 1 and info()[0] == 5
    fixture([39, 2], rectangle=(110, 154, 126, 199))
    lib.actor(0, 110, 154, 0, 1)
    assert lib.visit(1) == 4
    lib.actor(0, 200, 154, 0, 1)
    assert lib.visit(1) == 1
    for compare, value, expected_actor in [(0, 0, 0), (0, 1, 5), (2, 65535, 0), (4, 65535, 5), (6, 1, 5), (7, 1, 0)]:
        fixture([10, 232, 3, compare, 13, value & 255, value >> 8, 3, 0, 2, 21, 5, 2])
        assert lib.visit(1) == 1 and info()[3] == expected_actor
    fixture([3, 255, 255])
    assert lib.visit(1) == 8 and info()[0] == 0 and info()[6] == 128
    fixture([255])
    assert lib.visit(1) == 6 and info()[0] == 0 and info()[6] == 0
    fixture([21, 255])
    assert lib.visit(1) == 7 and info()[0] == 0
    fixture([2], flags=4)
    lib.actor(0, 0, 0, 0, 2)
    assert lib.visit(1) == 3
    lib.actor(0, 0, 0, 1, 2)
    assert lib.visit(1) == 1
    fixture([27, 2, 2], extra=[21, 4, 2])
    lib.tick()
    assert info(0)[6] == 0 and info(2)[3] == 4 and info(2)[5] == 1
    fixture([2], flags=8)
    assert lib.visit(1) == 5 and info()[0] == 0
    assert lib.visit(17) == 7
    assert not lib.actor(11, 0, 0, 0, 1)

    for seed in (0, 1, 0xffffffff, 123456789):
        for limit in (0, 1, 2, 255):
            fixture([79, limit, 9, 0, 3, 251, 255])
            lib.seed(c.c_uint(seed))
            expected = seed
            for _ in range(4):
                if limit >= 2:
                    expected = (expected * 0x015a4e35 + 1) & 0xffffffff
                assert lib.visit(1) == 2
                assert lib.rng() == expected
                assert lib.randomValue() == (((expected >> 16) & 32767) % limit if limit >= 2 else 0)
    # Variable 33 is shared across tasks and can drive comparisons.
    fixture([79, 1, 27, 2, 2], extra=[10, 33, 0, 0, 12, 0, 3, 0, 2, 255])
    lib.tick()
    assert info(2)[5] == 1
    for pending, x, expected_pc, remains in ((False, 110, 10, False), (True, 200, 10, True), (True, 110, 7, False)):
        fixture([66, 55, 154, 63, 199, 5, 0, 2, 0, 0, 2], rectangle=(1, 2, 3, 4))
        lib.actor(0, x, 154, 0, 1)
        lib.event(pending)
        assert lib.visit(1) == 1 and info()[0] == expected_pc
        assert bool(lib.pending()) == remains
        assert [lib.rect(j) for j in range(4)] == ([110, 154, 126, 199] if pending else [1, 2, 3, 4])
    for pending in (False, True):
        fixture([64, 3, 0, 2, 2])
        lib.event(pending)
        assert lib.visit(1) == 1 and info()[0] == (3 if pending else 4)
        assert not lib.pending()
    for script in ([66, 1, 2], [66, 1, 2, 3, 4, 255, 127], [79]):
        fixture(script)
        assert lib.visit(1) == 7 and info()[0] == 0

    for opcode in (77, 99):
        for sample in (0, 6, 255):
            fixture([opcode, sample, 2])
            lib.soundHost(False, False)
            assert lib.visit(1) == 10 and info()[0] == 0
            lib.soundHost(True, False)
            assert lib.visit(1) == 10 and info()[0] == 0 and lib.soundCalls() == 1
            lib.soundHost(True, True)
            assert lib.visit(1) == 1 and info()[0] == 2
            assert lib.soundCalls() == 1 and lib.soundLast() == sample
    lib.soundHost(False, False)
    fixture([36,52,171,2])
    lib.animationHost(False,False)
    assert lib.visit(1)==11 and info()[0]==0
    lib.animationHost(True,True)
    assert lib.visit(1)==4 and info()[0]==3 and info()[1]&1
    assert lib.visit(1)==4 and info()[0]==3
    lib.finishMove()
    assert lib.visit(1)==1 and not info()[1]&1
    fixture([36,52])
    assert lib.visit(1)==7 and info()[0]==0
    fixture([18,0,19,2])
    lib.animationHost(False,False)
    assert lib.visit(1)==11 and info()[0]==0
    lib.animationHost(True,False)
    assert lib.visit(1)==11 and info()[0]==0
    lib.animationHost(True,True)
    assert lib.visit(1)==1 and info()[0]==3 and lib.controlLock()==0
    fixture([18,0,2])
    assert lib.visit(1)==1 and lib.controlLock()==15
    for script in ([18],[18,5,2],[18,255,2]):
        fixture(script)
        assert lib.visit(1)==7 and info()[0]==0
    for opcode, argument in ((90,5),(91,13)):
        fixture([opcode,argument,2])
        lib.animationHost(False,False)
        assert lib.visit(1)==11 and info()[0]==0
        lib.animationHost(True,False)
        assert lib.visit(1)==11 and info()[0]==0
        lib.animationHost(True,True)
        assert lib.visit(1)==1 and info()[0]==2
    fixture([90,0,7,255,199,2])
    lib.animationHost(False,False)
    assert lib.visit(1)==1 and info()[0]==5 and info()[3]==0
    for script in ([90,11,2],[90],[91],[7,1]):
        fixture(script)
        assert lib.visit(1)==7 and info()[0]==0
    for opcode in (87,88):
        fixture([opcode,3,2])
        lib.animationHost(False,False)
        assert lib.visit(1)==11 and info()[0]==0
        lib.animationHost(True,False)
        assert lib.visit(1)==11 and info()[0]==0
        lib.animationHost(True,True)
        if opcode==87:
            assert lib.visit(1)==3 and info()[0:2]==[2,4]
            assert lib.visit(1)==3
            lib.actor(0,0,0,1,2)
            assert lib.visit(1)==1
        else:
            assert lib.visit(1)==1 and info()[0]==2
    lib.animationHost(False,False)
    fixture([41,2])
    assert lib.visit(1)==1  # Original hide ignores the player, even without a scene host.
    fixture([21,1,41,2])
    assert lib.visit(1)==11 and info()[0]==2
    lib.animationHost(True,True)
    assert lib.visit(1)==1 and info()[0]==3
    lib.animationHost(False,False)
    save = (args.directory / 'INIT.SAV').read_bytes()
    state = bytes(v ^ ((i + 1) * 5 & 255) for i, v in enumerate(save[64:]))
    bank, room = (struct.unpack_from('<H', state, a - 0x2dee)[0] for a in (0x2df2, 0x2df8))
    cc4 = (args.directory / f'R{bank:02}.CC4').read_bytes()
    start, end = struct.unpack_from('<II', cc4, room * 4)
    block = cc4[start:end]
    assert lib.load(save, len(save), block, len(block))
    lib.tick()
    result = [dict(zip(('pc', 'flags', 'timer', 'actor', 'opcode', 'result', 'executed'), info(i)), task=i) for i in range(15)]
    assert result[1]['result'] == 4 and result[1]['pc'] == 14
    assert [(r['task'], r['pc'], r['opcode']) for r in result if r['result'] >= 5] == [(12, 2, 99), (14, 46, 77)]
    assert result[14]['result'] == 10  # Missing audio host cannot silently skip effects.
    assert sum(2 <= r['result'] <= 4 for r in result) == 7
    assert sum(r['result'] == 1 for r in result) == 6
    lib.soundHost(True, True)
    assert lib.visit(14) == 2 and info(14)[0:3] == [2, 2, 5]
    assert lib.soundLast() == 6 and lib.soundCalls() == 1
    assert lib.visit(12) == 2 and info(12)[:3] == [2, 2, 1]
    assert lib.soundLast() == 0
    print(json.dumps(result, indent=2))
