#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
static int allocator_ready=1, fail_alloc, fail_map, fail_libc, allocated, mapped_count;
static size_t sceKernelGetDirectMemorySize(void) { return (size_t)4<<30; }
static int sceKernelAllocateDirectMemory(off_t a,off_t b,size_t n,size_t align,int type,off_t *out) {
 (void)a;(void)b; assert(n && !(n%16384) && align==16384 && type==0);
 if(fail_alloc)return -1; *out=123; ++allocated;return 0;
}
static int sceKernelMapDirectMemory(void **out,size_t n,int prot,int flags,off_t off,size_t align) {
 assert(prot==3 && !flags && off==123 && align==16384);
 if(fail_map)return -1; *out=aligned_alloc(align,n);assert(*out);++mapped_count;return 0;
}
static int sceKernelReleaseDirectMemory(off_t off,size_t n) { (void)off;(void)n;--allocated;return 0; }
static int sceKernelMunmap(void *p,size_t n) { (void)n;free(p);--mapped_count;return 0; }
static void *__real_malloc(size_t n) { return fail_libc?NULL:malloc(n); }
static void *__real_realloc(void *p,size_t n) { return fail_libc?NULL:realloc(p,n); }
#include "compat/ps4gl/ps4libc-direct-heap.h"
int main(void) {
 unsigned char *p=ps4_heap_alloc(300000); assert(p && ps4_direct_find(p)); memset(p,0x5a,300000);
 p=ps4_heap_realloc(p,700000);assert(p);for(int i=0;i<300000;i++)assert(p[i]==0x5a);
 void *old=p;fail_alloc=fail_libc=1;assert(!ps4_heap_realloc(p,2000000));assert(ps4_direct_find(old));assert(p[123]==0x5a);fail_alloc=fail_libc=0;
 assert(ps4_heap_realloc(p,100)==p);assert(ps4_heap_realloc(p,0)==NULL);assert(!allocated&&!mapped_count);
 fail_map=1;assert(!ps4_direct_alloc(300000));assert(!allocated&&!mapped_count);fail_map=0;
 assert(!ps4_direct_alloc(SIZE_MAX));
 void *items[PS4_DIRECT_SLOTS];for(int i=0;i<PS4_DIRECT_SLOTS;i++){items[i]=ps4_direct_alloc(1);assert(items[i]);}
 assert(!ps4_direct_alloc(1));
 for(int i=0;i<PS4_DIRECT_SLOTS;i+=2)ps4_direct_free(ps4_direct_find(items[i]));
 for(int i=1;i<PS4_DIRECT_SLOTS;i+=2){assert(ps4_direct_find(items[i]));ps4_direct_free(ps4_direct_find(items[i]));}
 assert(!allocated&&!mapped_count);
 p=ps4_direct_alloc(5);assert(p&&ps4_direct_find(p));ps4_direct_free(ps4_direct_find(p));
 p=ps4_heap_alloc(16);assert(p&&!ps4_direct_find(p));p=ps4_heap_realloc(p,32);free(p);
 return 0;
}
