#!/usr/bin/env python3
"""Offline, bounded TWP GLSL bridge. Not a general GL implementation.

Extract original shader bodies, assign a shared interface, compile SPIR-V,
reflect compiler-assigned std140 offsets, and check native GCN compilation.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
ATTRIBUTES = {'a_position': 0, 'a_color': 1, 'a_texCoords': 2}
VARYINGS = {'v_color': 0, 'v_texCoords': 1}
SAMPLERS = {'u_texture': 2, 'u_texture2': 3}
DECL = re.compile(r'\b(uniform|attribute|varying)\s+(\w+)\s+(\w+)(\s*\[\s*(\d+)\s*\])?\s*;')


def translate(source, stage):
    # This deliberately handles only the simple declarations used in these sources.
    source = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)
    uniforms = []

    def declaration(match):
        kind, typ, name, array, count = match.groups()
        if kind == 'uniform':
            if typ == 'sampler2D':
                if array or name not in SAMPLERS:
                    raise ValueError('unsupported sampler declaration: ' + match[0])
                return f'layout(set=0,binding={SAMPLERS[name]}) uniform sampler2D {name};'
            if typ not in ('float', 'int', 'vec2', 'vec3', 'vec4', 'mat4'):
                raise ValueError('unsupported uniform type: ' + typ)
            uniforms.append((typ, name, int(count) if count else None))
            return ''
        if array:
            raise ValueError('interface arrays unsupported')
        locations = ATTRIBUTES if kind == 'attribute' else VARYINGS
        direction = 'in' if kind == 'attribute' or stage == 'fragment' else 'out'
        if kind == 'attribute' and stage != 'vertex':
            raise ValueError('fragment attribute')
        return f'layout(location={locations[name]}) {direction} {typ} {name};'

    body = DECL.sub(declaration, source)
    if re.search(r'\b(uniform|attribute|varying)\b', body.replace('uniform sampler2D', '')):
        raise ValueError('unparsed interface declaration')
    body = re.sub(r'\btexture2D\b', 'texture', body)
    body = re.sub(r'\bgl_FragColor\b', 'twp_fragColor', body)
    header = '#version 450\n'
    if uniforms:
        binding = 0 if stage == 'vertex' else 1
        header += f'layout(std140,set=0,binding={binding}) uniform TwpUniforms {{\n'
        for typ, name, count in uniforms:
            header += f'    {typ} {name}' + (f'[{count}]' if count else '') + ';\n'
        header += '};\n'
    if stage == 'fragment':
        header += 'layout(location=0) out vec4 twp_fragColor;\n'
    return header + body


def reflect(assembly):
    """Read decorations and type IDs from spirv-dis --raw-id output, not guesses."""
    names, member_names, decorations, members, types, constants, variables = {}, {}, {}, {}, {}, {}, {}
    for line in assembly.splitlines():
        line = line.strip()
        m = re.fullmatch(r'OpName %(\d+) "([^"]+)"', line)
        if m:
            names[int(m[1])] = m[2]
        m = re.fullmatch(r'OpMemberName %(\d+) (\d+) "([^"]+)"', line)
        if m:
            member_names[(int(m[1]), int(m[2]))] = m[3]
        m = re.fullmatch(r'OpDecorate %(\d+) (\w+)(?: (\d+))?', line)
        if m:
            decorations.setdefault(int(m[1]), {})[m[2]] = int(m[3]) if m[3] else True
        m = re.fullmatch(r'OpMemberDecorate %(\d+) (\d+) (\w+)(?: (\d+))?', line)
        if m:
            members.setdefault((int(m[1]), int(m[2])), {})[m[3]] = int(m[4]) if m[4] else True
        m = re.fullmatch(r'%(\d+) = OpType(\w+)(.*)', line)
        if m:
            types[int(m[1])] = (m[2], m[3].split())
        m = re.fullmatch(r'%(\d+) = OpConstant %\d+ (\d+)', line)
        if m:
            constants[int(m[1])] = int(m[2])
        m = re.fullmatch(r'%(\d+) = OpVariable %(\d+) (\w+)', line)
        if m:
            variables[int(m[1])] = (int(m[2]), m[3])
    def layout(tid):
        kind, args = types[tid]
        if kind in ('Float', 'Int') and args[0] == '32':
            return {'type': 'float' if kind == 'Float' else ('int' if args[1] == '1' else 'uint'), 'size': 4}
        if kind == 'Vector':
            return {'type': 'vec' + args[1], 'size': int(args[1]) * 4}
        if kind == 'Matrix':
            return {'type': 'mat' + args[1], 'size': int(args[1]) * 16}
        if kind == 'Array':
            element = layout(int(args[0][1:]))
            stride = decorations[tid]['ArrayStride']
            count = constants[int(args[1][1:])]
            return {'type': element['type'], 'size': count * stride,
                    'ArrayStride': stride, 'count': count}
        raise ValueError('unsupported reflected type: ' + kind)

    result = {'inputs': [], 'outputs': [], 'descriptors': []}
    for ident, (pointer, storage) in variables.items():
        dec = decorations.get(ident, {})
        entry = {'name': names.get(ident, ''), **dec}
        if storage in ('Input', 'Output') and 'Location' in dec:
            entry.update(layout(int(types[pointer][1][1][1:])))
            result['inputs' if storage == 'Input' else 'outputs'].append(entry)
        if 'Binding' not in dec:
            continue
        kind, args = types[int(types[pointer][1][1][1:])]
        entry['kind'] = kind
        if kind == 'Struct':
            struct_id = int(types[pointer][1][1][1:])
            entry['members'] = []
            for index, type_id in enumerate(args):
                tid = int(type_id[1:])
                member = {'name': member_names[(struct_id, index)], **members[(struct_id, index)]}
                member.update(layout(tid))
                entry['members'].append(member)
            end = max(m['Offset'] + m['size'] for m in entry['members'])
            entry['block_size'] = (end + 15) & ~15
        result['descriptors'].append(entry)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT / '.tools/scummvm-src')
    parser.add_argument('--output', type=Path, default=ROOT / 'artifacts/vulkan/twp-shaders')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / 'manifest.json').unlink(missing_ok=True)
    compiler = ROOT / '.tools/opengnm-stack/opengnm-psbc'
    spec = importlib.util.spec_from_file_location('verify_sb', compiler / 'tests/verify_sb.py')
    verifier = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(verifier)
    sources = {}
    for filename in ('shaders.cpp', 'gfx.cpp'):
        path = args.source / 'engines/twp' / filename
        raw = path.read_bytes()
        bodies = re.findall(r'const\s+char\s*\*\s*(\w+)\s*=\s*R"\((.*?)\)";', raw.decode(), re.S)
        for name, body in bodies:
            if name in sources:
                raise ValueError('duplicate shader: ' + name)
            sources[name] = (body, str(path.relative_to(args.source)), hashlib.sha256(raw).hexdigest())
    expected = {'vsrc', 'fragmentSrc', 'bwShader', 'ghostShader', 'sepiaShader', 'fadeShader'}
    if set(sources) != expected:
        raise ValueError(f'shader inventory changed: {set(sources)}')
    manifest = {'scope': 'offline translation and host GCN compilation; no game or GPU execution', 'shaders': {}}
    for name, (body, origin, digest) in sources.items():
        stage = 'vertex' if name == 'vsrc' else 'fragment'
        glsl = args.output / (name + '.glsl')
        spv = args.output / (name + '.spv')
        glsl.write_text(translate(body, stage))
        subprocess.run(['glslc', '-fshader-stage=' + stage, str(glsl), '-o', str(spv)], check=True)
        subprocess.run(['spirv-val', '--target-env', 'vulkan1.0', str(spv)], check=True)
        assembly = subprocess.check_output(['spirv-dis', '--raw-id', str(spv)], text=True)
        (args.output / (name + '.spvasm')).write_text(assembly)
        reflection = reflect(assembly)
        for target, suffix in (('-4', ''), ('-n', '.neo')):
            sb = args.output / (name + suffix + '.sb')
            sb.unlink(missing_ok=True)
            result = subprocess.run([str(compiler / 'opengnm-psbc'), target, '-s', stage,
                                     '-f', str(spv), '-o', str(sb)], capture_output=True, text=True)
            log = args.output / (name + suffix + '.log')
            log.write_text(result.stdout + result.stderr)
            if result.returncode or not verifier.verify_shader(str(sb), 1 if stage == 'vertex' else 2):
                raise RuntimeError(f'{name}: GCN compile/validation failed; see {log}')
        manifest['shaders'][name] = {'stage': stage, 'source': origin, 'source_sha256': digest,
                                   'spirv_sha256': hashlib.sha256(spv.read_bytes()).hexdigest(), **reflection}
        print('PASS', name, 'SPIR-V, reflection and base/Neo GCN binary validation')
    (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')


if __name__ == '__main__':
    main()
