#!/usr/bin/env python3
"""Enable native mouse input in an existing isolated shadPS4 profile (restart required)."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('profile', type=Path, help='Existing data/shadPS4 directory')
args = parser.parse_args()
config_path = args.profile / 'config.json'
global_path = args.profile / 'input_config/global.ini'
game_path = args.profile / 'input_config/SCVM00265.ini'
config = json.loads(config_path.read_text())
global_text = global_path.read_text()
game_text = game_path.read_text()
config.setdefault('Input', {})['use_mice_as_mice'] = True
config['Input']['use_unified_input_config'] = False

def filter_bindings(text):
    result = []
    for line in text.splitlines():
        key, sep, value = line.partition('=')
        key, value = key.strip(), value.strip()
        if sep and key in ('mouse_to_joystick', 'hotkey_toggle_mouse_to_joystick'):
            continue
        if sep and key in ('cross', 'circle') and value in ('leftbutton', 'rightbutton'):
            continue
        result.append(line)
    return '\n'.join(result) + '\n'

for path, content in ((config_path, json.dumps(config, indent=2) + '\n'),
                      (global_path, filter_bindings(global_text)),
                      (game_path, filter_bindings(game_text))):
    backup = path.with_name(path.name + '.before-direct-mouse')
    if not backup.exists():
        backup.write_bytes(path.read_bytes())
    path.write_text(content)
print('Native mouse configured. Restart shadPS4 to clear any active mouse-to-stick mode.')
print('Use a package without scummvm-controller-only.txt; do not use F7 for mouse focus.')
