#!/usr/bin/env python3
"""Compare all patched files in two separately prepared Vulkan source trees."""
from pathlib import Path
import json, re, sys
expected, replay = map(Path, sys.argv[1:])
patches=Path(__file__).resolve().parent
names=set()
for name in ('vulkan.patch','local-engines.patch','extra-engines.patch'):
 names.update(re.findall(r'^\+\+\+ b/(.+)$',(patches/name).read_text(),re.M))
for name in sorted(names):
 if (expected/name).read_bytes() != (replay/name).read_bytes():
  raise SystemExit('Prepared source mismatch: '+name)
print(json.dumps({'matching_modified_files':len(names)},indent=2))
