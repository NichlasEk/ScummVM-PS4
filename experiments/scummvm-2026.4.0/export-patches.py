#!/usr/bin/env python3
"""Export the isolated experiment against verified clean upstream/local engines."""
from pathlib import Path
import difflib, re
root=Path(__file__).resolve().parents[2]; out=Path(__file__).resolve().parent
clean=root/'.tools/upstream-2026.4.0/scummvm-2026.4.0'
paths=set(re.findall(r'^\+\+\+ b/(.+)$',(root/'patches/scummvm/ps4.patch').read_text(),re.M))
paths.update(n for n in re.findall(r'^\+\+\+ b/(.+)$',(out/'vulkan.patch').read_text(),re.M))
paths.add('common/str-base.cpp')
paths.add('engines/hpl1/engine/impl/low_level_graphics_tgl.cpp')
paths.add('backends/keymapper/virtual-mouse.h')
paths.update(('engines/hpl1/engine/libraries/newton/Newton.cpp', 'engines/hpl1/metaengine.cpp', 'engines/hpl1/engine/input/Mouse.cpp'))
paths.update(('backends/graphics/opengl/renderer3d.cpp', 'backends/graphics/opengl/renderer3d.h', 'graphics/opengl/shader.cpp'))
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

# Rebase only the Director compatibility delta; upstream's D7 parsers stay intact.
changes=[]
source=clean.parent/'ps4-vulkan'
for new in sorted((source/'engines/director').rglob('*')):
 if not new.is_file(): continue
 name=str(new.relative_to(source)); old=clean/name
 if old.exists() and old.read_bytes() == new.read_bytes(): continue
 a=old.read_text() if old.exists() else ''; b=new.read_text()
 changes.extend(difflib.unified_diff(a.splitlines(True),b.splitlines(True),fromfile='a/'+name if old.exists() else '/dev/null',tofile='b/'+name))
(out/'director7.patch').write_text(''.join(changes))

# Hardware renderer adaptations for the 2026.4 engines.
changes=[]
for engine in ('grim', 'myst3', 'stark', 'twp', 'dgds', 'freescape'):
 for old in sorted((clean/'engines'/engine).rglob('*')):
  if not old.is_file(): continue
  name=str(old.relative_to(clean)); new=source/name
  if name in paths: continue  # Already carried by vulkan.patch.
  if old.read_bytes() == new.read_bytes(): continue
  a=old.read_text(); b=new.read_text()
  changes.extend(difflib.unified_diff(a.splitlines(True),b.splitlines(True),fromfile='a/'+name,tofile='b/'+name))
(out/'3d-engines.patch').write_text(''.join(changes))
