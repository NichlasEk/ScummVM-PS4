#!/usr/bin/env python3
"""Embed original ScummVM shader strings and compatibility macros, unchanged."""
from pathlib import Path
import ast, hashlib, json, re, sys
root=Path(__file__).resolve().parents[1]
source=root/'.tools/scummvm-src'
out=Path(sys.argv[1]); out.mkdir(parents=True,exist_ok=True)
compat=(source/'graphics/opengl/shader.cpp').read_text()
def preamble(name):
    matches=re.findall(r'\b'+name+r'\s*=\s*((?:"(?:[^"\\]|\\.)*"\s*)+);',compat,re.S)
    assert len(matches)==(2 if name=='compatUniformBool' else 1)
    return ''.join(ast.literal_eval(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"',matches[-1]))
vs='#version 100\n'+preamble('compatVertex')+preamble('compatUniformBool')
fs='#version 100\n'+preamble('compatFragment')+preamble('compatUniformBool')
bodies={}
for filename in ('shaders.cpp','gfx.cpp','lighting.cpp'):
    raw=(source/'engines/twp'/filename).read_text()
    for name,body in re.findall(r'const\s+char\s*\*\s*(\w+)\s*=\s*R"\((.*?)\)";',raw,re.S): bodies[name]=body
assert set(bodies)=={'vsrc','fragmentSrc','bwShader','ghostShader','sepiaShader','fadeShader','fshader','debug_fshader','vshader'}
parts=[]; manifest={}
for name,body in bodies.items():
    text=(vs if name in ('vsrc','vshader') else fs)+body
    parts.append(f'static const char legacy_{name}[] = {json.dumps(text)};\n')
    manifest[name]=hashlib.sha256(text.encode()).hexdigest()
parts.append('static const char legacy_color[] = "#version 100\\nprecision mediump float;uniform vec4 color;uniform int mode;uniform int modes[2];void main(){gl_FragColor=(mode==-7 && modes[0]==3 && modes[1]==-11)?color:vec4(0.0);}";\n')
(out/'legacy-sources.h').write_text(''.join(parts))
(out/'legacy-sources.json').write_text(json.dumps(manifest,indent=2)+'\n')
# The package carries the shader source used for reproduction and its notices.
license_text=(source/'COPYING').read_text()+'\n'+(source/'COPYRIGHT').read_text()
(out/'scummvm-shaders.txt').write_text(license_text+'\n'+''.join(parts))
