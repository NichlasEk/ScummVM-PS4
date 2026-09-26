#include "ps4gl-glsl.h"
#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ps4gl-stage-interface.h"
static int initialized;
static glsl_include_result_t *reject_include(void *ctx, const char *name, const char *from, size_t depth) {
  (void)ctx; (void)name; (void)from; (void)depth; return NULL;
}
int ps4glGLSLInitialize(void) {
  if(!initialized) initialized=glslang_initialize_process();
  return initialized;
}
void ps4glGLSLFinalize(void) {
  if(initialized) glslang_finalize_process();
  initialized=0;
}
static void diagnostic(char *log, size_t capacity, const char *text) {
  if(log && capacity) snprintf(log,capacity,"%s",text ? text : "Shader compilation failed");
}
int ps4glCompileVulkanGLSL(int fragment, const char *source, uint32_t **words,
                          size_t *bytes, char *log, size_t log_capacity) {
  if(words) *words=NULL;
  if(bytes) *bytes=0;
  diagnostic(log,log_capacity,"");
  if(!initialized || !source || !words || !bytes || (fragment!=0 && fragment!=1)) {
    diagnostic(log,log_capacity,"Invalid compiler state or arguments"); return 0;
  }
  size_t length=0;
  while(length<=256u*1024u && source[length]) ++length;
  if(!length || length>256u*1024u) { diagnostic(log,log_capacity,"Shader source size outside 1..262144 bytes"); return 0; }
  glslang_input_t input={.language=GLSLANG_SOURCE_GLSL,
    .stage=fragment ? GLSLANG_STAGE_FRAGMENT : GLSLANG_STAGE_VERTEX,
    .client=GLSLANG_CLIENT_VULKAN,.client_version=GLSLANG_TARGET_VULKAN_1_0,
    .target_language=GLSLANG_TARGET_SPV,.target_language_version=GLSLANG_TARGET_SPV_1_0,
    .callbacks={.include_local=reject_include,.include_system=reject_include},
    .code=source,.default_version=450,.default_profile=GLSLANG_CORE_PROFILE,
    .messages=GLSLANG_MSG_SPV_RULES_BIT|GLSLANG_MSG_VULKAN_RULES_BIT,.resource=glslang_default_resource()};
  glslang_shader_t *shader=glslang_shader_create(&input);
  glslang_program_t *program=NULL; int ok=0;
  if(!shader) { diagnostic(log,log_capacity,"Cannot allocate shader"); return 0; }
  if(!glslang_shader_preprocess(shader,&input)) goto shader_error;
  input.code=glslang_shader_get_preprocessed_code(shader);
  if(!glslang_shader_parse(shader,&input)) goto shader_error;
  program=glslang_program_create();
  if(!program) { diagnostic(log,log_capacity,"Cannot allocate program"); goto done; }
  glslang_program_add_shader(program,shader);
  if(!glslang_program_link(program,input.messages)) {
    diagnostic(log,log_capacity,glslang_program_get_info_log(program)); goto done;
  }
  glslang_spv_options_t options={.disable_optimizer=true};
  glslang_program_SPIRV_generate_with_options(program,input.stage,&options);
  size_t count=glslang_program_SPIRV_get_size(program);
  if(count<5 || count>256u*1024u) {
    diagnostic(log,log_capacity,"SPIR-V size outside supported range"); goto done;
  }
  uint32_t *result=malloc(count*sizeof(*result));
  if(!result) { diagnostic(log,log_capacity,"Cannot allocate SPIR-V"); goto done; }
  glslang_program_SPIRV_get(program,result);
  *words=result; *bytes=count*sizeof(*result); ok=1;
  diagnostic(log,log_capacity,glslang_program_SPIRV_get_messages(program));
  goto done;
shader_error:
  diagnostic(log,log_capacity,glslang_shader_get_info_log(shader));
done:
  if(program) glslang_program_delete(program);
  glslang_shader_delete(shader);
  return ok;
}

int ps4glLinkVulkanGLSL(const char *vertex, const char *fragment,
                       uint32_t **vs, size_t *vs_bytes, uint32_t **fs, size_t *fs_bytes,
                       char *log, size_t capacity) {
  if(vs) *vs=NULL;
  if(fs) *fs=NULL;
  if(vs_bytes) *vs_bytes=0;
  if(fs_bytes) *fs_bytes=0;
  diagnostic(log,capacity,"");
  if(!initialized || !vertex || !fragment || !vs || !fs || !vs_bytes || !fs_bytes || vs==fs || vs_bytes==fs_bytes) {
    diagnostic(log,capacity,"Invalid linker state or arguments"); return 0;
  }
  const char *sources[]={vertex,fragment};
  glslang_shader_t *shaders[2]={NULL,NULL}; glslang_program_t *program=NULL;
  uint32_t *output[2]={NULL,NULL}; size_t sizes[2]={0,0}; int ok=0;
  for(unsigned stage=0;stage<2;++stage) {
    size_t length=0; while(length<=256u*1024u && sources[stage][length]) ++length;
    if(!length || length>256u*1024u) { diagnostic(log,capacity,"Shader source size outside 1..262144 bytes"); goto done; }
    glslang_input_t input={.language=GLSLANG_SOURCE_GLSL,
      .stage=stage ? GLSLANG_STAGE_FRAGMENT : GLSLANG_STAGE_VERTEX,
      .client=GLSLANG_CLIENT_VULKAN,.client_version=GLSLANG_TARGET_VULKAN_1_0,
      .target_language=GLSLANG_TARGET_SPV,.target_language_version=GLSLANG_TARGET_SPV_1_0,
      .code=sources[stage],.default_version=450,.default_profile=GLSLANG_CORE_PROFILE,
      .messages=GLSLANG_MSG_SPV_RULES_BIT|GLSLANG_MSG_VULKAN_RULES_BIT,.resource=glslang_default_resource(),
      .callbacks={.include_local=reject_include,.include_system=reject_include}};
    shaders[stage]=glslang_shader_create(&input);
    if(!shaders[stage]) { diagnostic(log,capacity,"Cannot allocate shader"); goto done; }
    if(!glslang_shader_preprocess(shaders[stage],&input)) {
      diagnostic(log,capacity,glslang_shader_get_info_log(shaders[stage])); goto done;
    }
    input.code=glslang_shader_get_preprocessed_code(shaders[stage]);
    if(!glslang_shader_parse(shaders[stage],&input)) {
      diagnostic(log,capacity,glslang_shader_get_info_log(shaders[stage])); goto done;
    }
  }
  program=glslang_program_create();
  if(!program) { diagnostic(log,capacity,"Cannot allocate program"); goto done; }
  for(unsigned i=0;i<2;++i) glslang_program_add_shader(program,shaders[i]);
  if(!glslang_program_link(program,GLSLANG_MSG_SPV_RULES_BIT|GLSLANG_MSG_VULKAN_RULES_BIT)) {
    diagnostic(log,capacity,glslang_program_get_info_log(program)); goto done;
  }
  for(unsigned i=0;i<2;++i) {
    glslang_spv_options_t options={.disable_optimizer=true};
    glslang_program_SPIRV_generate_with_options(program,i ? GLSLANG_STAGE_FRAGMENT : GLSLANG_STAGE_VERTEX,&options);
    size_t count=glslang_program_SPIRV_get_size(program);
    if(count<5 || count>256u*1024u) { diagnostic(log,capacity,"SPIR-V size outside supported range"); goto done; }
    sizes[i]=count*sizeof(uint32_t); output[i]=malloc(sizes[i]);
    if(!output[i]) { diagnostic(log,capacity,"Cannot allocate SPIR-V"); goto done; }
    glslang_program_SPIRV_get(program,output[i]);
  }
  if(!compatible_stages(output[0],sizes[0],output[1],sizes[1])) {
    diagnostic(log,capacity,"Missing, incompatible or unsupported inter-stage interface (float locations 0..15 required)"); goto done;
  }
  *vs=output[0]; *fs=output[1]; *vs_bytes=sizes[0]; *fs_bytes=sizes[1]; ok=1;
done:
  if(!ok) { free(output[0]); free(output[1]); }
  if(program) glslang_program_delete(program);
  for(unsigned i=0;i<2;++i) if(shaders[i]) glslang_shader_delete(shaders[i]);
  return ok;
}

#include "ps4gl-legacy.inc"
int ps4glCompileLegacyGLSL(int fragment, const char *source, uint32_t **words, size_t *bytes, char *log, size_t cap) {
  if(words) *words=NULL;
  if(bytes) *bytes=0;
  diagnostic(log,cap,"");
  if(!words || !bytes || (fragment!=0 && fragment!=1)) { diagnostic(log,cap,"Invalid legacy compiler arguments"); return 0; }
  LegacySource parsed={0}; int ok=0; char *translated=NULL;
  if(legacy_preprocess(fragment,source,&parsed,log,cap)) {
    translated=legacy_rewrite(&parsed,fragment,&parsed,&parsed,NULL,0,log,cap);
    if(translated) ok=ps4glCompileVulkanGLSL(fragment,translated,words,bytes,log,cap);
  }
  free(translated); free(parsed.text); return ok;
}
int ps4glLinkLegacyGLSL(const char *vertex, const char *fragment, const PS4GLAttributeBinding *bindings, size_t count,
                       uint32_t **vs, size_t *vb, uint32_t **fs, size_t *fb, char *log, size_t cap) {
  if(vs) *vs=NULL;
  if(fs) *fs=NULL;
  if(vb) *vb=0;
  if(fb) *fb=0;
  diagnostic(log,cap,"");
  if(!vs || !vb || !fs || !fb || vs==fs || vb==fb) { diagnostic(log,cap,"Invalid legacy linker arguments"); return 0; }
  char *v=NULL,*f=NULL;
  if(!legacy_pair(vertex,fragment,bindings,count,&v,&f,log,cap)) return 0;
  int ok=ps4glLinkVulkanGLSL(v,f,vs,vb,fs,fb,log,cap); free(v); free(f); return ok;
}
int ps4glLinkBoundVulkanGLSL(const char *v, const char *f, const PS4GLAttributeBinding *bindings, size_t count,
                            uint32_t **vs, size_t *vb, uint32_t **fs, size_t *fb, char *log, size_t cap) {
  /* Explicit GLSL locations take precedence over API bindings. */
  (void)bindings; (void)count; return ps4glLinkVulkanGLSL(v,f,vs,vb,fs,fb,log,cap);
}
