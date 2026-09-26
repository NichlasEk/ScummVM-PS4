#ifdef GLSTATE_PROBE
#include "ps4gl.c"
#endif
/* All offscreen resources are created through public Vulkan API. */
static VkImage off_image;
static VkDeviceMemory off_memory;
static VkImageView off_view;
static VkFramebuffer off_fb;
static VkRenderPass off_pass;
static VkDescriptorSetLayout off_set_layout;
static VkDescriptorPool off_pool;
static VkDescriptorSet off_set;
static VkSampler off_sampler;
#ifdef READBACK_PROBE
#include "readback.h"
#endif
static int create_offscreen(VkDevice device, VkPhysicalDevice physical) {
  VkImageCreateInfo ic = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                          .imageType = VK_IMAGE_TYPE_2D,
                          .format = VK_FORMAT_R8G8B8A8_UNORM,
                          .extent = {80, 60, 1},
                          .mipLevels = 1,
                          .arrayLayers = 1,
                          .samples = VK_SAMPLE_COUNT_1_BIT,
                          .tiling = VK_IMAGE_TILING_OPTIMAL,
                          .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                   VK_IMAGE_USAGE_SAMPLED_BIT
#ifdef READBACK_PROBE
                                   | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
#endif
                                   };
  CHECK(vkCreateImage(device, &ic, NULL, &off_image));
  VkMemoryRequirements req;
  vkGetImageMemoryRequirements(device, off_image, &req);
  VkPhysicalDeviceMemoryProperties props;
  vkGetPhysicalDeviceMemoryProperties(physical, &props);
  uint32_t type = UINT32_MAX;
  for (uint32_t i = 0; i < props.memoryTypeCount; ++i)
    if ((req.memoryTypeBits & (1u << i)) &&
        (props.memoryTypes[i].propertyFlags &
         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
      type = i;
      break;
    }
  VkMemoryAllocateInfo ai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                             .allocationSize = req.size,
                             .memoryTypeIndex = type};
  CHECK(vkAllocateMemory(device, &ai, NULL, &off_memory));
  CHECK(vkBindImageMemory(device, off_image, off_memory, 0));
  VkImageViewCreateInfo vi = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = off_image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
  CHECK(vkCreateImageView(device, &vi, NULL, &off_view));
  VkAttachmentDescription att = {
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkAttachmentReference ref = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkSubpassDescription sub = {.pipelineBindPoint =
                                  VK_PIPELINE_BIND_POINT_GRAPHICS,
                              .colorAttachmentCount = 1,
                              .pColorAttachments = &ref};
  VkRenderPassCreateInfo rp = {.sType =
                                   VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                               .attachmentCount = 1,
                               .pAttachments = &att,
                               .subpassCount = 1,
                               .pSubpasses = &sub};
  CHECK(vkCreateRenderPass(device, &rp, NULL, &off_pass));
  VkFramebufferCreateInfo fb = {.sType =
                                    VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                .renderPass = off_pass,
                                .attachmentCount = 1,
                                .pAttachments = &off_view,
                                .width = 80,
                                .height = 60,
                                .layers = 1};
  CHECK(vkCreateFramebuffer(device, &fb, NULL, &off_fb));
  VkSamplerCreateInfo sc = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_NEAREST,
      .minFilter = VK_FILTER_NEAREST,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE};
  CHECK(vkCreateSampler(device, &sc, NULL, &off_sampler));
  VkDescriptorSetLayoutBinding binding = {
      3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
      VK_SHADER_STAGE_FRAGMENT_BIT, NULL};
  VkDescriptorSetLayoutCreateInfo dl = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &binding};
  CHECK(vkCreateDescriptorSetLayout(device, &dl, NULL, &off_set_layout));
  VkDescriptorPoolSize size = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
  VkDescriptorPoolCreateInfo pc = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = 1,
      .poolSizeCount = 1,
      .pPoolSizes = &size};
  CHECK(vkCreateDescriptorPool(device, &pc, NULL, &off_pool));
  VkDescriptorSetAllocateInfo sa = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = off_pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &off_set_layout};
  CHECK(vkAllocateDescriptorSets(device, &sa, &off_set));
  VkDescriptorImageInfo ii = {off_sampler, off_view,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                .dstSet = off_set,
                                .dstBinding = 3,
                                .descriptorCount = 1,
                                .descriptorType =
                                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                .pImageInfo = &ii};
  vkUpdateDescriptorSets(device, 1, &write, 0, NULL);
  #ifdef READBACK_PROBE
  if (create_readback(device, physical)) return 2;
  #endif
  return 0;
}
static void draw_offscreen(VkCommandBuffer cmd, unsigned frame,
                           const VkPipeline *pipelines) {
  VkImageMemoryBarrier barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = frame ? VK_ACCESS_SHADER_READ_BIT : 0,
      .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .oldLayout = frame ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                         : VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = off_image,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
#ifndef READBACK_PROBE
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                       NULL, 0, NULL, 1, &barrier);
#endif
#ifdef READBACK_PROBE
  readback_clear(cmd,frame);
  readback_barrier(cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
#endif
  VkRenderPassBeginInfo rp = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                              .renderPass = off_pass,
                              .framebuffer = off_fb,
                              .renderArea = {{0, 0}, {80, 60}}};
  vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
  VkViewport vp = {.width = 80, .height = 60, .maxDepth = 1};
  VkRect2D full = {{0, 0}, {80, 60}}, left = {{0, 0}, {40, 60}},
           topright = {{40, 0}, {40, 30}};
  vkCmdSetViewport(cmd, 0, 1, &vp);
  vkCmdSetScissor(cmd, 0, 1, &full);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[0]);
  vkCmdDraw(cmd, 3, 1, 0, 0);
  vkCmdSetScissor(cmd, 0, 1, &left);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[1]);
#ifdef RECTCLEAR_PROBE
#ifdef GLSTATE_PROBE
  PS4GLContext gl;
  ps4glInit(&gl,80,60);
  ps4glMakeCurrent(&gl);
  ps4glBeginPass(cmd);
  void (*clear_gl)(GLbitfield) = (void (*)(GLbitfield))ps4glGetProcAddress("glClear");
  glViewport(1,2,3,4); /* GL viewport must not restrict clears. */
  glEnable(GL_SCISSOR_TEST);
  glClearColor(.2f,.4f,.6f,.8f);
  glScissor(6,39,11,13); clear_gl(GL_COLOR_BUFFER_BIT);
  glScissor(49,14,17,9); clear_gl(GL_COLOR_BUFFER_BIT);
  glClearColor(.7f,.1f,.3f,.9f);
  glScissor(12,35,13,9); clear_gl(GL_COLOR_BUFFER_BIT);
  fprintf(report,"GL state frame %u error=%u\n",frame,glGetError());
  ps4glEndPass();
  ps4glMakeCurrent(NULL);
#else
  VkClearAttachment clear = {.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
    .colorAttachment=0, .clearValue.color.float32={.2f,.4f,.6f,.8f}};
  VkClearRect rects[]={{{{6,8},{11,13}},0,1}, {{{49,37},{17,9}},0,1}};
  /* Ignore bound blending/scissor during clear, restore them for the next draw. */
  vkCmdClearAttachments(cmd,1,&clear,2,rects);
  clear.clearValue.color=(VkClearColorValue){.float32={.7f,.1f,.3f,.9f}};
  VkClearRect overlap={{{12,16},{13,9}},0,1};
  vkCmdClearAttachments(cmd,1,&clear,1,&overlap);
#endif
#endif
  vkCmdDraw(cmd, 3, 1, 0, 0);
  vkCmdSetScissor(cmd, 0, 1, &topright);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[2]);
  vkCmdDraw(cmd, 3, 1, 0, 0);
  vkCmdEndRenderPass(cmd);
#ifdef READBACK_PROBE
  readback_render(cmd);
  return;
#endif
  barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                       NULL, 1, &barrier);
}
static void destroy_offscreen(VkDevice device) {
#ifdef READBACK_PROBE
  vkUnmapMemory(device,readback_memory);
  vkDestroyBuffer(device,readback_buffer,NULL);
  vkFreeMemory(device,readback_memory,NULL);
#endif
  vkDestroyDescriptorPool(device, off_pool, NULL);
  vkDestroyDescriptorSetLayout(device, off_set_layout, NULL);
  vkDestroySampler(device, off_sampler, NULL);
  vkDestroyFramebuffer(device, off_fb, NULL);
  vkDestroyRenderPass(device, off_pass, NULL);
  vkDestroyImageView(device, off_view, NULL);
  vkDestroyImage(device, off_image, NULL);
  vkFreeMemory(device, off_memory, NULL);
}
