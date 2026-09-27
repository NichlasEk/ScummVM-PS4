#include "ps4gl-glsl.h"
#include "legacy-sources.h"
#include "ps4gl.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ps4gl-reflect.h"
#include "ps4gl-fixed-shaders.h"
int main(void) {
  const char *valid="#version 450\nvoid main(){gl_Position=vec4(0,0,0,1);}";
  uint32_t *words=(uint32_t *)1; size_t bytes=99; char log[512];
  assert(!ps4glCompileVulkanGLSL(0,valid,&words,&bytes,log,sizeof(log)) && !words && !bytes && log[0]);
  assert(ps4glGLSLInitialize() && ps4glGLSLInitialize());
  for(int textured=0;textured<2;++textured) {
    char fragment[4096]; ps4glFixedFragment(textured,fragment,sizeof(fragment));
    uint32_t *v=NULL,*f=NULL; size_t vb=0,fb=0;
    PS4GLAttributeBinding binding[3]={{"position",0},{"color",1},{"uv",2}};
    if(!ps4glLinkLegacyGLSL(ps4gl_fixed_vertex,fragment,binding,3,&v,&vb,&f,&fb,log,sizeof(log))) {
      fprintf(stderr,"Fixed-function shader: %s\n",log); abort();
    }
    free(v); free(f);
  }
  puts("PASS production fixed-function shader variants (textured and untextured)");
  {
    const char *vsrc="#version 110\nvec4 position=gl_Vertex; vec2 uv=gl_MultiTexCoord0.xy; varying vec2 vUv; void main(){gl_Position=position;vUv=uv;}";
    const char *fsrc="#version 110\nvarying vec2 vUv;uniform sampler2D tex0;void main(){gl_FragColor=texture2D(tex0,vUv);}";
    uint32_t *v=NULL,*f=NULL;size_t vb=0,fb=0;
    if(!ps4glLinkLegacyGLSL(vsrc,fsrc,NULL,0,&v,&vb,&f,&fb,log,sizeof(log))) {fprintf(stderr,"HPL1 legacy attributes: %s\n",log);abort();}
    free(v);free(f);
    if(!ps4glLinkLegacyGLSL(legacy_hpl_gamma_vertex,legacy_hpl_gamma_fragment,NULL,0,&v,&vb,&f,&fb,log,sizeof(log))) {fprintf(stderr,"HPL1 gamma shader: %s\n",log);abort();}
    PS4GLProgram reflected={0};
    assert(gl_reflect_uniforms(f,fb,1,&reflected) && gl_reflect_samplers(f,fb,1,&reflected));
    assert(reflected.rectangle_sampler_mask==1 && reflected.sampler_mask==1);
    free(v);free(f);
    if(!ps4glLinkLegacyGLSL(legacy_hpl_fog_vertex,legacy_hpl_fog_fragment,NULL,0,&v,&vb,&f,&fb,log,sizeof(log))) {fprintf(stderr,"HPL1 fog shader: %s\n",log);abort();}
    memset(&reflected,0,sizeof(reflected));assert(gl_reflect_samplers(f,fb,1,&reflected));
    assert(reflected.one_dimensional_sampler_mask==1 && reflected.sampler_mask==1 && !strcmp(reflected.uniforms[0].name,"tex0"));
    free(v);free(f);
    if(!ps4glLinkLegacyGLSL(legacy_hpl_refract_vertex,legacy_hpl_refract_fragment,NULL,0,&v,&vb,&f,&fb,log,sizeof(log))) {fprintf(stderr,"HPL1 refraction shader: %s\n",log);abort();}
    memset(&reflected,0,sizeof(reflected));assert(gl_reflect_uniforms(f,fb,1,&reflected) && gl_reflect_samplers(f,fb,1,&reflected));
    assert(reflected.sampler_mask==7 && reflected.rectangle_sampler_mask==1);free(v);free(f);
    if(!ps4glLinkLegacyGLSL(legacy_hpl_cube_vertex,legacy_hpl_cube_fragment,NULL,0,&v,&vb,&f,&fb,log,sizeof(log))) {fprintf(stderr,"HPL1 cube lighting shader: %s\n",log);abort();}
    memset(&reflected,0,sizeof(reflected));assert(gl_reflect_uniforms(f,fb,1,&reflected) && gl_reflect_samplers(f,fb,1,&reflected));
    assert(reflected.sampler_mask==7 && reflected.cube_sampler_mask==2 && reflected.one_dimensional_sampler_mask==4);free(v);free(f);
    if(!ps4glLinkLegacyGLSL(legacy_hpl_ambient_vertex,legacy_hpl_ambient_fragment,NULL,0,&v,&vb,&f,&fb,log,sizeof(log))) {fprintf(stderr,"HPL1 ambient shader: %s\n",log);abort();}
    free(v);free(f);
    char many[4096]="#version 120\n";char line[128];
    for(unsigned i=0;i<PS4GL_MAX_SAMPLERS;++i) {snprintf(line,sizeof(line),"uniform sampler2D tex%u;\n",i);strcat(many,line);}
    strcat(many,"void main(){gl_FragColor=vec4(0.0);");
    for(unsigned i=0;i<PS4GL_MAX_SAMPLERS;++i) {snprintf(line,sizeof(line),"gl_FragColor+=texture2D(tex%u,vec2(0.5));",i);strcat(many,line);}strcat(many,"}");
    assert(ps4glCompileLegacyGLSL(1,many,&f,&fb,log,sizeof(log)));memset(&reflected,0,sizeof(reflected));assert(gl_reflect_samplers(f,fb,1,&reflected));
    assert(reflected.sampler_mask==(1u<<PS4GL_MAX_SAMPLERS)-1);free(f);
    strcat(many,"\nuniform sampler2D overLimit;\n");assert(!ps4glCompileLegacyGLSL(1,many,&f,&fb,log,sizeof(log)));
    puts("PASS HPL1 gamma, fog and refraction shaders, 1D/RG textures and bounded eight-sampler interfaces");
  }

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
  const char *legacy_fragments[]={legacy_fragmentSrc,legacy_bwShader,legacy_ghostShader,legacy_sepiaShader,legacy_fadeShader,legacy_color,legacy_fshader,legacy_debug_fshader};
  for(unsigned i=0;i<sizeof(legacy_fragments)/sizeof(legacy_fragments[0]);++i) {
    if(!ps4glLinkLegacyGLSL(i>=6 ? legacy_vshader : legacy_vsrc,legacy_fragments[i],bindings,3,&words,&bytes,&fs,&fb,log,sizeof(log))) { fprintf(stderr,"legacy %u: %s\n",i,log); abort(); }
    if(i>=6) {
      PS4GLProgram lighting={0};
      assert(gl_reflect_uniforms(words,bytes,0,&lighting) && gl_reflect_uniforms(fs,fb,1,&lighting));
      unsigned arrays=0;
      for(unsigned u=0;u<lighting.uniform_count;++u) if(lighting.uniforms[u].array_length) {
        assert(lighting.uniforms[u].array_length==50 && lighting.uniforms[u].array_stride[1]==16);
        ++arrays;
      }
      assert(arrays==8);
    }
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
  const char *bv="#version 120\nuniform bool flipY; attribute vec2 position; void main(){vec2 p=position; if(flipY) p.y=-p.y; gl_Position=vec4(p,0.,1.);}";
  const char *bf="#version 120\nvoid main(){gl_FragColor=vec4(1.);}";
  PS4GLAttributeBinding ba={.index=0}; strcpy(ba.name,"position");
  assert(ps4glLinkLegacyGLSL(bv,bf,&ba,1,&words,&bytes,&fs,&fb,log,sizeof(log)));
  PS4GLProgram boolean_program={0};
  assert(gl_reflect_uniforms(words,bytes,0,&boolean_program));
  assert(boolean_program.uniform_count==1 && !strcmp(boolean_program.uniforms[0].name,"flipY") && boolean_program.uniforms[0].integer==2);
  free(words); free(fs);
  /* Desktop compatibility macros must expand correctly too, not just ES100. */
  char *desktop_v=malloc(sizeof(legacy_vsrc)); assert(desktop_v);
  memcpy(desktop_v,legacy_vsrc,sizeof(legacy_vsrc)); desktop_v[10]='1';
  for(unsigned i=0;i<sizeof(legacy_fragments)/sizeof(legacy_fragments[0]);++i) {
    size_t length=strlen(legacy_fragments[i])+1; char *desktop_f=malloc(length); assert(desktop_f);
    memcpy(desktop_f,legacy_fragments[i],length); desktop_f[10]='1';
    /* The small probe-only ES shader contains an unconditional precision statement. */
    if(i==5) { free(desktop_f); continue; }
    assert(ps4glLinkLegacyGLSL(i>=6 ? legacy_vshader : desktop_v,desktop_f,NULL,0,&words,&bytes,&fs,&fb,log,sizeof(log)));
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
  const char *builtin="#version 110\nvoid main(){gl_FragColor=gl_FrontFacing?vec4(1.):vec4(0.);}";
  assert(!ps4glCompileLegacyGLSL(1,builtin,&words,&bytes,log,sizeof(log)) && !words && !bytes && strstr(log,"builtin"));
  const char *missing="#version 110\nvarying vec2 absent;void main(){gl_FragColor=vec4(absent,0,1);}";
  assert(!ps4glLinkLegacyGLSL(legacy_vsrc,missing,bindings,3,&words,&bytes,&fs,&fb,log,sizeof(log)));
  /* Eight std140 vec4[64] blocks exactly fill the 8 KiB stage budget. */
  char blocks[2048]="#version 110\n";
  for(unsigned i=0;i<9;++i) {
    char decl[64]; snprintf(decl,sizeof(decl),"uniform vec4 block%u[64];\n",i);
    strcat(blocks,decl);
    if(i==7 || i==8) {
      char source[2200]; snprintf(source,sizeof(source),"%svoid main(){gl_FragColor=block0[63];}",blocks);
      assert(ps4glCompileLegacyGLSL(1,source,&words,&bytes,log,sizeof(log)));
      PS4GLProgram bounded={0};
      assert(gl_reflect_uniforms(words,bytes,1,&bounded)==(i==7));
      free(words);
    }
  }
  assert(!ps4glCompileLegacyGLSL(1,"#version 110\nuniform vec4 tooMany[65];void main(){gl_FragColor=tooMany[64];}",&words,&bytes,log,sizeof(log)));
  /* Original Grim actor shader: flattened struct fields retain GL names and types. */
  if(!ps4glCompileLegacyGLSL(0,legacy_grim_actor_vertex,&words,&bytes,log,sizeof(log))) {
    fprintf(stderr,"Grim actor vertex: %s\n",log); abort();
  }
  PS4GLProgram grim={0}; assert(gl_reflect_uniforms(words,bytes,0,&grim));
  unsigned fields=0;
  for(unsigned u=0;u<grim.uniform_count;++u) {
    const PS4GLUniform *field=&grim.uniforms[u];
    if(!strncmp(field->name,"shadow.",7)) {
      ++fields;
      if(!strcmp(field->name,"shadow._active")) assert(field->integer==2 && field->width==1);
      else assert(!field->integer && field->width==3 && field->offset[0]%16==0);
    }
  }
  assert(fields==5); free(words);
  if(!ps4glLinkLegacyGLSL(legacy_grim_actor_vertex,legacy_grim_actor_fragment,NULL,0,&words,&bytes,&fs,&fb,log,sizeof(log))) {
    fprintf(stderr,"Grim actor pair: %s\n",log); abort();
  }
  PS4GLProgram actor={0};
  assert(gl_reflect_uniforms(words,bytes,0,&actor) && gl_reflect_uniforms(fs,fb,1,&actor));
  assert(gl_reflect_samplers(words,bytes,0,&actor) && gl_reflect_samplers(fs,fb,1,&actor));
  unsigned coord=0;
  for(unsigned u=0;u<actor.uniform_count;++u) if(!strcmp(actor.uniforms[u].name,"ps4gl_fragcoord_y")) { assert(actor.uniforms[u].width==2 && actor.uniforms[u].offset[1]>=0); ++coord; }
  assert(coord==1); free(words); free(fs);
  for(unsigned i=0;i<sizeof(grim_pairs)/sizeof(grim_pairs[0]);++i) {
    if(!ps4glLinkLegacyGLSL(grim_pairs[i].vertex,grim_pairs[i].fragment,NULL,0,&words,&bytes,&fs,&fb,log,sizeof(log))) {
      fprintf(stderr,"%s: %s\n",grim_pairs[i].name,log); abort();
    }
    PS4GLProgram pair={0};
    assert(gl_reflect_uniforms(words,bytes,0,&pair) && gl_reflect_uniforms(fs,fb,1,&pair));
    assert(gl_reflect_samplers(words,bytes,0,&pair) && gl_reflect_samplers(fs,fb,1,&pair));
    free(words); free(fs);
  }
  puts("PASS all original Grim shader pairs compiled, linked and reflected");
  const char *struct_v="#version 110\nstruct Params { bool enabled; vec3 color; }; uniform Params p; void main(){gl_Position=vec4(p.color,1.0);}";
  const char *struct_f="#version 110\nstruct Params { bool enabled; vec3 color; }; uniform Params p; void main(){gl_FragColor=p.enabled?vec4(p.color,1.0):vec4(0.0);}";
  if(!ps4glLinkLegacyGLSL(struct_v,struct_f,NULL,0,&words,&bytes,&fs,&fb,log,sizeof(log))) { fprintf(stderr,"struct pair: %s\n",log); abort(); }
  PS4GLProgram shared={0};
  assert(gl_reflect_uniforms(words,bytes,0,&shared) && gl_reflect_uniforms(fs,fb,1,&shared));
  assert(shared.uniform_count==2);
  for(unsigned u=0;u<shared.uniform_count;++u) assert(shared.uniforms[u].offset[0]>=0 && shared.uniforms[u].offset[1]>=0);
  free(words); free(fs);
  const char *struct_bad="#version 110\nstruct Params { bool enabled; vec4 color; }; uniform Params p; void main(){gl_FragColor=p.color;}";
  assert(!ps4glLinkLegacyGLSL(struct_v,struct_bad,NULL,0,&words,&bytes,&fs,&fb,log,sizeof(log)) && strstr(log,"fields differ"));
  const char *nested="#version 110\nstruct Inner { float x; }; struct Outer { Inner child; }; uniform Outer p; void main(){gl_Position=vec4(p.child.x);}";
  assert(!ps4glCompileLegacyGLSL(0,nested,&words,&bytes,log,sizeof(log)));
  puts("PASS original Grim actor vertex struct, bool/vector reflection, shared stage fields, mismatched and nested struct rejection");
  ps4glGLSLFinalize(); ps4glGLSLFinalize();
  assert(ps4glGLSLInitialize());
  assert(ps4glCompileVulkanGLSL(0,valid,&words,&bytes,log,sizeof(log))); free(words);
  ps4glGLSLFinalize();
  puts("PASS signed integer and array reflection, unsigned rejection; original TWP GLSL110/ES100 pairs, varying order, bindings and declaration/builtin rejection; cross-stage link and varying mismatch rejection; runtime GLSL compilation, diagnostics, bounds, stage rejection and reinitialization");
}
