#!/usr/bin/env python3
"""Embed ScummVM shaders, compatibility macros and required HPL1 extensions."""
from pathlib import Path
import ast, hashlib, json, re, sys, subprocess, tempfile
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
for stage, prefix in (('vertex',vs),('fragment',fs)):
    text=prefix+(source/f'engines/grim/shaders/grim_actor.{stage}').read_text()
    parts.append(f'static const char legacy_grim_actor_{stage}[] = {json.dumps(text)};\n')
    manifest[f'grim_actor_{stage}']=hashlib.sha256(text.encode()).hexdigest()
grim_pairs=[]
for vertex in sorted((source/'engines/grim/shaders').glob('grim_*.vertex')):
    name=vertex.stem
    for stage,prefix in (('vertex',vs),('fragment',fs)):
        if name=='grim_actor': continue
        text=prefix+vertex.with_suffix('.'+stage).read_text()
        parts.append(f'static const char legacy_{name}_{stage}[] = {json.dumps(text)};\n')
    grim_pairs.append(f'{{"{name}",legacy_{name}_vertex,legacy_{name}_fragment}}')
parts.append('static const struct { const char *name,*vertex,*fragment; } grim_pairs[] = {'+','.join(grim_pairs)+'};\n')
parts.append('static const char legacy_color[] = "#version 100\\nprecision mediump float;struct TestParams { bool enabled; vec3 tint; vec2 yBounds; };uniform TestParams settings;uniform vec4 color;uniform int mode;uniform int modes[2];void main(){gl_FragColor=(settings.enabled && mode==-7 && modes[0]==3 && modes[1]==-11 && gl_FragCoord.x>0.0 && gl_FragCoord.y>settings.yBounds.x && gl_FragCoord.y<settings.yBounds.y)?color*vec4(settings.tint,1.0):vec4(0.0);}";\n')
# Test exactly the HPL1 source patch used by the isolated PS4 overlay.
patch=root/'patches/scummvm/hpl1-experimental.patch'
patched_hpl={}
with tempfile.TemporaryDirectory(prefix='ps4gl-hpl-shaders-') as directory:
    work=Path(directory)
    names={line[6:] for line in patch.read_text().splitlines() if line.startswith('+++ b/')}
    for name in names:
        destination=work/name
        destination.parent.mkdir(parents=True,exist_ok=True)
        destination.write_bytes((source/name).read_bytes())
    subprocess.run(['patch','--batch','--silent','--fuzz=0','-p1','-d',str(work),'-i',str(patch)],check=True)
    patched_hpl={name:(work/name).read_text() for name in names}
for label,vertex_name,fragment_name in (('gamma','hpl1_gamma_correction','hpl1_gamma_correction'),('fog','hpl1_Fog_Solid','hpl1_Fog_Solid'),('refract','hpl1_refract','hpl1_refract_special'),('cube','hpl1_Diffuse_Light','hpl1_Diffuse_Light'),('ambient','hpl1_Diffuse_Color','hpl1_Ambient_Color')):
    for stage,macro in (('vertex','compatVertex'),('fragment','compatFragment')):
        name=vertex_name if stage=='vertex' else fragment_name
        path=source/f'engines/hpl1/engine/impl/shaders/{name}.{stage}'
        body=patched_hpl.get(str(path.relative_to(source)),path.read_text())
        text='#version 120\n'+preamble(macro)+preamble('compatUniformBool')+body
        parts.append(f'static const char legacy_hpl_{label}_{stage}[] = {json.dumps(text)};\n')
        manifest[f'hpl_{label}_{stage}']=hashlib.sha256(text.encode()).hexdigest()
(out/'legacy-sources.h').write_text(''.join(parts))
(out/'legacy-sources.json').write_text(json.dumps(manifest,indent=2)+'\n')
# The package carries the shader source used for reproduction and its notices.
license_text=(source/'COPYING').read_text()+'\n'+(source/'COPYRIGHT').read_text()
(out/'scummvm-shaders.txt').write_text(license_text+'\n'+''.join(parts))
