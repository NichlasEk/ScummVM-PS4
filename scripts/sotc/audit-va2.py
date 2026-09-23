#!/usr/bin/env python3
"""Audit the engine VA2 decoder against original PAK assets, without extracting them."""
import argparse
import ctypes as c
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('directory', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('pak', Path(__file__).with_name('inspect-pak.py'))
pak = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pak)
results = []
with tempfile.TemporaryDirectory(prefix='sotc-va2-') as temp:
    temp = Path(temp)
    (temp / 'test.cpp').write_text('''#include "engines/sotc/explode.h"
#include "engines/sotc/va2.h"
#include "engines/sotc/sequence.h"
extern "C" bool unpack(const uint8_t*s,unsigned n,uint8_t*d,unsigned m,unsigned f) {return Sotc::explode(s,n,d,m,f);}
extern "C" unsigned sequences(const uint8_t*p,unsigned n) {return Sotc::va2Records(p,n,2);}
extern "C" unsigned steps(const uint8_t*p,unsigned n,unsigned s) {return Sotc::va2SequenceLength(p,n,s);}
extern "C" int pose(const uint8_t*p,unsigned n,unsigned s,unsigned i) {Sotc::SequenceStep r;return Sotc::va2Step(p,n,s,i,r) ? int(r.pose) : -1;}
extern "C" int mirror(const uint8_t*p,unsigned n,unsigned pose) {Sotc::PoseLayer r;return Sotc::va2Layer(p,n,pose,0,r) ? int(r.mirror) : -1;}
extern "C" unsigned count(const uint8_t*p,unsigned n) {return Sotc::va2FrameCount(p,n);}
extern "C" bool frame(const uint8_t*p,unsigned n,unsigned i,uint8_t*d,uint8_t*m,unsigned cap,unsigned*w,unsigned*h) {return Sotc::va2Frame(p,n,i,d,m,cap,*w,*h);}
''')
    subprocess.run(['c++', '-shared', '-fPIC', '-I', str(root), str(temp / 'test.cpp'), '-o', str(temp / 'test.so')], check=True)
    lib = c.CDLL(str(temp / 'test.so'))
    lib.unpack.restype = lib.frame.restype = c.c_bool
    lib.count.restype = c.c_uint
    pixels = c.create_string_buffer(640 * 256)
    mask = c.create_string_buffer(len(pixels))
    width, height = c.c_uint(), c.c_uint()

    def decode(data, index):
        return lib.frame(data, len(data), index, pixels, mask, len(pixels), c.byref(width), c.byref(height))

    # Transparent skips versus opaque colour zero, plus exact input boundaries.
    frame = bytes([1, 1, 1, 2, 1, 0, 0, 1, 2, 3, 10])
    end = 28 + len(frame)
    fixture = struct.pack('<4I', 16, 22, end, end) + struct.pack('<IH', 6, 0) * 2 + frame
    assert decode(fixture, 0)
    assert (width.value, height.value) == (16, 1)
    assert mask.raw[:16] == bytes([0, 0, 1, 1, 1, 1] + [0] * 10)
    assert pixels.raw[2:6] == bytes([0, 1, 2, 3])
    blank = struct.pack('<4I', 16, 22, 30, 30) + struct.pack('<IH', 6, 0) * 2 + b'\0\0'
    assert decode(blank, 0) and width.value == height.value == 0
    assert not decode(fixture, 1)
    for stop in range(len(fixture)):
        assert not decode(fixture[:stop], 0)
    damaged = bytearray(fixture)
    damaged[32] = 255  # Copy run larger than the output row.
    assert not decode(bytes(damaged), 0)

    for path in sorted(args.directory.glob('*.PAK')):
        archive = path.read_bytes()
        for entry in pak.inspect(path)['entries']:
            if not entry['name'].endswith('.VA2'):
                continue
            data = archive[entry['payload']:entry['payload'] + entry['packed']]
            if entry['method'] == 1:
                unpacked = c.create_string_buffer(entry['unpacked'])
                assert lib.unpack(data, len(data), unpacked, len(unpacked), entry['flags']), entry
                data = unpacked.raw
            else:
                assert entry['method'] == 0, entry
            count = lib.count(data, len(data))
            supported = sum(bool(decode(data, i)) for i in range(count))
            sequences = lib.sequences(data, len(data))
            sequence_steps = 0
            unsupported_steps = 0
            for sequence in range(sequences):
                steps = lib.steps(data, len(data), sequence)
                for step in range(steps):
                    unsupported_steps += lib.pose(data, len(data), sequence, step) < 0
                assert lib.pose(data, len(data), sequence, steps) == -1
                sequence_steps += steps
            if path.name == 'A00.PAK' and entry['name'] == 'STUBBS0.VA2':
                assert sequences == 11
                assert [lib.pose(data, len(data), 4, i) for i in range(12)] == list(range(4, 16))
                assert [lib.pose(data, len(data), 5, i) for i in range(12)] == list(range(16, 28))
                assert lib.mirror(data, len(data), 0) == 0
                assert lib.mirror(data, len(data), 10) == 1
                for stop in (0, 15, len(data) // 2, len(data) - 1):
                    assert lib.pose(data, stop, 4, 0) == -1
            results.append(dict(sequences=sequences, sequence_steps=sequence_steps, unsupported_steps=unsupported_steps,archive=path.name, name=entry['name'], frames=count,
                                decoded=supported, unsupported=count-supported,
                                directory_supported=bool(count)))
if not results:
    parser.error('No VA2 resources found')
print(json.dumps(dict(resources=len(results), decoded=sum(x['decoded'] for x in results),
                      unsupported=sum(x['unsupported'] for x in results),
                      unknown_directories=sum(not x['directory_supported'] for x in results),
                      entries=results), indent=2))
