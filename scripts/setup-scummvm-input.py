#!/usr/bin/env python3
"""Seed project-local shadPS4 input settings; retain existing user bindings."""
import argparse
import json
import shutil
from pathlib import Path

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--vulkan', action='store_true', help='Use the isolated experimental profile')
parser.add_argument('--profile', type=Path, help='Explicit shadPS4 profile directory for an isolated test')
args = parser.parse_args()
profile = args.profile if args.profile else root / ('.tools/xdg-scummvm-vulkan/shadPS4' if args.vulkan else '.tools/xdg/shadPS4')
profile.mkdir(parents=True, exist_ok=True)
config_path = profile / 'config.json'
config = json.loads(config_path.read_text()) if config_path.exists() else {}
config.setdefault('Input', {})['use_unified_input_config'] = False
config_path.write_text(json.dumps(config, indent=2) + '\n')
input_dir = profile / 'input_config'
input_dir.mkdir(exist_ok=True)
bindings = input_dir / ('SCVM00010.ini' if args.vulkan else 'SCVM00002.ini')
if not bindings.exists():
    shutil.copyfile(root / 'config/emulator/SCVM00002.ini', bindings)
    if args.vulkan:
        # Mouse deltas become stick deflections before ScummVM's cubic curve.
        # Keep physical stick mappings and existing user profiles unchanged.
        bindings.write_text(bindings.read_text().replace(
            'mouse_movement_params = 0.15, 3.0, 0.0',
            'mouse_movement_params = 0.40, 3.0, 0.05'))

# shadPS4 supplies external mounts directly and does not emulate the HEN kernel service.
import configparser
ini_path = profile / ('data/scummvm-vulkan/scummvm.ini' if args.vulkan else 'data/scummvm/scummvm.ini')
ini_path.parent.mkdir(parents=True, exist_ok=True)
ini = configparser.ConfigParser(interpolation=None)
ini.optionxform = str  # Game-specific keys can be case-sensitive.
if ini_path.exists(): ini.read(ini_path)
if not ini.has_section('scummvm'): ini.add_section('scummvm')
ini.set('scummvm', 'ps4_usb_sandbox', 'false')
with ini_path.open('w') as out: ini.write(out)
