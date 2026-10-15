#!/usr/bin/env python3
"""Export the isolated experiment against verified clean upstream/local engines."""
from pathlib import Path
import difflib, re
root=Path(__file__).resolve().parents[2]; out=Path(__file__).resolve().parent
clean=root/'.tools/upstream-2026.4.0/scummvm-2026.4.0'
paths=set(re.findall(r'^\+\+\+ b/(.+)$',(root/'patches/scummvm/ps4.patch').read_text(),re.M))
paths.update(n for n in re.findall(r'^\+\+\+ b/(.+)$',(out/'vulkan.patch').read_text(),re.M))
paths.update(('backends/events/sdl/sdl2-events.cpp','backends/events/sdl/sdl-common-events.cpp'))
for mode,dirname in [('software','ps4-audit'),('vulkan','ps4-vulkan')]:
 source=clean.parent/dirname; changes=[]
 for name in sorted(paths):
  new=source/name; old=clean/name
  if not new.exists(): continue
  a=old.read_text() if old.exists() else ''; b=new.read_text()
  changes.extend(difflib.unified_diff(a.splitlines(True),b.splitlines(True),fromfile='a/'+name if old.exists() else '/dev/null',tofile='b/'+name))
 (out/(mode+'.patch')).write_text(''.join(changes))
changes=[]
for name in ('interface.cpp','input.cpp','detection.cpp'):
 path='engines/pice/'+name
 a=(root/path).read_text(); b=(clean.parent/'ps4-audit'/path).read_text()
 changes.extend(difflib.unified_diff(a.splitlines(True),b.splitlines(True),fromfile='a/'+path,tofile='b/'+path))
(out/'local-engines.patch').write_text(''.join(changes))

changes=[]
for engine in ('macs2','watchmaker'):
 for old in sorted((clean/'engines'/engine).rglob('*')):
  if not old.is_file(): continue
  name=str(old.relative_to(clean)); new=clean.parent/'ps4-vulkan'/name
  a=old.read_text(); b=new.read_text()
  changes.extend(difflib.unified_diff(a.splitlines(True),b.splitlines(True),fromfile='a/'+name,tofile='b/'+name))
(out/'extra-engines.patch').write_text(''.join(changes))
