/* Shared production sources, also compiled by the host shader tests. */
#include <stdio.h>
static const char *ps4gl_fixed_vertex="#version 120\nattribute vec3 position; attribute vec4 color; attribute vec2 uv;\n"
    "uniform mat4 projection; uniform mat4 modelview; varying vec4 tint; varying vec2 texcoord;\n"
    "void main(){gl_Position=projection*modelview*vec4(position,1.0); tint=clamp(color,0.0,1.0); texcoord=uv;}\n";
static void ps4glFixedFragment(int textured, char *fs, size_t capacity) {
  const char *prefix="#version 120\nvarying vec4 tint; varying vec2 texcoord; uniform int alphaOp; uniform float alphaRef;\n";
  const char *alpha="if(alphaOp==0 || (alphaOp==1 && !(c.a<alphaRef)) || (alphaOp==2 && !(c.a==alphaRef)) || "
    "(alphaOp==3 && !(c.a<=alphaRef)) || (alphaOp==4 && !(c.a>alphaRef)) || "
    "(alphaOp==5 && !(c.a!=alphaRef)) || (alphaOp==6 && !(c.a>=alphaRef))) discard; gl_FragColor=c;}\n";
  snprintf(fs,capacity,"%s%s void main(){vec4 c=%s; %s",prefix,
    textured ? "uniform sampler2D image;" : "",textured ? "texture2D(image,texcoord)*tint" : "tint",alpha);
}
