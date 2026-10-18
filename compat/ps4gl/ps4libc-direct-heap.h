#ifndef PS4LIBC_DIRECT_HEAP_H
#define PS4LIBC_DIRECT_HEAP_H
#include <string.h>
#include <errno.h>
/* Optional large CPU buffers in write-back direct memory. Caller holds the
 * existing recursive allocator lock. No allocator calls in the registry. */
#define PS4_DIRECT_SLOTS 4096
struct PS4DirectAllocation { void *ptr; size_t bytes, mapped; off_t offset; };
static struct PS4DirectAllocation ps4_direct_allocations[PS4_DIRECT_SLOTS];
static unsigned ps4_direct_hash(void *p) {
  uintptr_t n=(uintptr_t)p >> 14;
  return (unsigned)((n ^ (n >> 12)) & (PS4_DIRECT_SLOTS-1));
}
static struct PS4DirectAllocation *ps4_direct_find(void *p) {
  unsigned h=ps4_direct_hash(p);
  for (unsigned i=0;i<PS4_DIRECT_SLOTS;i++) {
    struct PS4DirectAllocation *a=&ps4_direct_allocations[(h+i)&(PS4_DIRECT_SLOTS-1)];
    if (!a->ptr) return NULL;
    if (a->ptr==p) return a;
  }
  return NULL;
}
static void *ps4_direct_alloc(size_t bytes) {
  if (bytes>SIZE_MAX-0x3fff) return NULL;
  size_t mapped=(bytes+0x3fff)&~(size_t)0x3fff;
  off_t offset=0;
  /* Type 0: CPU write-back (Onion); protections 3: CPU read/write. */
  if (sceKernelAllocateDirectMemory(0,sceKernelGetDirectMemorySize(),mapped,0x4000,0,&offset)) return NULL;
  void *p=NULL;
  if (sceKernelMapDirectMemory(&p,mapped,3,0,offset,0x4000)) {
    sceKernelReleaseDirectMemory(offset,mapped); return NULL;
  }
  unsigned h=ps4_direct_hash(p);
  for (unsigned i=0;i<PS4_DIRECT_SLOTS;i++) {
    struct PS4DirectAllocation *a=&ps4_direct_allocations[(h+i)&(PS4_DIRECT_SLOTS-1)];
    if (!a->ptr || a->ptr==(void *)(uintptr_t)1) {
      *a=(struct PS4DirectAllocation){p,bytes,mapped,offset}; return p;
    }
  }
  sceKernelMunmap(p,mapped); sceKernelReleaseDirectMemory(offset,mapped); return NULL;
}
static void ps4_direct_free(struct PS4DirectAllocation *a) {
  sceKernelMunmap(a->ptr,a->mapped);
  sceKernelReleaseDirectMemory(a->offset,a->mapped);
  a->ptr=(void *)(uintptr_t)1; /* Tombstone preserves probe chains. */
}
static void *ps4_heap_alloc(size_t bytes) {
  void *p=NULL;
  if (allocator_ready && bytes>=256*1024) p=ps4_direct_alloc(bytes);
  return p ? p : __real_malloc(bytes);
}
static void *ps4_heap_realloc(void *p,size_t bytes) {
  if (!p) return ps4_heap_alloc(bytes);
  struct PS4DirectAllocation *a=ps4_direct_find(p);
  if (!a) return __real_realloc(p,bytes);
  if (!bytes) { ps4_direct_free(a); return NULL; }
  if (bytes<=a->mapped) { a->bytes=bytes; return p; }
  void *next=ps4_heap_alloc(bytes);
  if (!next) return NULL; /* Preserve old allocation on failure. */
  memcpy(next,p,a->bytes); ps4_direct_free(a); return next;
}
#endif
