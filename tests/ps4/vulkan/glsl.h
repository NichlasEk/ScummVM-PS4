#include "ps4gl-glsl.c"
#include "glsl-sources.h"
static GLuint compile_glsl_program(VkDevice device, VkRenderPass pass, const char *vertex, const char *fragment) {
  uint32_t *vs=NULL,*fs=NULL; size_t vb=0,fb=0; char log[2048]; GLuint result=0;
  if(!ps4glCompileVulkanGLSL(0,vertex,&vs,&vb,log,sizeof(log))) {
    fprintf(report,"GLSL vertex failed: %s\n",log); goto done;
  }
  if(!ps4glCompileVulkanGLSL(1,fragment,&fs,&fb,log,sizeof(log))) {
    fprintf(report,"GLSL fragment failed: %s\n",log); goto done;
  }
  result=ps4glCreateProgramSPIRV(device,pass,vs,vb,fs,fb);
  fprintf(report,"GLSL runtime program=%u vertex=%zu fragment=%zu\n",result,vb,fb);
done:
  free(vs); free(fs); return result;
}
static int reject_glsl(void) {
  const char *bad[]={"#version 450\nvoid main( {", "#version 450\nvoid main(){gl_Position=missing;}",
    "#version 450\n#extension GL_GOOGLE_include_directive : require\n#include \"absent.glsl\"\nvoid main(){}"};
  for(unsigned i=0;i<3;++i) {
    uint32_t *words=(uint32_t *)(uintptr_t)1; size_t bytes=99; char log[256];
    if(ps4glCompileVulkanGLSL(0,bad[i],&words,&bytes,log,sizeof(log)) || words || bytes || !log[0]) return 2;
    fprintf(report,"PASS GLSL runtime rejection %u: %s\n",i,log);
  }
  return 0;
}
