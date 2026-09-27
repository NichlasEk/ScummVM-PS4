#include "ps4gl.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ps4gl-reflect.h"
int main(int argc, char **argv) {
  assert(argc==2); FILE *f=fopen(argv[1],"rb"); assert(f);
  assert(!fseek(f,0,SEEK_END)); long size=ftell(f); assert(size>=20); rewind(f);
  uint32_t *s=malloc((size_t)size); assert(s && fread(s,1,(size_t)size,f)==(size_t)size); fclose(f);
  PS4GLProgram p={0};
  assert(gl_reflect_uniforms(s,size,1,&p) && gl_reflect_samplers(s,size,1,&p));
  assert(p.uniform_stages==2 && p.sampler_mask==3 && p.uniform_count==3);
  assert(p.uniforms[1].sampler_binding==2 && !strcmp(p.uniforms[1].name,"imageA"));
  assert(p.uniforms[2].sampler_binding==3 && !strcmp(p.uniforms[2].name,"imageB"));
  p=(PS4GLProgram){0}; assert(!gl_reflect_samplers(s,size,0,&p));
  unsigned rejected=0;
  for(size_t i=5;i<(size_t)size/4;i+=s[i]>>16) {
    unsigned op=s[i]&65535, wc=s[i]>>16; size_t at=0; uint32_t old=0;
    if(op==71 && wc==4 && s[i+2]==33 && s[i+3]>=2) { at=i+3; old=s[at]; s[at]=2+PS4GL_MAX_SAMPLERS; }
    if(op==25 && wc==9) { at=i+3; old=s[at]; s[at]=2; }
    if(op==27 && wc==3) { at=i; old=s[at]; s[at]=(3u<<16)|28; }
    if(at) { p=(PS4GLProgram){0}; assert(!gl_reflect_samplers(s,size,1,&p)); ++rejected; s[at]=old; }
  }
  assert(rejected>=4); free(s);
  puts("PASS sampler reflection: named fragment sampler2D bindings, bad bindings/types/stages rejected");
}
