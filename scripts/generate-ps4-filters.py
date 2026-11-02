#!/usr/bin/env python3
"""Generate the port's original, bounded single-pass GLSL 1.20 filter collection."""
from pathlib import Path
import json
root = Path(__file__).resolve().parents[1]
out = root / 'assets/ps4-shaders/PS4'
out.mkdir(parents=True, exist_ok=True)
vertex = '''#if defined(VERTEX)
attribute vec4 VertexCoord;
attribute vec4 TexCoord;
uniform mat4 MVPMatrix;
varying vec2 ps4TexCoord;
void main() { gl_Position = MVPMatrix * VertexCoord; ps4TexCoord = TexCoord.xy; }
#elif defined(FRAGMENT)
uniform sampler2D Texture;
uniform vec2 TextureSize;
uniform vec2 InputSize;
uniform vec2 OutputSize;
varying vec2 ps4TexCoord;
vec3 sampleAt(vec2 uv) { return texture2D(Texture, clamp(uv, vec2(0.5)/TextureSize, (InputSize-vec2(0.5))/TextureSize)).rgb; }
float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }
vec3 softAt(vec2 uv) {
 vec2 d=vec2(1.0)/TextureSize;
 return sampleAt(uv)*0.5 + (sampleAt(uv+vec2(d.x,0.0))+sampleAt(uv-vec2(d.x,0.0))+sampleAt(uv+vec2(0.0,d.y))+sampleAt(uv-vec2(0.0,d.y)))*0.125;
}
void main() {
 vec2 uv=ps4TexCoord;
 vec2 screen=uv*TextureSize/InputSize;
 vec3 c=sampleAt(uv);
'''
filters = []
def add(name, category, description, body, linear=False):
 filters.append(dict(name=name,category=category,description=description,linear=linear))
 (out/(name+'.glsl')).write_text('// SPDX-License-Identifier: GPL-3.0-or-later\n// '+description+'\n#version 120\n'+vertex+body+'\n gl_FragColor=vec4(clamp(c,0.0,1.0),1.0);\n}\n#endif\n')
 (out/(name+'.glslp')).write_text('shaders = 1\nshader0 = '+name+'.glsl\nfilter_linear0 = '+str(linear).lower()+'\nwrap_mode0 = clamp_to_edge\nscale_type0 = viewport\nscale0 = 1.0\n')
scan='c *= 0.78 + 0.22 * cos(6.2831853 * (uv.y*TextureSize.y-0.5));\n'
mask='float col=mod(floor(gl_FragCoord.x),3.0); vec3 mask=vec3(0.80); if(col<1.0) mask.r=1.0; else if(col<2.0) mask.g=1.0; else mask.b=1.0; c*=mask;\n'
curve='''vec2 q=(screen-0.5)*2.0;
 q*=1.0+0.085*dot(q,q);
 if (abs(q.x)>1.0 || abs(q.y)>1.0) { gl_FragColor=vec4(0.0,0.0,0.0,1.0); return; }
 uv=(q*0.5+0.5)*InputSize/TextureSize;
 c=sampleAt(uv);
'''
add('CRT-Soft','CRT','Soft phosphor image with smooth scanlines.', 'c=softAt(uv);\n'+scan,True)
add('CRT-Curved','CRT','Curved tube with scanlines and dark corners.',curve+scan+'c*=1.0-0.16*dot(q,q);',True)
add('CRT-Glow','CRT','Bright phosphors bloom into neighbouring pixels.', 'vec3 glow=softAt(uv); c+=max(glow-vec3(0.35),vec3(0.0))*0.32;\n'+scan,True)
add('CRT-Aperture','CRT','RGB aperture grille with smooth scanlines.', scan+mask)
add('CRT-Slot-Mask','CRT','Staggered RGB slots and horizontal phosphor gaps.',scan+mask+'float slot=mod(floor(gl_FragCoord.y)+mod(floor(gl_FragCoord.x/3.0),2.0)*2.0,4.0); c*=mix(0.70,1.0,step(1.0,slot));')
add('CRT-Shadow-Mask','CRT','Staggered triad dot mask, softer than the slot mask.', 'vec2 p=gl_FragCoord.xy; p.x+=mod(floor(p.y/2.0),2.0); float dotMask=step(1.0,mod(floor(p.x)+floor(p.y),3.0)); c*=mix(0.72,1.0,dotMask);\n'+scan)
add('CRT-Warm','CRT','Warm television whites with horizontal colour softness.', 'c=mix(c,softAt(uv),0.4)*vec3(1.06,1.0,0.88);\n'+scan,True)
add('CRT-Curved-Glow','CRT','Curved tube with phosphor bloom and aperture grille.',curve+'c+=max(softAt(uv)-vec3(0.4),vec3(0.0))*0.3;\n'+scan+mask,True)
add('Monitor-Green','Monochrome','Green phosphor monochrome monitor.', 'c=vec3(0.13,1.0,0.28)*luma(c);\n'+scan)
add('Monitor-Amber','Monochrome','Amber phosphor monochrome monitor.', 'c=vec3(1.0,0.64,0.12)*luma(c);\n'+scan)
add('Monitor-White','Monochrome','Cool white monochrome phosphor display.', 'c=vec3(0.88,0.94,1.0)*luma(c);\n'+scan)
grid='vec2 cell=fract(uv*TextureSize); float grid=step(0.12,cell.x)*step(0.12,cell.y); c*=mix(0.62,1.0,grid);'
add('LCD-Grid','LCD','Visible pixel-cell borders.',grid)
add('LCD-RGB','LCD','RGB subpixels and pixel-cell borders.',mask+grid)
add('LCD-Handheld-Green','LCD','Four olive-green shades and a handheld pixel grid.', 'float v=floor(luma(c)*3.0+0.5)/3.0; c=mix(vec3(0.06,0.19,0.06),vec3(0.70,0.79,0.28),v);'+grid)
add('LCD-Handheld-Gray','LCD','Four neutral shades and a handheld pixel grid.', 'float v=floor(luma(c)*3.0+0.5)/3.0; c=vec3(0.13+0.77*v);'+grid)
add('LCD-Backlit','LCD','Cool backlit display with a lifted black level.', 'c=vec3(0.04,0.06,0.09)+c*vec3(0.88,0.91,0.91);'+grid)
add('Scale-Smooth','Scaling','Bilinear image scaling.', '',True)
add('Scale-Sharp-Bilinear','Scaling','Keeps pixel interiors crisp and blends their edges.', '''vec2 p=uv*TextureSize; vec2 f=fract(p);
 vec2 width=min(vec2(0.5),InputSize/OutputSize*0.65);
 vec2 t=smoothstep(vec2(0.5)-width,vec2(0.5)+width,f);
 c=sampleAt((floor(p)+t)/TextureSize);''',True)
add('Scale-Soft','Scaling','Gentle five-tap image softening.', 'c=softAt(uv);',True)
add('Scale-Sharpen','Scaling','Mild unsharp masking with bounded contrast.', 'c=c+(c-softAt(uv))*0.55;',True)
add('Video-Composite','Analog','Horizontal chroma bleed and softened composite video.', '''vec2 d=vec2(1.5/TextureSize.x,0.0); float y=luma(c);
 vec3 spread=sampleAt(uv-d)*0.25+c*0.5+sampleAt(uv+d)*0.25;
 c=vec3(y)+(spread-vec3(luma(spread)))*0.85;''',True)
add('Video-RGB-Fringe','Analog','Subtle horizontal RGB channel misalignment.', 'vec2 d=vec2(0.65/TextureSize.x,0.0); c=vec3(sampleAt(uv+d).r,c.g,sampleAt(uv-d).b);',True)
add('Video-Tape','Analog','Static tape-style chroma bleed, grain and edge tracking.', '''float y=screen.y; float band=smoothstep(0.91,1.0,y);
 uv.x+=sin(y*90.0)*band*0.003;
 vec2 d=vec2(1.2/TextureSize.x,0.0);
 c=vec3(sampleAt(uv+d).r,sampleAt(uv).g,sampleAt(uv-d).b);
 float noise=fract(sin(dot(floor(uv*TextureSize),vec2(12.9898,78.233)))*43758.5453);
 c=c*0.92+(noise-0.5)*0.045;''',True)
add('Color-Grayscale','Color','Neutral grayscale conversion.', 'c=vec3(luma(c));')
add('Color-Sepia','Color','Warm sepia photographic tint.', 'c=vec3(dot(c,vec3(0.393,0.769,0.189)),dot(c,vec3(0.349,0.686,0.168)),dot(c,vec3(0.272,0.534,0.131)));')
add('Color-Vivid','Color','Moderate saturation and contrast boost.', 'c=mix(vec3(luma(c)),c,1.25); c=(c-0.5)*1.08+0.5;')
add('Color-Poster','Color','Three-bit colour channels for a posterized retro palette.', 'c=floor(c*7.0+0.5)/7.0;')
add('Color-Dither','Color','Ordered 4-level colour quantization using a 2x2 dither.', '''vec2 p=mod(floor(uv*TextureSize),2.0);
 float threshold=(p.y<1.0 ? (p.x<1.0 ? 0.0:2.0):(p.x<1.0 ? 3.0:1.0));
 c=floor(c*3.0+(threshold+0.5)/4.0)/3.0;''')
# Preserve the two original presets and their saved configuration paths.
filters[:0]=[dict(name='Scanlines',category='CRT',description='Crisp source-row scanlines.',linear=False),dict(name='CRT-Light',category='CRT',description='Scanlines and a light RGB mask.',linear=False)]
(root/'assets/ps4-shaders/catalog.json').write_text(json.dumps(filters,indent=2)+'\n')
print('Generated catalog:',len(filters),'presets')
header = '// Generated by scripts/generate-ps4-filters.py. GPL-3.0-or-later.\n'
header += 'static const char *const kPS4FilterPresets[] = {\n'
header += ''.join('    "PS4/'+f['name']+'.glslp",\n' for f in filters)
header += '};\n'
(root/'compat/ps4gl/scummvm-filter-presets.inc').write_text(header)
