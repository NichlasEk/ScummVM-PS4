/* Immutable coherent linear textures. Each upload owns a short command buffer
 * so its layout transition is recorded outside the caller's active render pass.
 * Single-threaded queue 0 is shared with the owning renderer. */
static void destroy_texture_snapshot(VkDevice device, PS4GLTextureSnapshot *t) {
  if(t->borrowed_snapshot) { *t=(PS4GLTextureSnapshot){0}; return; }
  if(t->sampler) vkDestroySampler(device,t->sampler,NULL);
  if(t->view && !t->borrowed_view) vkDestroyImageView(device,t->view,NULL);
  if(t->image) vkDestroyImage(device,t->image,NULL);
  if(t->memory) vkFreeMemory(device,t->memory,NULL);
  *t=(PS4GLTextureSnapshot){0};
}
#include "ps4gl-texture-cache.h"
/* Cache owners survive retired frames; current-frame snapshots borrow them.
 * Revisions and sampler state must both match, never only the GL name. */
static const PS4GLTextureSnapshot *find_texture_snapshot(const PS4GLContext *ctx, const PS4GLTexture *source) {
  for(unsigned i=0;i<PS4GL_TEXTURE_CACHE_SLOTS;++i)
    if(texture_snapshot_matches(&ctx->texture_cache[i],source)) return &ctx->texture_cache[i];
  for(unsigned i=0;i<ctx->snapshot_count;++i) for(unsigned j=0;j<PS4GL_MAX_SAMPLERS;++j) {
    const PS4GLTextureSnapshot *t=&ctx->snapshots[i].textures[j];
    if(texture_snapshot_matches(t,source)) return t;
  }
  return NULL;
}
static void texture_snapshot_key(PS4GLTextureSnapshot *t, const PS4GLTexture *source) {
  t->source_name=source->name; t->source_revision=source->revision; t->source_target=source->render_target;
  t->min_filter=source->min_filter; t->mag_filter=source->mag_filter;
  t->wrap_s=source->wrap_s; t->wrap_t=source->wrap_t;
}
static int texture_snapshot(PS4GLContext *ctx, const PS4GLTexture *source, PS4GLTextureSnapshot *t) {
  const PS4GLTextureSnapshot *cached=find_texture_snapshot(ctx,source);
  if(cached) { *t=*cached; t->borrowed_snapshot=1; ++ctx->texture_snapshot_reuses; return 1; }
  VkDevice device=ctx->program_device;
  if(source->render_target) {
    t->view=ctx->targets[source->render_target-1].view; t->borrowed_view=1;
    goto sampler;
  }
  VkImageCreateInfo ci={.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,.imageType=VK_IMAGE_TYPE_2D,
    .format=VK_FORMAT_R8G8B8A8_UNORM,.extent={(uint32_t)source->width,(uint32_t)source->height,1},
    .mipLevels=1,.arrayLayers=1,.samples=VK_SAMPLE_COUNT_1_BIT,.tiling=VK_IMAGE_TILING_LINEAR,
    .usage=VK_IMAGE_USAGE_SAMPLED_BIT,.initialLayout=VK_IMAGE_LAYOUT_PREINITIALIZED};
  if(vkCreateImage(device,&ci,NULL,&t->image)!=VK_SUCCESS) return 0;
  VkMemoryRequirements req; vkGetImageMemoryRequirements(device,t->image,&req);
  VkPhysicalDeviceMemoryProperties properties; vkGetPhysicalDeviceMemoryProperties(ctx->physical,&properties);
  uint32_t type=UINT32_MAX;
  for(uint32_t i=0;i<properties.memoryTypeCount;++i) if((req.memoryTypeBits&(1u<<i)) &&
      (properties.memoryTypes[i].propertyFlags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==
      (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) { type=i; break; }
  if(type==UINT32_MAX) return 0;
  VkMemoryAllocateInfo ai={.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,.allocationSize=req.size,.memoryTypeIndex=type};
  if(vkAllocateMemory(device,&ai,NULL,&t->memory)!=VK_SUCCESS || vkBindImageMemory(device,t->image,t->memory,0)!=VK_SUCCESS) return 0;
  t->allocation_bytes=(size_t)req.size;
  VkImageSubresource sub={.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT}; VkSubresourceLayout layout;
  vkGetImageSubresourceLayout(device,t->image,&sub,&layout);
  size_t row=(size_t)source->width*4;
  if(layout.rowPitch<row || layout.offset>req.size || row>req.size-layout.offset ||
      (uint64_t)(source->height-1)>(req.size-layout.offset-row)/layout.rowPitch) return 0;
  void *mapped;
  if(vkMapMemory(device,t->memory,0,VK_WHOLE_SIZE,0,&mapped)!=VK_SUCCESS) return 0;
  for(GLsizei y=0;y<source->height;++y) memcpy((char *)mapped+layout.offset+(size_t)y*layout.rowPitch,source->pixels+(size_t)y*row,row);
  vkUnmapMemory(device,t->memory);
  VkImageViewCreateInfo vi={.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,.image=t->image,
    .viewType=VK_IMAGE_VIEW_TYPE_2D,.format=VK_FORMAT_R8G8B8A8_UNORM,.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
  if(vkCreateImageView(device,&vi,NULL,&t->view)!=VK_SUCCESS) return 0;
sampler:;
  VkSamplerCreateInfo si={.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .minFilter=source->min_filter==GL_LINEAR ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
    .magFilter=source->mag_filter==GL_LINEAR ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
    .mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST,
    .addressModeU=source->wrap_s==GL_REPEAT ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    .addressModeV=source->wrap_t==GL_REPEAT ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    .addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE};
  if(vkCreateSampler(device,&si,NULL,&t->sampler)!=VK_SUCCESS) return 0;
  if(t->borrowed_view) { texture_snapshot_key(t,source); return 1; }
  VkCommandPool pool=VK_NULL_HANDLE; VkFence fence=VK_NULL_HANDLE; int ok=0;
  VkCommandPoolCreateInfo pc={.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,.queueFamilyIndex=0};
  if(vkCreateCommandPool(device,&pc,NULL,&pool)!=VK_SUCCESS) return 0;
  VkCommandBuffer command;
  VkCommandBufferAllocateInfo ca={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,.commandPool=pool,.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,.commandBufferCount=1};
  if(vkAllocateCommandBuffers(device,&ca,&command)!=VK_SUCCESS) goto done;
  VkCommandBufferBeginInfo begin={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
  if(vkBeginCommandBuffer(command,&begin)!=VK_SUCCESS) goto done;
  VkImageMemoryBarrier barrier={.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT,
    .dstAccessMask=VK_ACCESS_SHADER_READ_BIT,.oldLayout=VK_IMAGE_LAYOUT_PREINITIALIZED,.newLayout=VK_IMAGE_LAYOUT_GENERAL,
    .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
    .image=t->image,.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
  vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,NULL,0,NULL,1,&barrier);
  if(vkEndCommandBuffer(command)!=VK_SUCCESS) goto done;
  VkFenceCreateInfo fc={.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  if(vkCreateFence(device,&fc,NULL,&fence)!=VK_SUCCESS) goto done;
  VkQueue queue; vkGetDeviceQueue(device,0,0,&queue);
  VkSubmitInfo submit={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.commandBufferCount=1,.pCommandBuffers=&command};
  if(vkQueueSubmit(queue,1,&submit,fence)!=VK_SUCCESS) goto done;
  /* No timeout cleanup of resources that could still be in flight. */
  ok=vkWaitForFences(device,1,&fence,VK_TRUE,UINT64_MAX)==VK_SUCCESS;
  if(!ok) vkDeviceWaitIdle(device);
done:
  if(fence) vkDestroyFence(device,fence,NULL);
  vkDestroyCommandPool(device,pool,NULL);
  if(ok) { texture_snapshot_key(t,source); ++ctx->texture_snapshot_uploads; }
  return ok;
}
