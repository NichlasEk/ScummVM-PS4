#!/usr/bin/env python3
"""Compile validated SPIR-V; the native PS4 probe compiles it to GCN at runtime."""
from pathlib import Path
import struct
import subprocess
import sys

source = Path(__file__).resolve().parent.parent / 'tests/ps4/vulkan'
target = Path(sys.argv[1])
target.mkdir(parents=True, exist_ok=True)
arrays = ['/* Generated SPIR-V, not precompiled GCN. */\n#include <stdint.h>\n']
if '--composite' in sys.argv:
    names = ('background_vert', 'composite_base_frag', 'composite_overlay_frag', 'composite_sample_frag')
else:
    names = ('background_vert', 'background_frag', 'triangle_vert', 'triangle_frag')
for name in names:
    binary = target / (name + '.spv')
    stage = 'vertex' if name.endswith('_vert') else 'fragment'
    filename = name
    if name.startswith('triangle') and ('--resources' in sys.argv or '--upload' in sys.argv):
        filename = name.replace('triangle', 'resources')
    subprocess.run(['glslc', '-fshader-stage=' + stage, str(source / (filename + '.glsl')),
                    '-o', str(binary)], check=True)
    subprocess.run(['spirv-val', str(binary)], check=True)
    data = binary.read_bytes()
    words = struct.unpack('<' + 'I' * (len(data) // 4), data)
    arrays.append('static const uint32_t ' + name + '[] = {\n' +
                  ',\n'.join(','.join(f'0x{w:08x}' for w in words[i:i+8])
                             for i in range(0, len(words), 8)) + '\n};\n')
(target / 'shaders.h').write_text(''.join(arrays))
