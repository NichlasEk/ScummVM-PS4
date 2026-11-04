#!/usr/bin/env python3
"""Run a staged PS4 build with a checkout-local shadPS4 profile."""
import argparse
import fcntl
import os
from pathlib import Path
import signal
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
STAGES = {
    'software': 'scummvm-2026.4.0-pkg',
    'vulkan': 'scummvm-2026.4.0-vulkan-pkg',
    'vulkan-full': 'scummvm-2026.4.0-vulkan-full-pkg',
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', nargs='?', choices=STAGES, default='vulkan')
    parser.add_argument('--stage', type=Path, help='Override the staged app directory')
    parser.add_argument('--emulator', type=Path, default=Path(os.environ.get(
        'SHADPS4_BIN', str(ROOT / 'artifacts/emulator/shadps4'))))
    parser.add_argument('--timeout', type=float, default=0, help='Seconds; 0 means interactive')
    parser.add_argument('--screenshot', action='store_true', help='Capture X11 screen before timeout')
    args = parser.parse_args()
    if args.timeout < 0 or (args.screenshot and args.timeout == 0):
        parser.error('Screenshot requires a positive timeout; timeout must not be negative')
    emulator = args.emulator.expanduser().resolve()
    stage = (args.stage or ROOT / 'build' / STAGES[args.mode]).resolve()
    if not emulator.is_file() or not os.access(emulator, os.X_OK):
        parser.error('Set SHADPS4_BIN to your built shadPS4 executable')
    if not (stage / 'eboot.bin').is_file():
        parser.error(f'Missing {stage / "eboot.bin"}; build/package or supply --stage')
    profile = ROOT / 'artifacts/emulator' / args.mode
    # A portable user directory takes precedence over global/XDG profiles.
    for directory in ('user', 'config', 'cache'):
        (profile / directory).mkdir(parents=True, exist_ok=True)
    lock = (profile / 'session.lock').open('w')
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        parser.error(f'Profile already running: {profile}')
    env = dict(os.environ, XDG_DATA_HOME=str(profile / 'data'),
               XDG_CONFIG_HOME=str(profile / 'config'), XDG_CACHE_HOME=str(profile / 'cache'))
    if args.screenshot:
        env.update(SDL_VIDEODRIVER='x11', SDL_VIDEO_DRIVER='x11')
    stamp = time.strftime('%Y%m%d-%H%M%S') + f'-{os.getpid()}'
    log_path = profile / f'{stamp}.log'
    command = [str(emulator), '--fullscreen', 'false', '--game', str(stage / 'eboot.bin')]
    print(f'Emulator: {emulator}\nStage: {stage}\nProfile: {profile}\nLog: {log_path}', flush=True)
    with log_path.open('w') as log:
        proc = subprocess.Popen(command, cwd=profile, env=env, stdout=log,
                                stderr=subprocess.STDOUT, start_new_session=True)
        def interrupted(signum, frame):
            raise KeyboardInterrupt
        signal.signal(signal.SIGTERM, interrupted)
        try:
            try:
                return proc.wait(timeout=args.timeout or None)
            except subprocess.TimeoutExpired:
                if args.screenshot:
                    subprocess.run(['import', '-window', 'root',
                                    str(profile / f'{stamp}.png')], check=True, timeout=10)
                print('Timed session finished; stopping emulator (not a guest clean shutdown).')
                return 124
            except KeyboardInterrupt:
                return 130
        finally:
            # Always clean up the owned process group, including remaining children.
            try:
                os.killpg(proc.pid, signal.SIGTERM)
                try:
                    proc.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    pass
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            proc.wait()


if __name__ == '__main__':
    raise SystemExit(main())
