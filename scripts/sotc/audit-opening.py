#!/usr/bin/env python3
"""Validate the supported opening prefix and UI clock against supplied R05.CC4."""
import argparse
import ctypes as c
from pathlib import Path
import subprocess
import tempfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('directory', type=Path)
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
source = r'''
#include "engines/sotc/opening.h"
extern "C" bool parse(const uint8_t *p,unsigned n,unsigned *out) {
 Sotc::OpeningPlan plan={};
 if (!plan.load(Sotc::Cc4View(p,n))) return false;
 for (unsigned i=0;i<2;++i) {
  out[i*5]=plan.logos[i].background; out[i*5+1]=plan.logos[i].fadeIn;
  out[i*5+2]=plan.logos[i].hold; out[i*5+3]=plan.logos[i].fadeOut; out[i*5+4]=plan.logos[i].black;
 }
 out[10]=plan.music; out[11]=plan.musicDelay; return true;
}
extern "C" bool clock_test(const uint8_t *p,unsigned n) {
 Sotc::OpeningPlan plan={}; if (!plan.load(Sotc::Cc4View(p,n))) return false;
 Sotc::OpeningClock small(plan),big(plan);
 for (unsigned t=0;t<20000 && !small.done();++t) {
  if (small.logo()>1 || small.brightness()>255) return false;
  small.advance(1);
 }
 big.advance(20000);
 if (!small.done() || !big.done() || !small.musicDue() || big.brightness()!=0) return false;
 Sotc::OpeningClock exact(plan);
 if (exact.logo()!=0 || exact.brightness()!=0 || exact.musicDue()) return false;
 exact.advance(512); if (exact.brightness()!=255 || exact.logo()!=0 || !exact.musicDue()) return false;
 exact.advance(5000); if (exact.brightness()!=255 || exact.logo()!=0) return false;
 exact.advance(512); if (exact.brightness()!=0 || exact.logo()!=0) return false;
 exact.advance(500); if (exact.logo()!=1 || exact.brightness()!=0) return false;
 exact.advance(6024); return exact.done();
}
'''
data = (a.directory / 'R05.CC4').read_bytes()
with tempfile.TemporaryDirectory(prefix='sotc-opening-') as d:
    d = Path(d)
    (d / 'test.cpp').write_text(source)
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC', '-I', str(root), str(d / 'test.cpp'), '-o', str(d / 'test.so')], check=True)
    lib = c.CDLL(str(d / 'test.so'))
    lib.parse.argtypes = [c.c_char_p, c.c_uint, c.POINTER(c.c_uint)]
    lib.parse.restype = c.c_bool
    lib.clock_test.argtypes = [c.c_char_p, c.c_uint]
    lib.clock_test.restype = c.c_bool
    out = (c.c_uint * 12)()
    assert lib.parse(data, len(data), out)
    assert list(out) == [0, 8, 50, 8, 0, 10, 8, 50, 8, 5, 8, 5], list(out)
    assert lib.clock_test(data, len(data))
    # Prefix truncation and damaged outer directory must fail, not play arbitrary assets.
    for n in range(24):
        assert not lib.parse(data, n, out)
    assert not lib.parse(b'\xff' * len(data), len(data), out)
print('Opening prefix: original logo resources 0/10, fades 8, holds 50, music 8; clock boundaries passed')
