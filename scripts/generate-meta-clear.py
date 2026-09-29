#!/usr/bin/env python3
"""Regenerate constant-color meta-clear SPIR-V and checked literal offsets.
Native sources and the generated header are carried by vulkan-runtime.patch.
"""
from pathlib import Path
import subprocess, struct, tempfile
r=Path(__file__).resolve().parent.parent/'.tools/opengnm-stack/vulkan-ps4'
d=r/'shaders'
out=['/* Generated SPIR-V: glslangValidator -V --target-env vulkan1.0 meta_clear.{vert,frag}. */','#pragma once']
for ext in ['vert','frag']:
 dest=Path('/tmp/meta_clear.'+ext+'.spv');subprocess.run(['glslangValidator','-V','--target-env','vulkan1.0',str(d/('meta_clear.'+ext)),'-o',str(dest)],check=True)
 w=list(struct.unpack('<'+'I'*(dest.stat().st_size//4),dest.read_bytes()))
 out.append('static const uint32_t meta_clear_'+ext+'[] = {\n'+','.join(hex(x) for x in w)+'\n};')
 if ext=='frag':
  offsets=[]
  for value in [.125,.25,.5,.75]:
   bits=struct.unpack('<I',struct.pack('<f',value))[0];i=5;found=[]
   while i<len(w):
    n=w[i]>>16;assert n
    if w[i]&65535==43 and n==4 and w[i+3]==bits:found.append(i+3)
    i+=n
   assert len(found)==1;offsets+=found
  out.append('static const unsigned meta_clear_color_offsets[4] = {'+','.join(map(str,offsets))+'};')
(d/'meta_clear_spirv.h').write_text('\n'.join(out)+'\n')
