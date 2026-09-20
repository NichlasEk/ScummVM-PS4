#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Replay the local Driscoll checkpoint and report bounded emulator evidence."""
import fcntl
import json
import os
from pathlib import Path
import re
import subprocess


def main():
    root = Path(__file__).resolve().parents[2]
    out = root / 'artifacts/pice'
    out.mkdir(parents=True, exist_ok=True)
    profile = root / '.tools/xdg-pice/shadPS4/data/scummvm'
    ini = profile / 'scummvm.ini'
    save = profile / 'saves/pice-pice-autosave.sav'
    if not ini.exists() or not save.exists():
        raise SystemExit('Requires the local Driscoll autosave and emulator profile.')
    with (out / 'regression.lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        if subprocess.run(['pgrep', '-f', '^timeout.*scummvm-pice-pkg/eboot.bin'],
                          stdout=subprocess.DEVNULL).returncode == 0:
            raise SystemExit('A PICE emulator is already running.')
        original = ini.read_bytes()
        checkpoint = save.read_bytes()
        text = ''.join(line for line in original.decode().splitlines(True)
                       if not line.startswith(('pice_test_', 'pice_restore_')))
        if '[scummvm]' not in text:
            raise SystemExit('Missing scummvm config section.')
        env = dict(os.environ, EMULATOR_TIMEOUT='40')
        log = out / 'emulator.log'
        if log.exists():
            log.rename(out / 'before-driscoll-check.log')
        try:
            ini.write_text(text.replace('[scummvm]', '[scummvm]\npice_restore_autosave=true'))
            with (out / 'driscoll-check-build.log').open('w') as build:
                result = subprocess.run([str(root / 'scripts/pice/run-emulator.sh')],
                                        cwd=root, env=env, stdout=build, stderr=subprocess.STDOUT)
        finally:
            ini.write_bytes(original)
            save.write_bytes(checkpoint)
        text = log.read_text(errors='replace') if log.exists() else ''
        text = re.sub(r'\x1b\[[0-9;]*m', '', text)
        (out / 'driscoll-check.log').write_text(text)
        checks = {
            'startup_tests': '[pice] parser self-tests PASS' in text,
            'restored_room_7': 'checkpoint restored bank=1147' in text,
            'driscoll_first_reply': 'actor-dialogue actor=1 key=1220' in text,
            'driscoll_second_reply': 'actor-dialogue actor=1 key=1235' in text,
            'both_replies_finished': text.count('scripted-dialogue complete actor=1') >= 2,
            'bounded_exit': result.returncode in (0, 124),
            'no_crash': not re.search(r'Unhandled access violation|Assertion failed|Segmentation fault', text),
        }
        stops = [dict(task=int(t), result=int(r), pc=int(p), opcode=int(o))
                 for t, r, p, o in re.findall(r'room-task=(\d+) result=(\d+) pc=(\d+) opcode=(\d+)', text)]
        report = dict(checks=checks, checks_passed=all(checks.values()), stops=stops,
                      scope='Bounded Driscoll dialogue regression, not full-game playability')
        (out / 'driscoll-check.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps(report, indent=2))
        return 0 if all(checks.values()) else 1


if __name__ == '__main__':
    raise SystemExit(main())
