/* Shared production sources, also compiled by the host shader tests. */
#include <stdio.h>
static const char *ps4gl_fixed_vertex="#version 120\nattribute vec3 position; attribute vec4 color; attribute vec3 uv;\n"
    "uniform mat4 projection; uniform mat4 modelview; varying vec4 tint; varying vec2 texcoord;\n"
    "void main(){gl_Position=projection*(modelview*vec4(position,1.0)); tint=clamp(color,0.0,1.0); texcoord=uv.xy;}\n";
static void ps4glFixedFragment(int textured, char *fs, size_t capacity) {
  const char *prefix="#version 120\nvarying vec4 tint; varying vec2 texcoord; uniform int alphaOp; uniform float alphaRef;\n";
  const char *alpha="if(alphaOp==0 || (alphaOp==1 && !(c.a<alphaRef)) || (alphaOp==2 && !(c.a==alphaRef)) || "
    "(alphaOp==3 && !(c.a<=alphaRef)) || (alphaOp==4 && !(c.a>alphaRef)) || "
    "(alphaOp==5 && !(c.a!=alphaRef)) || (alphaOp==6 && !(c.a>=alphaRef))) discard; gl_FragColor=c;}\n";
  snprintf(fs,capacity,"%s%s void main(){vec4 c=%s; %s",prefix,
    textured ? "uniform sampler2D image; uniform int envRgb; uniform int envAlpha; uniform int envOperand;" : "",
    textured ? "texture2D(image,texcoord); vec3 b=envOperand==0 ? tint.rgb : envOperand==1 ? vec3(1.0)-tint.rgb : envOperand==2 ? vec3(tint.a) : vec3(1.0-tint.a); c.rgb=envRgb==1 ? c.rgb : envRgb==2 ? min(c.rgb+b,vec3(1.0)) : c.rgb*b; c.a=envAlpha==1 ? c.a : envAlpha==2 ? min(c.a+tint.a,1.0) : c.a*tint.a" : "tint",alpha);
}
