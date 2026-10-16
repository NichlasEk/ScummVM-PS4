/* Managed color targets, with optional private 3D depth/stencil storage. */
static void target_barrier(PS4GLContext *c, PS4GLRenderTarget *t, VkImageLayout layout) {
  VkImageMemoryBarrier b={.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask=t->layout==VK_IMAGE_LAYOUT_UNDEFINED ? 0 : VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,
    .dstAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,
    .oldLayout=t->layout,.newLayout=layout,.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.image=t->image,.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
  vkCmdPipelineBarrier(c->command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,NULL,0,NULL,1,&b);
  t->layout=layout;
}
static void destroy_target(PS4GLContext *c, PS4GLRenderTarget *t) {
  if(t->framebuffer) vkDestroyFramebuffer(c->program_device,t->framebuffer,NULL);
  if(t->depth_view) vkDestroyImageView(c->program_device,t->depth_view,NULL);
  if(t->depth_image) vkDestroyImage(c->program_device,t->depth_image,NULL);
  if(t->depth_memory) vkFreeMemory(c->program_device,t->depth_memory,NULL);
  if(t->view) vkDestroyImageView(c->program_device,t->view,NULL);
  if(t->image) vkDestroyImage(c->program_device,t->image,NULL);
  if(t->memory) vkFreeMemory(c->program_device,t->memory,NULL);
  *t=(PS4GLRenderTarget){0};
}
static void collect_targets(PS4GLContext *c, int all) {
  for(unsigned i=0;i<PS4GL_MAX_TARGETS;++i) if(c->targets[i].image) {
    int keep=0;
    if(!all) for(unsigned j=0;j<PS4GL_MAX_TEXTURES;++j) if(c->textures[j].render_target==i+1) keep=1;
    if(!keep) {
      c->target_bytes-=(size_t)c->targets[i].width*c->targets[i].height*4+c->targets[i].depth_bytes;
      destroy_target(c,&c->targets[i]);
    }
  }
  if(all) for(unsigned j=0;j<PS4GL_MAX_TEXTURES;++j) c->textures[j].render_target=0;
}
static void end_render_target(PS4GLContext *c) {
  if(!c->pass_open) return;
  vkCmdEndRenderPass(c->command); c->pass_open=0;
  if(c->active_target) target_barrier(c,&c->targets[c->active_target-1],VK_IMAGE_LAYOUT_GENERAL);
  {
    VkMemoryBarrier b={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER,
      .srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      .dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT};
    vkCmdPipelineBarrier(c->command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,1,&b,0,NULL,0,NULL);
  }
  c->active_target=0;
}
static int create_target(PS4GLContext *c, PS4GLTexture *texture) {
  unsigned slot;
  for(slot=0;slot<PS4GL_MAX_TARGETS;++slot) if(!c->targets[slot].image) break;
  size_t bytes=(size_t)texture->width*texture->height*4;
  if(slot==PS4GL_MAX_TARGETS || bytes>PS4GL_TEXTURE_BUDGET-c->target_bytes) goto fail;
  const int depth=texture->owned_depth_stencil;
  VkRenderPass *pass=depth ? &c->offscreen_depth_pass : &c->offscreen_pass;
  if(!*pass) {
    VkAttachmentDescription a[2]={{.format=VK_FORMAT_R8G8B8A8_UNORM,.samples=VK_SAMPLE_COUNT_1_BIT,
      .loadOp=VK_ATTACHMENT_LOAD_OP_LOAD,.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
      .initialLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
      {.format=VK_FORMAT_D32_SFLOAT_S8_UINT,.samples=VK_SAMPLE_COUNT_1_BIT,
       .loadOp=VK_ATTACHMENT_LOAD_OP_LOAD,.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
       .stencilLoadOp=VK_ATTACHMENT_LOAD_OP_LOAD,.stencilStoreOp=VK_ATTACHMENT_STORE_OP_STORE,
       .initialLayout=VK_IMAGE_LAYOUT_GENERAL,.finalLayout=VK_IMAGE_LAYOUT_GENERAL}};
    VkAttachmentReference r={0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference dr={1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription s={.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS,.colorAttachmentCount=1,.pColorAttachments=&r,.pDepthStencilAttachment=depth ? &dr : NULL};
    VkRenderPassCreateInfo p={.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,.attachmentCount=depth ? 2 : 1,.pAttachments=a,.subpassCount=1,.pSubpasses=&s};
    if(vkCreateRenderPass(c->program_device,&p,NULL,pass)!=VK_SUCCESS) goto fail;
  }
  PS4GLRenderTarget t={.width=texture->width,.height=texture->height};
  VkImageCreateInfo ci={.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,.imageType=VK_IMAGE_TYPE_2D,
    .format=VK_FORMAT_R8G8B8A8_UNORM,.extent={(uint32_t)t.width,(uint32_t)t.height,1},.mipLevels=1,.arrayLayers=1,
    .samples=VK_SAMPLE_COUNT_1_BIT,.tiling=VK_IMAGE_TILING_OPTIMAL,
    .usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT};
  if(vkCreateImage(c->program_device,&ci,NULL,&t.image)!=VK_SUCCESS) goto cleanup;
  VkMemoryRequirements req; vkGetImageMemoryRequirements(c->program_device,t.image,&req);
  VkPhysicalDeviceMemoryProperties properties; vkGetPhysicalDeviceMemoryProperties(c->physical,&properties);
  unsigned type;
  for(type=0;type<properties.memoryTypeCount;++type) if((req.memoryTypeBits&(1u<<type)) &&
      (properties.memoryTypes[type].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) break;
  if(type==properties.memoryTypeCount) goto cleanup;
  VkMemoryAllocateInfo ai={.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,.allocationSize=req.size,.memoryTypeIndex=type};
  if(vkAllocateMemory(c->program_device,&ai,NULL,&t.memory)!=VK_SUCCESS ||
      vkBindImageMemory(c->program_device,t.image,t.memory,0)!=VK_SUCCESS) goto cleanup;
  VkImageViewCreateInfo vi={.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,.image=t.image,
    .viewType=VK_IMAGE_VIEW_TYPE_2D,.format=VK_FORMAT_R8G8B8A8_UNORM,.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
  if(vkCreateImageView(c->program_device,&vi,NULL,&t.view)!=VK_SUCCESS) goto cleanup;
  if(depth) {
    ci.format=VK_FORMAT_D32_SFLOAT_S8_UINT;
    ci.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if(vkCreateImage(c->program_device,&ci,NULL,&t.depth_image)!=VK_SUCCESS) goto cleanup;
    vkGetImageMemoryRequirements(c->program_device,t.depth_image,&req);
    if(req.size>PS4GL_TEXTURE_BUDGET-c->target_bytes-bytes) goto cleanup;
    for(type=0;type<properties.memoryTypeCount;++type) if((req.memoryTypeBits&(1u<<type)) &&
        (properties.memoryTypes[type].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) break;
    if(type==properties.memoryTypeCount) goto cleanup;
    ai.allocationSize=req.size; ai.memoryTypeIndex=type;
    if(vkAllocateMemory(c->program_device,&ai,NULL,&t.depth_memory)!=VK_SUCCESS ||
        vkBindImageMemory(c->program_device,t.depth_image,t.depth_memory,0)!=VK_SUCCESS) goto cleanup;
    t.depth_bytes=req.size;
    vi.image=t.depth_image; vi.format=ci.format;
    vi.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT;
    if(vkCreateImageView(c->program_device,&vi,NULL,&t.depth_view)!=VK_SUCCESS) goto cleanup;

  }
  VkImageView views[]={t.view,t.depth_view};
  VkFramebufferCreateInfo fi={.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,.renderPass=*pass,
    .attachmentCount=depth ? 2 : 1,.pAttachments=views,.width=t.width,.height=t.height,.layers=1};
  if(vkCreateFramebuffer(c->program_device,&fi,NULL,&t.framebuffer)!=VK_SUCCESS) goto cleanup;
  /* glTexImage2D(NULL) leaves contents undefined. No CPU import is needed
   * until client texels were supplied; partial uploads conservatively import
   * their full CPU shadow. A complete clear also makes prior contents dead. */
  if(texture->cpu_contents_defined && !c->discard_target_contents) {
    if(!upload_vertices(c,texture->pixels,bytes,0,0)) goto cleanup;
    target_barrier(c,&t,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkBufferImageCopy copy={.bufferOffset=c->snapshots[c->snapshot_count-1].offset,.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},.imageExtent={(uint32_t)t.width,(uint32_t)t.height,1}};
    vkCmdCopyBufferToImage(c->command,c->snapshots[c->snapshot_count-1].buffer,t.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
  }
  if(depth) {
    VkImageMemoryBarrier barrier={.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      .oldLayout=VK_IMAGE_LAYOUT_UNDEFINED,.newLayout=VK_IMAGE_LAYOUT_GENERAL,
      .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
      .image=t.depth_image,.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT,0,1,0,1}};
    vkCmdPipelineBarrier(c->command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,NULL,0,NULL,1,&barrier);
  }
  target_barrier(c,&t,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  c->targets[slot]=t; c->target_bytes+=bytes+t.depth_bytes; texture->render_target=slot+1;
  return 1;
cleanup:
  destroy_target(c,&t);
fail:
  if(!c->error) c->error=GL_OUT_OF_MEMORY;
  return 0;
}
static int prepare_render_target(PS4GLContext *c) {
  if(!c->managed_frame) {
    if(c->framebuffer) { if(!c->error) c->error=GL_INVALID_OPERATION; return 0; }
    return 1;
  }
  if(c->pass_open) return 1;
  if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE) {
    if(!c->error) c->error=GL_INVALID_FRAMEBUFFER_OPERATION;
    return 0;
  }
  VkFramebuffer fb=c->default_framebuffer;
  c->width=c->default_width; c->height=c->default_height; c->active_pass=c->default_pass;
  if(c->framebuffer) {
    PS4GLTexture *texture=ps4glFramebufferTextureObject(c);
    if(!texture || (!texture->render_target && !create_target(c,texture))) return 0;
    c->active_target=texture->render_target;
    PS4GLRenderTarget *t=&c->targets[c->active_target-1];
    target_barrier(c,t,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    fb=t->framebuffer; c->width=t->width; c->height=t->height; c->active_pass=t->depth_image ? c->offscreen_depth_pass : c->offscreen_pass;
  }
  VkRenderPassBeginInfo begin={.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,.renderPass=c->active_pass,
    .framebuffer=fb,.renderArea={{0,0},{(uint32_t)c->width,(uint32_t)c->height}}};
  vkCmdBeginRenderPass(c->command,&begin,VK_SUBPASS_CONTENTS_INLINE); c->pass_open=1;
  return 1;
}
void ps4glBeginFrame(VkCommandBuffer command, VkRenderPass pass, VkFramebuffer fb, GLint width, GLint height) {
  PS4GLContext *c=ps4glCurrentContext(); if(!c) return;
  if(c->command || !command || !pass || !fb || width<=0 || height<=0 || !c->physical || pass!=c->program_pass) {
    if(!c->error) c->error=GL_INVALID_OPERATION;
    return;
  }
  c->command=command; c->managed_frame=1; c->default_pass=pass; c->default_framebuffer=fb;
  c->default_width=width; c->default_height=height;
}
void ps4glEndFrame(void) {
  PS4GLContext *c=ps4glCurrentContext(); if(!c) return;
  if(!c->managed_frame) { if(!c->error) c->error=GL_INVALID_OPERATION; return; }
  end_render_target(c); c->managed_frame=0; c->command=VK_NULL_HANDLE; c->default_image=VK_NULL_HANDLE;
  c->width=c->default_width; c->height=c->default_height; c->active_pass=c->program_pass;
}
