#!/usr/bin/env python3
"""Test the actual pose renderer with synthetic data and supplied original PAKs.

The host shim only supplies the Array container; decoding/rasterization is the
same header compiled into the PS4 engine. No original pixels are embedded here.
"""
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
with tempfile.TemporaryDirectory(prefix='sotc-poses-') as temp:
    temp = Path(temp)
    (temp / 'common').mkdir()
    (temp / 'common/array.h').write_text('#include <vector>\n#include <stdint.h>\ntypedef uint8_t byte;\nnamespace Common { template<class T> using Array = std::vector<T>; }\n')
    (temp / 'test.cpp').write_text('''
#include "engines/sotc/poserenderer.h"
#include "engines/sotc/toolbar.h"
#include "engines/sotc/explode.h"
extern "C" bool toolbar(const uint8_t *p,unsigned n) {
 Common::Array<byte> data(p,p+n); Sotc::Toolbar bar;
 if(!bar.load(data) || bar.visible())return false;
 bar.move(20,10);if(bar.visible())return false;
 bar.move(20,1);if(!bar.visible() || bar.hover()!=0)return false;
 auto previous=bar.pixels();
 for(int i=0;i<7;++i) {
  bar.move(i*45+20,20);
  if(bar.hover()!=i || !bar.render() || bar.pixels()==previous)return false;
  if(bar.menuAt(i*45+20,20)!=(i==6))return false;
  previous=bar.pixels();
 }
 bar.move(300,39);if(!bar.visible())return false;
 bar.move(300,40);if(bar.visible() || bar.menuAt(300,20))return false;
 bar.move(-1,0);if(bar.visible())return false;
 bar.move(320,0);if(bar.visible())return false;
 bar.move(300,0);bar.hide();return !bar.visible();
}
extern "C" bool unpack(const uint8_t *p,unsigned n,uint8_t *o,unsigned m,unsigned f) {return Sotc::explode(p,n,o,m,f);}
extern "C" bool render(const uint8_t *p,unsigned n,unsigned pose,int x,int y,unsigned clip,uint8_t *out,int next,unsigned fraction) {
 Common::Array<byte> data(p,p+n), screen(64000,119);
 Sotc::PoseRenderer r(screen); bool ok=r.pose(data,pose,x,y,clip,clip,319,199,-1,next,fraction);
 for(unsigned i=0;i<64000;++i) out[i]=screen[i];
 return ok;
}
''')
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
                    '-I', str(temp), '-I', str(root), '-I', str(root/'engines'), str(temp / 'test.cpp'), '-o', str(temp / 'test.so')], check=True)
    lib = c.CDLL(str(temp / 'test.so'))
    lib.render.restype = lib.unpack.restype = c.c_bool
    lib.render.argtypes = [c.c_char_p, c.c_uint, c.c_uint, c.c_int, c.c_int, c.c_uint, c.c_void_p, c.c_int, c.c_uint]
    out = c.create_string_buffer(64000)
    def render(data, pose=0, x=0, y=0, clip=0, next_pose=-1, fraction=0):
        return lib.render(data, len(data), pose, x, y, clip, out, next_pose, fraction)
    def fixture(poses):
        section = b''
        offsets = []
        for pose in poses:
            section += b'\0\0'
            offsets.append(len(poses) * 4 + len(section))
            section += pose
        section = struct.pack('<' + 'I' * len(poses), *offsets) + section
        end = 16 + len(section)
        return struct.pack('<4I', 16, end, end, end) + section
    def command(kind, color, points):
        return bytes([kind, len(points)]) + struct.pack('<H', color) + b''.join(struct.pack('<hh', *p) for p in points)
    line = b'\0\1' + command(6, 0x2000, [(2, 2), (4, 2), (4, 4)])
    assert render(fixture([line]))
    expected = {(2, 2), (3, 2), (4, 2), (4, 3), (4, 4)}
    assert {(i % 320, i // 320) for i, v in enumerate(out.raw) if v == 32} == expected
    assert render(fixture([line]), clip=3)
    assert {(i % 320, i // 320) for i, v in enumerate(out.raw) if v == 32} == {(4, 3), (4, 4)}
    nested = b'\0\1' + command(0, 1, [(10, 10)])
    assert render(fixture([nested, line])) and out.raw[12 * 320 + 12] == 32
    mirrored = b'\0\1' + command(0, 0x8001, [(10, 10)])
    assert render(fixture([mirrored, line]))
    assert {(i % 320, i // 320) for i, v in enumerate(out.raw) if v == 32} == {(8, 12), (7, 12), (6, 12), (6, 13), (6, 14)}
    cyclic = b'\0\1' + command(0, 0, [(0, 0)])
    assert not render(fixture([cyclic])) and set(out.raw) == {119}
    partial = b'\0\2' + line[2:] + command(254, 0, [(0, 0)])
    assert not render(fixture([partial])) and set(out.raw) == {119}
    assert not render(fixture([line[:-1]])) and set(out.raw) == {119}
    square = b'\0\1' + command(4, 0x2110, [(2, 2), (6, 2), (6, 6), (2, 6)])
    assert render(fixture([square]))
    assert out.raw[4 * 320 + 4] == 33 and out.raw[2 * 320 + 4] == 16
    dots = b'\0\1' + command(7, 0x3400, [(2, 2), (6, 6), (-1, 0)])
    assert render(fixture([dots]))
    assert sum(v == 52 for v in out.raw) == 2
    assert out.raw[3 * 320 + 3] == 119  # Independent points, no connecting line.
    rectangle = b'\0\1' + command(5, 0x2110, [(6, 6), (2, 2)])
    assert render(fixture([rectangle]))
    assert out.raw[4 * 320 + 4] == 33 and out.raw[2 * 320 + 4] == 16
    assert sum(v != 119 for v in out.raw) == 25
    no_outline = b'\0\1' + command(5, 0x21ff, [(-2, -2), (2, 2)])
    assert render(fixture([no_outline]))
    assert sum(v == 33 for v in out.raw) == 9
    first = b'\0\1' + command(6, 0x2000, [(4, 4), (6, 4)])
    second = b'\0\1' + command(6, 0x2000, [(1, 4), (3, 4)])
    assert render(fixture([first, second]), next_pose=1, fraction=64)
    assert {(i % 320, i // 320) for i, v in enumerate(out.raw) if v == 32} == {(3, 4), (4, 4), (5, 4)}
    assert not render(fixture([first, second]), next_pose=1, fraction=257) and set(out.raw) == {119}
    assert not render(fixture([first, line]), next_pose=1, fraction=64) and set(out.raw) == {119}
    report = []
    for archive_name, name, pose, x, y in [('RES.PAK', 'ICONE.VA2', 0, 0, 0),
                                         ('A00.PAK', 'VOITURE.VA2', 0, 120, 161),
                                         ('A00.PAK', 'BIRDS.VA2', 0, 72, 5),
                                         ('A00.PAK', 'BIRDS.VA2', 4, 84, 1),
                                         ('A00.PAK', 'SEAGULLS.VA2', 0, 0, 160)]:
        path = args.directory / archive_name
        archive = path.read_bytes()
        entry = next(e for e in pak.inspect(path)['entries'] if e['name'] == name)
        data = archive[entry['payload']:entry['payload'] + entry['packed']]
        if entry['method'] == 1:
            decoded = c.create_string_buffer(entry['unpacked'])
            assert lib.unpack(data, len(data), decoded, len(decoded), entry['flags'])
            data = decoded.raw
        else:
            assert entry['method'] == 0
        if name == 'ICONE.VA2':
            lib.toolbar.argtypes=[c.c_char_p,c.c_uint];lib.toolbar.restype=c.c_bool
            assert lib.toolbar(data,len(data))
        ok = render(data, pose, x, y, next_pose=1 if name == 'SEAGULLS.VA2' else -1, fraction=64)
        assert ok, (name, pose)
        report.append(dict(resource=name, pose=pose, supported=ok,
                           changed_pixels=sum(v != 119 for v in out.raw)))
    print(json.dumps(report, indent=2))
