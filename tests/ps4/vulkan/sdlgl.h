#include <SDL2/SDL.h>
#include "ps4gl-sdl.h"
typedef struct SDLProbeOwner {
  VkDevice device;
  VkQueue queue;
  VkSwapchainKHR swap;
  VkCommandBuffer command;
  uint32_t index;
  unsigned presented,quiesced;
  int fail_present,fail_quiesce;
} SDLProbeOwner;
static int sdl_present(void *data) {
  SDLProbeOwner *state=data;
  if(state->fail_present) return -1;
  VkPresentInfoKHR info={.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,.swapchainCount=1,
    .pSwapchains=&state->swap,.pImageIndices=&state->index};
  CHECK(vkQueuePresentKHR(state->queue,&info)); ++state->presented; return 0;
}
static int sdl_quiesce(void *data) {
  SDLProbeOwner *state=data;
  if(state->fail_quiesce) return -1;
  CHECK(vkDeviceWaitIdle(state->device));
  if(state->command) CHECK(vkResetCommandBuffer(state->command,0));
  ++state->quiesced; return 0;
}
static SDL_GLContext sdl_create(SDL_Window **window) {
  if(SDL_Init(SDL_INIT_VIDEO)) return NULL;
  if(SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,0) || SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE,0)) return NULL;
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,0); SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,0);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,0); SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES,0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
  *window=SDL_CreateWindow("OpenGNM SDL diagnostic",0,0,1280,720,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
  if(!*window) return NULL;
  if(SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24)) return NULL;
  if(SDL_GL_CreateContext(*window)) return NULL;
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,0);
  SDL_GLContext context=SDL_GL_CreateContext(*window);
  if(!context || SDL_GL_GetCurrentContext()!=context || SDL_GL_GetCurrentWindow()!=*window) return NULL;
  if(SDL_GL_GetProcAddress("glUniform1i")!=(void *)ps4glGetProcAddress("glUniform1i") ||
     SDL_GL_GetProcAddress("glNotImplemented") || SDL_GL_CreateContext(*window)) return NULL;
  int w=0,h=0; SDL_GL_GetDrawableSize(*window,&w,&h); if(w!=1280 || h!=720) return NULL;
  if(SDL_GL_SetSwapInterval(0)!=-1 || SDL_GL_SetSwapInterval(1) || SDL_GL_GetSwapInterval()!=1) return NULL;
  if(SDL_GL_MakeCurrent(NULL,NULL) || ps4glCurrentContext() || SDL_GL_GetCurrentContext()) return NULL;
  if(SDL_GL_MakeCurrent(*window,context) || ps4glCurrentContext()!=context) return NULL;
  if(SDL_GL_MakeCurrent(*window,(SDL_GLContext)(uintptr_t)1)!=-1 || ps4glCurrentContext()!=context) return NULL;
  if(ps4glSDLSetOwner(NULL)!=-1) return NULL;
  SDL_ClearError(); fprintf(report,"PASS SDL context proc lookup detach reattach and rejected operations\n");
  return context;
}
