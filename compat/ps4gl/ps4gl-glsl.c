#include "ps4gl-glsl.h"
#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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
