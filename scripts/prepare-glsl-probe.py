#!/usr/bin/env python3
"""Embed text, not SPIR-V, for target-side compilation through glslang."""
from pathlib import Path
import json
import sys
root=Path(__file__).resolve().parents[1]
parts=[]
for name,filename in [('vertex','glfbo_vert'),('color','glblend_frag'),('texture','glfbo_frag')]:
    source=(root/'tests/ps4/vulkan'/f'{filename}.glsl').read_text()
    parts.append(f'static const char glsl_{name}[] = {json.dumps(source)};\n')
(Path(sys.argv[1])/'glsl-sources.h').write_text(''.join(parts))

# Preserve the separate C interface attribution as well as the aggregate license.
source=root/'.tools/glslang'
notices=(source/'LICENSE.txt').read_text()
notices+='\nC interface attribution:\n'+(source/'glslang/CInterface/glslang_c_interface.cpp').read_text().split('#include',1)[0]
(root/'.tools/opengnm-stack/vulkan-ps4/glslang-LICENSE.txt').write_text(notices)
