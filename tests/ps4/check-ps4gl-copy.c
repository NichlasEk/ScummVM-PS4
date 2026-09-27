#include "ps4gl.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static unsigned reads,writes,ends;
static unsigned char source[6*4*4],target[5*4*4],staging[512];
static PS4GLSnapshot record;
static int allocation_failure(PS4GLContext *c,const char *reason){(void)reason;c->error=GL_OUT_OF_MEMORY;return 0;}
static int prepare_render_target(PS4GLContext *c){return c->managed_frame;}
static void end_render_target(PS4GLContext *c){++ends;c->active_target=0;}
static int create_target(PS4GLContext *c,PS4GLTexture *t){(void)c;(void)t;assert(0);return 0;}
static int upload_vertices(PS4GLContext *c,const void *data,size_t size,size_t vb,GLenum type){
  assert(data && size<=sizeof(staging)-16 && !vb && !type);record=(PS4GLSnapshot){.buffer=(VkBuffer)1,.offset=16};c->snapshots=&record;c->snapshot_count=1;return 1;
}
static void target_barrier(PS4GLContext *c,PS4GLRenderTarget *t,VkImageLayout layout){(void)c;t->layout=layout;}
VKAPI_ATTR void VKAPI_CALL vkCmdPipelineBarrier(VkCommandBuffer cmd,VkPipelineStageFlags a,VkPipelineStageFlags b,VkDependencyFlags flags,uint32_t m,const VkMemoryBarrier *mb,uint32_t n,const VkBufferMemoryBarrier *bb,uint32_t p,const VkImageMemoryBarrier *ib){
  (void)cmd;(void)a;(void)b;(void)flags;(void)m;(void)mb;(void)p;(void)ib;
  if(n) assert(bb->srcAccessMask==VK_ACCESS_TRANSFER_WRITE_BIT && bb->dstAccessMask==VK_ACCESS_TRANSFER_READ_BIT);
}
VKAPI_ATTR void VKAPI_CALL vkCmdCopyImageToBuffer(VkCommandBuffer cmd,VkImage image,VkImageLayout layout,VkBuffer buffer,uint32_t count,const VkBufferImageCopy *r){
  (void)cmd;assert(image && layout==VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && buffer==(VkBuffer)1 && count==1);++reads;
  for(unsigned y=0;y<r->imageExtent.height;++y) memcpy(staging+r->bufferOffset+y*r->imageExtent.width*4,source+((r->imageOffset.y+y)*6+r->imageOffset.x)*4,r->imageExtent.width*4);
}
VKAPI_ATTR void VKAPI_CALL vkCmdCopyBufferToImage(VkCommandBuffer cmd,VkBuffer buffer,VkImage image,VkImageLayout layout,uint32_t count,const VkBufferImageCopy *regions){
  (void)cmd;assert(image==(VkImage)3 && buffer==(VkBuffer)1 && layout==VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);++writes;
  for(unsigned i=0;i<count;++i){const VkBufferImageCopy *r=&regions[i];assert(r->imageExtent.height==1);memcpy(target+(r->imageOffset.y*5+r->imageOffset.x)*4,staging+r->bufferOffset,r->imageExtent.width*4);}
}
#include "ps4gl-copy-vulkan.h"
int main(void){
  static PS4GLContext c;unsigned char pixels[5*4*4]={0};
  c.managed_frame=1;c.command=(VkCommandBuffer)1;c.default_image=(VkImage)1;c.width=6;c.height=4;
  c.targets[0].image=(VkImage)2;c.targets[1].image=(VkImage)3;
  PS4GLTexture texture={.width=5,.height=4,.pixels=pixels,.render_target=2};
  for(unsigned y=0;y<4;++y)for(unsigned x=0;x<6;++x)memset(source+(y*6+x)*4,y*10+x,4);
  assert(copy_texture(&c,&texture,1,1,2,1,3,2));
  assert(target[(1*5+1)*4]==22 && target[(2*5+1)*4]==12 && target[(2*5+3)*4]==14 && target[0]==0 && reads==1 && writes==1);
  memset(target,0,sizeof(target));c.active_target=1;
  assert(copy_texture(&c,&texture,1,1,2,1,3,2));assert(target[(1*5+1)*4]==12 && target[(2*5+1)*4]==22);
  unsigned before=ends;assert(!copy_texture(&c,&texture,0,0,5,0,2,1) && c.error==GL_INVALID_VALUE && ends==before && reads==2);
  puts("PASS GPU copy regions: default framebuffer row reversal, offscreen row preservation, partial offsets, bounds and transfer dependency");
}
