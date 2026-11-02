#!/usr/bin/env python3
"""Exercise production frame retirement with mocked Vulkan resource teardown."""
from pathlib import Path
import subprocess, tempfile
root = Path(__file__).resolve().parents[2]
s = (root / 'compat/ps4gl/ps4gl-vulkan.c').read_text()
a = s.index('static void retire_vertex_snapshots(')
b = s.index('\n}', a) + 2
code = r'''
#include "ps4gl.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
static unsigned pools, buffers, memories, unmaps, queries;
static void release_queries(PS4GLContext *c) {(void)c;++queries;}
static void retire_texture_cache(PS4GLContext *c,int all) {(void)c;(void)all;}
static void cache_retired_texture(PS4GLContext *c,PS4GLTextureSnapshot *t) {(void)c;(void)t;}
static void destroy_texture_snapshot(VkDevice d,PS4GLTextureSnapshot *t) {(void)d;(void)t;}
static void collect_targets(PS4GLContext *c,int all) {(void)c;(void)all;}
VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorPool(VkDevice d,VkDescriptorPool p,const VkAllocationCallbacks *a) {(void)d;(void)p;(void)a;++pools;}
VKAPI_ATTR void VKAPI_CALL vkUnmapMemory(VkDevice d,VkDeviceMemory m) {(void)d;(void)m;++unmaps;}
VKAPI_ATTR void VKAPI_CALL vkDestroyBuffer(VkDevice d,VkBuffer b,const VkAllocationCallbacks *a) {(void)d;(void)b;(void)a;++buffers;}
VKAPI_ATTR void VKAPI_CALL vkFreeMemory(VkDevice d,VkDeviceMemory m,const VkAllocationCallbacks *a) {(void)d;(void)m;(void)a;++memories;}
'''
code += s[a:b]
code += r'''
int main(void) {
 PS4GLContext *c=calloc(1,sizeof(*c)); assert(c);
 c->snapshot_capacity=64;c->snapshots=calloc(64,sizeof(*c->snapshots));assert(c->snapshots);
 PS4GLSnapshot *records=c->snapshots;
 c->upload_page_count=1;c->upload_memory_bytes=1048576;
 c->upload_pages[0]=(PS4GLUploadPage){.buffer=(VkBuffer)1,.memory=(VkDeviceMemory)2,.mapped=(void*)3,.size=1048576,.allocation_bytes=1048576,.used=4096};
 c->command=(VkCommandBuffer)1;
 retire_vertex_snapshots(c,0);
 assert(c->error==GL_INVALID_OPERATION && c->upload_pages[0].used==4096 && queries==0);
 c->command=0;c->error=0;
 for(unsigned frame=0;frame<1000;++frame) {
  c->snapshot_count=1;c->snapshot_bytes=4096;c->snapshots[0].pool=(VkDescriptorPool)4;c->upload_pages[0].used=4096;
  retire_vertex_snapshots(c,0);
  assert(c->snapshots==records && c->snapshot_capacity==64 && !c->snapshot_count && !c->snapshot_bytes);
  assert(c->upload_page_count==1 && c->upload_memory_bytes==1048576 && !c->upload_pages[0].used);
  assert(c->upload_pages[0].buffer==(VkBuffer)1 && c->upload_pages[0].mapped==(void*)3);
 }
 assert(pools==1000 && !buffers && !memories && !unmaps);
 retire_vertex_snapshots(c,1);
 assert(!c->snapshots && !c->snapshot_capacity && !c->upload_page_count && !c->upload_memory_bytes);
 assert(buffers==1 && memories==1 && unmaps==1);
 retire_vertex_snapshots(c,1);assert(buffers==1 && memories==1);free(c);
 puts("PASS upload storage reused for 1000 frames, recording rejected, full and repeated teardown");
}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c11','-fsanitize=address,undefined','-g','-I'+str(root/'compat/ps4gl'),'-I'+str(root/'.tools/opengnm-stack/Vulkan-Headers/include'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
