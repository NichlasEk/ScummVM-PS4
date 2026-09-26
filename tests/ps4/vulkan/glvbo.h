/* Interleaved position/color/UV with a 16-byte prefix and two buffer identities. */
static int draw_gl_vertices(void (*draw_gl)(GLenum,GLint,GLsizei)) {
  GLuint buffers[2];
  glGenBuffers(2,buffers);
  float vertices[28]={99,99,99,99,
    -.5f,-.4f, 1,.1f,0,1, .25f,0,
    0,.6f,    1,.5f,0,1, .25f,0,
    .5f,-.4f, 1,.9f,0,1, .25f,0};
  glBindBuffer(GL_ARRAY_BUFFER,buffers[0]);
  glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STREAM_DRAW);
  glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,32,(const void *)16);
  glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,32,(const void *)24);
  glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,32,(const void *)40);
  for(unsigned i=0;i<3;++i) glEnableVertexAttribArray(i);
  float poison[28]; memcpy(poison,vertices,sizeof(poison));
  for(unsigned i=0;i<3;++i) poison[4+i*8]+=10;
  glBindBuffer(GL_ARRAY_BUFFER,buffers[1]);
  glBufferData(GL_ARRAY_BUFFER,sizeof(poison),poison,GL_STATIC_DRAW);
  /* Binding B must not redirect the attribute pointers captured from A. */
  glViewport(0,0,640,720);
  draw_gl(GL_TRIANGLES,0,3);
  glBindBuffer(GL_ARRAY_BUFFER,buffers[0]);
  for(unsigned i=0;i<3;++i) { vertices[6+i*8]=.25f; vertices[10+i*8]=.75f; }
  glBufferSubData(GL_ARRAY_BUFFER,16,sizeof(vertices)-16,vertices+4);
  glViewport(640,0,640,720);
  draw_gl(GL_TRIANGLES,0,3);
  /* Reallocate and delete before queue submission: both draws must survive. */
  glBufferData(GL_ARRAY_BUFFER,sizeof(poison),poison,GL_DYNAMIC_DRAW);
  glDeleteBuffers(2,buffers);
  if(glGetError()!=GL_NO_ERROR) return 2;
  draw_gl(GL_TRIANGLES,0,3);
  if(glGetError()!=GL_INVALID_OPERATION) return 2;
  fprintf(report,"PASS GL vertex deleted-buffer rejection\n");
  return 0;
}
