/* Interleaved position/color/UV with a 16-byte prefix and two buffer identities. */
static int draw_gl_vertices(void (*draw_gl)(GLenum,GLint,GLsizei)) {
  (void)draw_gl;
  GLint name; glGetIntegerv(GL_CURRENT_PROGRAM,&name);
  GLint matrix=glGetUniformLocation(name,"transform"), tint=glGetUniformLocation(name,"tint"), gain=glGetUniformLocation(name,"gain");
  if(matrix<0 || tint<0 || gain<0 || glGetUniformLocation(name,"missing")!=-1) return 2;
  float transform[16]={2,0,0,0, 0,2,0,0, 0,0,1,0, 0,0,0,1};
  glUniformMatrix4fv(matrix,1,GL_FALSE,transform);
  glUniform4f(tint,1,1,1,1); glUniform1f(gain,1);
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
  for(unsigned i=0;i<4;++i) { vertices[4+i*8]*=.5f; vertices[5+i*8]*=.5f; }
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
  glBindBuffer(GL_ARRAY_BUFFER,buffers[0]);
  glUniform4f(tint,.25f,1,3,1);
  glBufferSubData(GL_ARRAY_BUFFER,16,sizeof(vertices)-16,vertices+4);
  /* Replace the index store after the first draw; its snapshot must survive. */
  uint32_t indices32[]={UINT32_MAX,UINT32_MAX, 1,2,3};
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices32),indices32,GL_DYNAMIC_DRAW);
  glViewport(640,0,640,720);
  elements(GL_TRIANGLES,3,GL_UNSIGNED_INT,(const void *)8);
  /* Mutate both shader stages before submission; earlier draws must retain values. */
  glUniform1f(gain,0); glUniform4f(tint,0,0,0,0);
  transform[12]=10; glUniformMatrix4fv(matrix,1,GL_FALSE,transform);
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
  fprintf(report,"PASS GL uniform snapshots and names\n");
  return 0;
}
