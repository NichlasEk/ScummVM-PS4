#!/usr/bin/env python3
"""Restore codegen/config steps missing from the pinned upstream host build."""
from pathlib import Path
import subprocess
import sys

p = Path(sys.argv[1]).resolve()

def generate(script, *args, output=None):
    result = subprocess.run([sys.executable, script, *args], cwd=p, check=True,
                            stdout=subprocess.PIPE if output else None)
    if output:
        (p / output).write_bytes(result.stdout)

# Linux/x86-64 host only; the upstream Orbis config remains separate.
s = (p / 'config.orbis.mak').read_text().split('# OpenOrbis toolchain')[0]
s += '''
CC=clang
CXX=clang++
LD=clang++
AR=llvm-ar
PYTHON=python3
CFLAGS=-std=gnu11 -O2 $(SHARED_FLAGS) -D_GNU_SOURCE -DHAVE_SYSCONF=1
CXXFLAGS=-std=c++17 -O2 $(SHARED_FLAGS) -D_GNU_SOURCE -DHAVE_SYSCONF=1
CFLAGS+=-DBLAKE3_NO_SSE2 -DBLAKE3_NO_SSE41 -DBLAKE3_NO_AVX2 -DBLAKE3_NO_AVX512 -DBLAKE3_USE_NEON=0
LDFLAGS=-lm -lpthread
'''
(p / 'config.mak').write_text(s)
for flag, filename in [('--enums', 'u_format_gen.h'), ('--header', 'u_format_pack.h'), ('', 'u_format_table.c')]:
    generate('src/util/format/u_format_table.py', 'src/util/format/u_format.yaml',
             *([flag] if flag else []), output='src/util/format/' + filename)
generate('src/compiler/builtin_types_h.py', 'src/compiler/builtin_types.h')
generate('src/compiler/builtin_types_c.py', 'src/compiler/builtin_types.c')
generate('src/util/process_shader_stats.py', 'src/util/shader_stats.rnc',
         'src/util/shader_stats.xml', output='src/util/shader_stats.h')
generate('src/vulkan/util/vk_struct_type_cast_gen.py', '--xml',
         '../Vulkan-Headers/registry/vk.xml', '--out', 'src/vulkan/util/vk_struct_type_cast.h', '--beta', 'false')
for gpu in ['gfx11', 'gfx12']:
    generate('src/amd/packets/parse_cp_pm4_table_data_json.py',
             'src/amd/packets/cp_pm4_table_data_gfx11.json', 'src/amd/packets/pm4_it_opcodes_gfx11.h',
             'src/amd/packets/cp_pm4_table_data_gfx12.json', 'src/amd/packets/pm4_it_opcodes_gfx12.h',
             gpu, 'packets_h', output='src/amd/common/amd_cp_packets_' + gpu + '.h')
generate('src/util/format_srgb.py', output='src/util/format_srgb.c')
generate('src/amd/common/gfx10_format_table.py', 'src/util/format/u_format.yaml',
         'src/amd/registers/gfx10-rsrc.json', 'src/amd/registers/gfx11-rsrc.json',
         output='src/amd/common/gfx10_format_table.c')
