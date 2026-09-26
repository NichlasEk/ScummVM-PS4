#!/usr/bin/env python3
"""Inventory GL calls against the experimental resolver; not a conformance test."""
import argparse
import hashlib
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent.parent
GROUPS = {
    'Context and SDL startup': [
        'graphics/opengl/context.cpp',
        'backends/graphics/openglsdl/openglsdl-graphics.cpp',
    ],
    'Shared shader helpers': ['graphics/opengl/shader.cpp'],
    'Launcher shader renderer (no fixed pipeline or libretro)': [
        'backends/graphics/opengl/opengl-graphics.cpp',
        'backends/graphics/opengl/texture.cpp',
        'backends/graphics/opengl/framebuffer.cpp',
        'backends/graphics/opengl/shader.cpp',
        'backends/graphics/opengl/pipelines/pipeline.cpp',
        'backends/graphics/opengl/pipelines/shader.cpp',
        'backends/graphics/opengl/pipelines/clut8.cpp',
    ],
    'Thimbleweed Park': ['engines/twp/gfx.cpp'],
}


def code_only(text):
    # Preserve newlines for source locations; ignore comments and quoted text.
    return re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                  lambda m: re.sub(r'[^\n]', ' ', m[0]), text, flags=re.S)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT / '.tools/scummvm-src')
    args = parser.parse_args()
    resolver = (ROOT / 'compat/ps4gl/ps4gl.c').read_text()
    exports = set(re.findall(r'\bPROC\((gl\w+)\)', code_only(resolver)))
    print('# ScummVM / PS4GL call inventory\n')
    print('Static source inventory, including conditional branches. An exported name does')
    print('not imply support for every enum, format, lifetime or call sequence. This does')
    print('not execute GLAD, SDL startup, the launcher or a game.\n')
    print(f'Resolver exports: {len(exports)}. Source root: `{args.source.resolve()}`.\n')
    for title, files in GROUPS.items():
        calls = {}
        digest = hashlib.sha256()
        for relative in files:
            path = args.source / relative
            raw = path.read_bytes()  # Missing source must fail, never imply coverage.
            digest.update(relative.encode() + b'\0' + raw + b'\0')
            code = code_only(raw.decode())
            for match in re.finditer(r'\b(gl[A-Z]\w*)\s*\(', code):
                line = code.count('\n', 0, match.start()) + 1
                calls.setdefault(match[1], []).append(f'{relative}:{line}')
        missing = sorted(calls.keys() - exports)
        print(f'## {title}\n')
        print(f'{len(calls)} distinct call names; {len(missing)} missing from resolver.\n')
        print(f'Input SHA-256: `{digest.hexdigest()}`.\n')
        print('Missing calls:')
        for name in missing:
            print(f'- `{name}` — `{calls[name][0]}`')
        if not missing:
            print('- None in these files; semantic restrictions still apply.')
        print('\nExported calls used here: ' + ', '.join(f'`{n}`' for n in sorted(calls.keys() & exports)) + '.\n')
    print('See docs/PS4-SCUMMVM-GL-STARTUP.md for semantic and lifecycle blockers.')


if __name__ == '__main__':
    main()
