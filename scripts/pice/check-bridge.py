#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Replay bridge re-entry from the isolated Hamsun checkpoint, never player saves."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--recorder', action='store_true')
    parser.add_argument('--early-recorder', action='store_true', help='Try self-use as soon as input permits, testing initialization gating.')
    args = parser.parse_args()
    if args.early_recorder:
        args.recorder = True
    root = Path(__file__).resolve().parents[2]
    if subprocess.run(['pgrep', '-f', '^timeout.*scummvm-pice-pkg/eboot.bin'],
                      stdout=subprocess.DEVNULL).returncode == 0:
        raise SystemExit('Wait for the current emulator replay to finish.')
    source = root / '.tools/xdg-pice-hamsun-check/shadPS4'
    save = source / 'data/scummvm/saves/pice-pice-slot0.sav'
    original = save.read_bytes()
    payload = original[16:]
    checksum = 2166136261
    for byte in payload:
        checksum = ((checksum ^ byte) * 16777619) & 0xffffffff
    if (original[:8] != b'PICESV01' or len(payload) < 8 or
            struct.unpack_from('<II', original, 8) != (len(payload), checksum) or
            struct.unpack_from('<I', payload, 4)[0] != 1142):
        raise SystemExit('Run check-hamsun.py --full-inventory to create a valid quarters checkpoint.')
    target = root / '.tools/xdg-pice-bridge-check/shadPS4'
    target.mkdir(parents=True, exist_ok=True)
    for name in ('config.json', 'users.json'):
        if (source / name).exists():
            shutil.copyfile(source / name, target / name)
    shutil.copytree(source / 'input_config', target / 'input_config', dirs_exist_ok=True)
    saves = target / 'data/scummvm/saves'
    saves.mkdir(parents=True, exist_ok=True)
    (saves / save.name).write_bytes(original)
    ini = ''.join(line for line in (source / 'data/scummvm/scummvm.ini').read_text().splitlines(True)
                  if not line.startswith(('pice_test_', 'pice_restore_')))
    hooks = '[scummvm]\npice_restore_slot0=true\npice_test_click_hotspot=1'
    if args.recorder:
        hooks += '\npice_test_use_room=7\npice_test_use_hotspot=0\npice_test_use_item=17\npice_test_use_ready_hotspot=10\npice_test_checkpoint_room=7'
    if args.early_recorder:
        hooks = hooks.replace('\npice_test_use_ready_hotspot=10', '')
    (target / 'data/scummvm/scummvm.ini').write_text(ini.replace('[scummvm]', hooks))
    subprocess.run([str(root / 'scripts/pice/build.sh')], cwd=root, check=True)
    env = dict(os.environ, XDG_DATA_HOME=str(target.parent),
               XDG_CONFIG_HOME=str(root / '.tools/config-pice'),
               XDG_CACHE_HOME=str(root / '.tools/cache-pice'))
    log = root / 'artifacts/pice/bridge-check.log'
    with log.open('w') as output:
        result = subprocess.run([
            'timeout', '--kill-after=3s', '120s', str(root / '.tools/shadps4/Shadps4-sdl.AppImage'),
            '--fullscreen', 'false', '--mount', str(root / 'build/scummvm-dott-pkg/games/pice/cd-eur'),
            '/mnt/usb0', '--', str(root / 'build/scummvm-pice-pkg/eboot.bin')],
            cwd=root, env=env, stdout=output, stderr=subprocess.STDOUT)
    text = log.read_text(errors='replace')
    checks = {
        'startup_tests': '[pice] parser self-tests PASS' in text,
        'restored_quarters': 'checkpoint restored bank=1142' in text,
        'door_transition': 'request-room chapter=2 room=7 bank=1147' in text,
        'prisoner_hotspot': 'label=A Prisoner' in text,
        'no_script_stop': 'room-task=' not in text and 'room initialization failed' not in text,
        'no_crash': not re.search(r'Unhandled access violation|Assertion failed|Segmentation fault', text),
        'source_save_unchanged': save.read_bytes() == original,
        'bounded_exit': result.returncode in (0, 124),
    }
    if args.recorder:
        checks['recorder_action'] = 'test use item=17 hotspot=0' in text
        checks['recording_played'] = 'narration key=1320' in text
        checks['ryan_closeup'] = 'animation=167 key=1340' in text
        checks['submarine_movie_complete'] = 'movie completed name=Vin1.mux frames=181 failed=0' in text
        checks['movie_audio_suspended'] = 'movie background audio paused channels=' in text and 'movie background audio resumed channels=' in text
        checks['next_room_checkpoint'] = 'test progression checkpoint bank=1147' in text
        checks['survived'] = 'game over reason=' not in text
    report = dict(checks=checks, passed=all(checks.values()),
                  scope='Focused bridge replay; not complete-game validation')
    (root / 'artifacts/pice/bridge-check.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    return 0 if all(checks.values()) else 1


if __name__ == '__main__':
    raise SystemExit(main())
