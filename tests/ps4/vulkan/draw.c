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
#ifdef SPRITES_PROBE
#include "sprites.h"
#endif
#endif
#ifdef COMPOSITE_PROBE
#include "composite.h"
#endif
#ifdef TWP_PROBE
#include "twp.h"
#endif
#ifdef GLDRAW_PROBE
#include "ps4gl.c"
#include "ps4gl-vulkan.c"
#ifdef GLFBO_PROBE
#include "glfbo.h"
#elif defined(GLBLEND_PROBE)
#include "glblend.h"
#elif defined(GLTEXTURE_PROBE)
#include "gltexture.h"
#elif defined(GLARRAY_PROBE)
#include "glarray.h"
#elif defined(GLUNIFORM_PROBE)
#include "gluniform.h"
#elif defined(GLINDEX_PROBE)
#include "glindex.h"
#elif defined(GLVBO_PROBE)
#include "glvbo.h"
#endif
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
#ifdef GLFBO_PROBE
  attachment.loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;
  attachment.initialLayout=attachment.finalLayout=VK_IMAGE_LAYOUT_GENERAL;
#endif
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
#ifdef COMPOSITE_PROBE
  if (create_offscreen(device, physical))
    return 2;
#endif
  VkShaderModule modules[6];
#ifdef TWP_PROBE
  const uint32_t *codes[]={twp_vsrc,twp_fragmentSrc,twp_bwShader,twp_ghostShader,twp_sepiaShader,twp_fadeShader};
  const size_t sizes[]={sizeof(twp_vsrc),sizeof(twp_fragmentSrc),sizeof(twp_bwShader),
    sizeof(twp_ghostShader),sizeof(twp_sepiaShader),sizeof(twp_fadeShader)};
#elif defined(COMPOSITE_PROBE)
  const uint32_t *codes[] = {background_vert, composite_base_frag,
                             composite_overlay_frag, composite_sample_frag};
  const size_t sizes[] = {sizeof(background_vert), sizeof(composite_base_frag),
                          sizeof(composite_overlay_frag),
                          sizeof(composite_sample_frag)};
#else
  const uint32_t *codes[] = {background_vert, background_frag, triangle_vert,
                             triangle_frag};
  const size_t sizes[] = {sizeof(background_vert), sizeof(background_frag),
                          sizeof(triangle_vert), sizeof(triangle_frag)};
#endif
  for (unsigned i = 0; i < sizeof(codes)/sizeof(codes[0]); i++) {
    VkShaderModuleCreateInfo mci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizes[i],
        .pCode = codes[i]};
    CHECK(vkCreateShaderModule(device, &mci, NULL, &modules[i]));
  }
#ifdef RESOURCE_PROBE
  #ifdef SPRITES_PROBE
  if (create_sprites(device, physical)) return 2;
  #endif
  if (create_resources(device, physical))
    return 2;
#endif
#ifdef TWP_PROBE
  if (create_twp(device, physical)) return 2;
#endif
  VkPipelineLayout layout;
  VkPipelineLayoutCreateInfo lci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
#ifdef RESOURCE_PROBE
  lci.setLayoutCount = 1;
  lci.pSetLayouts = &resource_layout;
#endif
#ifdef COMPOSITE_PROBE
  lci.setLayoutCount = 1;
  lci.pSetLayouts = &off_set_layout;
#endif
#ifdef TWP_PROBE
  lci.setLayoutCount=1; lci.pSetLayouts=&twp_set_layout;
#endif
  CHECK(vkCreatePipelineLayout(device, &lci, NULL, &layout));
#ifdef GLDRAW_PROBE
  const unsigned pipeline_count = 0;
#elif defined(TWP_PROBE)
  const unsigned pipeline_count = 5;
#elif defined(COMPOSITE_PROBE)
  const unsigned pipeline_count = 4;
#else
  const unsigned pipeline_count = 2;
#endif
  VkPipeline pipelines[5];
  for (unsigned i = 0; i < pipeline_count; i++) {
#ifdef TWP_PROBE
    unsigned vs_index=0, fs_index=i+1;
#elif defined(COMPOSITE_PROBE)
    unsigned vs_index = 0, fs_index = i == 0 ? 1 : (i < 3 ? 2 : 3);
#else
    unsigned vs_index = i * 2, fs_index = i * 2 + 1;
#endif
    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_VERTEX_BIT,
         .module = modules[vs_index],
         .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
         .module = modules[fs_index],
         .pName = "main"}};
    VkPipelineVertexInputStateCreateInfo vi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
#if defined(SPRITES_PROBE) || defined(TWP_PROBE)
    VkVertexInputBindingDescription vb = {0, 32, VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs[] = {
      {0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
      {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 8},
      {2, 0, VK_FORMAT_R32G32_SFLOAT, 24}};
    if (
#ifdef TWP_PROBE
        1
#else
        i == 1
#endif
    ) {
      vi.vertexBindingDescriptionCount = 1; vi.pVertexBindingDescriptions = &vb;
      vi.vertexAttributeDescriptionCount = 3; vi.pVertexAttributeDescriptions = attrs;
    }
#endif
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
#ifdef COMPOSITE_PROBE
    if (i == 1 || i == 2) {
      blend.blendEnable = VK_TRUE;
      blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
      blend.dstColorBlendFactor =
          i == 1 ? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA : VK_BLEND_FACTOR_ONE;
      blend.colorBlendOp = VK_BLEND_OP_ADD;
      blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
      blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      blend.alphaBlendOp = VK_BLEND_OP_ADD;
      if (i == 2)
        blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
    }
#endif
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
#ifdef COMPOSITE_PROBE
    pci.renderPass = i < 3 ? off_pass : pass;
#endif
    CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pci, NULL,
                                    &pipelines[i]));
  }
#ifdef GLDRAW_PROBE
  PS4GLContext gl;
  if(!ps4glInit(&gl,1280,720)) return 2;
  ps4glMakeCurrent(&gl);
  GLuint glprograms[]={ps4glCreateProgramSPIRV(device,pass,background_vert,sizeof(background_vert),background_frag,sizeof(background_frag)),
    ps4glCreateProgramSPIRV(device,pass,triangle_vert,sizeof(triangle_vert),triangle_frag,sizeof(triangle_frag))};
  ps4glConfigureVertexDevice(physical,device);
  if(!glprograms[0] || !glprograms[1] || glGetError()!=GL_NO_ERROR) return 2;
  uint32_t rejected_spv[]={0x07230203,0x00010000,0,10,0,(4u<<16)|59,1,2,0};
  for(unsigned test=0;test<3;++test) {
    if(test==1) rejected_spv[8]=1; /* Vertex input with no BuiltIn decoration. */
    if(test==2) rejected_spv[5]=0; /* Zero-sized instruction. */
    if(ps4glCreateProgramSPIRV(device,pass,rejected_spv,sizeof(rejected_spv),triangle_frag,sizeof(triangle_frag)) ||
       glGetError()!=GL_INVALID_OPERATION) return 2;
    fprintf(report,"PASS GL program rejection %u\n",test);
  }
#ifndef GLFBO_PROBE
  void (*draw_gl)(GLenum,GLint,GLsizei)=(void (*)(GLenum,GLint,GLsizei))ps4glGetProcAddress("glDrawArrays");
#endif
#endif
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
#ifdef READBACK_PROBE
  if (check_readback_rejections(cmd)) return 2;
#ifdef RECTCLEAR_PROBE
  if (check_rectclear_rejections(cmd)) return 2;
#endif
#endif
#ifdef UPLOAD_PROBE
  if (check_upload_rejections(cmd))
    return 2;
#endif
  for (unsigned frame = 0; frame < 8; frame++) {
#ifdef UPLOAD_PROBE
    memset(staging_data, 0, 8192);
#endif
    uint32_t index;
    CHECK(vkAcquireNextImageKHR(device, swap, 1000000000ULL, VK_NULL_HANDLE,
                                fence, &index));
    CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, 1000000000ULL));
    CHECK(vkResetFences(device, 1, &fence));
    VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
#ifdef READBACK_PROBE
    memset(readback_bytes, 0xcd, 65536);
#endif
    CHECK(vkBeginCommandBuffer(cmd, &begin));
#ifdef RESOURCE_PROBE
    resource_barrier(cmd, frame);
#ifdef SPRITES_PROBE
    /* Bind before the pipeline: attribute formats belong to draw-time state. */
    VkDeviceSize initial_vertex_offset = 32;
    vkCmdBindVertexBuffers(cmd, 0, 1, &sprite_buffer, &initial_vertex_offset);
#endif
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1,
                            &resource_sets[1], 0, NULL);
#endif
#ifdef COMPOSITE_PROBE
    draw_offscreen(cmd, frame, pipelines);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1,
                            &off_set, 0, NULL);
#endif
#ifdef TWP_PROBE
    twp_barrier(cmd, frame);
#endif
#ifdef GLFBO_PROBE
    frame_image_barrier(cmd,images[index],0);
    ps4glBeginFrame(cmd,pass,fb[index],1280,720);
    if(draw_gl_framebuffer(glprograms,frame)) return 2;
    ps4glEndFrame();
    fprintf(report,"GL draw frame %u error=%u\n",frame,glGetError());
    frame_image_barrier(cmd,images[index],1);
#else
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
#ifdef GLDRAW_PROBE
    ps4glBeginPass(cmd);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0,0,1280,720);
    glUseProgram(glprograms[0]); draw_gl(GL_TRIANGLES,0,3);
    glUseProgram(glprograms[1]);
    if(frame==7) glDeleteProgram(glprograms[1]);
#ifdef GLBLEND_PROBE
    if(draw_gl_blend(draw_gl)) return 2;
#elif defined(GLVBO_PROBE)
    if(draw_gl_vertices(draw_gl)) return 2;
#else
    glViewport(160,90,960,540);
    glEnable(GL_SCISSOR_TEST); glScissor(500,180,300,260);
    draw_gl(GL_TRIANGLES,0,3);
#endif
    fprintf(report,"GL draw frame %u error=%u\n",frame,glGetError());
    ps4glEndPass();
#elif defined(TWP_PROBE)
    draw_twp(cmd, layout, pipelines);
#else
#ifdef COMPOSITE_PROBE
    for (unsigned i = 3; i < 4; i++) {
#else
    for (unsigned i = 0; i < 2; i++) {
#endif
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[i]);
#ifdef SPRITES_PROBE
      if (i == 1) {
        VkDeviceSize offset;
        vkCmdBindIndexBuffer(cmd, sprite_buffer, 160, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, 3, 1, 1, 0, 0);
        /* A later bind must not overwrite the first draw's descriptor snapshot. */
        offset = 256;
        vkCmdBindVertexBuffers(cmd, 0, 1, &sprite_buffer, &offset);
        vkCmdDraw(cmd, 3, 1, 0, 0);
      } else
#endif
      vkCmdDraw(cmd, 3, 1, 0, 0);
    }
#endif
    vkCmdEndRenderPass(cmd);
#endif
    CHECK(vkEndCommandBuffer(cmd));
#ifdef UPLOAD_PROBE
    /* Write after recording: a CPU copy during recording would sample zeros. */
    fill_staging();
#endif
    VkSubmitInfo submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                           .commandBufferCount = 1,
                           .pCommandBuffers = &cmd};
    CHECK(vkQueueSubmit(queue, 1, &submit, fence));
    CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, 5000000000ULL));
#if defined(GLVBO_PROBE) || defined(GLBLEND_PROBE) || defined(GLFBO_PROBE)
    if(frame==7) {
      CHECK(vkResetFences(device,1,&fence));
      CHECK(vkQueueSubmit(queue,1,&submit,fence));
      CHECK(vkWaitForFences(device,1,&fence,VK_TRUE,5000000000ULL));
      fprintf(report,"PASS GL vertex command buffer resubmission\n");
    }
#endif
#ifdef READBACK_PROBE
    if (check_readback(frame)) return 2;
    if (frame == 7) {
      memset(readback_bytes, 0xcd, 65536);
      CHECK(vkResetFences(device, 1, &fence));
      CHECK(vkQueueSubmit(queue, 1, &submit, fence));
      CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, 5000000000ULL));
      if (check_readback(frame)) return 2;
      fprintf(report,"PASS readback command buffer resubmission\n");
    }
#endif
#ifdef UPLOAD_PROBE
    if (frame == 7) {
      CHECK(vkResetFences(device, 1, &fence));
      CHECK(vkQueueSubmit(queue, 1, &submit, fence));
      CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, 5000000000ULL));
      fprintf(report, "PASS upload command buffer resubmission\n");
    }
#endif
    VkPresentInfoKHR present = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                .swapchainCount = 1,
                                .pSwapchains = &swap,
                                .pImageIndices = &index};
    CHECK(vkQueuePresentKHR(queue, &present));
    fprintf(report, "FRAME %u Vulkan submitted and presented\n", frame);
    CHECK(vkResetCommandBuffer(cmd, 0));
#ifdef GLDRAW_PROBE
    ps4glReleaseVertexSnapshots(&gl);
#ifdef GLFBO_PROBE
    if(gl.target_bytes!=(frame==7 ? 0 : 256) || gl.snapshot_count || gl.texture_bytes!=(frame==7 ? 0 : 256)) return 2;
    fprintf(report,"PASS GL framebuffer storage collected\n");
#endif
#endif
    sceKernelUsleep(100000); /* Give the native screenshot hotkey a present. */
  }
  CHECK(vkDeviceWaitIdle(device));
  fprintf(report, "PASS Vulkan draw loop; awaiting pixel validation\n");
  sceKernelUsleep(10000000);
#ifdef GLDRAW_PROBE
  glDeleteProgram(glprograms[0]); glDeleteProgram(glprograms[1]); glUseProgram(0);
  if(glIsProgram(glprograms[0]) || glIsProgram(glprograms[1]) || glGetError()!=GL_NO_ERROR) return 2;
  ps4glDestroyFramebuffers(&gl); ps4glDestroyTextures(&gl); ps4glDestroyBuffers(&gl);
  ps4glDestroyPrograms(device,&gl);
  ps4glMakeCurrent(NULL);
#endif
  vkDestroyFence(device, fence, NULL);
  vkDestroyCommandPool(device, pool, NULL);
  for (unsigned i = 0; i < 2; i++) {

    vkDestroyFramebuffer(device, fb[i], NULL);
    vkDestroyImageView(device, views[i], NULL);
  }
  for (unsigned i = 0; i < pipeline_count; ++i)
    vkDestroyPipeline(device, pipelines[i], NULL);
  for (unsigned i = 0; i < sizeof(codes)/sizeof(codes[0]); i++)
    vkDestroyShaderModule(device, modules[i], NULL);
  vkDestroyPipelineLayout(device, layout, NULL);
  vkDestroyRenderPass(device, pass, NULL);
  vkDestroySwapchainKHR(device, swap, NULL);
#ifdef RESOURCE_PROBE
  #ifdef SPRITES_PROBE
  vkDestroyBuffer(device, sprite_buffer, NULL);
  vkFreeMemory(device, sprite_memory, NULL);
  #endif
  if (destroy_resources(device))
    return 2;
#endif
#ifdef COMPOSITE_PROBE
  destroy_offscreen(device);
#endif
#ifdef TWP_PROBE
  destroy_twp(device);
#endif
  vkDestroyDevice(device, NULL);
  vkDestroyInstance(instance, NULL);
  fprintf(report, "PASS Vulkan lifecycle\n");
  fclose(report);
  return 0;
}
