#include "ps4gl.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "ps4gl-reflect.h"
#include "ps4gl-texture-vulkan.h"
static void collect_targets(PS4GLContext *, int);
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
static VkStencilOp native_stencil(GLenum op) {
  switch(op) { case GL_ZERO:return VK_STENCIL_OP_ZERO; case GL_REPLACE:return VK_STENCIL_OP_REPLACE;
  case GL_INCR:return VK_STENCIL_OP_INCREMENT_AND_CLAMP; case GL_DECR:return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
  case GL_INVERT:return VK_STENCIL_OP_INVERT; case GL_INCR_WRAP:return VK_STENCIL_OP_INCREMENT_AND_WRAP;
  case GL_DECR_WRAP:return VK_STENCIL_OP_DECREMENT_AND_WRAP; default:return VK_STENCIL_OP_KEEP; }
}
static VkResult create_pipeline(VkDevice device, VkRenderPass pass, const PS4GLProgram *p,
                                const VkShaderModule *modules, const PS4GLBlend *blend, const PS4GLRaster *raster, VkPipeline *out) {
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
  VkPipelineColorBlendAttachmentState ba={.colorWriteMask=raster->color_mask,.blendEnable=blend->enabled,
    .srcColorBlendFactor=native_factor(blend->src_rgb),.dstColorBlendFactor=native_factor(blend->dst_rgb),
    .srcAlphaBlendFactor=native_factor(blend->src_alpha),.dstAlphaBlendFactor=native_factor(blend->dst_alpha),
    .colorBlendOp=native_equation(blend->equation_rgb),.alphaBlendOp=native_equation(blend->equation_alpha)};
  VkPipelineColorBlendStateCreateInfo bs={.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,.attachmentCount=1,.pAttachments=&ba};
  VkDynamicState states[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo ds={.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,.dynamicStateCount=2,.pDynamicStates=states};
  VkStencilOpState stencil={.failOp=native_stencil(raster->stencil_fail),.passOp=native_stencil(raster->stencil_pass),
    .depthFailOp=native_stencil(raster->stencil_depth_fail),.compareOp=(VkCompareOp)(raster->stencil_func-GL_NEVER),
    .compareMask=raster->stencil_mask,.writeMask=raster->stencil_write_mask,.reference=raster->stencil_ref};
  VkStencilOpState back_stencil={.failOp=native_stencil(raster->back_stencil_fail),.passOp=native_stencil(raster->back_stencil_pass),
    .depthFailOp=native_stencil(raster->back_stencil_depth_fail),.compareOp=(VkCompareOp)(raster->back_stencil_func-GL_NEVER),
    .compareMask=raster->back_stencil_mask,.writeMask=raster->back_stencil_write_mask,.reference=raster->back_stencil_ref};
  VkPipelineDepthStencilStateCreateInfo depth={.sType=VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .depthTestEnable=raster->depth_enabled,.depthWriteEnable=raster->depth_write,
    .depthCompareOp=(VkCompareOp)(raster->depth_func-GL_NEVER),.stencilTestEnable=raster->stencil_enabled,
    .front=stencil,.back=back_stencil,.maxDepthBounds=1.f};
  VkGraphicsPipelineCreateInfo pci={.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,.stageCount=2,.pStages=stages,
    .pVertexInputState=&vi,.pInputAssemblyState=&ia,.pViewportState=&vp,.pRasterizationState=&rs,
    .pMultisampleState=&ms,.pDepthStencilState=&depth,.pColorBlendState=&bs,.pDynamicState=&ds,.layout=p->layout,.renderPass=pass};
  return vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pci,NULL,out);
}
GLuint ps4glCreateProgramSPIRV(VkDevice device, VkRenderPass pass,
                             const uint32_t *vs, size_t vs_size,
                             const uint32_t *fs, size_t fs_size) {
  PS4GLContext *ctx=ps4glCurrentContext();
  if(!ctx) return 0;
  GLenum err=GL_INVALID_OPERATION;
  if(!device || !pass || (ctx->program_device &&
      (ctx->program_device!=device || (ctx->program_pass && ctx->program_pass!=pass)))) goto fail;
  if(!vs || !fs || vs_size<20 || fs_size<20 || vs_size%4 || fs_size%4 ||
     vs[0]!=0x07230203 || fs[0]!=0x07230203) { err=GL_INVALID_VALUE; goto fail; }
  unsigned components[PS4GL_MAX_ATTRIBS]={0}, stride=0;
  if(!supported_interface(vs,vs_size,components) || !supported_interface(fs,fs_size,NULL)) goto fail;
  PS4GLProgram reflected={0};
  if(!gl_reflect_uniforms(vs,vs_size,0,&reflected) || !gl_reflect_uniforms(fs,fs_size,1,&reflected)) goto fail;
  if(!gl_reflect_samplers(fs,fs_size,1,&reflected)) goto fail;
  unsigned slot;
  if(ctx->link_destination) {
    slot=ctx->link_destination-1;
    if(slot>=PS4GL_MAX_PROGRAMS || ctx->programs[slot].pipeline || !ctx->programs[slot].allocated) goto fail;
  } else for(slot=0;slot<PS4GL_MAX_PROGRAMS;++slot) if(!ctx->programs[slot].pipeline && !ctx->programs[slot].allocated) break;
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
    VkDescriptorSetLayoutBinding bindings[2+PS4GL_MAX_SAMPLERS]; unsigned count=0;
    for(unsigned stage=0;stage<2;++stage) if(reflected.uniform_stages&(1u<<stage))
      bindings[count++]=(VkDescriptorSetLayoutBinding){.binding=stage,.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount=1,.stageFlags=stage ? VK_SHADER_STAGE_FRAGMENT_BIT : VK_SHADER_STAGE_VERTEX_BIT};
    for(unsigned sampler=0;sampler<PS4GL_MAX_SAMPLERS;++sampler) if(reflected.sampler_mask&(1u<<sampler))
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
  /* Read compiler-emitted input names at explicit locations for GL queries. */
  for(size_t i=5;i<vs_size/4;i+=vs[i]>>16) if((vs[i]&0xffff)==59 && (vs[i]>>16)>=4 && vs[i+3]==1) {
    unsigned location=PS4GL_MAX_ATTRIBS; const char *name=NULL;
    for(size_t j=5;j<vs_size/4;j+=vs[j]>>16) {
      if((vs[j]&0xffff)==71 && (vs[j]>>16)>=4 && vs[j+1]==vs[i+2] && vs[j+2]==30) location=vs[j+3];
      if((vs[j]&0xffff)==5 && (vs[j]>>16)>=3 && vs[j+1]==vs[i+2]) {
        const char *candidate=(const char *)&vs[j+2]; size_t bytes=((vs[j]>>16)-2)*4;
        const char *end=memchr(candidate,0,bytes);
        if(end && (size_t)(end-candidate)<64) name=candidate;
      }
    }
    if(location<PS4GL_MAX_ATTRIBS && name) strcpy(reflected.attribute_names[location],name);
  }
  reflected.layout=layout;
  for(unsigned i=0;i<PS4GL_MAX_ATTRIBS;++i) { reflected.components[i]=components[i]; stride+=components[i]*4; }
  PS4GLBlend disabled={.src_rgb=GL_ONE,.src_alpha=GL_ONE,.equation_rgb=GL_FUNC_ADD,.equation_alpha=GL_FUNC_ADD};
  const PS4GLRaster raster=PS4GL_RASTER_DEFAULT;
  result=create_pipeline(device,pass,&reflected,modules,&disabled,&raster,&pipeline);
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
  for(unsigned i=0;i<PS4GL_MAX_QUERIES;++i) {
    if(ctx->queries[i].pool) vkDestroyQueryPool(device,ctx->queries[i].pool,NULL);
    ctx->queries[i]=(PS4GLQuery){0};
  }
  ctx->query_command=NULL;ctx->complete_queries=NULL;ctx->active_query=0;
  ps4glDestroyShaderObjects(ctx);
  for(unsigned i=0;i<PS4GL_MAX_PROGRAMS;++i) {
    for(unsigned v=0;v<ctx->programs[i].variant_count;++v) vkDestroyPipeline(device,ctx->programs[i].variants[v].pipeline,NULL);
    for(unsigned stage=0;stage<2;++stage) if(ctx->programs[i].modules[stage]) vkDestroyShaderModule(device,ctx->programs[i].modules[stage],NULL);
    if(ctx->programs[i].pipeline) vkDestroyPipeline(device,ctx->programs[i].pipeline,NULL);
    if(ctx->programs[i].layout) vkDestroyPipelineLayout(device,ctx->programs[i].layout,NULL);
    if(ctx->programs[i].descriptors) vkDestroyDescriptorSetLayout(device,ctx->programs[i].descriptors,NULL);
    ctx->programs[i]=(PS4GLProgram){0};
  }
  ps4glReleaseVertexSnapshots(ctx);
  retire_texture_cache(ctx,1);
  collect_targets(ctx,1);
  if(ctx->offscreen_pass) vkDestroyRenderPass(device,ctx->offscreen_pass,NULL);
  if(ctx->offscreen_depth_pass) vkDestroyRenderPass(device,ctx->offscreen_depth_pass,NULL);
  ctx->offscreen_depth_pass=VK_NULL_HANDLE; ctx->offscreen_pass=VK_NULL_HANDLE; ctx->prepare_target=NULL; ctx->end_target=NULL;
  ctx->physical=VK_NULL_HANDLE; ctx->copy_texture=NULL; ctx->read_pixels=NULL; ctx->upload_vertices=NULL; ctx->upload_uniforms=NULL; ctx->select_pipeline=NULL; ctx->import_program=NULL;
  ctx->program=0; ctx->program_device=VK_NULL_HANDLE; ctx->program_pass=VK_NULL_HANDLE;
}

static int allocation_failure(PS4GLContext *ctx, const char *reason) {
  if(!ctx->allocation_failure) {
    ctx->allocation_failure=reason;
    fprintf(stderr,"PS4GL allocation rejected: %s snapshots=%u payload=%zu reserved=%zu pages=%u textures=%zu targets=%zu\n",
      reason,ctx->snapshot_count,ctx->snapshot_bytes,ctx->upload_memory_bytes,ctx->upload_page_count,ctx->texture_bytes,ctx->target_bytes);
  }
  if(!ctx->error) ctx->error=GL_OUT_OF_MEMORY;
  return 0;
}
static int reserve_snapshot(PS4GLContext *ctx) {
  if(ctx->snapshot_count==PS4GL_MAX_SNAPSHOTS) return allocation_failure(ctx,"snapshot record limit");
  if(ctx->snapshot_count<ctx->snapshot_capacity) return 1;
  unsigned capacity=ctx->snapshot_capacity ? ctx->snapshot_capacity*2 : 64;
  if(capacity>PS4GL_MAX_SNAPSHOTS) capacity=PS4GL_MAX_SNAPSHOTS;
  PS4GLSnapshot *records=realloc(ctx->snapshots,capacity*sizeof(*records));
  if(!records) return allocation_failure(ctx,"snapshot host allocation");
  ctx->snapshots=records; ctx->snapshot_capacity=capacity;
  return 1;
}
static PS4GLUploadPage *create_upload_page(PS4GLContext *ctx, size_t bytes) {
  if(ctx->upload_page_count==PS4GL_MAX_UPLOAD_PAGES || bytes>PS4GL_UPLOAD_BUDGET) {
    allocation_failure(ctx,"upload page limit"); return NULL;
  }
  size_t size=bytes<PS4GL_UPLOAD_PAGE_BYTES ? PS4GL_UPLOAD_PAGE_BYTES : (bytes+65535u)&~(size_t)65535u;
  PS4GLUploadPage page={.size=size}; VkDevice device=ctx->program_device;
  VkBufferCreateInfo ci={.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,.size=size,
    .usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_INDEX_BUFFER_BIT|VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT};
  if(vkCreateBuffer(device,&ci,NULL,&page.buffer)!=VK_SUCCESS) goto fail;
  VkMemoryRequirements req; vkGetBufferMemoryRequirements(device,page.buffer,&req);
  if(req.size>PS4GL_UPLOAD_BUDGET-ctx->upload_memory_bytes) {
    allocation_failure(ctx,"upload reserved-byte budget"); goto fail;
  }
  VkPhysicalDeviceMemoryProperties properties; vkGetPhysicalDeviceMemoryProperties(ctx->physical,&properties);
  uint32_t type=UINT32_MAX;
  for(uint32_t i=0;i<properties.memoryTypeCount;++i) if((req.memoryTypeBits&(1u<<i)) &&
      (properties.memoryTypes[i].propertyFlags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==
      (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) { type=i; break; }
  if(type==UINT32_MAX) goto fail;
  VkMemoryAllocateInfo ai={.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,.allocationSize=req.size,.memoryTypeIndex=type};
  if(vkAllocateMemory(device,&ai,NULL,&page.memory)!=VK_SUCCESS ||
     vkBindBufferMemory(device,page.buffer,page.memory,0)!=VK_SUCCESS ||
     vkMapMemory(device,page.memory,0,VK_WHOLE_SIZE,0,&page.mapped)!=VK_SUCCESS) goto fail;
  page.allocation_bytes=(size_t)req.size;
  ctx->upload_memory_bytes+=page.allocation_bytes;
  ctx->upload_pages[ctx->upload_page_count++]=page;
  return &ctx->upload_pages[ctx->upload_page_count-1];
fail:
  if(page.buffer) vkDestroyBuffer(device,page.buffer,NULL);
  if(page.memory) vkFreeMemory(device,page.memory,NULL);
  allocation_failure(ctx,"Vulkan upload allocation"); return NULL;
}
static int upload_vertices(PS4GLContext *ctx, const void *data, size_t bytes, size_t vertex_bytes, GLenum index_type) {
  if(!ctx->physical || !ctx->program_device || !bytes || !data) {
    if(!ctx->error) ctx->error=GL_INVALID_OPERATION;
    return 0;
  }
  if(bytes>PS4GL_UPLOAD_BUDGET-ctx->snapshot_bytes) return allocation_failure(ctx,"frame payload budget");
  if(!reserve_snapshot(ctx)) return 0;
  PS4GLUploadPage *page=NULL; size_t offset=0;
  for(unsigned i=0;i<ctx->upload_page_count;++i) {
    size_t aligned=(ctx->upload_pages[i].used+255u)&~(size_t)255u;
    if(aligned<=ctx->upload_pages[i].size && bytes<=ctx->upload_pages[i].size-aligned) {
      page=&ctx->upload_pages[i]; offset=aligned; break;
    }
  }
  if(!page) { page=create_upload_page(ctx,bytes); if(!page) return 0; }
  memcpy((unsigned char *)page->mapped+offset,data,bytes);
  page->used=offset+bytes;
  PS4GLSnapshot snap={.buffer=page->buffer,.offset=offset};
  ctx->snapshots[ctx->snapshot_count++]=snap; ctx->snapshot_bytes+=bytes;
  VkDeviceSize native_offset=offset;
  if(vertex_bytes) vkCmdBindVertexBuffers(ctx->command,0,1,&snap.buffer,&native_offset);
  if(index_type) vkCmdBindIndexBuffer(ctx->command,snap.buffer,offset+vertex_bytes,
      index_type==GL_UNSIGNED_SHORT ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
  return 1;
}
#include "ps4gl-framebuffer-vulkan.h"
#include "ps4gl-readback-vulkan.h"
#include "ps4gl-copy-vulkan.h"
#include "ps4gl-query-vulkan.h"
static int upload_uniforms(PS4GLContext *ctx, const PS4GLProgram *p) {
  size_t texture_bytes=0;
  const PS4GLTexture *sources[PS4GL_MAX_SAMPLERS]={0};
  for(unsigned i=0;i<PS4GL_MAX_SAMPLERS;++i) if(p->sampler_mask&(1u<<i)) {
    sources[i]=(p->cube_sampler_mask&(1u<<i)) ? ps4glCubeAtUnit(ctx,(unsigned)p->sampler_units[i]) : (p->one_dimensional_sampler_mask&(1u<<i)) ? ps4glOneDimensionalAtUnit(ctx,(unsigned)p->sampler_units[i]) : (p->rectangle_sampler_mask&(1u<<i)) ? ps4glRectangleAtUnit(ctx,(unsigned)p->sampler_units[i]) : ps4glTextureAtUnit(ctx,(unsigned)p->sampler_units[i]);
    if(!sources[i] && !((p->cube_sampler_mask|p->one_dimensional_sampler_mask|p->rectangle_sampler_mask)&(1u<<i)))
      sources[i]=unbound_texture_2d();
    if((sources[i] && ctx->framebuffer && sources[i]==ps4glFramebufferTextureObject(ctx)) || !sources[i] || !sources[i]->pixels || sources[i]->width<=0 || sources[i]->height<=0 ||
        (sources[i]->min_filter!=GL_NEAREST && sources[i]->min_filter!=GL_LINEAR)) {
      static unsigned failures[PS4GL_MAX_PROGRAMS]; if(ctx->program<PS4GL_MAX_PROGRAMS && failures[ctx->program]++<16) fprintf(stderr,"HPL sampler rejected program=%u slot=%u unit=%d tex=%u size=%dx%d filter=%x\n",ctx->program,i,p->sampler_units[i],sources[i]?sources[i]->name:0,sources[i]?sources[i]->width:0,sources[i]?sources[i]->height:0,sources[i]?sources[i]->min_filter:0);
      if(!ctx->error) ctx->error=GL_INVALID_OPERATION;
      return 0;
    }
    if((p->cube_sampler_mask&(1u<<i)) && sources[i]->cube_faces!=63) {if(!ctx->error)ctx->error=GL_INVALID_OPERATION;return 0;}
    int duplicate=0;for(unsigned j=0;j<i;++j) if(sources[j]==sources[i]) duplicate=1;
    if(!sources[i]->render_target && !find_texture_snapshot(ctx,sources[i]) && !duplicate)
      texture_bytes+=(size_t)sources[i]->width*sources[i]->height*4;
  }
  static unsigned trace_draws;
  if(trace_draws++<120) {fprintf(stderr,"HPL draw p=%u depth=%u/%x blend=%u mask=%u tex",ctx->program,ctx->raster.depth_enabled,ctx->raster.depth_func,ctx->blend.enabled,ctx->raster.color_mask);for(unsigned i=0;i<PS4GL_MAX_SAMPLERS;++i) if(sources[i]) fprintf(stderr," %u:u%d:t%u:%dx%d",i,p->sampler_units[i],sources[i]->name,sources[i]->width,sources[i]->height);fprintf(stderr,"\n");}
  /* Compact each reflected stage separately, preserving std140 offsets and
   * the native device's 256-byte uniform-buffer offset alignment. */
  size_t stage_bytes[2]={0,0};
  for(unsigned i=0;i<p->uniform_count;++i) {
    const PS4GLUniform *u=&p->uniforms[i];
    if(u->sampler_binding) continue;
    for(unsigned st=0;st<2;++st) if(u->offset[st]>=0) {
      size_t end=(size_t)u->offset[st]+u->width*4;
      if(u->array_length) end+=(u->array_length-1)*u->array_stride[st];
      if(end>PS4GL_UNIFORM_BYTES) return allocation_failure(ctx,"uniform reflection bounds");
      if(end>stage_bytes[st]) stage_bytes[st]=end;
    }
  }
  for(unsigned st=0;st<2;++st) stage_bytes[st]=(stage_bytes[st]+255u)&~(size_t)255u;
  size_t uniform_bytes=stage_bytes[0]+stage_bytes[1];
  if(!uniform_bytes) uniform_bytes=256; /* Sampler-only descriptor owner. */
  if(texture_bytes+uniform_bytes>PS4GL_UPLOAD_BUDGET-ctx->snapshot_bytes)
    return allocation_failure(ctx,"texture/uniform frame budget");
  unsigned char packed[2*PS4GL_UNIFORM_BYTES]={0};
  memcpy(packed,p->uniform_data[0],stage_bytes[0]);
  memcpy(packed+stage_bytes[0],p->uniform_data[1],stage_bytes[1]);
  for(unsigned i=0;i<p->uniform_count;++i) {
    const PS4GLUniform *u=&p->uniforms[i];
    if(!strncmp(u->name,"ps4gl_rect_size",15) || !strncmp(u->name,"ps4gl_cube_size",15)) {
      unsigned slot=u->name[15]-'0';
      if(slot>=PS4GL_MAX_SAMPLERS || !sources[slot] || u->offset[1]<0) return 0;
      GLfloat dimensions[2]={(GLfloat)sources[slot]->width,(GLfloat)(sources[slot]->height/(!strncmp(u->name,"ps4gl_cube_size",15) ? 6 : 1))};
      memcpy(packed+stage_bytes[0]+u->offset[1],dimensions,sizeof(dimensions));
    }
  }
  if(!upload_vertices(ctx,packed,uniform_bytes,0,0)) return 0;
  PS4GLSnapshot *snap=&ctx->snapshots[ctx->snapshot_count-1];
  VkDescriptorPoolSize sizes[]={{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,2},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,PS4GL_MAX_SAMPLERS}};
  VkDescriptorPoolCreateInfo ci={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .maxSets=1,.poolSizeCount=2,.pPoolSizes=sizes};
  if(vkCreateDescriptorPool(ctx->program_device,&ci,NULL,&snap->pool)!=VK_SUCCESS) goto fail;
  VkDescriptorSet set;
  VkDescriptorSetAllocateInfo ai={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool=snap->pool,.descriptorSetCount=1,.pSetLayouts=&p->descriptors};
  if(vkAllocateDescriptorSets(ctx->program_device,&ai,&set)!=VK_SUCCESS) goto fail;
  for(unsigned stage=0;stage<2;++stage) if(p->uniform_stages&(1u<<stage)) {
    VkDescriptorBufferInfo info={snap->buffer,snap->offset+(stage ? stage_bytes[0] : 0),stage_bytes[stage]};
    VkWriteDescriptorSet write={.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=set,
      .dstBinding=stage,.descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,.pBufferInfo=&info};
    vkUpdateDescriptorSets(ctx->program_device,1,&write,0,NULL);
  }
  for(unsigned i=0;i<PS4GL_MAX_SAMPLERS;++i) if(sources[i]) {
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
  for(unsigned i=0;i<PS4GL_MAX_SAMPLERS;++i) destroy_texture_snapshot(ctx->program_device,&snap->textures[i]);
  if(snap->pool) vkDestroyDescriptorPool(ctx->program_device,snap->pool,NULL);
  /* The page slice is not reused until frame retirement, even on failure. */
  *snap=(PS4GLSnapshot){0}; --ctx->snapshot_count;
  return allocation_failure(ctx,"uniform descriptors or texture snapshot");
}
static VkPipeline select_pipeline(PS4GLContext *ctx, PS4GLProgram *p) {
  VkRenderPass pass=ctx->active_pass ? ctx->active_pass : ctx->program_pass;
  PS4GLRaster raster=ctx->raster, defaults=PS4GL_RASTER_DEFAULT;
  if(!ps4glHasDepthStencil(ctx)) { raster.depth_enabled=0; raster.stencil_enabled=0; }
  if(!ctx->blend.enabled && !memcmp(&raster,&defaults,sizeof(raster)) && pass==ctx->program_pass) return p->pipeline;
  for(unsigned i=0;i<p->variant_count;++i)
    if(p->variants[i].pass==pass && !memcmp(&p->variants[i].raster,&raster,sizeof(raster)) && !memcmp(&p->variants[i].state,&ctx->blend,sizeof(ctx->blend))) return p->variants[i].pipeline;
  if(p->variant_count==PS4GL_BLEND_VARIANTS) {
    if(!ctx->error) ctx->error=GL_OUT_OF_MEMORY;
    return VK_NULL_HANDLE;
  }
  VkPipeline pipeline=VK_NULL_HANDLE;
  VkResult result=create_pipeline(ctx->program_device,pass,p,p->modules,&ctx->blend,&raster,&pipeline);
  if(result!=VK_SUCCESS) {
    if(pipeline) vkDestroyPipeline(ctx->program_device,pipeline,NULL);
    if(!ctx->error) ctx->error=result==VK_ERROR_OUT_OF_HOST_MEMORY || result==VK_ERROR_OUT_OF_DEVICE_MEMORY ? GL_OUT_OF_MEMORY : GL_INVALID_OPERATION;
    return VK_NULL_HANDLE;
  }
  p->variants[p->variant_count++]=(PS4GLBlendVariant){.state=ctx->blend,.raster=raster,.pipeline=pipeline,.pass=pass};
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
  ctx->prepare_target=prepare_render_target; ctx->end_target=end_render_target;
  ctx->query_command=query_command; ctx->complete_queries=complete_queries; ctx->physical=physical; ctx->copy_texture=copy_texture; ctx->program_device=device; ctx->read_pixels=read_pixels; ctx->upload_vertices=upload_vertices; ctx->upload_uniforms=upload_uniforms; ctx->select_pipeline=select_pipeline; ctx->import_program=ps4glCreateProgramSPIRV;
}
void ps4glReleaseVertexSnapshots(PS4GLContext *ctx) {
  if(!ctx) return;
  if(ctx->command) { if(!ctx->error) ctx->error=GL_INVALID_OPERATION; return; }
  release_queries(ctx);
  retire_texture_cache(ctx,0);
  for(unsigned i=0;i<ctx->snapshot_count;++i) {
    if(ctx->snapshots[i].pool) vkDestroyDescriptorPool(ctx->program_device,ctx->snapshots[i].pool,NULL);
    for(unsigned t=0;t<PS4GL_MAX_SAMPLERS;++t) {
      cache_retired_texture(ctx,&ctx->snapshots[i].textures[t]);
      destroy_texture_snapshot(ctx->program_device,&ctx->snapshots[i].textures[t]);
    }
    ctx->snapshots[i]=(PS4GLSnapshot){0};
  }
  free(ctx->snapshots); ctx->snapshots=NULL; ctx->snapshot_capacity=0;
  for(unsigned i=0;i<ctx->upload_page_count;++i) {
    PS4GLUploadPage *p=&ctx->upload_pages[i];
    if(p->mapped) vkUnmapMemory(ctx->program_device,p->memory);
    vkDestroyBuffer(ctx->program_device,p->buffer,NULL);
    vkFreeMemory(ctx->program_device,p->memory,NULL);
    *p=(PS4GLUploadPage){0};
  }
  ctx->upload_page_count=0; ctx->upload_memory_bytes=0; ctx->allocation_failure=NULL;
  ctx->snapshot_count=0; ctx->snapshot_bytes=0;
  ctx->texture_snapshot_uploads=ctx->texture_snapshot_reuses=0;
  collect_targets(ctx,0);
}
