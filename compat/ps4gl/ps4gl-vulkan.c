#include "ps4gl.h"
#include <string.h>
#include "ps4gl-reflect.h"
#include "ps4gl-texture-vulkan.h"
/* Explicit bootstrap import. The caller supplies a compatible color render pass.
 * Program code is compiled to GCN by the native Vulkan driver, not by the host. */
/* Structural/interface guard only; callers still supply validated Vulkan SPIR-V.
 * Bounded default-uniform blocks and plain float vertex inputs. */
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
    if((storage==0 && components) || storage==9 || storage==12) return 0;
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
static VkBlendFactor native_factor(GLenum factor) {
  switch(factor) {
    case GL_ZERO:return VK_BLEND_FACTOR_ZERO; case GL_ONE:return VK_BLEND_FACTOR_ONE;
    case GL_SRC_COLOR:return VK_BLEND_FACTOR_SRC_COLOR; case GL_ONE_MINUS_SRC_COLOR:return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case GL_DST_COLOR:return VK_BLEND_FACTOR_DST_COLOR; case GL_ONE_MINUS_DST_COLOR:return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case GL_SRC_ALPHA:return VK_BLEND_FACTOR_SRC_ALPHA; case GL_ONE_MINUS_SRC_ALPHA:return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case GL_DST_ALPHA:return VK_BLEND_FACTOR_DST_ALPHA; default:return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
  }
}
static VkBlendOp native_equation(GLenum equation) {
  switch(equation) {
    case GL_FUNC_SUBTRACT:return VK_BLEND_OP_SUBTRACT; case GL_FUNC_REVERSE_SUBTRACT:return VK_BLEND_OP_REVERSE_SUBTRACT;
    case GL_MIN:return VK_BLEND_OP_MIN; case GL_MAX:return VK_BLEND_OP_MAX; default:return VK_BLEND_OP_ADD;
  }
}
static VkResult create_pipeline(VkDevice device, VkRenderPass pass, const PS4GLProgram *p,
                                const VkShaderModule *modules, const PS4GLBlend *blend, VkPipeline *out) {
  const unsigned *components=p->components; unsigned stride=0;
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
  VkPipelineColorBlendAttachmentState ba={.colorWriteMask=15,.blendEnable=blend->enabled,
    .srcColorBlendFactor=native_factor(blend->src_rgb),.dstColorBlendFactor=native_factor(blend->dst_rgb),
    .srcAlphaBlendFactor=native_factor(blend->src_alpha),.dstAlphaBlendFactor=native_factor(blend->dst_alpha),
    .colorBlendOp=native_equation(blend->equation_rgb),.alphaBlendOp=native_equation(blend->equation_alpha)};
  VkPipelineColorBlendStateCreateInfo bs={.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,.attachmentCount=1,.pAttachments=&ba};
  VkDynamicState states[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo ds={.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,.dynamicStateCount=2,.pDynamicStates=states};
  VkGraphicsPipelineCreateInfo pci={.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,.stageCount=2,.pStages=stages,
    .pVertexInputState=&vi,.pInputAssemblyState=&ia,.pViewportState=&vp,.pRasterizationState=&rs,
    .pMultisampleState=&ms,.pColorBlendState=&bs,.pDynamicState=&ds,.layout=p->layout,.renderPass=pass};
  return vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pci,NULL,out);
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
  PS4GLProgram reflected={0};
  if(!gl_reflect_uniforms(vs,vs_size,0,&reflected) || !gl_reflect_uniforms(fs,fs_size,1,&reflected)) goto fail;
  if(!gl_reflect_samplers(fs,fs_size,1,&reflected)) goto fail;
  unsigned slot;
  for(slot=0;slot<PS4GL_MAX_PROGRAMS;++slot) if(!ctx->programs[slot].pipeline) break;
  if(slot==PS4GL_MAX_PROGRAMS) { err=GL_OUT_OF_MEMORY; goto fail; }
  VkShaderModule modules[2]={VK_NULL_HANDLE,VK_NULL_HANDLE};
  VkPipelineLayout layout=VK_NULL_HANDLE;
  VkPipeline pipeline=VK_NULL_HANDLE;
  VkDescriptorSetLayout descriptors=VK_NULL_HANDLE;
  VkResult result;
  for(unsigned i=0;i<2;++i) {
    VkShaderModuleCreateInfo ci={.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize=i?fs_size:vs_size,.pCode=i?fs:vs};
    result=vkCreateShaderModule(device,&ci,NULL,&modules[i]);
    if(result!=VK_SUCCESS) goto cleanup;
  }
  VkPipelineLayoutCreateInfo lci={.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  if(reflected.uniform_stages || reflected.sampler_mask) {
    VkDescriptorSetLayoutBinding bindings[4]; unsigned count=0;
    for(unsigned stage=0;stage<2;++stage) if(reflected.uniform_stages&(1u<<stage))
      bindings[count++]=(VkDescriptorSetLayoutBinding){.binding=stage,.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount=1,.stageFlags=stage ? VK_SHADER_STAGE_FRAGMENT_BIT : VK_SHADER_STAGE_VERTEX_BIT};
    for(unsigned sampler=0;sampler<2;++sampler) if(reflected.sampler_mask&(1u<<sampler))
      bindings[count++]=(VkDescriptorSetLayoutBinding){.binding=sampler+2,.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount=1,.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT};
    VkDescriptorSetLayoutCreateInfo dci={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount=count,.pBindings=bindings};
    result=vkCreateDescriptorSetLayout(device,&dci,NULL,&descriptors);
    if(result!=VK_SUCCESS) goto cleanup;
    lci.setLayoutCount=1; lci.pSetLayouts=&descriptors;
  }
  result=vkCreatePipelineLayout(device,&lci,NULL,&layout);
  if(result!=VK_SUCCESS) goto cleanup;
  reflected.layout=layout;
  for(unsigned i=0;i<PS4GL_MAX_ATTRIBS;++i) { reflected.components[i]=components[i]; stride+=components[i]*4; }
  PS4GLBlend disabled={.src_rgb=GL_ONE,.src_alpha=GL_ONE,.equation_rgb=GL_FUNC_ADD,.equation_alpha=GL_FUNC_ADD};
  result=create_pipeline(device,pass,&reflected,modules,&disabled,&pipeline);
cleanup:
  if(result!=VK_SUCCESS) {
    for(unsigned i=0;i<2;++i) if(modules[i]) vkDestroyShaderModule(device,modules[i],NULL);
    if(pipeline) vkDestroyPipeline(device,pipeline,NULL);
    if(layout) vkDestroyPipelineLayout(device,layout,NULL);
    if(descriptors) vkDestroyDescriptorSetLayout(device,descriptors,NULL);
    if(result==VK_ERROR_OUT_OF_HOST_MEMORY || result==VK_ERROR_OUT_OF_DEVICE_MEMORY) err=GL_OUT_OF_MEMORY;
    goto fail;
  }
  ctx->program_device=device; ctx->program_pass=pass;
  reflected.pipeline=pipeline; reflected.layout=layout; reflected.vertex_stride=stride; reflected.descriptors=descriptors;
  for(unsigned i=0;i<2;++i) reflected.modules[i]=modules[i];
  ctx->programs[slot]=reflected;
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
    for(unsigned v=0;v<ctx->programs[i].variant_count;++v) vkDestroyPipeline(device,ctx->programs[i].variants[v].pipeline,NULL);
    for(unsigned stage=0;stage<2;++stage) if(ctx->programs[i].modules[stage]) vkDestroyShaderModule(device,ctx->programs[i].modules[stage],NULL);
    if(ctx->programs[i].pipeline) vkDestroyPipeline(device,ctx->programs[i].pipeline,NULL);
    if(ctx->programs[i].layout) vkDestroyPipelineLayout(device,ctx->programs[i].layout,NULL);
    if(ctx->programs[i].descriptors) vkDestroyDescriptorSetLayout(device,ctx->programs[i].descriptors,NULL);
    ctx->programs[i]=(PS4GLProgram){0};
  }
  ps4glReleaseVertexSnapshots(ctx);
  ctx->physical=VK_NULL_HANDLE; ctx->upload_vertices=NULL; ctx->upload_uniforms=NULL; ctx->select_pipeline=NULL;
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
  VkBufferCreateInfo ci={.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,.size=bytes,.usage=VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_INDEX_BUFFER_BIT|VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT};
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
static int upload_uniforms(PS4GLContext *ctx, const PS4GLProgram *p) {
  size_t texture_bytes=0;
  const PS4GLTexture *sources[2]={0};
  for(unsigned i=0;i<2;++i) if(p->sampler_mask&(1u<<i)) {
    sources[i]=ps4glTextureAtUnit(ctx,(unsigned)p->sampler_units[i]);
    if(!sources[i] || !sources[i]->pixels || sources[i]->width<=0 || sources[i]->height<=0 ||
        (sources[i]->min_filter!=GL_NEAREST && sources[i]->min_filter!=GL_LINEAR)) {
      if(!ctx->error) ctx->error=GL_INVALID_OPERATION;
      return 0;
    }
    texture_bytes+=(size_t)sources[i]->width*sources[i]->height*4;
  }
  if(texture_bytes+sizeof(p->uniform_data)>64u*1024u*1024u-ctx->snapshot_bytes) {
    if(!ctx->error) ctx->error=GL_OUT_OF_MEMORY;
    return 0;
  }
  /* Fixed 256-byte blocks match the native device's uniform offset alignment. */
  if(!upload_vertices(ctx,p->uniform_data,sizeof(p->uniform_data),0,0)) return 0;
  PS4GLSnapshot *snap=&ctx->snapshots[ctx->snapshot_count-1];
  VkDescriptorPoolSize sizes[]={{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,2},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2}};
  VkDescriptorPoolCreateInfo ci={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .maxSets=1,.poolSizeCount=2,.pPoolSizes=sizes};
  if(vkCreateDescriptorPool(ctx->program_device,&ci,NULL,&snap->pool)!=VK_SUCCESS) goto fail;
  VkDescriptorSet set;
  VkDescriptorSetAllocateInfo ai={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool=snap->pool,.descriptorSetCount=1,.pSetLayouts=&p->descriptors};
  if(vkAllocateDescriptorSets(ctx->program_device,&ai,&set)!=VK_SUCCESS) goto fail;
  for(unsigned stage=0;stage<2;++stage) if(p->uniform_stages&(1u<<stage)) {
    VkDescriptorBufferInfo info={snap->buffer,stage*PS4GL_UNIFORM_BYTES,PS4GL_UNIFORM_BYTES};
    VkWriteDescriptorSet write={.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=set,
      .dstBinding=stage,.descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,.pBufferInfo=&info};
    vkUpdateDescriptorSets(ctx->program_device,1,&write,0,NULL);
  }
  for(unsigned i=0;i<2;++i) if(sources[i]) {
    if(!texture_snapshot(ctx,sources[i],&snap->textures[i])) goto fail;
    VkDescriptorImageInfo info={snap->textures[i].sampler,snap->textures[i].view,VK_IMAGE_LAYOUT_GENERAL};
    VkWriteDescriptorSet write={.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=set,.dstBinding=i+2,
      .descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,.pImageInfo=&info};
    vkUpdateDescriptorSets(ctx->program_device,1,&write,0,NULL);
  }
  ctx->snapshot_bytes+=texture_bytes;
  vkCmdBindDescriptorSets(ctx->command,VK_PIPELINE_BIND_POINT_GRAPHICS,p->layout,0,1,&set,0,NULL);
  return 1;
fail:
  for(unsigned i=0;i<2;++i) destroy_texture_snapshot(ctx->program_device,&snap->textures[i]);
  if(snap->pool) vkDestroyDescriptorPool(ctx->program_device,snap->pool,NULL);
  vkDestroyBuffer(ctx->program_device,snap->buffer,NULL);
  vkFreeMemory(ctx->program_device,snap->memory,NULL);
  *snap=(PS4GLSnapshot){0}; --ctx->snapshot_count; ctx->snapshot_bytes-=sizeof(p->uniform_data);
  if(!ctx->error) ctx->error=GL_OUT_OF_MEMORY;
  return 0;
}
static VkPipeline select_pipeline(PS4GLContext *ctx, PS4GLProgram *p) {
  for(unsigned i=0;i<p->variant_count;++i)
    if(!memcmp(&p->variants[i].state,&ctx->blend,sizeof(ctx->blend))) return p->variants[i].pipeline;
  if(p->variant_count==PS4GL_BLEND_VARIANTS) {
    if(!ctx->error) ctx->error=GL_OUT_OF_MEMORY;
    return VK_NULL_HANDLE;
  }
  VkPipeline pipeline=VK_NULL_HANDLE;
  VkResult result=create_pipeline(ctx->program_device,ctx->program_pass,p,p->modules,&ctx->blend,&pipeline);
  if(result!=VK_SUCCESS) {
    if(pipeline) vkDestroyPipeline(ctx->program_device,pipeline,NULL);
    if(!ctx->error) ctx->error=result==VK_ERROR_OUT_OF_HOST_MEMORY || result==VK_ERROR_OUT_OF_DEVICE_MEMORY ? GL_OUT_OF_MEMORY : GL_INVALID_OPERATION;
    return VK_NULL_HANDLE;
  }
  p->variants[p->variant_count++]=(PS4GLBlendVariant){ctx->blend,pipeline};
  return pipeline;
}
void ps4glConfigureVertexDevice(VkPhysicalDevice physical, VkDevice device) {
  PS4GLContext *ctx=ps4glCurrentContext();
  if(!ctx) return;
  if(!physical || !device || ctx->command || (ctx->program_device && ctx->program_device!=device) ||
      (ctx->physical && ctx->physical!=physical)) {
    if(!ctx->error) ctx->error=GL_INVALID_OPERATION;
    return;
  }
  ctx->physical=physical; ctx->program_device=device; ctx->upload_vertices=upload_vertices; ctx->upload_uniforms=upload_uniforms; ctx->select_pipeline=select_pipeline;
}
void ps4glReleaseVertexSnapshots(PS4GLContext *ctx) {
  if(!ctx) return;
  if(ctx->command) { if(!ctx->error) ctx->error=GL_INVALID_OPERATION; return; }
  for(unsigned i=0;i<ctx->snapshot_count;++i) {
    if(ctx->snapshots[i].pool) vkDestroyDescriptorPool(ctx->program_device,ctx->snapshots[i].pool,NULL);
    for(unsigned t=0;t<2;++t) destroy_texture_snapshot(ctx->program_device,&ctx->snapshots[i].textures[t]);
    vkDestroyBuffer(ctx->program_device,ctx->snapshots[i].buffer,NULL);
    vkFreeMemory(ctx->program_device,ctx->snapshots[i].memory,NULL);
    ctx->snapshots[i]=(PS4GLSnapshot){0};
  }
  ctx->snapshot_count=0; ctx->snapshot_bytes=0;
}
