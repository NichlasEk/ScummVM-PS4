#include "shaders.h"
#include "vk_ps4_log.h"
#include <orbis/libkernel.h>
#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan.h>
static FILE *report;
#define CHECK(expr)                                                            \
  do {                                                                         \
    VkResult r = (expr);                                                       \
    fprintf(report, "%s = %d\n", #expr, r);                                    \
    if (r != VK_SUCCESS)                                                       \
      return 2;                                                                \
  } while (0)
#define EXPECT_RESULT(expr, expected)                                          \
  do {                                                                         \
    VkResult r = (expr);                                                       \
    fprintf(report, "EXPECT %s = %d (wanted %d)\n", #expr, r,                  \
            (int)(expected));                                                  \
    if (r != (expected))                                                       \
      return 2;                                                                \
  } while (0)
#ifdef RESOURCE_PROBE
#include "resources.h"
#endif
int main(void) {
  report = fopen("/data/scummvm-vulkan-probe.log", "w");
  if (!report)
    return 2;
  setvbuf(report, NULL, _IONBF, 0);
  vk_ps4_log_open("/data/scummvm-vulkan-driver.log");
  fprintf(report, "START Vulkan draw probe\n");
  VkInstance instance;
  VkInstanceCreateInfo ici = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  CHECK(vkCreateInstance(&ici, NULL, &instance));
  uint32_t count = 1;
  VkPhysicalDevice physical;
  CHECK(vkEnumeratePhysicalDevices(instance, &count, &physical));
  float priority = 1;
  VkDeviceQueueCreateInfo qci = {.sType =
                                     VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                 .queueFamilyIndex = 0,
                                 .queueCount = 1,
                                 .pQueuePriorities = &priority};
  VkDeviceCreateInfo dci = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                            .queueCreateInfoCount = 1,
                            .pQueueCreateInfos = &qci};
  VkDevice device;
  CHECK(vkCreateDevice(physical, &dci, NULL, &device));
  VkQueue queue;
  vkGetDeviceQueue(device, 0, 0, &queue);
  VkSwapchainCreateInfoKHR sci = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .minImageCount = 2,
      .imageFormat = VK_FORMAT_R8G8B8A8_SRGB,
      .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
      .imageExtent = {1280, 720},
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .presentMode = VK_PRESENT_MODE_FIFO_KHR,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR};
  VkSwapchainKHR swap;
  CHECK(vkCreateSwapchainKHR(device, &sci, NULL, &swap));
  VkImage images[2];
  count = 2;
  CHECK(vkGetSwapchainImagesKHR(device, swap, &count, images));
  VkImageView views[2];
  VkFramebuffer fb[2];
  VkAttachmentDescription attachment = {
      .format = VK_FORMAT_R8G8B8A8_SRGB,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};
  VkAttachmentReference reference = {
      .attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkSubpassDescription subpass = {.pipelineBindPoint =
                                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  .colorAttachmentCount = 1,
                                  .pColorAttachments = &reference};
  VkRenderPassCreateInfo rpci = {.sType =
                                     VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                 .attachmentCount = 1,
                                 .pAttachments = &attachment,
                                 .subpassCount = 1,
                                 .pSubpasses = &subpass};
  VkRenderPass pass;
  CHECK(vkCreateRenderPass(device, &rpci, NULL, &pass));
  for (unsigned i = 0; i < 2; i++) {
    VkImageViewCreateInfo vci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = images[i],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .levelCount = 1,
                             .layerCount = 1}};
    CHECK(vkCreateImageView(device, &vci, NULL, &views[i]));
    VkFramebufferCreateInfo fci = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = pass,
        .attachmentCount = 1,
        .pAttachments = &views[i],
        .width = 1280,
        .height = 720,
        .layers = 1};
    CHECK(vkCreateFramebuffer(device, &fci, NULL, &fb[i]));
  }
  VkShaderModule modules[4];
  const uint32_t *codes[] = {background_vert, background_frag, triangle_vert,
                             triangle_frag};
  const size_t sizes[] = {sizeof(background_vert), sizeof(background_frag),
                          sizeof(triangle_vert), sizeof(triangle_frag)};
  for (unsigned i = 0; i < 4; i++) {
    VkShaderModuleCreateInfo mci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizes[i],
        .pCode = codes[i]};
    CHECK(vkCreateShaderModule(device, &mci, NULL, &modules[i]));
  }
#ifdef RESOURCE_PROBE
  if (create_resources(device, physical))
    return 2;
#endif
  VkPipelineLayout layout;
  VkPipelineLayoutCreateInfo lci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
#ifdef RESOURCE_PROBE
  lci.setLayoutCount = 1;
  lci.pSetLayouts = &resource_layout;
#endif
  CHECK(vkCreatePipelineLayout(device, &lci, NULL, &layout));
  VkPipeline pipelines[2];
  for (unsigned i = 0; i < 2; i++) {
    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_VERTEX_BIT,
         .module = modules[i * 2],
         .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
         .module = modules[i * 2 + 1],
         .pName = "main"}};
    VkPipelineVertexInputStateCreateInfo vi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo ia = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    VkPipelineViewportStateCreateInfo vp = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1};
    VkPipelineRasterizationStateCreateInfo rs = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1};
    VkPipelineMultisampleStateCreateInfo ms = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
    VkPipelineColorBlendAttachmentState blend = {.colorWriteMask = 15};
    VkPipelineColorBlendStateCreateInfo bs = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blend};
    VkDynamicState states[] = {VK_DYNAMIC_STATE_VIEWPORT,
                               VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo ds = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = states};
    VkGraphicsPipelineCreateInfo pci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = &vi,
        .pInputAssemblyState = &ia,
        .pViewportState = &vp,
        .pRasterizationState = &rs,
        .pMultisampleState = &ms,
        .pColorBlendState = &bs,
        .pDynamicState = &ds,
        .layout = layout,
        .renderPass = pass};
#ifdef RESOURCE_PROBE
    if (i == 1) {
      VkPipelineLayout empty_layout;
      VkPipelineLayoutCreateInfo empty = {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
      CHECK(vkCreatePipelineLayout(device, &empty, NULL, &empty_layout));
      pci.layout = empty_layout;
      VkPipeline rejected = VK_NULL_HANDLE;
      EXPECT_RESULT(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pci,
                                              NULL, &rejected),
                    VK_ERROR_FEATURE_NOT_PRESENT);
      if (rejected != VK_NULL_HANDLE)
        return 2;
      vkDestroyPipelineLayout(device, empty_layout, NULL);
      pci.layout = layout;
    }
#endif
    CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pci, NULL,
                                    &pipelines[i]));
  }
  VkCommandPool pool;
  VkCommandPoolCreateInfo cpci = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .queueFamilyIndex = 0,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT};
  CHECK(vkCreateCommandPool(device, &cpci, NULL, &pool));
  VkCommandBuffer cmd;
  VkCommandBufferAllocateInfo cai = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1};
  CHECK(vkAllocateCommandBuffers(device, &cai, &cmd));
  VkFence fence;
  VkFenceCreateInfo fci = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  CHECK(vkCreateFence(device, &fci, NULL, &fence));
  for (unsigned frame = 0; frame < 8; frame++) {
    uint32_t index;
    CHECK(vkAcquireNextImageKHR(device, swap, 1000000000ULL, VK_NULL_HANDLE,
                                fence, &index));
    CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, 1000000000ULL));
    CHECK(vkResetFences(device, 1, &fence));
    VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    CHECK(vkBeginCommandBuffer(cmd, &begin));
#ifdef RESOURCE_PROBE
    resource_barrier(cmd, frame);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1,
                            &resource_sets[1], 0, NULL);
#endif
    VkRenderPassBeginInfo rpbegin = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = pass,
        .framebuffer = fb[index],
        .renderArea = {{0, 0}, {1280, 720}}};
    vkCmdBeginRenderPass(cmd, &rpbegin, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport = {.width = 1280, .height = 720, .maxDepth = 1};
    VkRect2D scissor = {{0, 0}, {1280, 720}};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    for (unsigned i = 0; i < 2; i++) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[i]);
      vkCmdDraw(cmd, 3, 1, 0, 0);
    }
    vkCmdEndRenderPass(cmd);
    CHECK(vkEndCommandBuffer(cmd));
    VkSubmitInfo submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                           .commandBufferCount = 1,
                           .pCommandBuffers = &cmd};
    CHECK(vkQueueSubmit(queue, 1, &submit, fence));
    CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, 5000000000ULL));
    VkPresentInfoKHR present = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                .swapchainCount = 1,
                                .pSwapchains = &swap,
                                .pImageIndices = &index};
    CHECK(vkQueuePresentKHR(queue, &present));
    fprintf(report, "FRAME %u Vulkan submitted and presented\n", frame);
    CHECK(vkResetCommandBuffer(cmd, 0));
    sceKernelUsleep(100000); /* Give the native screenshot hotkey a present. */
  }
  CHECK(vkDeviceWaitIdle(device));
  fprintf(report, "PASS Vulkan draw loop; awaiting pixel validation\n");
  sceKernelUsleep(10000000);
  vkDestroyFence(device, fence, NULL);
  vkDestroyCommandPool(device, pool, NULL);
  for (unsigned i = 0; i < 2; i++) {
    vkDestroyPipeline(device, pipelines[i], NULL);
    vkDestroyFramebuffer(device, fb[i], NULL);
    vkDestroyImageView(device, views[i], NULL);
  }
  for (unsigned i = 0; i < 4; i++)
    vkDestroyShaderModule(device, modules[i], NULL);
  vkDestroyPipelineLayout(device, layout, NULL);
  vkDestroyRenderPass(device, pass, NULL);
  vkDestroySwapchainKHR(device, swap, NULL);
#ifdef RESOURCE_PROBE
  if (destroy_resources(device))
    return 2;
#endif
  vkDestroyDevice(device, NULL);
  vkDestroyInstance(instance, NULL);
  fprintf(report, "PASS Vulkan lifecycle\n");
  fclose(report);
  return 0;
}
