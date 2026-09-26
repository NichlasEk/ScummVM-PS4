/* Managed, color-only render passes. Storage outlives recorded commands. */
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
      c->target_bytes-=(size_t)c->targets[i].width*c->targets[i].height*4;
      destroy_target(c,&c->targets[i]);
    }
  }
  if(all) for(unsigned j=0;j<PS4GL_MAX_TEXTURES;++j) c->textures[j].render_target=0;
}
static void end_render_target(PS4GLContext *c) {
  if(!c->pass_open) return;
  vkCmdEndRenderPass(c->command); c->pass_open=0;
  if(c->active_target) target_barrier(c,&c->targets[c->active_target-1],VK_IMAGE_LAYOUT_GENERAL);
  else {
    VkMemoryBarrier b={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER,
      .srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT};
    vkCmdPipelineBarrier(c->command,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,0,1,&b,0,NULL,0,NULL);
  }
  c->active_target=0;
}
static int create_target(PS4GLContext *c, PS4GLTexture *texture) {
  unsigned slot;
  for(slot=0;slot<PS4GL_MAX_TARGETS;++slot) if(!c->targets[slot].image) break;
  size_t bytes=(size_t)texture->width*texture->height*4;
  if(slot==PS4GL_MAX_TARGETS || bytes>PS4GL_TEXTURE_BUDGET-c->target_bytes) goto fail;
  if(!c->offscreen_pass) {
    VkAttachmentDescription a={.format=VK_FORMAT_R8G8B8A8_UNORM,.samples=VK_SAMPLE_COUNT_1_BIT,
      .loadOp=VK_ATTACHMENT_LOAD_OP_LOAD,.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
      .initialLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference r={0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription s={.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS,.colorAttachmentCount=1,.pColorAttachments=&r};
    VkRenderPassCreateInfo p={.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,.attachmentCount=1,.pAttachments=&a,.subpassCount=1,.pSubpasses=&s};
    if(vkCreateRenderPass(c->program_device,&p,NULL,&c->offscreen_pass)!=VK_SUCCESS) goto fail;
  }
  PS4GLRenderTarget t={.width=texture->width,.height=texture->height};
  VkImageCreateInfo ci={.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,.imageType=VK_IMAGE_TYPE_2D,
    .format=VK_FORMAT_R8G8B8A8_UNORM,.extent={(uint32_t)t.width,(uint32_t)t.height,1},.mipLevels=1,.arrayLayers=1,
    .samples=VK_SAMPLE_COUNT_1_BIT,.tiling=VK_IMAGE_TILING_OPTIMAL,
    .usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT};
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
  VkFramebufferCreateInfo fi={.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,.renderPass=c->offscreen_pass,
    .attachmentCount=1,.pAttachments=&t.view,.width=t.width,.height=t.height,.layers=1};
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
  target_barrier(c,&t,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  c->targets[slot]=t; c->target_bytes+=bytes; texture->render_target=slot+1;
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
    fb=t->framebuffer; c->width=t->width; c->height=t->height; c->active_pass=c->offscreen_pass;
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
  end_render_target(c); c->managed_frame=0; c->command=VK_NULL_HANDLE;
  c->width=c->default_width; c->height=c->default_height; c->active_pass=c->program_pass;
}
