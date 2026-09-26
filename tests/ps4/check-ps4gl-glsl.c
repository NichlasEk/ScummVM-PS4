#include "ps4gl-glsl.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  const char *valid="#version 450\nvoid main(){gl_Position=vec4(0,0,0,1);}";
  uint32_t *words=(uint32_t *)1; size_t bytes=99; char log[512];
  assert(!ps4glCompileVulkanGLSL(0,valid,&words,&bytes,log,sizeof(log)) && !words && !bytes && log[0]);
  assert(ps4glGLSLInitialize() && ps4glGLSLInitialize());
  assert(ps4glCompileVulkanGLSL(0,valid,&words,&bytes,log,sizeof(log)) && words && bytes>=20 && words[0]==0x07230203);
  FILE *out=fopen("build/glslang-host/test.spv","wb"); assert(out);
  assert(fwrite(words,1,bytes,out)==bytes); fclose(out); free(words);
  const char *bad[]={"", "#version 450\nvoid main( {", "#version 450\nvoid main(){gl_Position=missing;}",
    "#version 450\n#extension GL_GOOGLE_include_directive : require\n#include \"absent.glsl\"\nvoid main(){}"};
  for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);++i) {
    words=(uint32_t *)1; bytes=99; memset(log,0xff,sizeof(log));
    assert(!ps4glCompileVulkanGLSL(0,bad[i],&words,&bytes,log,sizeof(log)) && !words && !bytes && log[0]);
    assert(memchr(log,0,sizeof(log)));
  }
  FILE *include=fopen("build/glslang-host/valid-include.glsl","w"); assert(include);
  fputs("void main(){gl_Position=vec4(0,0,0,1);}",include); fclose(include);
  assert(!ps4glCompileVulkanGLSL(0,"#version 450\n#extension GL_GOOGLE_include_directive : require\n#include \"build/glslang-host/valid-include.glsl\"\n",&words,&bytes,log,sizeof(log)));
  char tiny[3]={1,2,3};
  assert(!ps4glCompileVulkanGLSL(0,bad[1],&words,&bytes,tiny,1) && tiny[0]==0 && tiny[1]==2);
  assert(!ps4glCompileVulkanGLSL(0,bad[1],&words,&bytes,NULL,0));
  assert(!ps4glCompileVulkanGLSL(2,valid,&words,&bytes,log,sizeof(log)));
  assert(!ps4glCompileVulkanGLSL(0,valid,NULL,&bytes,log,sizeof(log)));
  assert(!ps4glCompileVulkanGLSL(1,valid,&words,&bytes,log,sizeof(log))); /* wrong stage */
  char *large=malloc(262146); assert(large); memset(large,' ',262145); large[262145]=0;
  assert(!ps4glCompileVulkanGLSL(0,large,&words,&bytes,log,sizeof(log))); free(large);
  ps4glGLSLFinalize(); ps4glGLSLFinalize();
  assert(ps4glGLSLInitialize());
  assert(ps4glCompileVulkanGLSL(0,valid,&words,&bytes,log,sizeof(log))); free(words);
  ps4glGLSLFinalize();
  puts("PASS runtime GLSL compilation, diagnostics, bounds, stage rejection and reinitialization");
}
