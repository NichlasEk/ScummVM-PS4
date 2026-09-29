/* Exercise actual PS4 command-pool allocation paths with tracked direct-memory
 * stubs under ASan/UBSan. This tests ownership, not physical GPU execution. */
#include "vk_ps4_internal.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static unsigned live_direct, allocations, fail_at;
void *vk_ps4_alloc(const VkAllocationCallbacks *a,size_t n,size_t alignment) { (void)a;(void)alignment;return malloc(n); }
void *vk_ps4_alloc_zero(const VkAllocationCallbacks *a,size_t n,size_t alignment) {void *p=vk_ps4_alloc(a,n,alignment);if(p)memset(p,0,n);return p;}
void vk_ps4_free(const VkAllocationCallbacks *a,void *p) {(void)a;free(p);}
void vk_ps4_log(const char *format,...) {(void)format;}
void vk_ps4_log_raw(const char *message) {(void)message;}
GnmError sceGnmDirectMemoryAllocate(GnmDirectMemory *m,uint64_t size,uint64_t alignment,int32_t type,int32_t prot) {
 assert(alignment==65536);assert(type==GNM_DIRECT_MEMORY_TYPE_WC_GARLIC);assert(prot==GNM_PROT_CPU_GPU_RW);
 if (fail_at && allocations+1==fail_at) return GNM_ERROR_INTERNAL_FAILURE;
 memset(m,0,sizeof(*m));m->mapped=malloc(size);assert(m->mapped);m->allocated=true;++live_direct;++allocations;return GNM_ERROR_OK;
}
void sceGnmDirectMemoryRelease(GnmDirectMemory *m) {assert(m->allocated);free(m->mapped);memset(m,0,sizeof(*m));assert(live_direct);--live_direct;}
int main(void) {
 VkPs4Device dev={0};VkCommandPool pool;
 VkCommandPoolCreateInfo ci={.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
 assert(vk_ps4_CreateCommandPool((VkDevice)&dev,&ci,NULL,&pool)==VK_SUCCESS);
 VkCommandBufferAllocateInfo ai={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,.commandPool=pool,.commandBufferCount=1};VkCommandBuffer cb;
 assert(vk_ps4_AllocateCommandBuffers((VkDevice)&dev,&ai,&cb)==VK_SUCCESS);
 VkPs4CommandBuffer *c=(void*)cb;void *address=c->pm4_buffer;
 assert(c->pm4_memory.allocated && address==c->pm4_memory.mapped && live_direct==1);
 c->pm4_buffer[0]=0xabcdef;
 vk_ps4_FreeCommandBuffers((VkDevice)&dev,pool,1,&cb);
 assert(live_direct==1);
 assert(vk_ps4_AllocateCommandBuffers((VkDevice)&dev,&ai,&cb)==VK_SUCCESS);
 c=(void*)cb;assert(c->pm4_buffer==address && c->pm4_memory.allocated && c->pm4_buffer[0]==0 && allocations==1);
 vk_ps4_DestroyCommandPool((VkDevice)&dev,pool,NULL);assert(live_direct==0);
 assert(vk_ps4_CreateCommandPool((VkDevice)&dev,&ci,NULL,&pool)==VK_SUCCESS);ai.commandPool=pool;
 assert(vk_ps4_AllocateCommandBuffers((VkDevice)&dev,&ai,&cb)==VK_SUCCESS);
 vk_ps4_FreeCommandBuffers((VkDevice)&dev,pool,1,&cb);
 vk_ps4_TrimCommandPool((VkDevice)&dev,pool,0);assert(live_direct==0);
 vk_ps4_DestroyCommandPool((VkDevice)&dev,pool,NULL);assert(live_direct==0);
 assert(vk_ps4_CreateCommandPool((VkDevice)&dev,&ci,NULL,&pool)==VK_SUCCESS);
 ai.commandPool=pool;ai.commandBufferCount=2;VkCommandBuffer pair[2]={0};fail_at=allocations+2;
 assert(vk_ps4_AllocateCommandBuffers((VkDevice)&dev,&ai,pair)==VK_ERROR_OUT_OF_DEVICE_MEMORY);
 assert(live_direct==0 && ((VkPs4CommandPool*)pool)->command_buffer_count==0);
 vk_ps4_DestroyCommandPool((VkDevice)&dev,pool,NULL);assert(live_direct==0);
 puts("PASS: partial-allocation rollback; direct allocation, stable reuse, destroy active and trim cached buffers");
}
