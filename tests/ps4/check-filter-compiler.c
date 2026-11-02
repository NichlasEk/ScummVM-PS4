#include "ps4gl-glsl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
 if (!ps4glGLSLInitialize()) return 1;
 for (int i=1;i<argc;++i) {
  FILE *file=fopen(argv[i],"rb"); if(!file) return 2;
  fseek(file,0,SEEK_END); long size=ftell(file); rewind(file);
  char *body=calloc((size_t)size+1,1); if(!body || fread(body,1,size,file)!=(size_t)size) return 3; fclose(file);
  char *version=strstr(body,"#version 120\n"); if(!version) return 4;
  const char *code=version+strlen("#version 120\n");
  char *vs=malloc(size+256),*fs=malloc(size+256);
  sprintf(vs,"#version 120\n#define VERTEX\n#define PARAMETER_UNIFORM\n%s",code);
  sprintf(fs,"#version 120\n#define FRAGMENT\n#define PARAMETER_UNIFORM\n%s",code);
  uint32_t *v=NULL,*f=NULL;size_t vb=0,fb=0;char log[4096];
  PS4GLAttributeBinding binding={"VertexCoord",0};
  if(!ps4glLinkLegacyGLSL(vs,fs,&binding,1,&v,&vb,&f,&fb,log,sizeof(log))) {fprintf(stderr,"FAIL %s: %s\n",argv[i],log);return 5;}
  printf("PASS %s (%zu/%zu bytes SPIR-V)\n",argv[i],vb,fb);
  free(body);free(vs);free(fs);free(v);free(f);
 }
 ps4glGLSLFinalize();return 0;
}
