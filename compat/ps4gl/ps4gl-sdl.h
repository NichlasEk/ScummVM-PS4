#ifndef PS4GL_SDL_H
#define PS4GL_SDL_H
#include "ps4gl.h"
/* Single-thread, one-window experimental SDL driver. Vulkan objects are borrowed.
 * Owner must outlive SDL video and discard ALL recorded GPU references in quiesce.
 * Frame acquire/record/submit remains the owner's responsibility in this slice. */
typedef struct PS4GLSDLOwner {
  VkPhysicalDevice physical;
  VkDevice device;
  VkRenderPass pass;
  int width,height;
  int (*present)(void *);
  int (*quiesce)(void *);
  void *data;
} PS4GLSDLOwner;
int ps4glSDLSetOwner(const PS4GLSDLOwner *);
#endif
