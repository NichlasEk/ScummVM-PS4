#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
"$root/scripts/test-ps4-gif-build.sh"
export XDG_DATA_HOME="$root/.tools/xdg-gif-smoke"
export XDG_CONFIG_HOME="$root/.tools/config-gif-smoke"
export XDG_CACHE_HOME="$root/.tools/cache-gif-smoke"
mkdir -p "$XDG_DATA_HOME" "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME" "$root/artifacts/emulator"
result="$XDG_DATA_HOME/shadPS4/data/gif-smoke.txt"
rm -f "$result"
log="$root/artifacts/emulator/gif-smoke.log"
# The emulator may keep its window open after guest exit. Stop only this
# process group, after a fresh completed report and the guest-exit marker.
python3 - "$root" "$log" "$result" <<'PY'
import os, signal, subprocess, sys, time
from pathlib import Path
root, log, result=map(Path,sys.argv[1:])
with log.open('w') as output:
 proc=subprocess.Popen(['xvfb-run','-a','-s','-screen 0 1280x720x24',
  str(root/'.tools/shadps4/Shadps4-sdl.AppImage'),'--fullscreen','false',
  str(root/'build/ps4-gif-smoke/stage/eboot.bin')],
  env=dict(os.environ,SDL_VIDEODRIVER='x11'),stdout=output,stderr=subprocess.STDOUT,start_new_session=True)
 try:
  deadline=time.monotonic()+90
  while True:
   if result.exists() and result.read_text().endswith('\nPASS\n') and 'Exiting with status code 0' in log.read_text(errors='replace'):
    break
   if proc.poll() is not None: raise SystemExit('Emulator exited before successful guest completion')
   if time.monotonic()>deadline: raise SystemExit('GIF smoke test timed out')
   time.sleep(.25)
 finally:
  try: os.killpg(proc.pid,signal.SIGTERM)
  except ProcessLookupError: pass
  try: proc.wait(timeout=3)
  except subprocess.TimeoutExpired:
   os.killpg(proc.pid,signal.SIGKILL); proc.wait()
PY
rg -q 'Exiting with status code 0' "$log"
diff -u "$root/build/ps4-gif-smoke/expected.txt" "$result"
cp "$result" "$root/artifacts/emulator/gif-smoke-result.txt"
cat "$result"
