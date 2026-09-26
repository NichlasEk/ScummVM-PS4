#ifndef PS4GL_FRAME_H
#define PS4GL_FRAME_H
#include "ps4gl.h"
/* Borrowed device/swapchain/attachments; owned command pool, buffer and fence.
 * Zero-initialize before Init; destroy before reinitializing.
 * One thread and one queue. LOAD/STORE RGBA8 GENERAL default render pass required.
 * Parent objects must outlive this owner. A failed Vulkan operation poisons it:
 * quiesce/destroy it and recreate the parent swapchain before resuming. */
typedef struct PS4GLFrameOwner {
  VkDevice device; VkQueue queue; VkSwapchainKHR swap;
  VkRenderPass pass; VkImage images[2],depth_images[2]; VkFramebuffer targets[2];
  VkCommandPool pool; VkCommandBuffer command; VkFence fence;
  PS4GLContext *context;
  uint32_t image; int width,height;
  unsigned state,failed,frames; /* 0 idle, 1 recording, 2 completed submission */
  VkResult last_result;
} PS4GLFrameOwner;
int ps4glFrameInit(PS4GLFrameOwner *, VkDevice, VkQueue, VkSwapchainKHR, VkRenderPass,
                   const VkImage[2], const VkFramebuffer[2], int, int);
int ps4glFrameBegin(void *);
int ps4glFrameSubmit(PS4GLFrameOwner *); /* May replay the completed recording before present. */
int ps4glFramePresent(void *);
int ps4glFrameQuiesce(void *); /* Wait, discard recording, release snapshots; no presentation. */
int ps4glFrameDestroy(PS4GLFrameOwner *); /* Failure retains resources for retry. */
#endif
