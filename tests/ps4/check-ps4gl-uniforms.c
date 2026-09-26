/* Reflection contract: compile probe shaders first, then pass their .spv paths. */
#include "ps4gl.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ps4gl-reflect.h"
static uint32_t *read_spv(const char *path, size_t *bytes) {
  FILE *f=fopen(path,"rb"); assert(f);
  assert(!fseek(f,0,SEEK_END)); long length=ftell(f); assert(length>=20);
  *bytes=(size_t)length; rewind(f);
  uint32_t *s=malloc(*bytes); assert(s && fread(s,1,*bytes,f)==*bytes); fclose(f); return s;
}
int main(int argc, char **argv) {
  assert(argc==3); size_t vn,fn;
  uint32_t *vs=read_spv(argv[1],&vn), *fs=read_spv(argv[2],&fn);
  PS4GLProgram p={0};
  assert(gl_reflect_uniforms(vs,vn,0,&p) && gl_reflect_uniforms(fs,fn,1,&p));
  assert(p.uniform_stages==3 && p.uniform_count==3);
  assert(!strcmp(p.uniforms[0].name,"transform") && p.uniforms[0].width==16 && p.uniforms[0].offset[0]==0);
  assert(!strcmp(p.uniforms[1].name,"gain") && p.uniforms[1].width==1 && p.uniforms[1].offset[0]==64 && p.uniforms[1].offset[1]==16);
  assert(!strcmp(p.uniforms[2].name,"tint") && p.uniforms[2].width==4 && p.uniforms[2].offset[0]==-1);
  unsigned rejected=0;
  for(size_t i=5;i<vn/4;i+=vs[i]>>16) {
    unsigned op=vs[i]&65535, wc=vs[i]>>16;
    uint32_t old=0; size_t at=0;
    if(op==71 && wc==4 && vs[i+2]==33) { at=i+3; old=vs[at]; vs[at]=2; }
    if(op==72 && wc==5 && vs[i+3]==7) { at=i+4; old=vs[at]; vs[at]=32; }
    if(op==22 && wc==3) { at=i+2; old=vs[at]; vs[at]=64; }
    if(op==72 && wc==5 && vs[i+3]==35 && vs[i+4]==64) { at=i+4; old=vs[at]; vs[at]=0; }
    if(at) { p=(PS4GLProgram){0}; assert(!gl_reflect_uniforms(vs,vn,0,&p)); ++rejected; vs[at]=old; }
  }
  assert(rejected==4);
  uint32_t first=vs[5]; vs[5]=0;
  p=(PS4GLProgram){0}; assert(!gl_reflect_uniforms(vs,vn,0,&p)); vs[5]=first;
  free(vs); free(fs);
  puts("PASS uniform reflection: stage bindings, names, shared locations, matrix layout and malformed input rejection");
}
