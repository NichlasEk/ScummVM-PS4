#include "ps4gl-frame.h"
#include <string.h>
static int frame_result(PS4GLFrameOwner *f, VkResult r) {
  f->last_result=r;
  if(r!=VK_SUCCESS) { f->failed=1; return -1; }
  return 0;
}
#define FRAME_TRY(call) do { if(frame_result(f,(call))) return -1; } while(0)
static void frame_barrier(PS4GLFrameOwner *f, int end) {
  VkImageMemoryBarrier b={.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask=end ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : 0,
    .dstAccessMask=end ? 0 : VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    .oldLayout=end ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED,
    .newLayout=end ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_GENERAL,
    .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
    .image=f->images[f->image],.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
  vkCmdPipelineBarrier(f->command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,NULL,0,NULL,1,&b);
}
int ps4glFrameInit(PS4GLFrameOwner *f, VkDevice device, VkQueue queue, VkSwapchainKHR swap,
                   VkRenderPass pass,const VkImage images[2],const VkFramebuffer targets[2],int w,int h) {
  if(!f || f->device || !device || !queue || !swap || !pass || !images || !targets || !images[0] || !images[1] ||
     !targets[0] || !targets[1] || w<=0 || h<=0) return -1;
  *f=(PS4GLFrameOwner){.device=device,.queue=queue,.swap=swap,.pass=pass,.width=w,.height=h};
  memcpy(f->images,images,sizeof(f->images)); memcpy(f->targets,targets,sizeof(f->targets));
  VkCommandPoolCreateInfo pool={.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,.queueFamilyIndex=0};
  VkCommandBufferAllocateInfo alloc={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,.commandBufferCount=1};
  VkFenceCreateInfo fence={.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  if(frame_result(f,vkCreateCommandPool(device,&pool,NULL,&f->pool))) goto fail;
  alloc.commandPool=f->pool;
  if(frame_result(f,vkAllocateCommandBuffers(device,&alloc,&f->command)) ||
     frame_result(f,vkCreateFence(device,&fence,NULL,&f->fence))) goto fail;
  return 0;
fail:
  if(f->pool) vkDestroyCommandPool(device,f->pool,NULL);
  f->pool=VK_NULL_HANDLE; f->command=VK_NULL_HANDLE; return -1;
}
int ps4glFrameBegin(void *data) {
  PS4GLFrameOwner *f=data; PS4GLContext *c=ps4glCurrentContext();
  if(!f || f->failed || !f->command || !c || c->command || c->program_device!=f->device || c->program_pass!=f->pass) return -1;
  if(f->state) return -1;
  FRAME_TRY(vkResetFences(f->device,1,&f->fence));
  FRAME_TRY(vkAcquireNextImageKHR(f->device,f->swap,1000000000ULL,VK_NULL_HANDLE,f->fence,&f->image));
  FRAME_TRY(vkWaitForFences(f->device,1,&f->fence,VK_TRUE,1000000000ULL));
  if(f->image>=2) { f->failed=1; return -1; }
  VkCommandBufferBeginInfo begin={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  FRAME_TRY(vkBeginCommandBuffer(f->command,&begin));
  f->context=c; f->state=1; frame_barrier(f,0);
  if(f->depth_images[f->image]) {
    VkImageMemoryBarrier depth={.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .oldLayout=VK_IMAGE_LAYOUT_UNDEFINED,.newLayout=VK_IMAGE_LAYOUT_GENERAL,
      .dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
      .image=f->depth_images[f->image],.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT,0,1,0,1}};
    vkCmdPipelineBarrier(f->command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,NULL,0,NULL,1,&depth);
  }
  ps4glBeginFrame(f->command,f->pass,f->targets[f->image],f->width,f->height);
  if(c->command!=f->command || !c->managed_frame) { f->failed=1; return -1; }
  return 0;
}
static void end_managed(PS4GLFrameOwner *f) {
  if(f->context && f->context->managed_frame && f->context->command==f->command) {
    PS4GLContext *previous=ps4glCurrentContext(); ps4glMakeCurrent(f->context);
    ps4glEndFrame(); ps4glMakeCurrent(previous);
  }
}
int ps4glFrameSubmit(PS4GLFrameOwner *f) {
  if(!f || f->failed || !f->state || !f->context) return -1;
  if(f->state==1) {
    end_managed(f); frame_barrier(f,1);
    FRAME_TRY(vkEndCommandBuffer(f->command)); f->state=2;
  }
  FRAME_TRY(vkResetFences(f->device,1,&f->fence));
  VkSubmitInfo submit={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.commandBufferCount=1,.pCommandBuffers=&f->command};
  FRAME_TRY(vkQueueSubmit(f->queue,1,&submit,f->fence));
  FRAME_TRY(vkWaitForFences(f->device,1,&f->fence,VK_TRUE,5000000000ULL));
  return 0;
}
static int frame_discard(PS4GLFrameOwner *f) {
  FRAME_TRY(vkResetCommandBuffer(f->command,0));
  if(f->context) ps4glReleaseVertexSnapshots(f->context);
  f->state=0; f->context=NULL; return 0;
}
int ps4glFramePresent(void *data) {
  PS4GLFrameOwner *f=data;
  if(!f || f->failed) return -1;
  /* Empty swaps preserve the last displayed image, without acquiring a new one. */
  if(!f->state) return 0;
  if(f->state==1 && ps4glFrameSubmit(f)) return -1;
  VkPresentInfoKHR present={.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,.swapchainCount=1,.pSwapchains=&f->swap,.pImageIndices=&f->image};
  FRAME_TRY(vkQueuePresentKHR(f->queue,&present));
  if(frame_discard(f)) return -1;
  ++f->frames; return 0;
}
int ps4glFrameQuiesce(void *data) {
  PS4GLFrameOwner *f=data;
  if(!f || !f->device || !f->command) return -1;
  FRAME_TRY(vkDeviceWaitIdle(f->device));
  if(f->state==1) end_managed(f);
  /* Discarding an acquired, unpresented image prevents further acquisition on
   * this swapchain. Parent recreation is needed after aborting a live frame. */
  if(f->state) f->failed=1;
  return frame_discard(f);
}
int ps4glFrameDestroy(PS4GLFrameOwner *f) {
  if(!f || !f->device) return -1;
  if(f->command && ps4glFrameQuiesce(f)) return -1;
  if(f->fence) vkDestroyFence(f->device,f->fence,NULL);
  if(f->pool) vkDestroyCommandPool(f->device,f->pool,NULL);
  memset(f,0,sizeof(*f)); return 0;
}
