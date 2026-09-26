#!/usr/bin/env python3
"""Capture the settled real launcher under Xvfb; does not test input or games."""
from pathlib import Path
import json
import os
import subprocess
import time

ROOT = Path(__file__).resolve().parent.parent
os.chdir(ROOT)
if not os.environ.get('DISPLAY'):
    raise SystemExit('Run with xvfb-run -a -s "-screen 0 1280x720x24" python3 scripts/capture-scummvm-vulkan.py')
reports = ROOT / 'artifacts/scummvm-vulkan'
bootstrap = ROOT / '.tools/xdg-scummvm-vulkan/shadPS4/data/scummvm-vulkan/bootstrap.log'
reports.mkdir(parents=True, exist_ok=True)
if bootstrap.exists():
    (reports / 'bootstrap-before-capture.log').write_bytes(bootstrap.read_bytes())
    bootstrap.unlink()
# The emulator F12 capture may retain the initial black frame when the idle GUI
# no longer redraws. Capture the actual X window after two successful presents.
with (reports / 'capture-emulator.log').open('w') as log:
    process = subprocess.Popen(['scripts/run-scummvm-vulkan.sh'],
        env=dict(os.environ, EMULATOR_TIMEOUT='20'), stdout=log, stderr=log)
    try:
        deadline = time.monotonic() + 15
        while True:
            state = bootstrap.read_text() if bootstrap.exists() else ''
            if 'FAIL application' in state:
                raise RuntimeError('Application rendering failed; inspect bootstrap.log')
            if 'PASS application present: state=0 failed=0 result=0 frames=2' in state:
                break
            if process.poll() is not None or time.monotonic() >= deadline:
                raise RuntimeError('No second presented frame; inspect capture-emulator.log')
            time.sleep(.1)
        time.sleep(1)
        subprocess.run(['import', '-window', 'root', str(reports / 'launcher-window.png')], check=True)
        (reports / 'launcher-bootstrap.log').write_text(state)
        result = process.wait(timeout=25)
        if result != 124:
            raise RuntimeError(f'Unexpected emulator exit {result}; expected diagnostic timeout')
        (reports / 'launcher-capture.json').write_text(json.dumps({
            'presented_frames_at_capture': 2, 'capture': 'launcher-window.png',
            'termination': 'diagnostic timeout', 'input_verified': False,
            'game_start_verified': False, 'graceful_cleanup_verified': False,
        }, indent=2) + '\n')
        print('Captured real launcher after two presents; inspect launcher-window.png')
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=25)
