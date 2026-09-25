#!/usr/bin/env python3
"""Compile PS4 resource fixtures; reject unimplemented layouts, never black stubs."""
import importlib.util
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
compiler = root / '.tools/opengnm-stack/opengnm-psbc'
reports = root / 'artifacts/opengnm/compiler-tests'
reports.mkdir(parents=True, exist_ok=True)
spec = importlib.util.spec_from_file_location('verify_sb', compiler / 'tests/verify_sb.py')
verify = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verify)

def compile_shader(name, stage, source, supported=True):
    glsl = reports / (name + '.glsl')
    spirv = reports / (name + '.spv')
    binary = reports / (name + '.sb')
    glsl.write_text(source)
    binary.unlink(missing_ok=True)
    subprocess.run(['glslc', '-fshader-stage=' + stage, str(glsl), '-o', str(spirv)], check=True)
    result = subprocess.run([str(compiler / 'opengnm-psbc'), '-4', '-s', stage,
                             '-f', str(spirv), '-o', str(binary)], capture_output=True, text=True)
    (reports / (name + '.log')).write_text(result.stdout + result.stderr)
    if supported:
        assert result.returncode == 0, (name, result.stderr)
        assert verify.verify_shader(str(binary), 1 if stage == 'vertex' else 2)
    else:
        assert result.returncode > 0, (name, result.returncode, result.stderr)
        assert 'unsupported resource layout' in result.stderr, (name, result.stderr)
        assert not binary.exists(), name
    print('PASS:', name, 'compiled' if supported else 'explicitly rejected')

triangle = root / '.tools/opengnm-stack/freegnm-examples/triangle/assets/misc'
for stage, suffix in [('vertex', 'vert'), ('fragment', 'frag')]:
    compile_shader('triangle-' + stage, stage, (triangle / ('tri.' + suffix + '.glsl')).read_text())
ubo = (root / 'tests/ps4/opengnm-uniforms.frag').read_text()
compile_shader('two-ubos', 'fragment', ubo)
compile_shader('wrong-set', 'fragment', ubo.replace('binding=0', 'set=1,binding=0'), False)
compile_shader('binding-overflow', 'fragment', ubo.replace('binding=1', 'binding=16'), False)
compile_shader('texture', 'fragment', '''#version 450
layout(binding=0) uniform sampler2D tex;
layout(location=0) out vec4 color;
void main() { color=texture(tex, vec2(.5)); }
''')
compile_shader('mixed-textures-ubos', 'fragment', (root / 'tests/ps4/opengnm-textures.frag').read_text())
compile_shader('texture-wrong-set', 'fragment', '''#version 450
layout(set=1,binding=0) uniform sampler2D tex;
layout(location=0) out vec4 color;
void main() { color=texture(tex, vec2(.5)); }
''', False)
compile_shader('texture-binding-overflow', 'fragment', '''#version 450
layout(binding=16) uniform sampler2D tex;
layout(location=0) out vec4 color;
void main() { color=texture(tex, vec2(.5)); }
''', False)
compile_shader('texture-cube', 'fragment', '''#version 450
layout(binding=0) uniform samplerCube tex;
layout(location=0) out vec4 color;
void main() { color=texture(tex, vec3(1,0,0)); }
''', False)
compile_shader('texture-array', 'fragment', '''#version 450
layout(binding=0) uniform sampler2D tex[2];
layout(location=0) out vec4 color;
void main() { color=texture(tex[1], vec2(.5)); }
''', False)
compile_shader('texture-query', 'fragment', '''#version 450
layout(binding=0) uniform sampler2D tex;
layout(location=0) out vec4 color;
void main() { color=vec4(textureSize(tex, 0), 0, 1); }
''', False)
compile_shader('descriptor-array', 'fragment', '''#version 450
layout(binding=0) uniform Params { vec4 color; } p[2];
layout(location=0) out vec4 color;
void main() { color=p[1].color; }
''', False)
compile_shader('push-constant', 'fragment', '''#version 450
layout(push_constant) uniform Params { vec4 color; } p;
layout(location=0) out vec4 color;
void main() { color=p.color; }
''', False)
compile_shader('storage-buffer', 'compute', (compiler / 'tests/test.comp').read_text(), False)
