#!/usr/bin/env python3
"""Run the production PFR1 bridge under sanitizers with a local PFR fixture.
No copyrighted font fixture is distributed. Usage: script SOURCE LOCAL_PFR
"""
from pathlib import Path
import os, subprocess, sys, tempfile
root=Path(sys.argv[1])/'engines/director/fonts/pfr1'
fixture=Path(sys.argv[2]).resolve()
harness=Path(__file__).with_name('pfr1-probe.cpp')
with tempfile.TemporaryDirectory() as tmp:
    tmp=Path(tmp)
    subprocess.run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined',
        '-I'+str(root),str(harness),*[str(root/(n+'.cpp')) for n in
        ('Pfr1Font','Pfr1TtfConverter','PfrBitReader','bridge')],'-o',str(tmp/'probe')],check=True)
    env=dict(os.environ,UBSAN_OPTIONS='halt_on_error=1')
    subprocess.run([str(tmp/'probe'),str(fixture),str(tmp/'font.ttf')],check=True,env=env)
    from fontTools.ttLib import TTFont
    from PIL import ImageFont
    font=TTFont(tmp/'font.ttf')
    assert font['head'].indexToLocFormat==0
    raster=ImageFont.truetype(str(tmp/'font.ttf'),9)
    for char in 'ABCDEFGHIJKLMNOPQRSTUVWXYZÅÄÖåäö':
        assert raster.getmask(char).getbbox(),char
print('PASS: embedded PFR1 conversion, short headers, magic and visible Swedish glyphs')
