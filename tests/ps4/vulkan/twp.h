/* Bounded native effect probe: two rows, five original TWP fragment shaders. */
#include "twp-data.h"
static VkDescriptorSetLayout twp_set_layout;
static VkDescriptorPool twp_pool;
static VkDescriptorSet twp_sets[10];
static VkBuffer twp_ubo, twp_vertices;
static VkDeviceMemory twp_ubo_memory, twp_vertex_memory, twp_image_memory[2];
static VkImage twp_images[2];
static VkImageView twp_views[2];
static VkSampler twp_sampler;
static uint32_t twp_host_type(VkPhysicalDevice physical, uint32_t bits) {
  VkPhysicalDeviceMemoryProperties props;
  vkGetPhysicalDeviceMemoryProperties(physical, &props);
  for (uint32_t i=0; i<props.memoryTypeCount; ++i)
    if ((bits & (1u<<i)) && (props.memoryTypes[i].propertyFlags &
        (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
        (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) return i;
  return UINT32_MAX;
}
static int twp_buffer(VkDevice device, VkPhysicalDevice physical, const void *data,
                      size_t size, VkBufferUsageFlags usage, VkBuffer *buffer,
                      VkDeviceMemory *memory) {
  VkBufferCreateInfo ci = {.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size=size, .usage=usage};
  CHECK(vkCreateBuffer(device,&ci,NULL,buffer));
  VkMemoryRequirements req;
  vkGetBufferMemoryRequirements(device,*buffer,&req);
  VkMemoryAllocateInfo ai={.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize=req.size,.memoryTypeIndex=twp_host_type(physical,req.memoryTypeBits)};
  CHECK(vkAllocateMemory(device,&ai,NULL,memory));
  CHECK(vkBindBufferMemory(device,*buffer,*memory,0));
  void *mapped;
  CHECK(vkMapMemory(device,*memory,0,VK_WHOLE_SIZE,0,&mapped));
  memcpy(mapped,data,size);
  vkUnmapMemory(device,*memory);
  return 0;
}
static int create_twp(VkDevice device, VkPhysicalDevice physical) {
  if (twp_buffer(device,physical,twp_uniforms,sizeof(twp_uniforms),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 &twp_ubo,&twp_ubo_memory)) return 2;
  float vertices[2][3][8];
  const float pos[3][2]={{-1,-1},{3,-1},{-1,3}};
  for (unsigned row=0; row<2; ++row)
    for (unsigned v=0; v<3; ++v) {
      float *p=vertices[row][v];
      p[0]=pos[v][0]; p[1]=pos[v][1];
      p[2]=.8f; p[3]=.6f; p[4]=.4f; p[5]=.5f;
      p[6]=row ? .75f : .25f; p[7]=row ? .75f : .25f;
    }
  if (twp_buffer(device,physical,vertices,sizeof(vertices),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 &twp_vertices,&twp_vertex_memory)) return 2;
  const unsigned char pixels[2][16]={
    {240,40,20,255, 30,220,70,128, 50,90,230,64, 180,160,100,192},
    {20,80,200,128, 200,30,80,255, 70,190,40,192, 40,200,220,64}};
  for (unsigned i=0; i<2; ++i) {
    VkImageCreateInfo ci={.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType=VK_IMAGE_TYPE_2D,.format=VK_FORMAT_R8G8B8A8_UNORM,.extent={2,2,1},
      .mipLevels=1,.arrayLayers=1,.samples=VK_SAMPLE_COUNT_1_BIT,.tiling=VK_IMAGE_TILING_LINEAR,
      .usage=VK_IMAGE_USAGE_SAMPLED_BIT,.initialLayout=VK_IMAGE_LAYOUT_PREINITIALIZED};
    CHECK(vkCreateImage(device,&ci,NULL,&twp_images[i]));
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(device,twp_images[i],&req);
    VkMemoryAllocateInfo ai={.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize=req.size,.memoryTypeIndex=twp_host_type(physical,req.memoryTypeBits)};
    CHECK(vkAllocateMemory(device,&ai,NULL,&twp_image_memory[i]));
    CHECK(vkBindImageMemory(device,twp_images[i],twp_image_memory[i],0));
    VkImageSubresource sub={.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT};
    VkSubresourceLayout sl;
    vkGetImageSubresourceLayout(device,twp_images[i],&sub,&sl);
    void *mapped;
    CHECK(vkMapMemory(device,twp_image_memory[i],0,VK_WHOLE_SIZE,0,&mapped));
    memcpy((char *)mapped+sl.offset,pixels[i],8);
    memcpy((char *)mapped+sl.offset+sl.rowPitch,pixels[i]+8,8);
    vkUnmapMemory(device,twp_image_memory[i]);
    VkImageViewCreateInfo vc={.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,.image=twp_images[i],
      .viewType=VK_IMAGE_VIEW_TYPE_2D,.format=VK_FORMAT_R8G8B8A8_UNORM,
      .subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
    CHECK(vkCreateImageView(device,&vc,NULL,&twp_views[i]));
  }
  VkSamplerCreateInfo sc={.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .magFilter=VK_FILTER_NEAREST,.minFilter=VK_FILTER_NEAREST,.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST,
    .addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,.addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    .addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE};
  CHECK(vkCreateSampler(device,&sc,NULL,&twp_sampler));
  VkDescriptorSetLayoutBinding bindings[]={
    {0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT,NULL},
    {1,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_FRAGMENT_BIT,NULL},
    {2,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,NULL},
    {3,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,NULL}};
  VkDescriptorSetLayoutCreateInfo dl={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount=4,.pBindings=bindings};
  CHECK(vkCreateDescriptorSetLayout(device,&dl,NULL,&twp_set_layout));
  VkDescriptorPoolSize sizes[]={{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,20},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,20}};
  VkDescriptorPoolCreateInfo pc={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .maxSets=10,.poolSizeCount=2,.pPoolSizes=sizes};
  CHECK(vkCreateDescriptorPool(device,&pc,NULL,&twp_pool));
  for (unsigned i=0; i<10; ++i) {
    VkDescriptorSetAllocateInfo ai={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool=twp_pool,.descriptorSetCount=1,.pSetLayouts=&twp_set_layout};
    CHECK(vkAllocateDescriptorSets(device,&ai,&twp_sets[i]));
    VkDescriptorBufferInfo bi[]={{twp_ubo,0,64},{twp_ubo,256*(i+1),256}};
    VkDescriptorImageInfo ii[]={{twp_sampler,twp_views[0],VK_IMAGE_LAYOUT_GENERAL},
                                {twp_sampler,twp_views[1],VK_IMAGE_LAYOUT_GENERAL}};
    VkWriteDescriptorSet writes[4];
    for (unsigned b=0;b<4;++b) writes[b]=(VkWriteDescriptorSet){
      .sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=twp_sets[i],.dstBinding=b,.descriptorCount=1,
      .descriptorType=b<2 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .pBufferInfo=b<2 ? &bi[b] : NULL,.pImageInfo=b>=2 ? &ii[b-2] : NULL};
    vkUpdateDescriptorSets(device,4,writes,0,NULL);
  }
  return 0;
}
static void twp_barrier(VkCommandBuffer cmd, unsigned frame) {
  VkImageMemoryBarrier barriers[2];
  for (unsigned i=0;i<2;++i) barriers[i]=(VkImageMemoryBarrier){
    .sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT,
    .dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
    .oldLayout=frame ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_PREINITIALIZED,
    .newLayout=VK_IMAGE_LAYOUT_GENERAL,.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.image=twp_images[i],
    .subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
  vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                       0,0,NULL,0,NULL,2,barriers);
}
static void draw_twp(VkCommandBuffer cmd, VkPipelineLayout layout, const VkPipeline *pipelines) {
  for (unsigned row=0;row<2;++row) for (unsigned col=0;col<5;++col) {
    VkViewport vp={.x=col*256,.y=row*360,.width=256,.height=360,.maxDepth=1};
    VkRect2D sc={{col*256,row*360},{256,360}};
    vkCmdSetViewport(cmd,0,1,&vp);
    vkCmdSetScissor(cmd,0,1,&sc);
    VkDeviceSize offset=row*96;
    vkCmdBindVertexBuffers(cmd,0,1,&twp_vertices,&offset);
    vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,0,1,&twp_sets[row*5+col],0,NULL);
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelines[col]);
    vkCmdDraw(cmd,3,1,0,0);
  }
}
static void destroy_twp(VkDevice device) {
  vkDestroyDescriptorPool(device,twp_pool,NULL);
  vkDestroyDescriptorSetLayout(device,twp_set_layout,NULL);
  vkDestroySampler(device,twp_sampler,NULL);
  for(unsigned i=0;i<2;++i) {
    vkDestroyImageView(device,twp_views[i],NULL);
    vkDestroyImage(device,twp_images[i],NULL);
    vkFreeMemory(device,twp_image_memory[i],NULL);
  }
  vkDestroyBuffer(device,twp_ubo,NULL); vkFreeMemory(device,twp_ubo_memory,NULL);
  vkDestroyBuffer(device,twp_vertices,NULL); vkFreeMemory(device,twp_vertex_memory,NULL);
}
