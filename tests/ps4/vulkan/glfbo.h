static void frame_image_barrier(VkCommandBuffer cmd, VkImage image, int end) {
  VkImageMemoryBarrier b={.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask=end ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : 0,
    .dstAccessMask=end ? 0 : VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    .oldLayout=end ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED,
    .newLayout=end ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_GENERAL,
    .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
    .image=image,.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
  vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,NULL,0,NULL,1,&b);
}
static int draw_gl_framebuffer(const GLuint *programs, unsigned frame) {
  GLuint texture, framebuffer;
  glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  unsigned char pixels[64*64*4];
  for(unsigned i=0;i<sizeof(pixels);i+=4) { pixels[i]=0; pixels[i+1]=0; pixels[i+2]=255; pixels[i+3]=255; }
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  glGenFramebuffers(1,&framebuffer); glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
  glClear(GL_COLOR_BUFFER_BIT);
  if(glGetError()!=GL_INVALID_FRAMEBUFFER_OPERATION) return 2;
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
  if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE) return 2;
  glViewport(0,0,64,32); glDisable(GL_BLEND); glDisable(GL_SCISSOR_TEST);
  glUseProgram(programs[0]); GLint color=glGetUniformLocation(programs[0],"color");
  if(color<0) return 2;
  glUniform4f(color,1,0,0,1); glDrawArrays(GL_TRIANGLES,0,3);
  glUseProgram(programs[1]); glDrawArrays(GL_TRIANGLES,0,3);
  if(glGetError()!=GL_INVALID_OPERATION) return 2; /* feedback */
  glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  if(glGetError()!=GL_INVALID_OPERATION) return 2; /* stale CPU shadow */
  glDeleteTextures(1,&texture);
  if(glGetError()!=GL_INVALID_OPERATION || !glIsTexture(texture)) return 2;
  glBindFramebuffer(GL_FRAMEBUFFER,0); glViewport(0,0,320,720); glDrawArrays(GL_TRIANGLES,0,3);
  glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
  glEnable(GL_SCISSOR_TEST); glScissor(32,32,32,32); glClearColor(0,1,0,1); glClear(GL_COLOR_BUFFER_BIT);
  glDisable(GL_SCISSOR_TEST); glBindFramebuffer(GL_FRAMEBUFFER,0);
  glViewport(320,0,320,720); glDrawArrays(GL_TRIANGLES,0,3);
  /* New storage must not overwrite either earlier sample before submission. */
  for(unsigned i=0;i<sizeof(pixels);i+=4) { pixels[i]=255; pixels[i+1]=255; pixels[i+2]=0; pixels[i+3]=255; }
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  glBindFramebuffer(GL_FRAMEBUFFER,framebuffer); glViewport(0,0,64,32);
  glUseProgram(programs[0]); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glUniform4f(color,0,1,1,.5f); glDrawArrays(GL_TRIANGLES,0,3); glDisable(GL_BLEND);
  glBindFramebuffer(GL_FRAMEBUFFER,0); glViewport(640,0,640,720);
  glUseProgram(programs[1]); glDrawArrays(GL_TRIANGLES,0,3);
  glDeleteFramebuffers(1,&framebuffer); glDeleteTextures(1,&texture);
  if(glGetError()!=GL_NO_ERROR) return 2;
  /* Keep a GPU-only result alive across fences and snapshot collection. */
  static GLuint persistent_texture, persistent_framebuffer;
  if(frame==0) {
    glGenTextures(1,&persistent_texture); glBindTexture(GL_TEXTURE_2D,persistent_texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,8,8,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glGenFramebuffers(1,&persistent_framebuffer); glBindFramebuffer(GL_FRAMEBUFFER,persistent_framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,persistent_texture,0);
    glViewport(0,0,8,8); glUseProgram(programs[0]);
    glUniform4f(color,1,0,1,1); glDrawArrays(GL_TRIANGLES,0,3);
  } else {
    glBindTexture(GL_TEXTURE_2D,persistent_texture);
    glBindFramebuffer(GL_FRAMEBUFFER,persistent_framebuffer);
    glEnable(GL_SCISSOR_TEST); glScissor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
  }
  glBindFramebuffer(GL_FRAMEBUFFER,0); glViewport(0,656,64,64);
  glUseProgram(programs[1]); glDrawArrays(GL_TRIANGLES,0,3);
  if(frame==7) { glDeleteFramebuffers(1,&persistent_framebuffer); glDeleteTextures(1,&persistent_texture); }
  if(glGetError()!=GL_NO_ERROR) return 2;
  fprintf(report,"PASS GL framebuffer targets orientation lifetime and rejected feedback\n");
  return 0;
}
