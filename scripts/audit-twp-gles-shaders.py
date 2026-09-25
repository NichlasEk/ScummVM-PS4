#!/usr/bin/env python3
"""Export and validate the local TWP GLSL ES inputs, not PS4 shader binaries.

Uses ScummVM's actual compatibility preamble and raw literals. Requires
glslangValidator; never treats SPIR-V or host binaries as Piglet binaries.
"""
import argparse
import ast
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT / '.tools/scummvm-src')
    parser.add_argument('--output', type=Path, default=ROOT / 'artifacts/opengl/twp-shaders')
    args = parser.parse_args()
    compat = (args.source / 'graphics/opengl/shader.cpp').read_text()

    def preamble(name):
        # Uniform bool has an Amiga-specific branch; select the normal branch.
        matches = re.findall(r'\b' + name + r'\s*=\s*((?:"(?:[^"\\]|\\.)*"\s*)+);', compat, re.S)
        if len(matches) != (2 if name == 'compatUniformBool' else 1):
            raise ValueError('Compatibility source changed: ' + name)
        strings = re.findall(r'"(?:[^"\\]|\\.)*"', matches[-1])
        if not strings:
            raise ValueError('Missing compatibility strings: ' + name)
        return ''.join(ast.literal_eval(s) for s in strings)

    prefixes = {stage: '#version 100\n' + preamble(name) + preamble('compatUniformBool')
                for stage, name in [('vert', 'compatVertex'), ('frag', 'compatFragment')]}
    sources = {}
    for filename in ('shaders.cpp', 'gfx.cpp', 'hud.cpp', 'lighting.cpp'):
        text = (args.source / 'engines/twp' / filename).read_text()
        for name, source in re.findall(r'const\s+char\s*\*\s*(\w+)\s*=\s*R"\((.*?)\)";', text, re.S):
            sources[filename + ':' + name] = source
    vertex = 'shaders.cpp:vsrc'
    pairs = {
        'default': (vertex, 'gfx.cpp:fragmentSrc'),
        'black-white': (vertex, 'shaders.cpp:bwShader'),
        'ghost': (vertex, 'shaders.cpp:ghostShader'),
        'sepia': (vertex, 'shaders.cpp:sepiaShader'),
        'fade': (vertex, 'shaders.cpp:fadeShader'),
        'hud': ('hud.cpp:v_source', 'hud.cpp:f_source'),
        'lighting': ('lighting.cpp:vshader', 'lighting.cpp:fshader'),
        'lighting-debug': ('lighting.cpp:vshader', 'lighting.cpp:debug_fshader'),
    }
    expected = {s for pair in pairs.values() for s in pair}
    if set(sources) != expected:
        raise ValueError('Shader inventory changed: ' + repr(set(sources) ^ expected))
    args.output.mkdir(parents=True, exist_ok=True)
    report = {'kind': 'GLSL ES 1.00 source audit; NOT PS4 binaries or runtime validation',
              'validator': subprocess.check_output(['glslangValidator', '--version'], text=True),
              'programs': []}
    for name, pair in pairs.items():
        entry = {'name': name, 'stages': []}
        paths = []
        for stage, key in zip(('vert', 'frag'), pair):
            source = (prefixes[stage] + sources[key]).encode()
            path = args.output / (name + '.' + stage)
            path.write_bytes(source)
            paths.append(str(path))
            entry['stages'].append({'stage': stage, 'origin': key, 'file': path.name,
                                    'sha256': hashlib.sha256(source).hexdigest()})
        result = subprocess.run(['glslangValidator', '-l', *paths], text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        entry.update(passed=result.returncode == 0, log=result.stdout)
        report['programs'].append(entry)
        print(('PASS' if entry['passed'] else 'FAIL') + ' ' + name)
        if not entry['passed']:
            print(result.stdout)
    (args.output / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n')
    raise SystemExit(0 if all(p['passed'] for p in report['programs']) else 1)


if __name__ == '__main__':
    main()
