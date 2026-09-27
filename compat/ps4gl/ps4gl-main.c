/* Experimental application entrypoint; linked with --wrap=main. */
#include "ps4gl-display.h"
#include "ps4libc-threading.h"
#include "ps4gl-frame.h"
#include "ps4gl-sdl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "vk_ps4_log.h"
static PS4GLDisplayOwner display;
static PS4GLFrameOwner frames;
static FILE *log_file;
static int installed;
static void cleanup(void) {
  /* ScummVM must release SDL before the borrowed native objects. Never destroy
   * parents under a retained context/window after failed SDL cleanup. */
  if(installed && ps4glSDLSetOwner(NULL)) {
    fprintf(log_file,"STOP cleanup: SDL still owns resources\n"); return;
  }
  installed=0;
  if(frames.device && ps4glFrameDestroy(&frames)) {
    fprintf(log_file,"STOP cleanup: frame owner retained (%d)\n",frames.last_result); return;
  }
  if(ps4glDisplayDestroy(&display)) {
    fprintf(log_file,"STOP cleanup: display retained (%d)\n",display.last_result); return;
  }
  fprintf(log_file,"PASS application Vulkan cleanup\n"); fclose(log_file); log_file=NULL;
}
static int application_begin(void *data) {
  int rc=ps4glFrameBegin(data);
  if(rc) fprintf(log_file,"FAIL application begin: state=%u failed=%u result=%d frames=%u\n",frames.state,frames.failed,frames.last_result,frames.frames);
  return rc;
}
static int application_present(void *data) {
  PS4GLContext *c=ps4glCurrentContext();
  if(c && (frames.frames<12 || frames.frames%120==0 || c->allocation_failure)) fprintf(log_file,"FRAME %u: snapshots=%u bytes=%zu texture_uploads=%u reuses=%u reserved=%zu pages=%u cpu_textures=%zu targets=%zu failure=%s\n",
    frames.frames,c->snapshot_count,c->snapshot_bytes,c->texture_snapshot_uploads,c->texture_snapshot_reuses,c->upload_memory_bytes,c->upload_page_count,c->texture_bytes,c->target_bytes,c->allocation_failure ? c->allocation_failure : "none");
  int rc=ps4glFramePresent(data);
  if(rc || frames.frames<=3) fprintf(log_file,"%s application present: state=%u failed=%u result=%d frames=%u\n",rc ? "FAIL" : "PASS",frames.state,frames.failed,frames.last_result,frames.frames);
  return rc;
}
static int application_quiesce(void *data) {
  fprintf(log_file,"application quiesce: state=%u failed=%u frames=%u\n",frames.state,frames.failed,frames.frames);
  /* Context replacement must return an acquired image before releasing its
   * resources. Raw abort semantics remain in the frame-owner diagnostic. */
  if(frames.state && !frames.failed && ps4glFramePresent(data)) return -1;
  return ps4glFrameQuiesce(data);
}
extern int __real_main(int, char **);
int __wrap_main(int argc, char **argv) {
  ps4EnableLibcLocks();
  (void)argc; (void)argv;
  mkdir("/data/scummvm-vulkan",0777);
  log_file=fopen("/data/scummvm-vulkan/bootstrap.log","w");
  if(!log_file) return 2;
  setvbuf(log_file,NULL,_IONBF,0);
  vk_ps4_log_open("/data/scummvm-vulkan/driver.log");
  if(ps4glDisplayInit(&display)) {
    fprintf(log_file,"FAIL application display init: %d\n",display.last_result); fclose(log_file); return 2;
  }
  if(atexit(cleanup)) { cleanup(); return 2; }
  if(ps4glFrameInit(&frames,display.device,display.queue,display.swap,display.pass,
                    display.images,display.targets,display.width,display.height)) {
    fprintf(log_file,"FAIL application frame init: %d\n",frames.last_result); return 2;
  }
  memcpy(frames.depth_images,display.depth_images,sizeof(frames.depth_images));
  PS4GLSDLOwner owner={.physical=display.physical,.device=display.device,.pass=display.pass,
    .width=display.width,.height=display.height,.depth_stencil=1,.begin=application_begin,
    .present=application_present,.quiesce=application_quiesce,.data=&frames};
  if(ps4glSDLSetOwner(&owner)) { fprintf(log_file,"FAIL application SDL owner\n"); return 2; }
  installed=1;
  fprintf(log_file,"PASS application Vulkan bootstrap; entering real ScummVM main\n");
  /* A one-shot marker enables repeatable startup probes in this isolated profile. */
  FILE *target_file=fopen("/data/scummvm-vulkan/start-game-once", "r");
  if(target_file) {
    char target[128]={0};
    int read_ok=fgets(target,sizeof(target),target_file)!=NULL;
    fclose(target_file); remove("/data/scummvm-vulkan/start-game-once");
    target[strcspn(target,"\r\n")]=0;
    if(!read_ok || !target[0]) return 2;
    for(const char *p=target;*p;++p)
      if(!((*p>='a' && *p<='z') || (*p>='A' && *p<='Z') ||
           (*p>='0' && *p<='9') || *p=='_' || *p=='-')) return 2;
    fprintf(log_file,"START diagnostic target %s\n",target);
    char *args[]={"scummvm","--gfx-mode=opengl",target,NULL};
    return __real_main(3,args);
  }
  FILE *probe=fopen("/data/scummvm-vulkan/start-twp-once", "r");
  if(probe) {
    fclose(probe); remove("/data/scummvm-vulkan/start-twp-once");
    fprintf(log_file,"START diagnostic target twp\n");
    char *args[]={"scummvm","--gfx-mode=opengl","twp",NULL};
    return __real_main(3,args);
  }
  /* Use ScummVM's default verbosity; scummvm.ini can opt into debuglevel.
   * Forced level 5 makes Director log every sprite refresh through the TTY. */
  char *args[]={"scummvm","--gfx-mode=opengl",NULL};
  return __real_main(2,args);
}
