/* Ordered GPU readback/upload keeps GL copy semantics without CPU readback.
 * The native transfer path detiles/tiles; CmdCopyImage cannot copy tiled PS4
 * surfaces. Copy rows in reverse order only for the top-origin display image. */
static int copy_texture(PS4GLContext *c, PS4GLTexture *texture, GLint dx, GLint dy, GLint x, GLint y, GLsizei width, GLsizei height) {
  if(!c->managed_frame || !prepare_render_target(c)) return 0;
  if(x>c->width || y>c->height || width>c->width-x || height>c->height-y) {if(!c->error)c->error=GL_INVALID_VALUE;return 0;}
  unsigned source_target=c->active_target;
  VkImage source=source_target ? c->targets[source_target-1].image : c->default_image;
  if(!source) return 0;
  GLint sy=source_target ? y : c->height-y-height;
  size_t bytes=(size_t)width*height*4;
  VkBufferImageCopy *rows=calloc((size_t)height,sizeof(*rows));if(!rows) return allocation_failure(c,"copy region allocation");
  end_render_target(c);
  c->discard_target_contents=dx==0 && dy==0 && width==texture->width && height==texture->height;
  int ready=texture->render_target || create_target(c,texture);c->discard_target_contents=0;
  if(!ready) {free(rows);return 0;}
  PS4GLRenderTarget *dest=&c->targets[texture->render_target-1];
  if(dest->image==source || !upload_vertices(c,texture->pixels,bytes,0,0)) {free(rows);return 0;}
  PS4GLSnapshot *staging=&c->snapshots[c->snapshot_count-1];
  VkImageMemoryBarrier barrier={.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask=VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT,.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT,
    .oldLayout=VK_IMAGE_LAYOUT_GENERAL,.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
    .image=source,.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
  vkCmdPipelineBarrier(c->command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,NULL,0,NULL,1,&barrier);
  target_barrier(c,dest,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  VkBufferImageCopy read={.bufferOffset=staging->offset,.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},
    .imageOffset={x,sy,0},.imageExtent={(uint32_t)width,(uint32_t)height,1}};
  vkCmdCopyImageToBuffer(c->command,source,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,staging->buffer,1,&read);
  VkBufferMemoryBarrier dependency={.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
    .srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT,
    .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
    .buffer=staging->buffer,.offset=staging->offset,.size=bytes};
  vkCmdPipelineBarrier(c->command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,NULL,1,&dependency,0,NULL);
  for(GLsizei row=0;row<height;++row) rows[row]=(VkBufferImageCopy){
    .bufferOffset=staging->offset+(size_t)(source_target ? row : height-1-row)*width*4,
    .imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},.imageOffset={dx,dy+row,0},.imageExtent={(uint32_t)width,1,1}};
  vkCmdCopyBufferToImage(c->command,staging->buffer,dest->image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,height,rows);
  free(rows);
  barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_GENERAL;
  barrier.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;barrier.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
  vkCmdPipelineBarrier(c->command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,NULL,0,NULL,1,&barrier);
  target_barrier(c,dest,VK_IMAGE_LAYOUT_GENERAL);
  return 1;
}
