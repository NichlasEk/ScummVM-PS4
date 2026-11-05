#!/usr/bin/env python3
"""Test the actual FileIO view and ScummVM memory stream with ASan/UBSan."""
from pathlib import Path
import subprocess, sys, tempfile
source, build = (Path(p).resolve() for p in sys.argv[1:])
with tempfile.TemporaryDirectory(prefix='fileio-readwrite-') as d:
    binary = Path(d) / 'test'
    subprocess.run(['c++', '-std=c++17', '-g', '-O1', '-fsanitize=address,undefined',
                    '-DHAVE_CONFIG_H', '-I'+str(build), '-I'+str(source),
                    '-I'+str(source/'engines'), str(Path(__file__).with_suffix('.cpp')),
                    '-Wl,--start-group', str(build/'common/libcommon.a'),
                    str(build/'common/formats/libformats.a'), '-Wl,--end-group',
                    '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('PASS: real FileIO buffer; shared read/write position, EOF, empty files, bounds, overwrite and growth; ASan/UBSan')
