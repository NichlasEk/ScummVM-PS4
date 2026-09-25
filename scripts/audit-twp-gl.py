#!/usr/bin/env python3
"""Static GL call inventory for the local TWP renderer and shader wrapper.

This is a source inventory, not a runtime trace or a compatibility result.
"""
import hashlib
import json
from pathlib import Path
import re

root = Path(__file__).resolve().parent.parent
source = root / '.tools/scummvm-src'
paths = sorted((source / 'engines/twp').rglob('*.cpp'))
paths += [source / 'graphics/opengl/shader.cpp', source / 'graphics/opengl/shader.h']
result = {'kind': 'static source inventory; no runtime coverage implied', 'sources': {}, 'calls': {}}
for path in paths:
    raw = path.read_bytes()
    content = raw.decode()
    # Retain line count when stripping C/C++ comments.
    content = re.sub(r'/\*.*?\*/|//[^\n]*', lambda m: '\n' * m[0].count('\n'), content, flags=re.S)
    matches = list(re.finditer(r'\b(gl[A-Z]\w*)\s*\(', content))
    if not matches:
        continue
    relative = str(path.relative_to(source))
    result['sources'][relative] = hashlib.sha256(raw).hexdigest()
    for match in matches:
        result['calls'].setdefault(match[1], []).append({'file': relative, 'line': content[:match.start()].count('\n') + 1})
result['calls'] = dict(sorted(result['calls'].items()))
output = root / 'artifacts/vulkan/twp-gl-inventory.json'
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(result, indent=2) + '\n')
print(f'{len(result["calls"])} distinct GL call names across {len(result["sources"])} source files; {output}')
