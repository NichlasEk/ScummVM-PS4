/* Resource probe uses only Vulkan API, no private GNM descriptor injection. */
static VkDescriptorSetLayout resource_layout;
static VkDescriptorPool resource_pool;
static VkDescriptorSet resource_sets[2];
static VkBuffer uniform_buffer;
#ifdef UPLOAD_PROBE
static VkBuffer staging_buffer;
static VkDeviceMemory staging_memory;
static unsigned char *staging_data;
#endif
static VkDeviceMemory uniform_memory, texture_memory;
static VkImage texture;
static VkImageView texture_view;
static VkSampler texture_samplers[2];
static uint32_t host_memory_type(VkPhysicalDevice physical, uint32_t bits) {
  VkPhysicalDeviceMemoryProperties props;
  vkGetPhysicalDeviceMemoryProperties(physical, &props);
  for (uint32_t i = 0; i < props.memoryTypeCount; ++i)
    if ((bits & (1u << i)) && (props.memoryTypes[i].propertyFlags &
                               (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                                  (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
      return i;
  return UINT32_MAX;
}
static int create_resources(VkDevice device, VkPhysicalDevice physical) {
  VkDescriptorSetLayoutBinding bindings[] = {
      {7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
       VK_SHADER_STAGE_FRAGMENT_BIT, NULL},
      {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
       NULL},
      {9, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT,
       NULL},
      {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
       VK_SHADER_STAGE_FRAGMENT_BIT, NULL},
      {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
       NULL}};
  VkDescriptorSetLayoutCreateInfo dl = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 5,
      .pBindings = bindings};
  VkDescriptorSetLayout rejected;
  VkDescriptorSetLayoutSupport support = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT};
  vkGetDescriptorSetLayoutSupport(device, &dl, &support);
  if (!support.supported)
    return 2;
  bindings[0].descriptorCount = 2;
  vkGetDescriptorSetLayoutSupport(device, &dl, &support);
  if (support.supported)
    return 2;
  EXPECT_RESULT(vkCreateDescriptorSetLayout(device, &dl, NULL, &rejected),
                VK_ERROR_FEATURE_NOT_PRESENT);
  bindings[0].descriptorCount = 1;
  bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  EXPECT_RESULT(vkCreateDescriptorSetLayout(device, &dl, NULL, &rejected),
                VK_ERROR_FEATURE_NOT_PRESENT);
  bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  CHECK(vkCreateDescriptorSetLayout(device, &dl, NULL, &resource_layout));
  VkDescriptorPoolSize sizes[] = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 6},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4}};
  VkDescriptorPoolCreateInfo pc = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
      .maxSets = 2,
      .poolSizeCount = 2,
      .pPoolSizes = sizes};
  CHECK(vkCreateDescriptorPool(device, &pc, NULL, &resource_pool));
  VkDescriptorSetLayout layouts[] = {resource_layout, resource_layout};
  VkDescriptorSetAllocateInfo ai = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = resource_pool,
      .descriptorSetCount = 2,
      .pSetLayouts = layouts};
  CHECK(vkAllocateDescriptorSets(device, &ai, resource_sets));
  VkDescriptorSet overfull[2];
  EXPECT_RESULT(vkAllocateDescriptorSets(device, &ai, overfull),
                VK_ERROR_OUT_OF_POOL_MEMORY);
  if (overfull[0] || overfull[1])
    return 2;
  VkPipelineLayoutCreateInfo invalid_pl = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 2,
      .pSetLayouts = layouts};
  VkPipelineLayout rejected_pl;
  EXPECT_RESULT(vkCreatePipelineLayout(device, &invalid_pl, NULL, &rejected_pl),
                VK_ERROR_FEATURE_NOT_PRESENT);

  VkBufferCreateInfo bc = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                           .size = 512,
                           .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT};
  CHECK(vkCreateBuffer(device, &bc, NULL, &uniform_buffer));
  VkMemoryRequirements req;
  vkGetBufferMemoryRequirements(device, uniform_buffer, &req);
  VkMemoryAllocateInfo ma = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = req.size,
      .memoryTypeIndex = host_memory_type(physical, req.memoryTypeBits)};
  CHECK(vkAllocateMemory(device, &ma, NULL, &uniform_memory));
  CHECK(vkBindBufferMemory(device, uniform_buffer, uniform_memory, 0));
  void *mapped;
  CHECK(vkMapMemory(device, uniform_memory, 0, VK_WHOLE_SIZE, 0, &mapped));
  memset(mapped, 0, 512);
  const float tint[] = {9, 9, 9, 9, .75, .5, .625, 1},
              bias[] = {.025, .125, .05, 0}, scale[] = {.5, .5, 1, 1};
  memcpy((char *)mapped + 64, tint, sizeof(tint));
  memcpy((char *)mapped + 128, bias, sizeof(bias));
  memcpy((char *)mapped + 192, scale, sizeof(scale));
#ifdef SPRITES_PROBE
  const float matrix[16] = {.5,0,0,0, 0,-.5,0,0, 0,0,1,0, 0,0,0,1};
  memcpy((char *)mapped + 192, matrix, sizeof(matrix));
#endif
  vkUnmapMemory(device, uniform_memory);
  VkImageCreateInfo ic = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                          .imageType = VK_IMAGE_TYPE_2D,
                          .format = VK_FORMAT_R8G8B8A8_UNORM,
                          .extent = {2, 2, 1},
                          .mipLevels = 1,
                          .arrayLayers = 1,
                          .samples = VK_SAMPLE_COUNT_1_BIT,
                          .tiling = VK_IMAGE_TILING_LINEAR,
                          .usage = VK_IMAGE_USAGE_SAMPLED_BIT,
                          .initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED};
#ifdef UPLOAD_PROBE
  ic.extent.width = 37;
  ic.extent.height = 23;
  ic.tiling = VK_IMAGE_TILING_OPTIMAL;
  ic.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  ic.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
#endif
  CHECK(vkCreateImage(device, &ic, NULL, &texture));
  vkGetImageMemoryRequirements(device, texture, &req);
  ma.allocationSize = req.size;
  ma.memoryTypeIndex = host_memory_type(physical, req.memoryTypeBits);
  CHECK(vkAllocateMemory(device, &ma, NULL, &texture_memory));
  CHECK(vkBindImageMemory(device, texture, texture_memory, 0));
#ifdef UPLOAD_PROBE
  VkBufferCreateInfo staging_ci = {.sType =
                                       VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                   .size = 8192,
                                   .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT};
  CHECK(vkCreateBuffer(device, &staging_ci, NULL, &staging_buffer));
  vkGetBufferMemoryRequirements(device, staging_buffer, &req);
  ma.allocationSize = req.size;
  ma.memoryTypeIndex = host_memory_type(physical, req.memoryTypeBits);
  CHECK(vkAllocateMemory(device, &ma, NULL, &staging_memory));
  CHECK(vkBindBufferMemory(device, staging_buffer, staging_memory, 0));
  CHECK(vkMapMemory(device, staging_memory, 0, VK_WHOLE_SIZE, 0,
                    (void **)&staging_data));
  memset(staging_data, 0, 8192);
#else
  VkImageSubresource sub = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT};
  VkSubresourceLayout sl;
  vkGetImageSubresourceLayout(device, texture, &sub, &sl);
  CHECK(vkMapMemory(device, texture_memory, 0, VK_WHOLE_SIZE, 0, &mapped));
  const unsigned char pixels[] = {240, 40, 20,  255, 30,  220, 70,  128,
                                  50,  90, 230, 64,  180, 160, 100, 192};
  memcpy((char *)mapped + sl.offset, pixels, 8);
  memcpy((char *)mapped + sl.offset + sl.rowPitch, pixels + 8, 8);
  vkUnmapMemory(device, texture_memory);
#endif
  VkImageViewCreateInfo vc = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = texture,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
  CHECK(vkCreateImageView(device, &vc, NULL, &texture_view));
  for (unsigned i = 0; i < 2; ++i) {
    VkSamplerCreateInfo sc = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = i ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
        .minFilter = i ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE};
    CHECK(vkCreateSampler(device, &sc, NULL, &texture_samplers[i]));
  }
  VkDescriptorBufferInfo bi[] = {{uniform_buffer, 64, 32},
                                 {uniform_buffer, 128, 16},
                                 {uniform_buffer, 192, 64}};
  VkDescriptorImageInfo ii[] = {
      {texture_samplers[0], texture_view, VK_IMAGE_LAYOUT_GENERAL},
      {texture_samplers[1], texture_view, VK_IMAGE_LAYOUT_GENERAL}};
  const uint32_t bn[] = {0, 1, 9, 3, 7};
  VkWriteDescriptorSet writes[5] = {0};
  VkCopyDescriptorSet copies[5] = {0};
  for (unsigned i = 0; i < 5; ++i) {
    writes[i] = (VkWriteDescriptorSet){
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = resource_sets[0],
        .dstBinding = bn[i],
        .descriptorCount = 1,
        .descriptorType = i < 3 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                                : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pBufferInfo = i < 3 ? &bi[i] : NULL,
        .pImageInfo = i >= 3 ? &ii[i - 3] : NULL};
    copies[i] =
        (VkCopyDescriptorSet){.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET,
                              .srcSet = resource_sets[0],
                              .srcBinding = bn[i],
                              .dstSet = resource_sets[1],
                              .dstBinding = bn[i],
                              .descriptorCount = 1};
  }
  vkUpdateDescriptorSets(device, 5, writes, 5, copies);
  CHECK(vkFreeDescriptorSets(device, resource_pool, 1, resource_sets));
  return 0;
}
#ifdef UPLOAD_PROBE
static int check_upload_rejections(VkCommandBuffer cmd) {
  VkCommandBufferBeginInfo begin = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT};
  EXPECT_RESULT(vkBeginCommandBuffer(cmd, &begin),
                VK_ERROR_FEATURE_NOT_PRESENT);
  begin.flags = 0;
  VkBufferImageCopy copy = {
      .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1},
      .imageExtent = {1, 1, 1}};
  CHECK(vkBeginCommandBuffer(cmd, &begin));
  vkCmdCopyBufferToImage(cmd, staging_buffer, texture,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
  EXPECT_RESULT(vkEndCommandBuffer(cmd), VK_ERROR_FEATURE_NOT_PRESENT);
  CHECK(vkResetCommandBuffer(cmd, 0));
  CHECK(vkBeginCommandBuffer(cmd, &begin));
  copy.imageSubresource.mipLevel = 0;
  copy.bufferOffset = 8192;
  vkCmdCopyBufferToImage(cmd, staging_buffer, texture,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
  EXPECT_RESULT(vkEndCommandBuffer(cmd), VK_ERROR_INITIALIZATION_FAILED);
  CHECK(vkResetCommandBuffer(cmd, 0));
  CHECK(vkBeginCommandBuffer(cmd, &begin));
  VkImageMemoryBarrier barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcQueueFamilyIndex = 0,
      .dstQueueFamilyIndex = 1,
      .image = texture,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                       NULL, 1, &barrier);
  EXPECT_RESULT(vkEndCommandBuffer(cmd), VK_ERROR_FEATURE_NOT_PRESENT);
  CHECK(vkResetCommandBuffer(cmd, 0));
  return 0;
}
static void fill_staging(void) {
  const unsigned char pixels[] = {240, 40, 20,  255, 30,  220, 70,  128,
                                  50,  90, 230, 64,  180, 160, 100, 192};
  memset(staging_data, 0xcc, 8192);
  for (unsigned y = 0; y < 23; ++y)
    for (unsigned x = 0; x < 37; ++x)
      memcpy(staging_data + 64 + y * 172 + x * 4,
             pixels + ((y >= 11) * 2 + (x >= 18)) * 4, 4);
}
static void resource_barrier(VkCommandBuffer cmd, unsigned frame) {
  VkImageMemoryBarrier b = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = frame ? VK_ACCESS_SHADER_READ_BIT : 0,
      .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .oldLayout = frame ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = texture,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1,
                       &b);
  VkBufferImageCopy regions[2] = {0};
  for (unsigned i = 0; i < 2; ++i) {
    regions[i].bufferOffset = 64 + i * 68;
    regions[i].bufferRowLength = 43;
    regions[i].bufferImageHeight = 29;
    regions[i].imageSubresource =
        (VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    regions[i].imageOffset.x = i * 17;
    regions[i].imageExtent = (VkExtent3D){i ? 20 : 17, 23, 1};
  }
  vkCmdCopyBufferToImage(cmd, staging_buffer, texture,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 2, regions);
  b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
  b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                       NULL, 1, &b);
}
#else
static void resource_barrier(VkCommandBuffer cmd, unsigned frame) {
  if (frame)
    return;
  VkImageMemoryBarrier b = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_HOST_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED,
      .newLayout = VK_IMAGE_LAYOUT_GENERAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = texture,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_HOST_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                       NULL, 1, &b);
}
#endif
static int destroy_resources(VkDevice device) {
  CHECK(vkResetDescriptorPool(device, resource_pool, 0));
  VkDescriptorSetLayout layouts[] = {resource_layout, resource_layout};
  VkDescriptorSetAllocateInfo again = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = resource_pool,
      .descriptorSetCount = 2,
      .pSetLayouts = layouts};
  CHECK(vkAllocateDescriptorSets(device, &again, resource_sets));
  /* Destroy with two live sets: both GPU tables must be released. */
  vkDestroyDescriptorPool(device, resource_pool, NULL);
  vkDestroyDescriptorSetLayout(device, resource_layout, NULL);
  for (unsigned i = 0; i < 2; ++i)
    vkDestroySampler(device, texture_samplers[i], NULL);
  vkDestroyImageView(device, texture_view, NULL);
  vkDestroyImage(device, texture, NULL);
  vkFreeMemory(device, texture_memory, NULL);
  vkDestroyBuffer(device, uniform_buffer, NULL);
  vkFreeMemory(device, uniform_memory, NULL);
#ifdef UPLOAD_PROBE
  vkUnmapMemory(device, staging_memory);
  vkDestroyBuffer(device, staging_buffer, NULL);
  vkFreeMemory(device, staging_memory, NULL);
#endif
  return 0;
}
