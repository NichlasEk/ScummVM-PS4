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
  const char *env="uniform sampler2D image; uniform int envRgb; uniform int envAlpha; uniform vec4 envColor;"
    "uniform vec3 envSources; uniform vec3 envAlphaSources; uniform vec3 envOperands; uniform vec3 envAlphaOperands;"
    "vec4 source(float s,vec4 t){return s<0.5 ? t : s<1.5 ? envColor : tint;}"
    "vec3 operand(float o,vec4 v){return o<0.5 ? v.rgb : o<1.5 ? vec3(1.0)-v.rgb : o<2.5 ? vec3(v.a) : vec3(1.0-v.a);}"
    "float alphaArg(float s,float o,vec4 t){float a=source(s,t).a; return o<0.5 ? a : 1.0-a;}";
  const char *combine="texture2D(image,texcoord);"
    "vec3 a=operand(envOperands.x,source(envSources.x,c)),b=operand(envOperands.y,source(envSources.y,c)),d=operand(envOperands.z,source(envSources.z,c));"
    "float x=alphaArg(envAlphaSources.x,envAlphaOperands.x,c),y=alphaArg(envAlphaSources.y,envAlphaOperands.y,c),z=alphaArg(envAlphaSources.z,envAlphaOperands.z,c);"
    "c.rgb=clamp(envRgb==1 ? a : envRgb==2 ? a+b : envRgb==3 ? a*d+b*(vec3(1.0)-d) : a*b,0.0,1.0);"
    "c.a=clamp(envAlpha==1 ? x : envAlpha==2 ? x+y : envAlpha==3 ? x*z+y*(1.0-z) : x*y,0.0,1.0)";
  snprintf(fs,capacity,"%s%s void main(){vec4 c=%s; %s",prefix,textured ? env : "",textured ? combine : "tint",alpha);
}
