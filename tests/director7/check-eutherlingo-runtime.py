#!/usr/bin/env python3
"""Run synthetic Guard regressions and the upstream Lingo suite in the real VM."""
import argparse
import os
import json
from pathlib import Path
import re
import shutil
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('source', type=Path)
parser.add_argument('binary', type=Path)
parser.add_argument('output', type=Path)
parser.add_argument('--baseline', type=Path, help='Pre-Guard binary for upstream failure comparison')
args = parser.parse_args()
source, binary, output = args.source.resolve(), args.binary.resolve(), args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
fixtures = Path(__file__).resolve().parent / 'eutherlingo'


def run(name, path, enabled, limit=0, executable=binary):
    config = output / (name + '.ini')
    config.write_text('[scummvm]\n' +
                      f'eutherlingo_guard={str(enabled).lower()}\n' +
                      f'eutherlingo_max_call_depth={limit}\n')
    command = [str(executable), '--config=' + str(config), '--path=' + str(path),
               '--debuglevel=0', '--no-fullscreen', 'directortest']
    try:
        result = subprocess.run(command, env=dict(os.environ, SDL_VIDEODRIVER='dummy',
                            SDL_AUDIODRIVER='dummy'), stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, input=b'exit\n', timeout=60)
    except subprocess.TimeoutExpired as exc:
        (output / (name + '.log')).write_bytes(exc.stdout or b'')
        raise
    text = result.stdout.decode('utf-8', errors='replace')
    (output / (name + '.log')).write_text(text)
    return result.returncode, text


def check_assertions(text):
    assert not re.search(r'b_scummvmassert.*(?:is false|is not equal)', text), text[-3000:]
    assert 'did not catch' not in text.lower(), text[-3000:]


for enabled in (False, True):
    directory = output / ('normal-data-' + str(enabled))
    directory.mkdir(exist_ok=True)
    (directory / 'lingotests').touch()
    shutil.copyfile(fixtures / 'normal.lingo', directory / 'normal.lingo')
    code, text = run('normal-' + str(enabled), directory, enabled, 8)
    assert code == 0 and 'ELG_NORMAL_PASS' in text, text[-3000:]
    check_assertions(text)
    assert ('ELG ' in text) == enabled
    assert text.count('WARNING: EutherLingo Guard:') <= 3

abort_dir = output / 'abort-data'
abort_dir.mkdir(exist_ok=True)
(abort_dir / 'lingotests').touch()
shutil.copyfile(fixtures / 'abort.lingo', abort_dir / 'a-abort.lingo')
shutil.copyfile(fixtures / 'after-abort.lingo', abort_dir / 'b-after.lingo')
code, text = run('abort', abort_dir, True)
assert code == 0 and 'ELG_AFTER_ABORT_PASS' in text and 'ELG_UNREACHABLE' not in text, text[-3000:]
check_assertions(text)
# The second error report contains the first error's unwind and the next root call.
assert re.search(r'ELG .* unwind .*\n(?:.*\n)*?ELG .* dispatch .* stack=0 depth=0', text)

disposed_dir = output / 'disposed-data'
disposed_dir.mkdir(exist_ok=True)
(disposed_dir / 'lingotests').touch()
shutil.copyfile(fixtures / 'disposed.lingo', disposed_dir / 'disposed.lingo')
code, text = run('disposed', disposed_dir, True)
assert code != 0 and 'property access on disposed object' in text and 'ELG_UNREACHABLE' not in text, text[-3000:]
check_assertions(text)

recursion_dir = output / 'recursion-data'
recursion_dir.mkdir(exist_ok=True)
(recursion_dir / 'lingotests').touch()
shutil.copyfile(fixtures / 'recursion.lingo', recursion_dir / 'recursion.lingo')
code, text = run('recursion', recursion_dir, True, 8)
assert code != 0 and 'call depth limit 8 reached' in text and 'ELG_UNREACHABLE' not in text, text[-3000:]
assert 'depth=8' in text

logs = []
for enabled in (False, True):
    code, text = run('upstream-' + str(enabled), source / 'engines/director/lingo/tests', enabled)
    assert code == 0, text[-3000:]
    logs.append(text)
# Ignore diagnostic output; compare which scripts executed and all assertion failures.
compiled = lambda text: re.findall(r'>> Compiling file (.+) of size', text)
assert compiled(logs[0]) == compiled(logs[1]) and len(compiled(logs[0])) >= 30
failures = lambda text: [line for line in text.splitlines() if
    re.search(r'b_scummvmassert.*(?:is false|is not equal)|did not catch error', line)]
assert failures(logs[0]) == failures(logs[1])
if args.baseline:
    code, baseline = run('upstream-baseline', source / 'engines/director/lingo/tests', False,
                         executable=args.baseline.resolve())
    assert code == 0 and compiled(baseline) == compiled(logs[0])
    assert failures(baseline) == failures(logs[0]), 'New or changed upstream assertion failures'
else:
    check_assertions(logs[0])
(output / 'summary.json').write_text(json.dumps({
    'upstream_scripts': len(compiled(logs[0])),
    'preexisting_assertion_failures': failures(logs[0]),
    'baseline_compared': bool(args.baseline),
    'synthetic_regressions': 'passed',
}, indent=2) + '\n')
print(f'PASS: normal calls, optional/extra arguments, caught and uncaught errors, depth limit; '
      f'{len(compiled(logs[0]))} upstream scripts with diagnostics off/on; '
      f'{len(failures(logs[0]))} unchanged baseline assertion failures')
