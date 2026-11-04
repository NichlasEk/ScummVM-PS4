#!/usr/bin/env python3
"""Capture a bounded run using the checkout-local emulator launcher/profile."""
from pathlib import Path
import os
import sys

root = Path(__file__).resolve().parents[2]
os.environ.update(SDL_VIDEODRIVER='x11', SDL_VIDEO_DRIVER='x11')
os.execv(sys.executable, [sys.executable, str(root / 'scripts/run-scummvm-shadps4.py'),
                         *sys.argv[1:], '--timeout', '30', '--screenshot'])
