#!/usr/bin/env python3
"""Validate the shader browser pack, allowlist, preset references and reproducibility."""
from pathlib import Path
import json,re,subprocess,tempfile,zipfile
root=Path(__file__).resolve().parents[2]
source=root/'assets/ps4-shaders'
catalog=json.loads((source/'catalog.json').read_text())
names=[f['name'] for f in catalog]
assert len(names)==len(set(names))
assert set(names)=={p.stem for p in (source/'PS4').glob('*.glslp')}
header=(root/'compat/ps4gl/scummvm-filter-presets.inc').read_text()
assert set(re.findall(r'"PS4/(.+)\.glslp"',header))==set(names)
for name in names:
 preset=(source/'PS4'/(name+'.glslp')).read_text()
 assert re.search(r'^shaders\s*=\s*1$',preset,re.M),name
 path=re.search(r'^shader0\s*=\s*(\S+)$',preset,re.M).group(1)
 assert (source/'PS4'/path).is_file(),(name,path)
 assert 'mipmap' not in preset and 'textures' not in preset,name
with tempfile.TemporaryDirectory() as directory:
 packs=[Path(directory)/name for name in ('a.dat','b.dat')]
 for pack in packs:subprocess.run(['python3',str(root/'scripts/package-ps4-shaders.py'),str(pack)],check=True)
 assert packs[0].read_bytes()==packs[1].read_bytes()
 with zipfile.ZipFile(packs[0]) as z:
  assert z.testzip() is None
  assert {p for p in z.namelist() if p.endswith('.glslp')}=={'PS4/'+n+'.glslp' for n in names}
 print('PASS',len(names),'presets, referenced sources, allowlist, deterministic ZIP and CRC')
