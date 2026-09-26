#ifndef PS4GL_DISPLAY_H
#define PS4GL_DISPLAY_H
#include <vulkan/vulkan.h>
/* Native OpenGNM display, not a portable surface-based Vulkan WSI backend.
 * Zero initialize. One thread, fixed 1280x720, two RGBA8-sRGB FIFO images.
 * Destroy every borrowing frame owner, GL context and device resource first.
 * Failed initialization unwinds automatically (no work was submitted).
 * Failed WaitIdle on destruction retains all handles for an explicit retry. */
typedef struct PS4GLDisplayOwner {
  VkInstance instance;
  VkPhysicalDevice physical;
  VkDevice device;
  VkQueue queue;
  VkSwapchainKHR swap;
  VkRenderPass pass;
  VkImage images[2];
  VkImageView views[2];
  VkFramebuffer targets[2];
  unsigned width,height,ready;
  VkResult last_result;
} PS4GLDisplayOwner;
#ifdef __cplusplus
extern "C" {
#endif
int ps4glDisplayInit(PS4GLDisplayOwner *);
int ps4glDisplayDestroy(PS4GLDisplayOwner *);
#ifdef __cplusplus
}
#endif
#endif
