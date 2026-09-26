#include "ps4gl-glsl.h"
#include "legacy-sources.h"
#include "ps4gl.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ps4gl-reflect.h"
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
  uint32_t *fs=NULL; size_t fb=0;
  const char *vsource="#version 450\nlayout(location=0) out vec2 varyingValue; void main(){varyingValue=vec2(1); gl_Position=vec4(0,0,0,1);}";
  const char *fsource="#version 450\nlayout(location=0) in vec2 varyingValue; layout(location=0) out vec4 result; void main(){result=vec4(varyingValue,0,1);}";
  assert(ps4glLinkVulkanGLSL(vsource,fsource,&words,&bytes,&fs,&fb,log,sizeof(log)));
  assert(words && fs && bytes>=20 && fb>=20); free(words); free(fs);
  const char *mismatch="#version 450\nlayout(location=0) in vec3 varyingValue; layout(location=0) out vec4 result; void main(){result=vec4(varyingValue,1);}";
  assert(!ps4glLinkVulkanGLSL(vsource,mismatch,&words,&bytes,&fs,&fb,log,sizeof(log)));
  assert(!words && !fs && !bytes && !fb && log[0]);
  assert(!ps4glLinkVulkanGLSL(valid,fsource,&words,&bytes,&fs,&fb,log,sizeof(log)));
  assert(!ps4glLinkVulkanGLSL(vsource,bad[1],&words,&bytes,&fs,&fb,log,sizeof(log)));
  assert(!ps4glLinkVulkanGLSL(vsource,fsource,&words,&bytes,&words,&fb,log,sizeof(log)));
  PS4GLAttributeBinding bindings[]={ {"a_position",4},{"a_color",1},{"a_texCoords",6} };
  const char *legacy_fragments[]={legacy_fragmentSrc,legacy_bwShader,legacy_ghostShader,legacy_sepiaShader,legacy_fadeShader,legacy_color};
  for(unsigned i=0;i<sizeof(legacy_fragments)/sizeof(legacy_fragments[0]);++i) {
    if(!ps4glLinkLegacyGLSL(legacy_vsrc,legacy_fragments[i],bindings,3,&words,&bytes,&fs,&fb,log,sizeof(log))) { fprintf(stderr,"legacy %u: %s\n",i,log); abort(); }
    if(i==4 || i==5) {
      PS4GLProgram program={0};
      assert(gl_reflect_uniforms(words,bytes,0,&program) && gl_reflect_uniforms(fs,fb,1,&program));
      unsigned found=0;
      for(unsigned u=0;u<program.uniform_count;++u) {
        PS4GLUniform *item=&program.uniforms[u];
        if(!strcmp(item->name,i==4 ? "u_fadeToSep" : "modes")) {
          assert(item->integer && item->width==1 && item->array_length==(i==4 ? 0u : 2u));
          if(i==5) assert(item->array_stride[1]==16);
          ++found;
        }
      }
      assert(found==1);
      PS4GLProgram conflict={.uniform_count=1};
      conflict.uniforms[0]=(PS4GLUniform){.width=1,.offset={0,-1},.array_length=i==4 ? 0u : 2u};
      strcpy(conflict.uniforms[0].name,i==4 ? "u_fadeToSep" : "modes");
      assert(!gl_reflect_uniforms(fs,fb,1,&conflict)); /* float/int shared-name mismatch */
      /* The same bytes cannot be reinterpreted as unsigned/default bool storage. */
      for(size_t at=5;at<fb/4;at+=fs[at]>>16) if((fs[at]&65535)==21 && fs[at+2]==32 && fs[at+3]==1) {
        fs[at+3]=0; PS4GLProgram rejected={0};
        assert(!gl_reflect_uniforms(fs,fb,1,&rejected)); fs[at+3]=1;
      }
    }
    free(words); free(fs);
  }
  /* Desktop compatibility macros must expand correctly too, not just ES100. */
  char *desktop_v=malloc(sizeof(legacy_vsrc)); assert(desktop_v);
  memcpy(desktop_v,legacy_vsrc,sizeof(legacy_vsrc)); desktop_v[10]='1';
  for(unsigned i=0;i<sizeof(legacy_fragments)/sizeof(legacy_fragments[0]);++i) {
    size_t length=strlen(legacy_fragments[i])+1; char *desktop_f=malloc(length); assert(desktop_f);
    memcpy(desktop_f,legacy_fragments[i],length); desktop_f[10]='1';
    /* The small probe-only ES shader contains an unconditional precision statement. */
    if(i==5) { free(desktop_f); continue; }
    assert(ps4glLinkLegacyGLSL(desktop_v,desktop_f,NULL,0,&words,&bytes,&fs,&fb,log,sizeof(log)));
    free(words); free(fs); free(desktop_f);
  }
  free(desktop_v);
  const char *ordered_v="#version 110\nattribute vec2 pos;varying vec2 first;varying vec4 second;void main(){first=pos;second=vec4(1);gl_Position=vec4(pos,0,1);}";
  const char *ordered_f="#version 110\nvarying vec4 second;varying vec2 first;uniform sampler2D a;uniform sampler2D b;void main(){gl_FragColor=second*texture2D(a,first)+texture2D(b,first);}";
  assert(ps4glLinkLegacyGLSL(ordered_v,ordered_f,NULL,0,&words,&bytes,&fs,&fb,log,sizeof(log)));
  free(words); free(fs);
  assert(ps4glCompileLegacyGLSL(0,legacy_vsrc,&words,&bytes,log,sizeof(log))); free(words);
  assert(ps4glCompileLegacyGLSL(1,legacy_fragmentSrc,&words,&bytes,log,sizeof(log))); free(words);
  bindings[2].index=4;
  assert(!ps4glLinkLegacyGLSL(legacy_vsrc,legacy_fragmentSrc,bindings,3,&words,&bytes,&fs,&fb,log,sizeof(log)));
  assert(!words && !fs && !bytes && !fb && log[0]); bindings[2].index=6;
  const char *comma="#version 110\nattribute vec2 a,b;void main(){gl_Position=vec4(a+b,0,1);}";
  assert(!ps4glCompileLegacyGLSL(0,comma,&words,&bytes,log,sizeof(log)));
  const char *builtin="#version 110\nvoid main(){gl_FragColor=gl_FragCoord;}";
  assert(!ps4glCompileLegacyGLSL(1,builtin,&words,&bytes,log,sizeof(log)) && !words && !bytes && strstr(log,"builtin"));
  const char *missing="#version 110\nvarying vec2 absent;void main(){gl_FragColor=vec4(absent,0,1);}";
  assert(!ps4glLinkLegacyGLSL(legacy_vsrc,missing,bindings,3,&words,&bytes,&fs,&fb,log,sizeof(log)));
  ps4glGLSLFinalize(); ps4glGLSLFinalize();
  assert(ps4glGLSLInitialize());
  assert(ps4glCompileVulkanGLSL(0,valid,&words,&bytes,log,sizeof(log))); free(words);
  ps4glGLSLFinalize();
  puts("PASS signed integer and array reflection, unsigned rejection; original TWP GLSL110/ES100 pairs, varying order, bindings and declaration/builtin rejection; cross-stage link and varying mismatch rejection; runtime GLSL compilation, diagnostics, bounds, stage rejection and reinitialization");
}
