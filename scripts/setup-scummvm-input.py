#!/usr/bin/env python3
"""Seed project-local shadPS4 input settings; retain existing user bindings."""
import json
import shutil
from pathlib import Path

root = Path(__file__).resolve().parent.parent
profile = root / '.tools/xdg/shadPS4'
profile.mkdir(parents=True, exist_ok=True)
config_path = profile / 'config.json'
config = json.loads(config_path.read_text()) if config_path.exists() else {}
config.setdefault('Input', {})['use_unified_input_config'] = False
config_path.write_text(json.dumps(config, indent=2) + '\n')
input_dir = profile / 'input_config'
input_dir.mkdir(exist_ok=True)
bindings = input_dir / 'SCVM00002.ini'
if not bindings.exists():
    shutil.copyfile(root / 'config/emulator/SCVM00002.ini', bindings)
