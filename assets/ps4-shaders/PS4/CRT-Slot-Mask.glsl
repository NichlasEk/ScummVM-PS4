// SPDX-License-Identifier: GPL-3.0-or-later
// Staggered RGB slots and horizontal phosphor gaps.
#version 120
#if defined(VERTEX)
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
c *= 0.78 + 0.22 * cos(6.2831853 * (uv.y*TextureSize.y-0.5));
float col=mod(floor(gl_FragCoord.x),3.0); vec3 mask=vec3(0.80); if(col<1.0) mask.r=1.0; else if(col<2.0) mask.g=1.0; else mask.b=1.0; c*=mask;
float slot=mod(floor(gl_FragCoord.y)+mod(floor(gl_FragCoord.x/3.0),2.0)*2.0,4.0); c*=mix(0.70,1.0,step(1.0,slot));
 gl_FragColor=vec4(clamp(c,0.0,1.0),1.0);
}
#endif
