#include <SDL2/SDL.h>
#include "ps4gl-sdl.h"
#include "ps4gl-frame.c"
#include "ps4gl-display.c"
typedef struct SDLProbeOwner {
  PS4GLFrameOwner frames;
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
  if(ps4glFramePresent(&state->frames)) return -1;
  ++state->presented; return 0;
}
static int sdl_quiesce(void *data) {
  SDLProbeOwner *state=data;
  if(state->fail_quiesce) return -1;
  if(ps4glFrameQuiesce(&state->frames)) return -1;
  ++state->quiesced; return 0;
}
static int sdl_begin(void *data) { return ps4glFrameBegin(&((SDLProbeOwner *)data)->frames); }
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
  const SDL_GLattr attrs[]={SDL_GL_RED_SIZE,SDL_GL_GREEN_SIZE,SDL_GL_BLUE_SIZE,SDL_GL_ALPHA_SIZE,SDL_GL_DEPTH_SIZE,SDL_GL_STENCIL_SIZE,SDL_GL_DOUBLEBUFFER,SDL_GL_MULTISAMPLEBUFFERS,SDL_GL_MULTISAMPLESAMPLES};
  const int expected[]={8,8,8,8,0,0,1,0,0};
  for(unsigned i=0;i<sizeof(attrs)/sizeof(attrs[0]);++i) { int value=-1;
    if(SDL_GL_GetAttribute(attrs[i],&value) || value!=expected[i]) return NULL;
  }
  GLint limit=0; glGetIntegerv(GL_MAX_TEXTURE_SIZE,&limit); if(limit!=PS4GL_TEXTURE_LIMIT) return NULL;
  glGetIntegerv(GL_MAX_VERTEX_ATTRIBS,&limit); if(limit!=PS4GL_MAX_ATTRIBS) return NULL;
  fprintf(report,"PASS SDL numeric framebuffer and shader limits\n");
  /* Replay the actual launcher's 2D startup state before any frame acquisition. */
  glDisable(GL_CULL_FACE); glDisable(GL_DEPTH_TEST); glDisable(GL_DITHER);
  glPixelStorei(GL_PACK_ALIGNMENT,4);
  glGetIntegerv(GL_PACK_ALIGNMENT,&limit);
  if(limit!=4 || glIsEnabled(GL_CULL_FACE) || glIsEnabled(GL_DEPTH_TEST) ||
     glIsEnabled(GL_DITHER) || glGetError()!=GL_NO_ERROR || ps4glCurrentContext()->command) return NULL;
  /* Pack state must never change texture upload row layout. */
  glPixelStorei(GL_PACK_ALIGNMENT,8); glPixelStorei(GL_UNPACK_ALIGNMENT,1);
  glGetIntegerv(GL_PACK_ALIGNMENT,&limit); if(limit!=8) return NULL;
  glGetIntegerv(GL_UNPACK_ALIGNMENT,&limit); if(limit!=1) return NULL;
  glPixelStorei(GL_PACK_ALIGNMENT,4); glPixelStorei(GL_UNPACK_ALIGNMENT,4);
  glEnable(GL_CULL_FACE); if(glGetError()!=GL_INVALID_OPERATION) return NULL;
  fprintf(report,"PASS SDL launcher 2D startup state and separate pack alignment\n");
  int w=0,h=0; SDL_GL_GetDrawableSize(*window,&w,&h); if(w!=1280 || h!=720) return NULL;
  if(SDL_GL_SetSwapInterval(0)!=-1 || SDL_GL_SetSwapInterval(1) || SDL_GL_GetSwapInterval()!=1) return NULL;
  if(SDL_GL_MakeCurrent(NULL,NULL) || ps4glCurrentContext() || SDL_GL_GetCurrentContext()) return NULL;
  if(SDL_GL_MakeCurrent(*window,context) || ps4glCurrentContext()!=context) return NULL;
  if(SDL_GL_MakeCurrent(*window,(SDL_GLContext)(uintptr_t)1)!=-1 || ps4glCurrentContext()!=context) return NULL;
  if(ps4glSDLSetOwner(NULL)!=-1) return NULL;
  SDL_ClearError(); fprintf(report,"PASS SDL context proc lookup detach reattach and rejected operations\n");
  return context;
}
