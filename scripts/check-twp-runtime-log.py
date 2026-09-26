#!/usr/bin/env python3
"""Reject warnings, script/GL errors and trivial startup-only TWP smoke runs."""
import argparse
import json
from pathlib import Path
import re

p = argparse.ArgumentParser()
p.add_argument('scummvm_log', type=Path)
p.add_argument('bootstrap_log', type=Path)
a = p.parse_args()
log = a.scummvm_log.read_text()
bootstrap = a.bootstrap_log.read_text()
errors = [line for line in log.splitlines() if re.search(r'WARNING:|GL ERROR:|error occurred in the script|Assertion failed|^.*ERROR:', line)]
assert not errors, '\n'.join(errors[:20])
assert 'Running Thimbleweed Park' in log, 'TWP never started'
assert not re.search(r'FAIL |STOP |failure=(?!none)\S+', bootstrap), 'Bridge failure'
frames = [dict(re.findall(r'(\w+)=(\d+)', line)) for line in bootstrap.splitlines() if line.startswith('FRAME ')]
assert frames and max(int(f.get('snapshots', 0)) for f in frames) > 64, 'Did not exercise the former snapshot limit'
print(json.dumps({'warnings': 0, 'sampled_frames': len(frames), 'peaks': {key: max(int(f.get(key, 0)) for f in frames) for key in ('snapshots', 'bytes', 'reserved', 'pages', 'cpu_textures', 'targets')}}, indent=2))
