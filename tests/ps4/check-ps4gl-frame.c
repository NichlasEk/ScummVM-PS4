#include "ps4gl-frame.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static PS4GLContext c,*current;
static unsigned creates,destroys,submits,presents,resets,releases,acquires;
static int recording,busy,fail_wait,fail_idle;
static unsigned completed_queries;
static void query_complete(PS4GLContext *ctx) {assert(ctx && !busy && !recording);++completed_queries;}
PS4GLContext *ps4glCurrentContext(void){return current;}
void ps4glMakeCurrent(PS4GLContext *p){current=p;}
void ps4glBeginFrame(VkCommandBuffer cmd,VkRenderPass pass,VkFramebuffer fb,GLint w,GLint h){
  assert(current && pass && fb && w==1280 && h==720);current->command=cmd;current->managed_frame=1;
}
void ps4glEndFrame(void){assert(current && recording);current->command=0;current->managed_frame=0;}
void ps4glRecycleVertexSnapshots(PS4GLContext *p){assert(p && !busy && !recording && resets>releases);++releases;}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateCommandPool(VkDevice d,const VkCommandPoolCreateInfo *i,const VkAllocationCallbacks *a,VkCommandPool *p){(void)d;(void)i;(void)a;*p=(VkCommandPool)1;++creates;return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL vkAllocateCommandBuffers(VkDevice d,const VkCommandBufferAllocateInfo *i,VkCommandBuffer *p){(void)d;(void)i;*p=(VkCommandBuffer)1;return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateFence(VkDevice d,const VkFenceCreateInfo *i,const VkAllocationCallbacks *a,VkFence *p){(void)d;(void)i;(void)a;*p=(VkFence)1;++creates;return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL vkDestroyCommandPool(VkDevice d,VkCommandPool p,const VkAllocationCallbacks *a){(void)d;(void)p;(void)a;assert(!busy);++destroys;}
VKAPI_ATTR void VKAPI_CALL vkDestroyFence(VkDevice d,VkFence p,const VkAllocationCallbacks *a){(void)d;(void)p;(void)a;assert(!busy);++destroys;}
VKAPI_ATTR VkResult VKAPI_CALL vkResetFences(VkDevice d,uint32_t n,const VkFence *p){(void)d;(void)n;(void)p;return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL vkAcquireNextImageKHR(VkDevice d,VkSwapchainKHR s,uint64_t t,VkSemaphore sem,VkFence f,uint32_t *index){(void)d;(void)s;(void)t;(void)sem;(void)f;*index=acquires++%2;return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL vkWaitForFences(VkDevice d,uint32_t n,const VkFence *p,VkBool32 all,uint64_t t){(void)d;(void)n;(void)p;(void)all;(void)t;if(fail_wait)return VK_TIMEOUT;busy=0;return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL vkBeginCommandBuffer(VkCommandBuffer cmd,const VkCommandBufferBeginInfo *i){(void)cmd;(void)i;assert(!recording && !busy);recording=1;return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL vkCmdPipelineBarrier(VkCommandBuffer cmd,VkPipelineStageFlags a,VkPipelineStageFlags b,VkDependencyFlags flags,uint32_t m,const VkMemoryBarrier *mb,uint32_t n,const VkBufferMemoryBarrier *bb,uint32_t p,const VkImageMemoryBarrier *ib){(void)cmd;(void)a;(void)b;(void)flags;(void)m;(void)mb;(void)n;(void)bb;(void)p;(void)ib;assert(recording);}
VKAPI_ATTR VkResult VKAPI_CALL vkEndCommandBuffer(VkCommandBuffer cmd){(void)cmd;assert(recording);recording=0;return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(VkQueue q,uint32_t n,const VkSubmitInfo *i,VkFence f){(void)q;(void)n;(void)i;(void)f;assert(!recording);busy=1;++submits;return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue q,const VkPresentInfoKHR *i){(void)q;(void)i;assert(!busy);++presents;return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL vkResetCommandBuffer(VkCommandBuffer cmd,VkCommandBufferResetFlags flags){(void)cmd;(void)flags;assert(!busy);recording=0;++resets;return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL vkDeviceWaitIdle(VkDevice d){(void)d;if(fail_idle)return VK_TIMEOUT;busy=0;return VK_SUCCESS;}
static void init(PS4GLFrameOwner *f){
  const VkImage images[]={(VkImage)1,(VkImage)2};const VkFramebuffer targets[]={(VkFramebuffer)1,(VkFramebuffer)2};
  assert(!ps4glFrameInit(f,(VkDevice)1,(VkQueue)1,(VkSwapchainKHR)1,(VkRenderPass)1,images,targets,1280,720));
  assert(ps4glFrameInit(f,(VkDevice)1,(VkQueue)1,(VkSwapchainKHR)1,(VkRenderPass)1,images,targets,1280,720));
  c=(PS4GLContext){.program_device=(VkDevice)1,.program_pass=(VkRenderPass)1};current=&c;c.complete_queries=query_complete;
}
int main(void){
  PS4GLFrameOwner f={0};init(&f);
  assert(!ps4glFramePresent(&f) && !acquires && !presents); /* empty swap */
  assert(!ps4glFrameBegin(&f) && c.command && f.state==1);
  assert(ps4glFrameBegin(&f)); /* no second acquisition */
  assert(!ps4glFrameSubmit(&f) && !ps4glFrameSubmit(&f) && submits==2 && !releases && completed_queries==2);
  assert(!ps4glFramePresent(&f) && f.frames==1 && presents==1 && releases==1 && !f.context);
  assert(!ps4glFrameBegin(&f)); current=NULL; /* SDL detaches before deleting */
  assert(!ps4glFrameQuiesce(&f) && !current && !c.command && !recording && f.failed);
  current=&c; assert(ps4glFrameBegin(&f)); assert(!ps4glFrameDestroy(&f));
  init(&f);assert(!ps4glFrameBegin(&f));fail_wait=1;
  unsigned released=releases,reset=resets,completed=completed_queries;
  assert(ps4glFrameSubmit(&f) && f.failed && busy && releases==released && completed_queries==completed);
  fail_idle=1;assert(ps4glFrameDestroy(&f) && f.command && releases==released && resets==reset);
  fail_idle=fail_wait=0;assert(!ps4glFrameDestroy(&f) && releases==released+1 && !busy);
  assert(creates==destroys);
  puts("PASS frame owner lazy acquire, replay, discard before release, empty swap, aborted recording, GPU timeout retention and cleanup retry");
}
