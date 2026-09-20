#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Replay medal selection during Hamsun's speech in an isolated emulator profile."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess


def checksum(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--full-inventory", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    if subprocess.run(['pgrep', '-f', '^timeout.*scummvm-pice-pkg/eboot.bin'],
                      stdout=subprocess.DEVNULL).returncode == 0:
        raise SystemExit('Close the current PICE emulator before running this replay.')
    source = root / '.tools/xdg-pice/shadPS4'
    save = source / 'data/scummvm/saves/pice-pice-autosave.sav'
    data = save.read_bytes()
    payload = bytearray(data[16:])
    if (data[:8] != b'PICESV01' or len(payload) < 2228 or
            struct.unpack_from('<II', data, 8) != (len(payload), checksum(payload)) or
            not 1 <= struct.unpack_from('<I', payload)[0] <= 11 or
            struct.unpack_from('<I', payload, 4)[0] != 1147):
        raise SystemExit('Requires a valid local bridge autosave (bank 1147, version 1..11).')
    # Isolated fixture: Wayne has left, Hamsun has not received the medal.
    # Scalar world tables have the same layout in snapshot versions 1..11.
    for index, value in [(92, 0 if args.full_inventory else 1), (94, 0), (118, 0)]:
        struct.pack_into('<i', payload, 28 + (250 + index) * 4, value)
    target = root / '.tools/xdg-pice-hamsun-check/shadPS4'
    (target / 'data/scummvm/saves').mkdir(parents=True, exist_ok=True)
    for name in ['config.json', 'users.json']:
        if (source / name).exists():
            shutil.copyfile(source / name, target / name)
    shutil.copytree(source / 'input_config', target / 'input_config', dirs_exist_ok=True)
    (target / 'data/scummvm/saves/pice-pice-autosave.sav').write_bytes(
        data[:12] + struct.pack('<I', checksum(payload)) + payload)
    ini = (source / 'data/scummvm/scummvm.ini').read_text()
    ini = ''.join(line for line in ini.splitlines(True)
                  if not line.startswith(('pice_test_', 'pice_restore_')))
    hooks = ('[scummvm]\npice_restore_autosave=true\npice_test_room=2\n'
             'pice_test_click_hotspot=2\npice_test_medal_ambient=true')
    if args.full_inventory:
        hooks += "\npice_test_hamsun_full_inventory=true"
    (target / 'data/scummvm/scummvm.ini').write_text(ini.replace('[scummvm]', hooks))
    subprocess.run([str(root / 'scripts/pice/build.sh')], cwd=root, check=True)
    env = dict(os.environ, XDG_DATA_HOME=str(target.parent),
               XDG_CONFIG_HOME=str(root / '.tools/config-pice'),
               XDG_CACHE_HOME=str(root / '.tools/cache-pice'))
    log = root / 'artifacts/pice/hamsun-check.log'
    with log.open('w') as output:
        result = subprocess.run([
            'timeout', '--kill-after=3s', '150s' if args.full_inventory else '45s', str(root / '.tools/shadps4/Shadps4-sdl.AppImage'),
            '--fullscreen', 'false', '--mount', str(root / 'build/scummvm-dott-pkg/games/pice/cd-eur'),
            '/mnt/usb0', '--', str(root / 'build/scummvm-pice-pkg/eboot.bin')],
            cwd=root, env=env, stdout=output, stderr=subprocess.STDOUT)
    text = log.read_text(errors='replace')
    checks = {
        'startup_tests': '[pice] parser self-tests PASS' in text,
        'medal_picked_up': 'inventory pickup item=10' in text,
        'inventory_during_speech': 'test inventory visible during ambient speech, selecting item=10' in text,
        'medal_action': 'test item used on Hamsun item=10 context=10' in text,
        'medal_scene': 'request-room chapter=2 room=10 bank=1150' in text,
        'next_hamsun_line': 'animation=155 key=1070' in text,
        'no_script_stop': 'room-task=' not in text and 'room initialization failed' not in text,
        'autosave_valid': 'Autosave failed' not in text and 'Invalid snapshot' not in text,
        'no_crash': not re.search(r'Unhandled access violation|Assertion failed|Segmentation fault', text),
        'bounded_exit': result.returncode in (0, 124),
        'original_save_unchanged': save.read_bytes() == data,
    }
    if args.full_inventory:
        checks.update(
            wayne_conversation='menu selected id=0 option=6' in text,
            recorder_selected='selecting item=9' in text,
            recorder_used='test item used on Hamsun item=9 context=9' in text,
            recorder_complete='test recorder complete and checkpoint saved' in text,
            axe_collected='inventory pickup item=13' in text,
            door_works='request-room chapter=2 room=7 bank=1147' in text)
    print(json.dumps(checks, indent=2))
    raise SystemExit(0 if all(checks.values()) else 1)


if __name__ == '__main__':
    main()
