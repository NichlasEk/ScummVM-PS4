#!/usr/bin/env python3
"""Build a deterministic shader pack consumed by ScummVM's shader browser."""
from pathlib import Path
import sys
import zipfile

source = Path(__file__).resolve().parents[1] / 'assets/ps4-shaders'
with zipfile.ZipFile(sys.argv[1], 'w', compression=zipfile.ZIP_DEFLATED) as archive:
    for path in sorted(source.rglob('*')):
        if path.is_file():
            info = zipfile.ZipInfo(path.relative_to(source).as_posix(), (2026, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, path.read_bytes())
