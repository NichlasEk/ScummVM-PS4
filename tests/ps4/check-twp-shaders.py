#!/usr/bin/env python3
"""Check real TWP interface contracts after prepare-twp-shaders.py."""
import importlib.util
import json
from pathlib import Path

root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('bridge', root / 'scripts/prepare-twp-shaders.py')
bridge = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bridge)
shaders = json.loads((root / 'artifacts/vulkan/twp-shaders/manifest.json').read_text())['shaders']
assert len(shaders) == 6
vertex = shaders['vsrc']
assert {(v['name'], v['Location'], v['type']) for v in vertex['inputs']} == {
    ('a_position', 0, 'vec2'), ('a_color', 1, 'vec4'), ('a_texCoords', 2, 'vec2')}
outputs = {(v['Location'], v['type']) for v in vertex['outputs']}
for name, shader in shaders.items():
    if shader['stage'] == 'fragment':
        assert {(v['Location'], v['type']) for v in shader['inputs']} <= outputs, name
        assert shader['outputs'][0]['Location'] == 0
    bindings = {d['Binding'] for d in shader['descriptors']}
    assert len(bindings) == len(shader['descriptors']), name
    assert all(d['DescriptorSet'] == 0 for d in shader['descriptors'])


def block(name):
    return next(d for d in shaders[name]['descriptors'] if d['kind'] == 'Struct')

transform = block('vsrc')
assert transform['Binding'] == 0 and transform['block_size'] == 64
assert transform['members'][0] == {'name': 'u_transform', 'ColMajor': True,
    'MatrixStride': 16, 'Offset': 0, 'type': 'mat4', 'size': 64}
ghost = block('ghostShader')
assert ghost['Binding'] == 1 and ghost['block_size'] == 64
assert [m['Offset'] for m in ghost['members']] == [0, 4, 8, 16, 32, 48]
sepia = block('sepiaShader')
assert sepia['block_size'] == 112
assert sepia['members'][1] == {'name': 'RandomValue', 'Offset': 16,
    'ArrayStride': 16, 'count': 5, 'type': 'float', 'size': 80}
assert sepia['members'][2]['Offset'] == 96
fade = block('fadeShader')
assert fade['block_size'] == 16
assert [m['Offset'] for m in fade['members']] == [0, 4, 8, 12]
assert fade['members'][2]['type'] == 'int'
assert {(d['name'], d['Binding']) for d in shaders['fadeShader']['descriptors'] if d['kind'] == 'SampledImage'} == {
    ('u_texture', 2), ('u_texture2', 3)}

# Fail closed for GLSL declarations outside this deliberately bounded bridge.
for source in ('uniform sampler2D u_texture[2];', 'uniform samplerCube cube;',
               'varying vec4 v_color[2];', 'uniform float a, b;'):
    try:
        bridge.translate(source, 'fragment')
    except ValueError:
        pass
    else:
        raise AssertionError('silently accepted: ' + source)
print('PASS TWP stage interfaces, descriptor bindings, std140 matrix/vector/array layouts and rejection checks')
