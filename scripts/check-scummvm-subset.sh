#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
mkdir -p build artifacts/scummvm-vulkan
python3 - <<'PY'
from pathlib import Path
import re
exports=set(re.findall(r'PROC\((gl\w+)\)',Path('compat/ps4gl/ps4gl.c').read_text()))
loads=re.findall(r'PS4GL_LOAD\((gl\w+)\)',Path('compat/ps4gl/scummvm-context.inc').read_text())
assert len(loads)==len(set(loads)), 'Duplicate subset loader entry'
assert set(loads)<=exports, 'Subset loader requires unavailable GL functions'
print(f'PASS all {len(loads)} required loader entries exist in the bridge')
PY
c++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
 -I.tools/scummvm-src -Icompat/ps4gl tests/ps4/check-scummvm-subset.cpp -o build/check-scummvm-subset
build/check-scummvm-subset | tee artifacts/scummvm-vulkan/subset-loader-host.log
