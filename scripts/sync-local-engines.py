#!/usr/bin/env python3
"""Copy maintained local engines into a generated ScummVM source tree.

Used by the full builds as well as isolated engine configurations. Testbench
entry points and automatic game selection are deliberately not copied.
"""
from pathlib import Path
import shutil
import sys

root = Path(__file__).resolve().parent.parent
source = Path(sys.argv[1]).resolve()
if not (source / 'configure').is_file():
    raise SystemExit('Expected a prepared ScummVM source tree: ' + str(source))
for engine in ('pice', 'sotc'):
    origin = root / 'engines' / engine
    destination = source / 'engines' / engine
    for item in origin.rglob('*'):
        if not item.is_file():
            continue
        target = destination / item.relative_to(origin)
        if not target.exists() or target.read_bytes() != item.read_bytes():
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(item, target)
    print('Local engine available:', engine)
