#include "ps4gl.h"
#include <string.h>
/* Explicit bootstrap import. The caller supplies a compatible color render pass.
 * Program code is compiled to GCN by the native Vulkan driver, not by the host. */
/* Structural/interface guard only; callers still supply validated Vulkan SPIR-V.
 * No descriptor resources; plain float vertex inputs are packed at draw time. */
static int supported_interface(const uint32_t *code, size_t bytes, unsigned *components) {
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
    if(components && storage==1) {
      int builtin=0; unsigned location=PS4GL_MAX_ATTRIBS;
      for(size_t j=5;j<words;j+=code[j]>>16)
        if((code[j]&0xffff)==71 && (code[j]>>16)>=4 &&
            code[j+1]==code[i+2] && code[j+2]==11) builtin=1;
      if(builtin) continue;
      for(size_t j=5;j<words;j+=code[j]>>16)
        if((code[j]&0xffff)==71 && (code[j]>>16)>=4 &&
           code[j+1]==code[i+2] && code[j+2]==30) location=code[j+3];
      if(location>=PS4GL_MAX_ATTRIBS || components[location]) return 0;
      uint32_t type=0, scalar=0; unsigned size=1;
      for(size_t j=5;j<words;j+=code[j]>>16)
        if((code[j]&0xffff)==32 && (code[j]>>16)==4 && code[j+1]==code[i+1]) type=code[j+3];
      if(!type) return 0;
      scalar=type;
      for(size_t j=5;j<words;j+=code[j]>>16)
        if((code[j]&0xffff)==23 && (code[j]>>16)==4 && code[j+1]==type) {
          scalar=code[j+2]; size=code[j+3];
        }
      if(!size || size>4) return 0;
      int is_float=0;
      for(size_t j=5;j<words;j+=code[j]>>16)
        if((code[j]&0xffff)==22 && (code[j]>>16)==3 && code[j+1]==scalar && code[j+2]==32) is_float=1;
      if(!is_float) return 0;
      components[location]=size;
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
      (ctx->program_device!=device || (ctx->program_pass && ctx->program_pass!=pass)))) goto fail;
  if(!vs || !fs || vs_size<20 || fs_size<20 || vs_size%4 || fs_size%4 ||
     vs[0]!=0x07230203 || fs[0]!=0x07230203) { err=GL_INVALID_VALUE; goto fail; }
  unsigned components[PS4GL_MAX_ATTRIBS]={0}, stride=0;
  if(!supported_interface(vs,vs_size,components) || !supported_interface(fs,fs_size,NULL)) goto fail;
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
  VkVertexInputAttributeDescription attributes[PS4GL_MAX_ATTRIBS];
  unsigned attribute_count=0;
  const VkFormat formats[]={VK_FORMAT_UNDEFINED,VK_FORMAT_R32_SFLOAT,VK_FORMAT_R32G32_SFLOAT,
    VK_FORMAT_R32G32B32_SFLOAT,VK_FORMAT_R32G32B32A32_SFLOAT};
  for(unsigned i=0;i<PS4GL_MAX_ATTRIBS;++i) if(components[i]) {
    attributes[attribute_count++]=(VkVertexInputAttributeDescription){i,0,formats[components[i]],stride};
    stride+=components[i]*4;
  }
  VkVertexInputBindingDescription binding={0,stride,VK_VERTEX_INPUT_RATE_VERTEX};
  if(attribute_count) {
    vi.vertexBindingDescriptionCount=1; vi.pVertexBindingDescriptions=&binding;
    vi.vertexAttributeDescriptionCount=attribute_count; vi.pVertexAttributeDescriptions=attributes;
  }
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
  ctx->programs[slot]=(PS4GLProgram){.pipeline=pipeline,.layout=layout,.vertex_stride=stride};
  for(unsigned i=0;i<PS4GL_MAX_ATTRIBS;++i) ctx->programs[slot].components[i]=components[i];
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
  ps4glReleaseVertexSnapshots(ctx);
  ctx->physical=VK_NULL_HANDLE; ctx->upload_vertices=NULL;
  ctx->program=0; ctx->program_device=VK_NULL_HANDLE; ctx->program_pass=VK_NULL_HANDLE;
}

static int upload_vertices(PS4GLContext *ctx, const void *data, size_t bytes, size_t vertex_bytes, GLenum index_type) {
  if(!ctx->physical || !ctx->program_device) {
    if(!ctx->error) ctx->error=GL_INVALID_OPERATION;
    return 0;
  }
  if(ctx->snapshot_count==PS4GL_MAX_SNAPSHOTS || bytes>64u*1024u*1024u-ctx->snapshot_bytes) {
    if(!ctx->error) ctx->error=GL_OUT_OF_MEMORY;
    return 0;
  }
  PS4GLSnapshot snap={0}; VkDevice device=ctx->program_device;
  VkBufferCreateInfo ci={.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,.size=bytes,.usage=VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_INDEX_BUFFER_BIT};
  VkResult result=vkCreateBuffer(device,&ci,NULL,&snap.buffer);
  if(result!=VK_SUCCESS) goto fail_upload;
  VkMemoryRequirements req;
  vkGetBufferMemoryRequirements(device,snap.buffer,&req);
  VkPhysicalDeviceMemoryProperties properties;
  vkGetPhysicalDeviceMemoryProperties(ctx->physical,&properties);
  uint32_t type=UINT32_MAX;
  for(uint32_t i=0;i<properties.memoryTypeCount;++i)
    if((req.memoryTypeBits&(1u<<i)) && (properties.memoryTypes[i].propertyFlags &
        (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
        (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) { type=i; break; }
  if(type==UINT32_MAX) goto fail_upload;
  VkMemoryAllocateInfo ai={.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,.allocationSize=req.size,.memoryTypeIndex=type};
  if(vkAllocateMemory(device,&ai,NULL,&snap.memory)!=VK_SUCCESS) goto fail_upload;
  if(vkBindBufferMemory(device,snap.buffer,snap.memory,0)!=VK_SUCCESS) goto fail_upload;
  void *mapped;
  if(vkMapMemory(device,snap.memory,0,VK_WHOLE_SIZE,0,&mapped)!=VK_SUCCESS) goto fail_upload;
  memcpy(mapped,data,bytes); vkUnmapMemory(device,snap.memory);
  ctx->snapshots[ctx->snapshot_count++]=snap; ctx->snapshot_bytes+=bytes;
  VkDeviceSize offset=0;
  if(vertex_bytes) vkCmdBindVertexBuffers(ctx->command,0,1,&snap.buffer,&offset);
  if(index_type) vkCmdBindIndexBuffer(ctx->command,snap.buffer,vertex_bytes,
      index_type==GL_UNSIGNED_SHORT ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
  return 1;
fail_upload:
  if(snap.buffer) vkDestroyBuffer(device,snap.buffer,NULL);
  if(snap.memory) vkFreeMemory(device,snap.memory,NULL);
  if(!ctx->error) ctx->error=GL_OUT_OF_MEMORY;
  return 0;
}
void ps4glConfigureVertexDevice(VkPhysicalDevice physical, VkDevice device) {
  PS4GLContext *ctx=ps4glCurrentContext();
  if(!ctx) return;
  if(!physical || !device || ctx->command || (ctx->program_device && ctx->program_device!=device) ||
      (ctx->physical && ctx->physical!=physical)) {
    if(!ctx->error) ctx->error=GL_INVALID_OPERATION;
    return;
  }
  ctx->physical=physical; ctx->program_device=device; ctx->upload_vertices=upload_vertices;
}
void ps4glReleaseVertexSnapshots(PS4GLContext *ctx) {
  if(!ctx) return;
  if(ctx->command) { if(!ctx->error) ctx->error=GL_INVALID_OPERATION; return; }
  for(unsigned i=0;i<ctx->snapshot_count;++i) {
    vkDestroyBuffer(ctx->program_device,ctx->snapshots[i].buffer,NULL);
    vkFreeMemory(ctx->program_device,ctx->snapshots[i].memory,NULL);
    ctx->snapshots[i]=(PS4GLSnapshot){0};
  }
  ctx->snapshot_count=0; ctx->snapshot_bytes=0;
}
