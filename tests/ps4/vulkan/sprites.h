/* TWP's actual interleaved layout: vec2 position, vec4 color, vec2 UV. */
static VkBuffer sprite_buffer;
static VkDeviceMemory sprite_memory;
static int create_sprites(VkDevice device, VkPhysicalDevice physical) {
  VkBufferCreateInfo ci = {.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size=512, .usage=VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT};
  CHECK(vkCreateBuffer(device, &ci, NULL, &sprite_buffer));
  VkMemoryRequirements req;
  vkGetBufferMemoryRequirements(device, sprite_buffer, &req);
  VkMemoryAllocateInfo ai = {.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize=req.size, .memoryTypeIndex=host_memory_type(physical, req.memoryTypeBits)};
  CHECK(vkAllocateMemory(device, &ai, NULL, &sprite_memory));
  CHECK(vkBindBufferMemory(device, sprite_buffer, sprite_memory, 0));
  void *mapped;
  CHECK(vkMapMemory(device, sprite_memory, 0, VK_WHOLE_SIZE, 0, &mapped));
  memset(mapped, 0xcc, 512);
  const float vertices[3][8] = {
    {-1,-.8, 1,1,1,1, 320.f/96,504.f/96},
    { 0,1.2, 1,1,1,1, 640.f/96,144.f/96},
    { 1,-.8, 1,1,1,1, 960.f/96,504.f/96}};
  const uint32_t indices[] = {99, 0, 1, 2};
  memcpy((char *)mapped + 32, vertices, sizeof(vertices));
  memcpy((char *)mapped + 160, indices, sizeof(indices));
  float hidden[3][8];
  memcpy(hidden, vertices, sizeof(hidden));
  for (unsigned i=0;i<3;i++) hidden[i][0] += 10;
  memcpy((char *)mapped + 256, hidden, sizeof(hidden));
  vkUnmapMemory(device, sprite_memory);
  return 0;
}
