static int glfbo_fail(int line) { fprintf(report,"FAIL GL framebuffer assertion line %d\n",line); fflush(report); return 2; }
#ifdef SDLGL_PROBE
static GLuint capture_texture,capture_fbo;
static int prepare_capture_probe(void) {
  glGenTextures(1,&capture_texture); glBindTexture(GL_TEXTURE_2D,capture_texture);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,8,8,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  glGenFramebuffers(1,&capture_fbo); glBindFramebuffer(GL_FRAMEBUFFER,capture_fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,capture_texture,0);
  glClearColor(1,0,1,1); glClear(GL_COLOR_BUFFER_BIT);
  glEnable(GL_SCISSOR_TEST); glScissor(0,0,8,3); glClearColor(1,0,0,1); glClear(GL_COLOR_BUFFER_BIT); glDisable(GL_SCISSOR_TEST);
  unsigned char untouched[4]={3,4,5,6};
  glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,untouched);
  if(glGetError()!=GL_INVALID_OPERATION || untouched[0]!=3) return glfbo_fail(__LINE__);
  glBindFramebuffer(GL_FRAMEBUFFER,0); return 0;
}
static int check_capture_probe(void) {
  glBindFramebuffer(GL_FRAMEBUFFER,capture_fbo); glPixelStorei(GL_PACK_ALIGNMENT,8);
  unsigned char pixels[64]; memset(pixels,0xa5,sizeof(pixels));
  glReadPixels(2,1,3,4,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  if(glGetError()) return glfbo_fail(__LINE__);
  for(unsigned y=0;y<4;++y) for(unsigned x=0;x<16;++x) {
    unsigned char expected=x>=12 ? 0xa5 : x%4==0 || x%4==3 || (x%4==2 && y>=2) ? 255 : 0;
    if(pixels[y*16+x]!=expected) { fprintf(report,"FAIL GL readback y=%u byte=%u got=%u expected=%u\n",y,x,pixels[y*16+x],expected); return glfbo_fail(__LINE__); }
  }
  glReadPixels(7,7,3,4,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  if(glGetError()!=GL_INVALID_VALUE || pixels[0]!=255) return glfbo_fail(__LINE__);
  glReadPixels(0,0,0,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  if(glGetError()) return glfbo_fail(__LINE__);
  glPixelStorei(GL_PACK_ALIGNMENT,4); glBindFramebuffer(GL_FRAMEBUFFER,0);
  glDeleteFramebuffers(1,&capture_fbo); glDeleteTextures(1,&capture_texture);
  ps4glReleaseVertexSnapshots(ps4glCurrentContext());
  fprintf(report,"PASS GL completed framebuffer readback orientation crop packing and rejection\n");
  return glGetError() ? 2 : 0;
}
#endif
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
  GLint enabled=glGetUniformLocation(program,"settings.enabled"), tint=glGetUniformLocation(program,"settings.tint");
  if(enabled<0 || tint<0 || glGetUniformLocation(program,"ps4gl_fragcoord_y")!=-1) return;
  GLint viewport[4]; glGetIntegerv(GL_VIEWPORT,viewport);
  GLint bounds=glGetUniformLocation(program,"settings.yBounds"); if(bounds<0) return;
  glUniform2f(bounds,(float)viewport[1],(float)(viewport[1]+viewport[3]));
  glUniform1i(enabled,-9); glUniform3f(tint,1.f,1.f,1.f);
#else
  (void)program;
#endif
  draw_gl_surface();
#ifdef GLLEGACY_PROBE
  /* Later CPU mutations must not alter already recorded draw snapshots. */
  glUniform1i(mode,0); glUniform1i(last,99);
  glUniform1i(enabled,0); glUniform3f(tint,0.f,0.f,0.f);
#endif
}
static int draw_gl_framebuffer(const GLuint *programs, unsigned frame) {
#ifdef SDLGL_PROBE
  glBindFramebuffer(GL_FRAMEBUFFER,0); glDisable(GL_SCISSOR_TEST);
  glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
  GLuint live_programs[2]={programs[0],programs[1]}, linked_during_frame=0;
  if(frame==0) {
    PS4GLContext *ctx=ps4glCurrentContext();
    if(!ctx->command) return glfbo_fail(__LINE__);
    const char *bool_vertex="#version 110\nattribute vec2 a_position; attribute vec4 a_color; attribute vec2 a_texCoords; uniform mat4 u_transform; uniform bool flipY; varying vec4 v_color; varying vec2 v_texCoords; void main(){v_color=a_color;v_texCoords=a_texCoords;vec4 p=u_transform*vec4(a_position,0.,1.);if(!flipY)p.y=-p.y;gl_Position=p;}";
    linked_during_frame=compile_glsl_program(ctx->program_device,ctx->program_pass,bool_vertex,legacy_color);
    glUniform1i(glGetUniformLocation(linked_during_frame,"flipY"),-7);
    if(!linked_during_frame || glGetError()) return glfbo_fail(__LINE__);
    live_programs[0]=linked_during_frame; programs=live_programs;
    fprintf(report,"PASS SDL fresh shader link during active recording\n");
  }
#endif
  GLuint texture, framebuffer;
  glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
#ifdef SDLGL_PROBE
  enum { target_width=1280, target_height=720 };
#else
  enum { target_width=64, target_height=64 };
#endif
  static unsigned char pixels[target_width*target_height*4];
  for(unsigned i=0;i<sizeof(pixels);i+=4) { pixels[i]=0; pixels[i+1]=0; pixels[i+2]=255; pixels[i+3]=255; }
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,target_width,target_height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  glGenFramebuffers(1,&framebuffer); glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
  glClear(GL_COLOR_BUFFER_BIT);
  if(glGetError()!=GL_INVALID_FRAMEBUFFER_OPERATION) return glfbo_fail(__LINE__);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
  if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE) return glfbo_fail(__LINE__);
#ifdef SDLGL_PROBE
  if(frame%2) {
    unsigned before=ps4glCurrentContext()->snapshot_count;
    glDisable(GL_SCISSOR_TEST); glClearColor(0,0,1,1); glClear(GL_COLOR_BUFFER_BIT);
    if(glGetError() || ps4glCurrentContext()->snapshot_count!=before) return glfbo_fail(__LINE__);
    fprintf(report,"PASS SDL full target clear skips obsolete upload\n");
  }
#endif
  glViewport(0,0,target_width,target_height/2); glDisable(GL_BLEND); glDisable(GL_SCISSOR_TEST);
  glUseProgram(programs[0]); GLint color=glGetUniformLocation(programs[0],"color");
  if(color<0) return glfbo_fail(__LINE__);
  glUniform4f(color,1,0,0,1); draw_gl_color(programs[0]);
  glUseProgram(programs[1]); draw_gl_surface();
  if(glGetError()!=GL_INVALID_OPERATION) return glfbo_fail(__LINE__); /* feedback */
  glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  if(glGetError()!=GL_INVALID_OPERATION) return glfbo_fail(__LINE__); /* stale CPU shadow */
  glBindFramebuffer(GL_FRAMEBUFFER,0); glViewport(0,0,320,720); draw_gl_surface();
  glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
  glEnable(GL_SCISSOR_TEST); glScissor(target_width/2,target_height/2,target_width/2,target_height/2); glClearColor(0,1,0,1); glClear(GL_COLOR_BUFFER_BIT);
  glDisable(GL_SCISSOR_TEST); glBindFramebuffer(GL_FRAMEBUFFER,0);
  glViewport(320,0,320,720); draw_gl_surface();
  /* New storage must not overwrite either earlier sample before submission. */
  for(unsigned i=0;i<sizeof(pixels);i+=4) { pixels[i]=255; pixels[i+1]=255; pixels[i+2]=0; pixels[i+3]=255; }
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,target_width,target_height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  glBindFramebuffer(GL_FRAMEBUFFER,framebuffer); glViewport(0,0,target_width,target_height/2);
  glUseProgram(programs[0]); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glUniform4f(color,0,1,1,.5f); draw_gl_color(programs[0]); glDisable(GL_BLEND);
  glBindFramebuffer(GL_FRAMEBUFFER,0); glViewport(640,0,640,720);
  glUseProgram(programs[1]); draw_gl_surface();
  glDeleteTextures(1,&texture);
  if(glGetError() || glIsTexture(texture)) return glfbo_fail(__LINE__);
  glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
  if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE) return glfbo_fail(__LINE__);
  glDeleteFramebuffers(1,&framebuffer);
  if(glGetError()!=GL_NO_ERROR) return glfbo_fail(__LINE__);
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
    unsigned uploads_before=ps4glCurrentContext()->texture_snapshot_uploads;
    glUniform4f(color,1,0,1,1); draw_gl_color(programs[0]);
    /* Vertex-ID shaders need no vertex snapshot; measure the actual property:
     * drawing into undefined storage must not upload undefined texture pixels. */
    if(glGetError() || ps4glCurrentContext()->texture_snapshot_uploads!=uploads_before)
      return glfbo_fail(__LINE__);
    fprintf(report,"PASS GL undefined target skips initialization copy\n");
  } else {
    glBindTexture(GL_TEXTURE_2D,persistent_texture);
    glBindFramebuffer(GL_FRAMEBUFFER,persistent_framebuffer);
    glEnable(GL_SCISSOR_TEST); glScissor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
  }
  glBindFramebuffer(GL_FRAMEBUFFER,0); glViewport(0,656,64,64);
  glUseProgram(programs[1]); draw_gl_surface();
  /* Import client pixels from a nonzero arena offset, preserving the same
   * magenta oracle in this corner. Zero-area clear opens the target but leaves
   * all imported texels untouched. */
  GLuint offset_texture,offset_fbo;
  unsigned char magenta[8*8*4];
  for(unsigned i=0;i<sizeof(magenta);i+=4) { magenta[i]=255; magenta[i+1]=0; magenta[i+2]=255; magenta[i+3]=255; }
  glGenTextures(1,&offset_texture); glBindTexture(GL_TEXTURE_2D,offset_texture);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,8,8,0,GL_RGBA,GL_UNSIGNED_BYTE,magenta);
  glGenFramebuffers(1,&offset_fbo); glBindFramebuffer(GL_FRAMEBUFFER,offset_fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,offset_texture,0);
  glEnable(GL_SCISSOR_TEST); glScissor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT); glDisable(GL_SCISSOR_TEST);
  PS4GLContext *offset_ctx=ps4glCurrentContext();
  if(!offset_ctx->snapshots[offset_ctx->snapshot_count-1].offset) return glfbo_fail(__LINE__);
  glBindFramebuffer(GL_FRAMEBUFFER,0); draw_gl_surface();
  glDeleteTextures(1,&offset_texture); glDeleteFramebuffers(1,&offset_fbo);
  fprintf(report,"PASS GL framebuffer import from upload page offset\n");
  if(frame==7) { glDeleteFramebuffers(1,&persistent_framebuffer); glDeleteTextures(1,&persistent_texture); }
  if(glGetError()!=GL_NO_ERROR) return glfbo_fail(__LINE__);
#ifdef SDLGL_PROBE
  /* Clear a small default-target rectangle after draws. Surrounding pixels
   * retain their earlier values; fractional colors expose sRGB conversion. */
  glEnable(GL_SCISSOR_TEST); glScissor(80,80,100,80);
  glClearColor(.25f,.5f,.75f,.5f); glClear(GL_COLOR_BUFFER_BIT);
  /* A narrow lower-half viewport verifies the window-coordinate Y transform
   * on the swapchain, complementing the offscreen draws above. */
  glUseProgram(programs[0]); glViewport(80,80,100,80);
  glDisable(GL_BLEND); glUniform4f(glGetUniformLocation(programs[0],"color"),.25f,.5f,.75f,1.f);
  draw_gl_color(programs[0]);
  glScissor(100,100,40,30); glClearColor(.002f,0,1,1); glClear(GL_COLOR_BUFFER_BIT);
  glDisable(GL_SCISSOR_TEST);
  if(glGetError()!=GL_NO_ERROR) return glfbo_fail(__LINE__);
  /* Stencil-only geometry must not leak color or depth writes. */
  glUseProgram(programs[0]); glDisable(GL_BLEND); glDisable(GL_SCISSOR_TEST);
  glDepthMask(GL_TRUE); glStencilMask(~0u); glClearDepth(1); glClearStencil(0);
  glClear(GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
  glEnable(GL_STENCIL_TEST); glStencilFunc(GL_ALWAYS,1,~0u); glStencilOp(GL_REPLACE,GL_REPLACE,GL_REPLACE);
  glColorMask(0,0,0,0); glDepthMask(GL_FALSE); glViewport(200,80,50,80);
  glUniform4f(glGetUniformLocation(programs[0],"color"),0,0,1,1); draw_gl_color(programs[0]);
  glColorMask(1,1,1,1); glStencilFunc(GL_EQUAL,1,~0u); glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
  glViewport(200,80,100,80); glUniform4f(glGetUniformLocation(programs[0],"color"),0,1,0,1); draw_gl_color(programs[0]);
  glDisable(GL_STENCIL_TEST); glDepthMask(GL_TRUE); glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS);
  glClearStencil(255); glClear(GL_STENCIL_BUFFER_BIT); /* Must preserve depth=1. */
  glViewport(320,80,80,80);
  GLfloat depth_matrix[]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,-.5f,1};
  GLint transform=glGetUniformLocation(programs[0],"u_transform");
  glUniformMatrix4fv(transform,1,GL_FALSE,depth_matrix);
  glUniform4f(glGetUniformLocation(programs[0],"color"),0,0,1,1); draw_gl_color(programs[0]);
  depth_matrix[14]=.5f; glUniformMatrix4fv(transform,1,GL_FALSE,depth_matrix);
  glUniform4f(glGetUniformLocation(programs[0],"color"),1,1,0,1); draw_gl_color(programs[0]);
  depth_matrix[14]=0; glUniformMatrix4fv(transform,1,GL_FALSE,depth_matrix); glDisable(GL_DEPTH_TEST);
  if(glGetError()) return glfbo_fail(__LINE__);
  fprintf(report,"PASS depth and stencil draw commands; awaiting asymmetric pixel oracle\n");
  if(linked_during_frame) glDeleteProgram(linked_during_frame);
  fprintf(report,"PASS SDL VideoOut full and partial sRGB attachment clears; struct uniforms and fragment coordinates\n");
  if(strip_modes!=7) return glfbo_fail(__LINE__);
  fprintf(report,"PASS SDL triangle strips arrays uint16 uint32 and retained indices\n");
  if(frame==7 && prepare_capture_probe()) return glfbo_fail(__LINE__);
#endif
  fprintf(report,"PASS GL framebuffer targets orientation lifetime and rejected feedback\n");
  return 0;
}
