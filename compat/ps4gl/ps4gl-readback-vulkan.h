/* Completed offscreen RGBA8 capture. The frame owner has already waited for
 * its fence; active recordings and the presentation-owned default target are
 * deliberately rejected by the public entrypoint. */
static int read_pixels(PS4GLContext *c, GLint x, GLint y, GLsizei width, GLsizei height, void *out) {
  PS4GLTexture *texture=ps4glFramebufferTextureObject(c);
  if(!texture || width>texture->width || height>texture->height ||
      x>texture->width-width || y>texture->height-height) {
    if(!c->error) c->error=GL_INVALID_VALUE;
    return 0;
  }
  size_t row=(size_t)width*4, stride=(row+c->pack_alignment-1)&~(size_t)(c->pack_alignment-1);
  if(!texture->render_target) {
    if(!texture->pixels) return 0;
    for(GLsizei j=0;j<height;++j) memcpy((char *)out+j*stride,texture->pixels+((size_t)(y+j)*texture->width+x)*4,row);
    return 1;
  }
  PS4GLRenderTarget *target=&c->targets[texture->render_target-1];
  VkDevice device=c->program_device;
  VkBuffer buffer=VK_NULL_HANDLE; VkDeviceMemory memory=VK_NULL_HANDLE;
  VkCommandPool pool=VK_NULL_HANDLE; VkFence fence=VK_NULL_HANDLE;
  void *mapped=NULL; int ok=0, submitted=0;
  VkBufferCreateInfo bc={.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,.size=row*height,.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT};
  if(vkCreateBuffer(device,&bc,NULL,&buffer)!=VK_SUCCESS) goto done;
  VkMemoryRequirements req; vkGetBufferMemoryRequirements(device,buffer,&req);
  if(req.size>PS4GL_TEXTURE_BUDGET) goto done;
  VkPhysicalDeviceMemoryProperties props; vkGetPhysicalDeviceMemoryProperties(c->physical,&props);
  uint32_t type=UINT32_MAX;
  for(uint32_t i=0;i<props.memoryTypeCount;++i) if((req.memoryTypeBits&(1u<<i)) &&
      (props.memoryTypes[i].propertyFlags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==
      (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) { type=i; break; }
  if(type==UINT32_MAX) goto done;
  VkMemoryAllocateInfo ai={.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,.allocationSize=req.size,.memoryTypeIndex=type};
  if(vkAllocateMemory(device,&ai,NULL,&memory)!=VK_SUCCESS || vkBindBufferMemory(device,buffer,memory,0)!=VK_SUCCESS ||
      vkMapMemory(device,memory,0,VK_WHOLE_SIZE,0,&mapped)!=VK_SUCCESS) goto done;
  VkCommandPoolCreateInfo pc={.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,.queueFamilyIndex=0};
  if(vkCreateCommandPool(device,&pc,NULL,&pool)!=VK_SUCCESS) goto done;
  VkCommandBuffer command;
  VkCommandBufferAllocateInfo ca={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,.commandPool=pool,.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,.commandBufferCount=1};
  if(vkAllocateCommandBuffers(device,&ca,&command)!=VK_SUCCESS) goto done;
  VkCommandBufferBeginInfo begin={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
  if(vkBeginCommandBuffer(command,&begin)!=VK_SUCCESS) goto done;
  VkImageMemoryBarrier b={.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_SHADER_READ_BIT,.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT,
    .oldLayout=target->layout,.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
    .image=target->image,.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
  vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,NULL,0,NULL,1,&b);
  /* Offscreen storage already uses GL row order (unlike VideoOut). */
  VkBufferImageCopy region={.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},
    .imageOffset={x,y,0},.imageExtent={(uint32_t)width,(uint32_t)height,1}};
  vkCmdCopyImageToBuffer(command,target->image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffer,1,&region);
  b.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; b.newLayout=target->layout;
  b.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT; b.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_SHADER_READ_BIT;
  VkBufferMemoryBarrier host={.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
    .srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,.dstAccessMask=VK_ACCESS_HOST_READ_BIT,
    .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
    .buffer=buffer,.offset=0,.size=VK_WHOLE_SIZE};
  vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_HOST_BIT,0,0,NULL,1,&host,1,&b);
  if(vkEndCommandBuffer(command)!=VK_SUCCESS) goto done;
  VkFenceCreateInfo fc={.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  if(vkCreateFence(device,&fc,NULL,&fence)!=VK_SUCCESS) goto done;
  VkQueue queue; vkGetDeviceQueue(device,0,0,&queue);
  VkSubmitInfo submit={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.commandBufferCount=1,.pCommandBuffers=&command};
  if(vkQueueSubmit(queue,1,&submit,fence)!=VK_SUCCESS) goto done;
  submitted=1;
  if(vkWaitForFences(device,1,&fence,VK_TRUE,UINT64_MAX)!=VK_SUCCESS) goto done;
  submitted=0;
  for(GLsizei j=0;j<height;++j) memcpy((char *)out+j*stride,(char *)mapped+j*row,row);
  ok=1;
done:
  /* Retain rather than free resources that may still be executing. */
  if(submitted && vkDeviceWaitIdle(device)!=VK_SUCCESS) return 0;
  if(fence) vkDestroyFence(device,fence,NULL);
  if(pool) vkDestroyCommandPool(device,pool,NULL);
  if(mapped) vkUnmapMemory(device,memory);
  if(buffer) vkDestroyBuffer(device,buffer,NULL);
  if(memory) vkFreeMemory(device,memory,NULL);
  return ok;
}
