#include "ps4gl-display.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static unsigned step,fail_at,live,created,destroyed,waits;
static unsigned bad_physical,bad_images,bad_queue,fail_idle;
static uintptr_t handles[32];
static VkResult result(void) { return ++step==fail_at ? VK_ERROR_OUT_OF_DEVICE_MEMORY : VK_SUCCESS; }
static uintptr_t create(void) { uintptr_t h=++created; assert(live<32); handles[live++]=h; return h; }
static void destroy(uintptr_t h) { unsigned i=0; while(i<live && handles[i]!=h) ++i; assert(i<live); memmove(handles+i,handles+i+1,(live-i-1)*sizeof(*handles)); --live; ++destroyed; }
#define CREATE(out, type) do { VkResult r=result(); if(r) return r; *(out)=(type)create(); return VK_SUCCESS; } while(0)
VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(const VkInstanceCreateInfo *i,const VkAllocationCallbacks *a,VkInstance *out) {
  assert(i->sType==VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO && !a); CREATE(out,VkInstance);
}
VKAPI_ATTR void VKAPI_CALL vkDestroyInstance(VkInstance h,const VkAllocationCallbacks *a) { assert(!a); destroy((uintptr_t)h); }
VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDevices(VkInstance h,uint32_t *n,VkPhysicalDevice *out) {
  assert(h && *n==1); VkResult r=result(); if(r) return r;
  *n=bad_physical ? 0 : 1; *out=(VkPhysicalDevice)(uintptr_t)100; return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice h,const VkDeviceCreateInfo *i,const VkAllocationCallbacks *a,VkDevice *out) {
  assert(h && !a && i->queueCreateInfoCount==1 && i->pQueueCreateInfos->queueFamilyIndex==0);
  assert(i->pQueueCreateInfos->queueCount==1 && *i->pQueueCreateInfos->pQueuePriorities==1); CREATE(out,VkDevice);
}
VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(VkDevice h,const VkAllocationCallbacks *a) { assert(!a); destroy((uintptr_t)h); }
VKAPI_ATTR void VKAPI_CALL vkGetDeviceQueue(VkDevice h,uint32_t family,uint32_t index,VkQueue *out) {
  assert(h && !family && !index); *out=bad_queue ? VK_NULL_HANDLE : (VkQueue)(uintptr_t)101;
}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateSwapchainKHR(VkDevice h,const VkSwapchainCreateInfoKHR *i,const VkAllocationCallbacks *a,VkSwapchainKHR *out) {
  assert(h && !a && i->minImageCount==2 && !i->surface && !i->oldSwapchain);
  assert(i->imageExtent.width==1280 && i->imageExtent.height==720 && i->imageFormat==VK_FORMAT_R8G8B8A8_UNORM);
  assert(i->presentMode==VK_PRESENT_MODE_FIFO_KHR && i->imageUsage==VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT); CREATE(out,VkSwapchainKHR);
}
VKAPI_ATTR void VKAPI_CALL vkDestroySwapchainKHR(VkDevice d,VkSwapchainKHR h,const VkAllocationCallbacks *a) { assert(d && !a); destroy((uintptr_t)h); }
VKAPI_ATTR VkResult VKAPI_CALL vkGetSwapchainImagesKHR(VkDevice d,VkSwapchainKHR h,uint32_t *n,VkImage *out) {
  assert(d && h && *n==2); VkResult r=result(); if(r) return r;
  *n=bad_images==1 ? 1 : 2; out[0]=(VkImage)(uintptr_t)102; out[1]=bad_images==2 ? out[0] : (VkImage)(uintptr_t)103;
  return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateImage(VkDevice d,const VkImageCreateInfo *i,const VkAllocationCallbacks *a,VkImage *out) {
 assert(d && !a && i->format==VK_FORMAT_D32_SFLOAT_S8_UINT && i->usage==VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT); CREATE(out,VkImage);
}
VKAPI_ATTR void VKAPI_CALL vkDestroyImage(VkDevice d,VkImage h,const VkAllocationCallbacks *a) { assert(d && !a); destroy((uintptr_t)h); }
VKAPI_ATTR void VKAPI_CALL vkGetImageMemoryRequirements(VkDevice d,VkImage i,VkMemoryRequirements *r) { assert(d && i); *r=(VkMemoryRequirements){.size=4096,.alignment=256,.memoryTypeBits=2}; }
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice d,VkPhysicalDeviceMemoryProperties *p) { assert(d); *p=(VkPhysicalDeviceMemoryProperties){.memoryTypeCount=2}; p->memoryTypes[1].propertyFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT; }
VKAPI_ATTR VkResult VKAPI_CALL vkAllocateMemory(VkDevice d,const VkMemoryAllocateInfo *i,const VkAllocationCallbacks *a,VkDeviceMemory *out) { assert(d && !a && i->allocationSize==4096 && i->memoryTypeIndex==1); CREATE(out,VkDeviceMemory); }
VKAPI_ATTR VkResult VKAPI_CALL vkBindImageMemory(VkDevice d,VkImage i,VkDeviceMemory m,VkDeviceSize o) { assert(d && i && m && !o); return result(); }
VKAPI_ATTR void VKAPI_CALL vkFreeMemory(VkDevice d,VkDeviceMemory m,const VkAllocationCallbacks *a) { assert(d && !a); destroy((uintptr_t)m); }
VKAPI_ATTR VkResult VKAPI_CALL vkCreateRenderPass(VkDevice h,const VkRenderPassCreateInfo *i,const VkAllocationCallbacks *a,VkRenderPass *out) {
  assert(h && !a && i->attachmentCount==2 && i->subpassCount==1);
  assert(i->pAttachments->format==VK_FORMAT_R8G8B8A8_UNORM);
  assert(i->pAttachments->loadOp==VK_ATTACHMENT_LOAD_OP_LOAD && i->pAttachments->storeOp==VK_ATTACHMENT_STORE_OP_STORE);
  assert(i->pAttachments->initialLayout==VK_IMAGE_LAYOUT_GENERAL && i->pAttachments->finalLayout==VK_IMAGE_LAYOUT_GENERAL); CREATE(out,VkRenderPass);
}
VKAPI_ATTR void VKAPI_CALL vkDestroyRenderPass(VkDevice d,VkRenderPass h,const VkAllocationCallbacks *a) { assert(d && !a); destroy((uintptr_t)h); }
VKAPI_ATTR VkResult VKAPI_CALL vkCreateImageView(VkDevice h,const VkImageViewCreateInfo *i,const VkAllocationCallbacks *a,VkImageView *out) {
  assert(h && !a && i->image && (i->format==VK_FORMAT_R8G8B8A8_UNORM || i->format==VK_FORMAT_D32_SFLOAT_S8_UINT));
  assert(i->subresourceRange.levelCount==1 && i->subresourceRange.layerCount==1); CREATE(out,VkImageView);
}
VKAPI_ATTR void VKAPI_CALL vkDestroyImageView(VkDevice d,VkImageView h,const VkAllocationCallbacks *a) { assert(d && !a); destroy((uintptr_t)h); }
VKAPI_ATTR VkResult VKAPI_CALL vkCreateFramebuffer(VkDevice h,const VkFramebufferCreateInfo *i,const VkAllocationCallbacks *a,VkFramebuffer *out) {
  assert(h && !a && i->renderPass && i->attachmentCount==2 && *i->pAttachments);
  assert(i->width==1280 && i->height==720 && i->layers==1); CREATE(out,VkFramebuffer);
}
VKAPI_ATTR void VKAPI_CALL vkDestroyFramebuffer(VkDevice d,VkFramebuffer h,const VkAllocationCallbacks *a) { assert(d && !a); destroy((uintptr_t)h); }
VKAPI_ATTR VkResult VKAPI_CALL vkDeviceWaitIdle(VkDevice h) { assert(h); ++waits; return fail_idle ? VK_TIMEOUT : VK_SUCCESS; }
static void balanced(void) { assert(!live && created==destroyed); }
int main(void) {
  PS4GLDisplayOwner d={0};
  assert(ps4glDisplayInit(NULL) && ps4glDisplayDestroy(NULL));
  for(unsigned failure=1;failure<=18;++failure) {
    step=0; fail_at=failure; unsigned before=waits;
    assert(ps4glDisplayInit(&d) && d.last_result==VK_ERROR_OUT_OF_DEVICE_MEMORY);
    assert(!d.instance && !d.device && !d.ready && waits==before); balanced();
    fail_at=0; step=0; assert(!ps4glDisplayInit(&d) && d.ready && step==18 && live==14);
    unsigned n=step; assert(ps4glDisplayInit(&d) && step==n && d.ready);
    assert(!ps4glDisplayDestroy(&d)); balanced();
    assert(!ps4glDisplayDestroy(&d)); balanced();
  }
  for(unsigned bad=0;bad<4;++bad) {
    bad_physical=bad==0; bad_queue=bad==1; bad_images=bad>=2 ? bad-1 : 0;
    assert(ps4glDisplayInit(&d) && d.last_result==VK_ERROR_INITIALIZATION_FAILED); balanced();
  }
  bad_physical=bad_queue=bad_images=0;
  assert(!ps4glDisplayInit(&d));
  PS4GLDisplayOwner saved=d;
  fail_idle=1; unsigned before=destroyed;
  assert(ps4glDisplayDestroy(&d) && d.last_result==VK_TIMEOUT && !d.ready && destroyed==before);
  saved.ready=0; saved.last_result=VK_TIMEOUT; assert(!memcmp(&d,&saved,sizeof(d)));
  assert(ps4glDisplayInit(&d)); /* Failed destroy still owns resources. */
  fail_idle=0; assert(!ps4glDisplayDestroy(&d)); balanced();
  assert(!ps4glDisplayInit(&d) && !ps4glDisplayDestroy(&d)); balanced();
  puts("PASS display owner eighteen startup failure points, resource cleanup, retry, fixed display contract and idle-timeout retention");
}
