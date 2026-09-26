#include "ps4gl.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ps4gl-reflect.h"
static uint32_t *read_spv(const char *path, size_t *bytes) {
  FILE *f=fopen(path,"rb"); assert(f); assert(!fseek(f,0,SEEK_END));
  long length=ftell(f); assert(length>=20); *bytes=(size_t)length; rewind(f);
  uint32_t *s=malloc(*bytes); assert(s && fread(s,1,*bytes,f)==*bytes); fclose(f); return s;
}
static PS4GLUniform *find(PS4GLProgram *p, const char *name) {
  for(unsigned i=0;i<p->uniform_count;++i) if(!strcmp(p->uniforms[i].name,name)) return &p->uniforms[i];
  assert(0); return NULL;
}
int main(int argc, char **argv) {
  assert(argc==3); size_t vn,fn;
  uint32_t *vs=read_spv(argv[1],&vn), *fs=read_spv(argv[2],&fn);
  PS4GLProgram p={0}; assert(gl_reflect_uniforms(vs,vn,0,&p) && gl_reflect_uniforms(fs,fn,1,&p));
  PS4GLUniform *u=find(&p,"RandomValue");
  assert(u->array_length==5 && u->array_stride[1]==16 && u->offset[1]==16 && u->width==1);
  assert(find(&p,"TimeLapse")->offset[1]==96);
  u=find(&p,"transform"); assert(u->array_length==2 && u->array_stride[0]==64 && u->width==16);
  unsigned rejected=0;
  for(size_t i=5;i<fn/4;i+=fs[i]>>16) {
    unsigned op=fs[i]&65535, wc=fs[i]>>16;
    if(op==71 && wc==4 && fs[i+2]==6) {
      uint32_t old=fs[i+3];
      const uint32_t bad[]={0,4,17,UINT32_MAX};
      for(unsigned b=0;b<4;++b) { fs[i+3]=bad[b]; p=(PS4GLProgram){0}; assert(!gl_reflect_uniforms(fs,fn,1,&p)); ++rejected; }
      fs[i+3]=old;
    }
    if(op==43 && wc==4 && fs[i+3]==5) {
      uint32_t old=fs[i+3];
      const uint32_t bad[]={0,17,UINT32_MAX};
      for(unsigned b=0;b<3;++b) { fs[i+3]=bad[b]; p=(PS4GLProgram){0}; assert(!gl_reflect_uniforms(fs,fn,1,&p)); ++rejected; }
      fs[i+3]=old;
    }
  }
  assert(rejected==7);
  free(vs); free(fs);
  puts("PASS array reflection: float[5] offsets/stride, mat4[2], invalid lengths and strides");
}
