#ifndef PS4GL_SDL_H
#define PS4GL_SDL_H
#include "ps4gl.h"
/* Single-thread, one-window experimental SDL driver. Vulkan objects are borrowed.
 * Owner must outlive SDL video and discard ALL recorded GPU references in quiesce.
 * With begin supplied, clear/draw lazily starts recording and present ends it.
 * Without begin, the caller still manages recording explicitly. */
typedef struct PS4GLSDLOwner {
  VkPhysicalDevice physical;
  VkDevice device;
  VkRenderPass pass;
  int width,height;
  int (*begin)(void *); /* optional lazy frame owner, 0 success */
  int (*present)(void *);
  int (*quiesce)(void *);
  void *data;
} PS4GLSDLOwner;
int ps4glSDLSetOwner(const PS4GLSDLOwner *);
#endif
