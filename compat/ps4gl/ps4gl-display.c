#include "ps4gl-display.h"
#include <string.h>
static void display_release(PS4GLDisplayOwner *d) {
  for(unsigned i=2;i>0;--i) {
    if(d->targets[i-1]) vkDestroyFramebuffer(d->device,d->targets[i-1],NULL);
    if(d->depth_views[i-1]) vkDestroyImageView(d->device,d->depth_views[i-1],NULL);
    if(d->depth_images[i-1]) vkDestroyImage(d->device,d->depth_images[i-1],NULL);
    if(d->depth_memory[i-1]) vkFreeMemory(d->device,d->depth_memory[i-1],NULL);
    if(d->views[i-1]) vkDestroyImageView(d->device,d->views[i-1],NULL);
  }
  if(d->pass) vkDestroyRenderPass(d->device,d->pass,NULL);
  if(d->swap) vkDestroySwapchainKHR(d->device,d->swap,NULL);
  if(d->device) vkDestroyDevice(d->device,NULL);
  if(d->instance) vkDestroyInstance(d->instance,NULL);
  memset(d,0,sizeof(*d));
}
#define DISPLAY_TRY(call) do { result=(call); if(result!=VK_SUCCESS) goto fail; } while(0)
int ps4glDisplayInit(PS4GLDisplayOwner *d) {
  if(!d || d->instance || d->device || d->ready) return -1;
  *d=(PS4GLDisplayOwner){.width=1280,.height=720};
  VkResult result=VK_SUCCESS;
  VkInstanceCreateInfo instance={.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  DISPLAY_TRY(vkCreateInstance(&instance,NULL,&d->instance));
  uint32_t count=1;
  DISPLAY_TRY(vkEnumeratePhysicalDevices(d->instance,&count,&d->physical));
  if(count!=1 || !d->physical) { result=VK_ERROR_INITIALIZATION_FAILED; goto fail; }
  float priority=1;
  VkDeviceQueueCreateInfo queue={.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    .queueFamilyIndex=0,.queueCount=1,.pQueuePriorities=&priority};
  VkDeviceCreateInfo device={.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .queueCreateInfoCount=1,.pQueueCreateInfos=&queue};
  DISPLAY_TRY(vkCreateDevice(d->physical,&device,NULL,&d->device));
  vkGetDeviceQueue(d->device,0,0,&d->queue);
  if(!d->queue) { result=VK_ERROR_INITIALIZATION_FAILED; goto fail; }
  VkSwapchainCreateInfoKHR swap={.sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .minImageCount=2,.imageFormat=VK_FORMAT_R8G8B8A8_SRGB,
    .imageColorSpace=VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,.imageExtent={d->width,d->height},
    .imageArrayLayers=1,.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    .imageSharingMode=VK_SHARING_MODE_EXCLUSIVE,.presentMode=VK_PRESENT_MODE_FIFO_KHR,
    .compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR};
  DISPLAY_TRY(vkCreateSwapchainKHR(d->device,&swap,NULL,&d->swap));
  count=2;
  DISPLAY_TRY(vkGetSwapchainImagesKHR(d->device,d->swap,&count,d->images));
  if(count!=2 || !d->images[0] || !d->images[1] || d->images[0]==d->images[1]) {
    result=VK_ERROR_INITIALIZATION_FAILED; goto fail;
  }
  VkAttachmentDescription attachments[2]={{.format=VK_FORMAT_R8G8B8A8_SRGB,
    .samples=VK_SAMPLE_COUNT_1_BIT,.loadOp=VK_ATTACHMENT_LOAD_OP_LOAD,
    .storeOp=VK_ATTACHMENT_STORE_OP_STORE,.initialLayout=VK_IMAGE_LAYOUT_GENERAL,
    .finalLayout=VK_IMAGE_LAYOUT_GENERAL},
    {.format=VK_FORMAT_D32_SFLOAT_S8_UINT,.samples=VK_SAMPLE_COUNT_1_BIT,
     .loadOp=VK_ATTACHMENT_LOAD_OP_LOAD,.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
     .stencilLoadOp=VK_ATTACHMENT_LOAD_OP_LOAD,.stencilStoreOp=VK_ATTACHMENT_STORE_OP_STORE,
     .initialLayout=VK_IMAGE_LAYOUT_GENERAL,.finalLayout=VK_IMAGE_LAYOUT_GENERAL}};
  VkAttachmentReference reference={.attachment=0,.layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkAttachmentReference depth_ref={1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
  VkSubpassDescription subpass={.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS,
    .colorAttachmentCount=1,.pColorAttachments=&reference,.pDepthStencilAttachment=&depth_ref};
  VkRenderPassCreateInfo pass={.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .attachmentCount=2,.pAttachments=attachments,.subpassCount=1,.pSubpasses=&subpass};
  DISPLAY_TRY(vkCreateRenderPass(d->device,&pass,NULL,&d->pass));
  for(unsigned i=0;i<2;++i) {
    VkImageViewCreateInfo view={.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image=d->images[i],.viewType=VK_IMAGE_VIEW_TYPE_2D,.format=VK_FORMAT_R8G8B8A8_SRGB,
      .subresourceRange={.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,.levelCount=1,.layerCount=1}};
    DISPLAY_TRY(vkCreateImageView(d->device,&view,NULL,&d->views[i]));
    VkImageCreateInfo image={.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,.imageType=VK_IMAGE_TYPE_2D,
      .format=VK_FORMAT_D32_SFLOAT_S8_UINT,.extent={d->width,d->height,1},.mipLevels=1,.arrayLayers=1,
      .samples=VK_SAMPLE_COUNT_1_BIT,.tiling=VK_IMAGE_TILING_OPTIMAL,.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT};
    DISPLAY_TRY(vkCreateImage(d->device,&image,NULL,&d->depth_images[i]));
    VkMemoryRequirements req; vkGetImageMemoryRequirements(d->device,d->depth_images[i],&req);
    VkPhysicalDeviceMemoryProperties props; vkGetPhysicalDeviceMemoryProperties(d->physical,&props);
    unsigned type;
    for(type=0;type<props.memoryTypeCount;++type) if((req.memoryTypeBits&(1u<<type)) &&
        (props.memoryTypes[type].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) break;
    if(type==props.memoryTypeCount) { result=VK_ERROR_FEATURE_NOT_PRESENT; goto fail; }
    VkMemoryAllocateInfo alloc={.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,.allocationSize=req.size,.memoryTypeIndex=type};
    DISPLAY_TRY(vkAllocateMemory(d->device,&alloc,NULL,&d->depth_memory[i]));
    DISPLAY_TRY(vkBindImageMemory(d->device,d->depth_images[i],d->depth_memory[i],0));
    view.image=d->depth_images[i]; view.format=VK_FORMAT_D32_SFLOAT_S8_UINT;
    view.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT;
    DISPLAY_TRY(vkCreateImageView(d->device,&view,NULL,&d->depth_views[i]));
    VkImageView target_views[]={d->views[i],d->depth_views[i]};
    VkFramebufferCreateInfo target={.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass=d->pass,.attachmentCount=2,.pAttachments=target_views,
      .width=d->width,.height=d->height,.layers=1};
    DISPLAY_TRY(vkCreateFramebuffer(d->device,&target,NULL,&d->targets[i]));
  }
  d->ready=1; return 0;
fail:
  display_release(d); d->last_result=result; return -1;
}
#undef DISPLAY_TRY
int ps4glDisplayDestroy(PS4GLDisplayOwner *d) {
  if(!d) return -1;
  if(d->device) {
    d->last_result=vkDeviceWaitIdle(d->device);
    if(d->last_result!=VK_SUCCESS) { d->ready=0; return -1; }
  }
  display_release(d); return 0;
}
