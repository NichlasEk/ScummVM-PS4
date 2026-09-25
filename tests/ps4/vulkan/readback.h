/* Fence-protected CPU oracle; the source image is never CPU-mapped. */
static VkBuffer readback_buffer;
static VkDeviceMemory readback_memory;
static unsigned char *readback_bytes;
static int create_readback(VkDevice device, VkPhysicalDevice physical) {
  VkBufferCreateInfo bc = {.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size=65536, .usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT};
  CHECK(vkCreateBuffer(device, &bc, NULL, &readback_buffer));
  VkMemoryRequirements req;
  vkGetBufferMemoryRequirements(device, readback_buffer, &req);
  VkPhysicalDeviceMemoryProperties props;
  vkGetPhysicalDeviceMemoryProperties(physical, &props);
  uint32_t type=UINT32_MAX;
  for (uint32_t i=0;i<props.memoryTypeCount;i++)
    if ((req.memoryTypeBits & (1u<<i)) &&
        (props.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) { type=i; break; }
  VkMemoryAllocateInfo ai = {.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize=req.size, .memoryTypeIndex=type};
  CHECK(vkAllocateMemory(device, &ai, NULL, &readback_memory));
  CHECK(vkBindBufferMemory(device, readback_buffer, readback_memory, 0));
  CHECK(vkMapMemory(device, readback_memory, 0, VK_WHOLE_SIZE, 0, (void **)&readback_bytes));
  return 0;
}
static void readback_barrier(VkCommandBuffer cmd, VkImageLayout old_layout,
                             VkImageLayout new_layout, VkAccessFlags src, VkAccessFlags dst) {
  VkImageMemoryBarrier b = {.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask=src, .dstAccessMask=dst, .oldLayout=old_layout, .newLayout=new_layout,
    .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
    .image=off_image, .subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0,NULL, 0,NULL, 1,&b);
}
static VkBufferImageCopy readback_region(unsigned part) {
  VkBufferImageCopy r = {.bufferOffset=part==0 ? 64 : part==1 ? 24000 : 48000,
    .bufferRowLength=part==2 ? 37 : 89, .bufferImageHeight=part==2 ? 21 : 64,
    .imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},
    .imageOffset={part==2 ? 31 : 0,part==2 ? 23 : 0,0},
    .imageExtent={part==2 ? 23 : 80,part==2 ? 19 : 60,1}};
  if (part == 3) {
    r.bufferOffset=54000; r.bufferRowLength=0; r.bufferImageHeight=0;
    r.imageOffset=(VkOffset3D){2,3,0}; r.imageExtent=(VkExtent3D){13,9,1};
  }
  return r;
}
static void readback_clear(VkCommandBuffer cmd, unsigned frame) {
  readback_barrier(cmd, frame ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, frame ? VK_ACCESS_SHADER_READ_BIT : 0, VK_ACCESS_TRANSFER_WRITE_BIT);
  VkClearColorValue color = {.float32={-1,frame%2 ? .75f : .25f,2,.5f}};
  VkImageSubresourceRange range={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
  vkCmdClearColorImage(cmd, off_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &range);
  readback_barrier(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
  VkBufferImageCopy region=readback_region(0);
  vkCmdCopyImageToBuffer(cmd, off_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback_buffer, 1, &region);
}
static void readback_render(VkCommandBuffer cmd) {
  readback_barrier(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
  VkBufferImageCopy regions[]={readback_region(1),readback_region(2),readback_region(3)};
  vkCmdCopyImageToBuffer(cmd, off_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback_buffer, 3, regions);
  VkBufferMemoryBarrier b={.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
    .srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask=VK_ACCESS_HOST_READ_BIT,
    .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
    .buffer=readback_buffer, .offset=0, .size=VK_WHOLE_SIZE};
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
    0,0,NULL,1,&b,0,NULL);
  readback_barrier(cmd,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT);
}
static int quantize(float v) { return (int)(v*255.f+.5f); }
static int check_readback(unsigned frame) {
  unsigned char expected[65536], touched[65536];
  memset(expected,0xcd,sizeof(expected)); memset(touched,0,sizeof(touched));
  for(unsigned part=0;part<4;part++) {
    VkBufferImageCopy r=readback_region(part);
    for(unsigned y=0;y<r.imageExtent.height;y++) for(unsigned x=0;x<r.imageExtent.width;x++) {
      int rgba[4]={0,frame%2 ? 191 : 64,255,128};
      if(part) {
        unsigned sx=x+r.imageOffset.x, sy=y+r.imageOffset.y;
        rgba[0]=quantize(sx<40 ? .125f : .625f);
        rgba[1]=quantize(sy<30 ? .25f : .5f);
        rgba[2]=quantize(.375f); rgba[3]=quantize(.5f);
        if(sx<40) {
          const float rgb[3]={.75f,.125f,.25f};
          for(unsigned c=0;c<3;c++) rgba[c]=quantize(rgb[c]*.25f+(rgba[c]/255.f)*.75f);
          rgba[3]=quantize(.25f+(rgba[3]/255.f)*.75f);
        } else if(sy<30) rgba[0]=quantize(.75f*.25f+rgba[0]/255.f);
      }
      unsigned offset=r.bufferOffset+(y*(r.bufferRowLength ? r.bufferRowLength : r.imageExtent.width)+x)*4;
      for(unsigned c=0;c<4;c++) { expected[offset+c]=rgba[c]; touched[offset+c]=part ? 2 : 1; }
    }
  }
  for(unsigned i=0;i<sizeof(expected);i++) {
    int diff=(int)readback_bytes[i]-expected[i];
    int tolerance=touched[i]==2 ? 1 : 0;
    if(diff < -tolerance || diff > tolerance) {
      fprintf(report,"FAIL readback frame=%u byte=%u actual=%u expected=%u region=%u\n",
        frame,i,readback_bytes[i],expected[i],touched[i]); return 2;
    }
  }
  fprintf(report,"PASS readback frame %u: clear, blended image, pitched/tight crops and padding (65536 bytes)\n",frame);
  return 0;
}

static int check_readback_rejections(VkCommandBuffer cmd) {
  VkCommandBufferBeginInfo begin={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  for(unsigned test=0;test<5;test++) {
    CHECK(vkBeginCommandBuffer(cmd,&begin));
    VkBufferImageCopy r=readback_region(2);
    if(test==0) r.bufferRowLength=1;
    if(test==1) r.bufferOffset=65536;
    if(test==2) r.imageSubresource.mipLevel=1;
    if(test==3) r.imageOffset.x=79;
    if(test<4) vkCmdCopyImageToBuffer(cmd,off_image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      readback_buffer,1,&r);
    else {
      VkClearColorValue color={.float32={0,0,0,0}};
      VkImageSubresourceRange range={VK_IMAGE_ASPECT_COLOR_BIT,1,1,0,1};
      vkCmdClearColorImage(cmd,off_image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&color,1,&range);
    }
    EXPECT_RESULT(vkEndCommandBuffer(cmd),test==1 ? VK_ERROR_INITIALIZATION_FAILED : VK_ERROR_FEATURE_NOT_PRESENT);
    CHECK(vkResetCommandBuffer(cmd,0));
  }
  CHECK(vkBeginCommandBuffer(cmd,&begin));
  VkBufferImageCopy tiny=readback_region(0);
  tiny.imageExtent=(VkExtent3D){1,1,1};
  for(unsigned i=0;i<17;i++)
    vkCmdCopyImageToBuffer(cmd,off_image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback_buffer,1,&tiny);
  EXPECT_RESULT(vkEndCommandBuffer(cmd),VK_ERROR_OUT_OF_DEVICE_MEMORY);
  CHECK(vkResetCommandBuffer(cmd,0));
  return 0;
}
