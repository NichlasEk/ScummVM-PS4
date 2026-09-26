/* Keep the same independent framebuffer oracle while exercising launcher's
 * four-vertex surfaces through three strip entry paths. */
#ifdef SDLGL_PROBE
static unsigned strip_sequence,strip_modes;
#endif
static void draw_gl_surface(void) {
#ifdef SDLGL_PROBE
  unsigned mode=strip_sequence++%3; strip_modes|=1u<<mode;
  if(!mode) { glDrawArrays(GL_TRIANGLE_STRIP,0,4); return; }
  GLuint ibo; glGenBuffers(1,&ibo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ibo);
  if(mode==1) {
    const uint16_t ix[]={65535,0,1,2,3};
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(ix),ix,GL_STREAM_DRAW);
    glDrawElements(GL_TRIANGLE_STRIP,4,GL_UNSIGNED_SHORT,(void *)2);
  } else {
    const uint32_t ix[]={UINT32_MAX,0,1,2,3};
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(ix),ix,GL_STREAM_DRAW);
    glDrawElements(GL_TRIANGLE_STRIP,4,GL_UNSIGNED_INT,(void *)4);
  }
  const uint32_t poison[5]={0};
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(poison),poison,GL_STREAM_DRAW);
  glDeleteBuffers(1,&ibo);
#else
  glDrawArrays(GL_TRIANGLES,0,3);
#endif
}
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
static void draw_gl_color(GLuint program) {
#ifdef GLLEGACY_PROBE
  GLint mode=glGetUniformLocation(program,"mode"), modes=glGetUniformLocation(program,"modes"), last=glGetUniformLocation(program,"modes[1]");
  if(mode<0 || modes<0 || last<0) return;
  const GLint values[]={3,0};
  glUniform1i(mode,-7); glUniform1iv(modes,2,values); glUniform1i(last,-11);
#else
  (void)program;
#endif
  draw_gl_surface();
#ifdef GLLEGACY_PROBE
  /* Later CPU mutations must not alter already recorded draw snapshots. */
  glUniform1i(mode,0); glUniform1i(last,99);
#endif
}
static int draw_gl_framebuffer(const GLuint *programs, unsigned frame) {
#ifdef SDLGL_PROBE
  glBindFramebuffer(GL_FRAMEBUFFER,0); glDisable(GL_SCISSOR_TEST);
  glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
#endif
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
  glUniform4f(color,1,0,0,1); draw_gl_color(programs[0]);
  glUseProgram(programs[1]); draw_gl_surface();
  if(glGetError()!=GL_INVALID_OPERATION) return 2; /* feedback */
  glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  if(glGetError()!=GL_INVALID_OPERATION) return 2; /* stale CPU shadow */
  glDeleteTextures(1,&texture);
  if(glGetError()!=GL_INVALID_OPERATION || !glIsTexture(texture)) return 2;
  glBindFramebuffer(GL_FRAMEBUFFER,0); glViewport(0,0,320,720); draw_gl_surface();
  glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
  glEnable(GL_SCISSOR_TEST); glScissor(32,32,32,32); glClearColor(0,1,0,1); glClear(GL_COLOR_BUFFER_BIT);
  glDisable(GL_SCISSOR_TEST); glBindFramebuffer(GL_FRAMEBUFFER,0);
  glViewport(320,0,320,720); draw_gl_surface();
  /* New storage must not overwrite either earlier sample before submission. */
  for(unsigned i=0;i<sizeof(pixels);i+=4) { pixels[i]=255; pixels[i+1]=255; pixels[i+2]=0; pixels[i+3]=255; }
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  glBindFramebuffer(GL_FRAMEBUFFER,framebuffer); glViewport(0,0,64,32);
  glUseProgram(programs[0]); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glUniform4f(color,0,1,1,.5f); draw_gl_color(programs[0]); glDisable(GL_BLEND);
  glBindFramebuffer(GL_FRAMEBUFFER,0); glViewport(640,0,640,720);
  glUseProgram(programs[1]); draw_gl_surface();
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
    glUniform4f(color,1,0,1,1); draw_gl_color(programs[0]);
  } else {
    glBindTexture(GL_TEXTURE_2D,persistent_texture);
    glBindFramebuffer(GL_FRAMEBUFFER,persistent_framebuffer);
    glEnable(GL_SCISSOR_TEST); glScissor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
  }
  glBindFramebuffer(GL_FRAMEBUFFER,0); glViewport(0,656,64,64);
  glUseProgram(programs[1]); draw_gl_surface();
  if(frame==7) { glDeleteFramebuffers(1,&persistent_framebuffer); glDeleteTextures(1,&persistent_texture); }
  if(glGetError()!=GL_NO_ERROR) return 2;
#ifdef SDLGL_PROBE
  /* Clear a small default-target rectangle after draws. Surrounding pixels
   * retain their earlier values; fractional colors expose sRGB conversion. */
  glEnable(GL_SCISSOR_TEST); glScissor(80,80,100,80);
  glClearColor(.25f,.5f,.75f,.5f); glClear(GL_COLOR_BUFFER_BIT);
  glScissor(100,100,40,30); glClearColor(.002f,0,1,1); glClear(GL_COLOR_BUFFER_BIT);
  glDisable(GL_SCISSOR_TEST);
  if(glGetError()!=GL_NO_ERROR) return 2;
  fprintf(report,"PASS SDL VideoOut full and partial sRGB attachment clears\n");
  if(strip_modes!=7) return 2;
  fprintf(report,"PASS SDL triangle strips arrays uint16 uint32 and retained indices\n");
#endif
  fprintf(report,"PASS GL framebuffer targets orientation lifetime and rejected feedback\n");
  return 0;
}
