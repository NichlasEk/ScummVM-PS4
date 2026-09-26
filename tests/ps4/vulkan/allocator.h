/* Stress the same small-allocation classes used concurrently by graphics/audio. */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { unsigned seed, round; int start, failed; } AllocatorWorker;
static void *allocator_worker(void *arg) {
  AllocatorWorker *worker=arg;
  while(!__atomic_load_n(&worker->start,__ATOMIC_ACQUIRE)) __asm__ volatile("pause");
  for(unsigned round=0;round<10000 && !worker->failed;++round) {
    worker->round=round;
    unsigned char *blocks[8]={0}; size_t sizes[8];
    for(unsigned i=0;i<8;++i) {
      sizes[i]=32+((round*17+i*41+worker->seed)%2048);
      blocks[i]=malloc(sizes[i]);
      if(!blocks[i]) { worker->failed=1; break; }
      memset(blocks[i],(unsigned char)(worker->seed+i+round),sizes[i]);
    }
    for(unsigned i=0;i<8 && !worker->failed;++i) {
      unsigned char *grown=realloc(blocks[i],sizes[i]+512);
      if(!grown) { worker->failed=2; break; }
      blocks[i]=grown;
      for(size_t b=0;b<sizes[i];++b) if(grown[b]!=(unsigned char)(worker->seed+i+round)) { worker->failed=3; break; }
      memset(grown,0xa5,sizes[i]+512);
    }
    for(unsigned i=0;i<8;++i) { free(blocks[i]); }
  }
  return NULL;
}
static int check_concurrent_allocator(FILE *report) {
  AllocatorWorker workers[2]={{.seed=13},{.seed=91}};
  pthread_t threads[2]; unsigned created=0;
  for(;created<2;++created) if(pthread_create(&threads[created],NULL,allocator_worker,&workers[created])) break;
  for(unsigned i=0;i<created;++i) __atomic_store_n(&workers[i].start,1,__ATOMIC_RELEASE);
  int ok=created==2;
  for(unsigned i=0;i<created;++i) { if(pthread_join(threads[i],NULL) || workers[i].failed) ok=0; }
  if(!ok) fprintf(report,"allocator details: created=%u worker0=%d at %u worker1=%d at %u gate=%d\n",created,workers[0].failed,workers[0].round,workers[1].failed,workers[1].round,__libc.threads_minus_1);
  fprintf(report,"%s concurrent allocator: two threads, 160000 allocate/reallocate/free cycles\n",ok ? "PASS" : "FAIL");
  return ok;
}
