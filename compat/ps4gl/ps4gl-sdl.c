/* Experimental ScummVM OpenGNM SDL video driver. Independently authored adapter;
 * compiled against the pinned SDL internal ABI, replacing only its PS4 bootstrap.
 * This diagnostic backend does not advertise a complete GL version. */
#include "SDL_internal.h"
/* SDL was built without Vulkan and declares dummy PFN typedefs internally.
 * Rename only those unused typedefs; do not change SDL's ABI/configuration. */
#define PFN_vkGetInstanceProcAddr SDL_unused_vkGetInstanceProcAddr
#define PFN_vkEnumerateInstanceExtensionProperties SDL_unused_vkEnumerateInstanceExtensionProperties
#include "video/SDL_sysvideo.h"
#undef PFN_vkGetInstanceProcAddr
#undef PFN_vkEnumerateInstanceExtensionProperties
#include "ps4gl-sdl.h"
#include "ps4gl-glsl.h"
#include "ps4gl-mouse.h"
#include "events/SDL_mouse_c.h"
#include "SDL_events.h"
#include "SDL_log.h"
#include <orbis/UserService.h>
#include <stdlib.h>
#include <string.h>
static PS4GLSDLOwner owner;
static PS4GLContext *context;
static SDL_Window *owned_window;
static SDL_bool video_active;
static SDL_threadID owner_thread;
static int mouse_handle=-1;
static uint32_t mouse_buttons;
static int on_thread(void) { return owner_thread==SDL_ThreadID(); }
int ps4glSDLSetOwner(const PS4GLSDLOwner *value) {
  if(video_active || context || owned_window) return SDL_SetError("GL owner is in use");
  if(!value) { memset(&owner,0,sizeof(owner)); return 0; }
  if(!value->physical || !value->device || !value->pass || value->width<=0 || value->height<=0 ||
     !value->present || !value->quiesce) return SDL_SetError("Incomplete Vulkan owner");
  owner=*value; owner_thread=SDL_ThreadID(); return 0;
}
static int available(void) { return owner.device!=VK_NULL_HANDLE; }
static int video_init(_THIS) {
  (void)_this;
  if(!available() || !on_thread()) return SDL_SetError("Vulkan owner missing or wrong thread");
  SDL_DisplayMode mode={.format=SDL_PIXELFORMAT_RGBA8888,.w=owner.width,.h=owner.height,.refresh_rate=60};
  if(SDL_AddBasicVideoDisplay(&mode)<0) return -1;
  int user;
  const uint8_t mouse_open[8]={1}; /* Merge host/USB mice into one pointer. */
  if(sceUserServiceGetInitialUser(&user)>=0 && sceMouseInit()>=0)
    mouse_handle=sceMouseOpen(user,0,0,mouse_open);
  SDL_Log("PS4 USB mouse handle: %d",mouse_handle);
  video_active=SDL_TRUE; return 0;
}
static int create_window(_THIS, SDL_Window *window) {
  (void)_this;
  if(!on_thread() || owned_window || !(window->flags&SDL_WINDOW_OPENGL) ||
     window->w!=owner.width || window->h!=owner.height || (window->flags&(SDL_WINDOW_RESIZABLE|SDL_WINDOW_FULLSCREEN)))
    return SDL_SetError("One fixed-size OpenGNM GL window required");
  owned_window=window; return 0;
}
static int load_gl(_THIS, const char *path) {
  (void)_this;
  return path ? SDL_SetError("External GL libraries are unsupported") : 0;
}
static void unload_gl(_THIS) { (void)_this; }
static void *get_proc(_THIS, const char *name) {
  (void)_this;
  PS4GLProc proc=ps4glGetProcAddress(name);
  if(!proc) SDL_SetError("Unsupported GL procedure: %s",name ? name : "(null)");
  return (void *)proc;
}
static int begin_frame(PS4GLContext *c) { (void)c; return owner.begin && !owner.begin(owner.data); }
static SDL_GLContext create_context(_THIS, SDL_Window *window) {
  if(!on_thread() || window!=owned_window || context || _this->gl_config.share_with_current_context) {
    SDL_SetError("Only one unshared GL context is supported"); return NULL;
  }
  if(_this->gl_config.depth_size || _this->gl_config.stencil_size || _this->gl_config.multisamplebuffers ||
     _this->gl_config.multisamplesamples || _this->gl_config.major_version!=2 ||
     _this->gl_config.minor_version>1 || _this->gl_config.profile_mask || _this->gl_config.flags) {
    SDL_SetError("GL request %d.%d profile=%d flags=%d depth=%d stencil=%d MSAA=%d/%d",_this->gl_config.major_version,_this->gl_config.minor_version,_this->gl_config.profile_mask,_this->gl_config.flags,_this->gl_config.depth_size,_this->gl_config.stencil_size,_this->gl_config.multisamplebuffers,_this->gl_config.multisamplesamples); return NULL;
  }
  context=calloc(1,sizeof(*context));
  if(!context) { SDL_OutOfMemory(); return NULL; }
  if(!ps4glInit(context,owner.width,owner.height) || !ps4glGLSLInitialize()) {
    free(context); context=NULL; SDL_SetError("GL context initialization failed"); return NULL;
  }
  context->default_depth_stencil=!!owner.depth_stencil;
  context->begin_frame=owner.begin ? begin_frame : NULL;
  ps4glMakeCurrent(context); ps4glConfigureVertexDevice(owner.physical,owner.device);
  ps4glConfigureShaderCompiler(owner.pass,ps4glCompileLegacyGLSL,ps4glLinkLegacyGLSL);
  return context;
}
static int make_current(_THIS, SDL_Window *window, SDL_GLContext value) {
  (void)_this;
  if(!on_thread() || (value && (value!=context || window!=owned_window))) return SDL_SetError("Invalid GL context/window/thread");
  ps4glMakeCurrent(value); return 0;
}
static int swap_window(_THIS, SDL_Window *window) {
  (void)_this;
  if(!on_thread() || window!=owned_window || !context || ps4glCurrentContext()!=context)
    return SDL_SetError("GL window is not current");
  if(context->command && !owner.begin) return SDL_SetError("Owner must end recording before SDL swap");
  if(owner.present(owner.data)) return SDL_SetError("Vulkan presentation failed");
  return 0;
}
static void delete_context(_THIS, SDL_GLContext value) {
  (void)_this;
  if(!value) return;
  if(!on_thread() || value!=context) { SDL_SetError("Invalid GL context deletion"); return; }
  if(context->command && !owner.begin) { SDL_SetError("Owner must end recording before context deletion"); return; }
  /* On failure retain resources: waiting alone is insufficient; the owner must
   * discard executable command buffers too before releasing descriptor snapshots. */
  if(owner.quiesce(owner.data)) { SDL_SetError("Vulkan owner could not quiesce"); return; }
  ps4glMakeCurrent(context);
  ps4glReleaseVertexSnapshots(context);
  ps4glDestroyFramebuffers(context); ps4glDestroyTextures(context); ps4glDestroyBuffers(context);
  ps4glDestroyPrograms(owner.device,context);
  ps4glMakeCurrent(NULL); ps4glGLSLFinalize(); free(context); context=NULL;
}
static void destroy_window(_THIS, SDL_Window *window) {
  if(window==owned_window) { if(context) delete_context(_this,context); owned_window=NULL; }
}
static void video_quit(_THIS) {
  if(context) delete_context(_this,context);
  owned_window=NULL; video_active=SDL_FALSE;
  if(mouse_handle>=0) sceMouseClose(mouse_handle);
  mouse_handle=-1; mouse_buttons=0;
}
static void drawable_size(_THIS, SDL_Window *window, int *w, int *h) {
  (void)_this; (void)window;
  if(w) *w=owner.width;
  if(h) *h=owner.height;
}
static int swap_interval(_THIS, int interval) { (void)_this; return interval==1 ? 0 : SDL_SetError("Only FIFO interval 1 supported"); }
static int get_interval(_THIS) { (void)_this; return 1; }
static void pump(_THIS) {
  (void)_this;
  if(mouse_handle<0 || !owned_window) return;
  SDL_SetMouseFocus(owned_window);
  /* Bound draining so a busy device cannot starve game/controller events. */
  for(int batch=0;batch<4;++batch) {
    PS4GLMouseData reports[64];
    int count=sceMouseRead(mouse_handle,reports,64);
    static unsigned report_log;
    if(count>0 && report_log++<12) SDL_Log("USB report count=%d first=%d,%d connected=%d buttons=%u",count,reports[0].x,reports[0].y,reports[0].connected,reports[0].buttons);
    if(count<=0 || count>64) break;
    for(int i=0;i<count;++i) {
      PS4GLMouseData *r=&reports[i];
      uint32_t buttons=r->connected ? r->buttons : 0;
      if(r->connected && (r->x || r->y))
        SDL_SendMouseMotion(owned_window,0,1,r->x,r->y);
      static const Uint8 mapping[]={SDL_BUTTON_LEFT,SDL_BUTTON_RIGHT,SDL_BUTTON_MIDDLE,SDL_BUTTON_X1,SDL_BUTTON_X2};
      for(unsigned b=0;b<5;++b)
        if((buttons^mouse_buttons)&(1u<<b))
          SDL_SendMouseButton(owned_window,0,(buttons&(1u<<b)) ? SDL_PRESSED : SDL_RELEASED,mapping[b]);
      mouse_buttons=buttons;
      if(r->connected && (r->wheel || r->tilt))
        SDL_SendMouseWheel(owned_window,0,r->tilt,r->wheel,SDL_MOUSEWHEEL_NORMAL);
    }
    if(count<64) break;
  }
}
static void free_device(SDL_VideoDevice *device) { SDL_free(device); }
static SDL_VideoDevice *create_device(int index) {
  (void)index;
  SDL_VideoDevice *device=SDL_calloc(1,sizeof(*device));
  if(!device) { SDL_OutOfMemory(); return NULL; }
  device->VideoInit=video_init; device->VideoQuit=video_quit;
  device->CreateSDLWindow=create_window; device->DestroyWindow=destroy_window;
  device->GL_LoadLibrary=load_gl; device->GL_UnloadLibrary=unload_gl; device->GL_GetProcAddress=get_proc;
  device->GL_CreateContext=create_context; device->GL_MakeCurrent=make_current;
  device->GL_DeleteContext=delete_context; device->GL_SwapWindow=swap_window;
  device->GL_GetDrawableSize=drawable_size;
  device->GL_SetSwapInterval=swap_interval; device->GL_GetSwapInterval=get_interval;
  device->PumpEvents=pump; device->free=free_device; return device;
}
VideoBootStrap PS4_bootstrap={"PS4", "Experimental OpenGNM GL adapter",available,create_device};
