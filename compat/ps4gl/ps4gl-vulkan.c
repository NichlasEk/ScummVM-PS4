#include "ps4gl.h"
/* Explicit bootstrap import. The caller supplies a compatible color render pass.
 * Program code is compiled to GCN by the native Vulkan driver, not by the host. */
/* Structural/interface guard only; callers still supply validated Vulkan SPIR-V.
 * No descriptor resources or vertex attributes can be bound by this bootstrap. */
static int supported_interface(const uint32_t *code, size_t bytes, int vertex) {
  size_t words=bytes/4;
  for(size_t i=5;i<words;) {
    uint32_t count=code[i]>>16, op=code[i]&0xffff;
    if(!count || count>words-i) return 0;
    i+=count;
    (void)op;
  }
  for(size_t i=5;i<words;i+=code[i]>>16) {
    uint32_t count=code[i]>>16, op=code[i]&0xffff;
    if(op!=59) continue; /* OpVariable */
    if(count<4) return 0;
    uint32_t storage=code[i+3];
    if(storage==0 || storage==2 || storage==9 || storage==12) return 0;
    if(vertex && storage==1) {
      int builtin=0;
      for(size_t j=5;j<words;j+=code[j]>>16)
        if((code[j]&0xffff)==71 && (code[j]>>16)>=4 &&
            code[j+1]==code[i+2] && code[j+2]==11) builtin=1;
      if(!builtin) return 0;
    }
  }
  return 1;
}
GLuint ps4glCreateProgramSPIRV(VkDevice device, VkRenderPass pass,
                             const uint32_t *vs, size_t vs_size,
                             const uint32_t *fs, size_t fs_size) {
  PS4GLContext *ctx=ps4glCurrentContext();
  if(!ctx) return 0;
  GLenum err=GL_INVALID_OPERATION;
  if(!device || !pass || ctx->command || (ctx->program_device &&
      (ctx->program_device!=device || ctx->program_pass!=pass))) goto fail;
  if(!vs || !fs || vs_size<20 || fs_size<20 || vs_size%4 || fs_size%4 ||
     vs[0]!=0x07230203 || fs[0]!=0x07230203) { err=GL_INVALID_VALUE; goto fail; }
  if(!supported_interface(vs,vs_size,1) || !supported_interface(fs,fs_size,0)) goto fail;
  unsigned slot;
  for(slot=0;slot<PS4GL_MAX_PROGRAMS;++slot) if(!ctx->programs[slot].pipeline) break;
  if(slot==PS4GL_MAX_PROGRAMS) { err=GL_OUT_OF_MEMORY; goto fail; }
  VkShaderModule modules[2]={VK_NULL_HANDLE,VK_NULL_HANDLE};
  VkPipelineLayout layout=VK_NULL_HANDLE;
  VkPipeline pipeline=VK_NULL_HANDLE;
  VkResult result;
  for(unsigned i=0;i<2;++i) {
    VkShaderModuleCreateInfo ci={.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize=i?fs_size:vs_size,.pCode=i?fs:vs};
    result=vkCreateShaderModule(device,&ci,NULL,&modules[i]);
    if(result!=VK_SUCCESS) goto cleanup;
  }
  VkPipelineLayoutCreateInfo lci={.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  result=vkCreatePipelineLayout(device,&lci,NULL,&layout);
  if(result!=VK_SUCCESS) goto cleanup;
  VkPipelineShaderStageCreateInfo stages[2]={
    {.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_VERTEX_BIT,.module=modules[0],.pName="main"},
    {.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_FRAGMENT_BIT,.module=modules[1],.pName="main"}};
  VkPipelineVertexInputStateCreateInfo vi={.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  VkPipelineInputAssemblyStateCreateInfo ia={.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
  VkPipelineViewportStateCreateInfo vp={.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,.viewportCount=1,.scissorCount=1};
  VkPipelineRasterizationStateCreateInfo rs={.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .polygonMode=VK_POLYGON_MODE_FILL,.cullMode=VK_CULL_MODE_NONE,.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE,.lineWidth=1};
  VkPipelineMultisampleStateCreateInfo ms={.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT};
  VkPipelineColorBlendAttachmentState ba={.colorWriteMask=15};
  VkPipelineColorBlendStateCreateInfo bs={.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,.attachmentCount=1,.pAttachments=&ba};
  VkDynamicState states[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo ds={.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,.dynamicStateCount=2,.pDynamicStates=states};
  VkGraphicsPipelineCreateInfo pci={.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,.stageCount=2,.pStages=stages,
    .pVertexInputState=&vi,.pInputAssemblyState=&ia,.pViewportState=&vp,.pRasterizationState=&rs,
    .pMultisampleState=&ms,.pColorBlendState=&bs,.pDynamicState=&ds,.layout=layout,.renderPass=pass};
  result=vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pci,NULL,&pipeline);
cleanup:
  for(unsigned i=0;i<2;++i) if(modules[i]) vkDestroyShaderModule(device,modules[i],NULL);
  if(result!=VK_SUCCESS) {
    if(pipeline) vkDestroyPipeline(device,pipeline,NULL);
    if(layout) vkDestroyPipelineLayout(device,layout,NULL);
    if(result==VK_ERROR_OUT_OF_HOST_MEMORY || result==VK_ERROR_OUT_OF_DEVICE_MEMORY) err=GL_OUT_OF_MEMORY;
    goto fail;
  }
  ctx->program_device=device; ctx->program_pass=pass;
  ctx->programs[slot]=(PS4GLProgram){.pipeline=pipeline,.layout=layout};
  return slot+1;
fail:
  if(!ctx->error) ctx->error=err;
  return 0;
}
void ps4glDestroyPrograms(VkDevice device, PS4GLContext *ctx) {
  if(!ctx || !device) return;
  if(ctx->command || (ctx->program_device && ctx->program_device!=device)) {
    if(!ctx->error) ctx->error=GL_INVALID_OPERATION;
    return;
  }
  for(unsigned i=0;i<PS4GL_MAX_PROGRAMS;++i) {
    if(ctx->programs[i].pipeline) vkDestroyPipeline(device,ctx->programs[i].pipeline,NULL);
    if(ctx->programs[i].layout) vkDestroyPipelineLayout(device,ctx->programs[i].layout,NULL);
    ctx->programs[i]=(PS4GLProgram){0};
  }
  ctx->program=0; ctx->program_device=VK_NULL_HANDLE; ctx->program_pass=VK_NULL_HANDLE;
}
