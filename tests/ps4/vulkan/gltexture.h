/* Interleaved position/color/UV with a 16-byte prefix and two buffer identities. */
static int draw_gl_vertices(void (*draw_gl)(GLenum,GLint,GLsizei)) {
  (void)draw_gl;
  GLint program; glGetIntegerv(GL_CURRENT_PROGRAM,&program);
  GLint image_a=glGetUniformLocation(program,"imageA"), image_b=glGetUniformLocation(program,"imageB");
  if(image_a<0 || image_b<0) return 2;
  GLuint textures[3]; glGenTextures(3,textures);
  uint16_t a[]={0,0xf800,0,0};
  /* RGB rows padded from six to eight bytes, with poison in the padding. */
  unsigned char b[]={0,0,0, 0,0,0, 99,99, 0,0,64, 0,0,0, 99,99};
  unsigned char c[]={0,0,0,255, 0,0,0,255, 0,0,191,255, 0,0,0,255};
  for(unsigned i=0;i<3;++i) {
    glActiveTexture(GL_TEXTURE0+i); glBindTexture(GL_TEXTURE_2D,textures[i]);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    if(i==0) glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,2,2,0,GL_RGB,GL_UNSIGNED_SHORT_5_6_5,a);
    else if(i==1) glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,2,0,GL_RGB,GL_UNSIGNED_BYTE,b);
    else glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,c);
  }
  glUniform1i(image_a,0); glUniform1i(image_b,1);
  GLint coordinates=glGetUniformLocation(program,"coordinates");
  if(coordinates<0) return 2;
  glUniform4f(coordinates,.6f,.25f,.25f,.75f);
  void (*elements)(GLenum,GLsizei,GLenum,const void *) =
      (void (*)(GLenum,GLsizei,GLenum,const void *))ps4glGetProcAddress("glDrawElements");
  if(!elements) return 2;
  GLuint buffers[2], ibo;
  glGenBuffers(1,&ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ibo);
  /* Vertex zero is poison. Actual vertices are shuffled in the buffer. */
  uint16_t indices16[]={65535,65535, 2,3,1};
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices16),indices16,GL_STREAM_DRAW);
  glGenBuffers(2,buffers);
  float vertices[36]={99,99,99,99,
    10,10, 0,0,0,1, 0,0,
    .5f,-.4f, 1,.9f,0,1, .25f,0,
    -.5f,-.4f, 1,.1f,0,1, .25f,0,
    0,.6f,    1,.5f,0,1, .25f,0};
  glBindBuffer(GL_ARRAY_BUFFER,buffers[0]);
  glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STREAM_DRAW);
  glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,32,(const void *)16);
  glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,32,(const void *)24);
  glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,32,(const void *)40);
  for(unsigned i=0;i<3;++i) glEnableVertexAttribArray(i);
  float poison[36]; memcpy(poison,vertices,sizeof(poison));
  for(unsigned i=0;i<4;++i) poison[4+i*8]+=10;
  glBindBuffer(GL_ARRAY_BUFFER,buffers[1]);
  glBufferData(GL_ARRAY_BUFFER,sizeof(poison),poison,GL_STATIC_DRAW);
  /* Binding B must not redirect the attribute pointers captured from A. */
  glViewport(0,0,640,720);
  elements(GL_TRIANGLES,3,GL_UNSIGNED_SHORT,(const void *)4);
  PS4GLContext *ctx=ps4glCurrentContext();
  unsigned cached=ctx->texture_snapshot_reuses, uploaded=ctx->texture_snapshot_uploads;
  /* Repeat exactly the same opaque draw; it must reuse both texture copies. */
  elements(GL_TRIANGLES,3,GL_UNSIGNED_SHORT,(const void *)4);
  if(ctx->texture_snapshot_reuses!=cached+2 || ctx->texture_snapshot_uploads!=uploaded) return 2;
  glBindBuffer(GL_ARRAY_BUFFER,buffers[0]);
  /* Change one texel after the left draw; switch the second sampler's unit. */
  glActiveTexture(GL_TEXTURE0); unsigned char red[]={128,0,0,255};
  glTexSubImage2D(GL_TEXTURE_2D,0,1,0,1,1,GL_RGB,GL_UNSIGNED_BYTE,red);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glUniform1i(image_b,2);
  glActiveTexture(GL_TEXTURE0+2);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
  glActiveTexture(GL_TEXTURE0);
  /* Linear average of red 0/128 is 64; repeated UV selects row 1, column 0. */
  glUniform4f(coordinates,.5f,.25f,1.25f,1.75f);
  glBufferSubData(GL_ARRAY_BUFFER,16,sizeof(vertices)-16,vertices+4);
  /* Replace the index store after the first draw; its snapshot must survive. */
  uint32_t indices32[]={UINT32_MAX,UINT32_MAX, 1,2,3};
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices32),indices32,GL_DYNAMIC_DRAW);
  glViewport(640,0,640,720);
  elements(GL_TRIANGLES,3,GL_UNSIGNED_INT,(const void *)8);
  if(ctx->texture_snapshot_uploads!=uploaded+2) return 2;
  cached=ctx->texture_snapshot_reuses;
  elements(GL_TRIANGLES,3,GL_UNSIGNED_INT,(const void *)8);
  if(ctx->texture_snapshot_reuses!=cached+2) return 2;
  fprintf(report,"PASS GL versioned texture snapshot reuse and mutation\n");
  /* Poison/reallocate and delete texture names before the main queue submission. */
  unsigned char poison_texture[16]={0};
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,poison_texture);
  glDeleteTextures(3,textures);
  if(glGetError()!=GL_NO_ERROR) return 2;
  elements(GL_TRIANGLES,3,GL_UNSIGNED_INT,(const void *)8);
  if(glGetError()!=GL_INVALID_OPERATION) return 2;
  uint32_t bad[]={0,0,0};
  glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,8,sizeof(bad),bad);
  glDeleteBuffers(1,&ibo);
  /* Reallocate and delete before queue submission: both draws must survive. */
  glBufferData(GL_ARRAY_BUFFER,sizeof(poison),poison,GL_DYNAMIC_DRAW);
  glDeleteBuffers(2,buffers);
  if(glGetError()!=GL_NO_ERROR) return 2;
  elements(GL_TRIANGLES,3,GL_UNSIGNED_INT,(const void *)8);
  if(glGetError()!=GL_INVALID_OPERATION) return 2;
  fprintf(report,"PASS GL vertex deleted-buffer rejection\n");
  fprintf(report,"PASS GL index uint16 uint32 offset mutation\n");
  fprintf(report,"PASS GL RGB8 RGB565 uploads and RGB subimage snapshots\n");
  fprintf(report,"PASS GL texture units updates filtering and deletion\n");
  return 0;
}
