/* Legacy arrays lower to the same bounded immutable draw snapshots as immediate geometry. */
void glHint(GLenum target, GLenum mode) {
  if(!fixed_outside()) return;
  if(mode!=0x1100 && mode!=0x1101 && mode!=0x1102) { error(GL_INVALID_ENUM); return; }
  if(target!=0x0c50 && target!=0x0c51 && target!=0x0c52 && target!=0x0c53 && target!=0x0c54 && target!=0x8192) error(GL_INVALID_ENUM);
  /* Hints allow implementation discretion; perspective-correct interpolation is always used. */
}
static PS4GLClientArray *client_array(GLenum kind) {
  switch(kind) {
  case 0x8074: return &current->fixed.clients[0];
  case 0x8076: return &current->fixed.clients[1];
  case 0x8075: return &current->fixed.clients[2];
  case 0x8078: return &current->fixed.clients[3+current->fixed.client_texture];
  default: error(GL_INVALID_ENUM); return NULL;
  }
}
void glEnableClientState(GLenum kind) {
  if(!fixed_outside()) return;
  PS4GLClientArray *a=client_array(kind); if(a) a->enabled=1;
}
void glDisableClientState(GLenum kind) {
  if(!fixed_outside()) return;
  if(kind==0x8077 || kind==0x8079) return; /* Unused index/edge arrays are always disabled. */
  PS4GLClientArray *a=client_array(kind); if(a) a->enabled=0;
}
void glClientActiveTexture(GLenum unit) {
  if(!fixed_outside()) return;
  if(unit<GL_TEXTURE0 || unit>=GL_TEXTURE0+7) { error(GL_INVALID_ENUM); return; }
  current->fixed.client_texture=unit-GL_TEXTURE0;
}
static void client_pointer(GLenum kind, GLint size, GLenum type, GLsizei stride, const void *pointer) {
  if(!fixed_outside()) return;
  if(size<1 || size>4 || stride<0) { error(GL_INVALID_VALUE); return; }
  if(type!=GL_FLOAT) { error(GL_INVALID_ENUM); return; }
  PS4GLClientArray *a=client_array(kind); if(!a) return;
  a->size=size; a->type=type; a->stride=stride; a->pointer=pointer; a->buffer=current->array_buffer;
}
void glVertexPointer(GLint n, GLenum t, GLsizei s, const void *p) { if(n<2) {error(GL_INVALID_VALUE);return;} client_pointer(0x8074,n,t,s,p); }
void glColorPointer(GLint n, GLenum t, GLsizei s, const void *p) { if(n<3) {error(GL_INVALID_VALUE);return;} client_pointer(0x8076,n,t,s,p); }
void glNormalPointer(GLenum t, GLsizei s, const void *p) { client_pointer(0x8075,3,t,s,p); }
void glTexCoordPointer(GLint n, GLenum t, GLsizei s, const void *p) { client_pointer(0x8078,n,t,s,p); }
void glOrtho(double l, double r, double b, double t, double n, double f) {
  if(!fixed_outside()) return;
  if(l==r || b==t || n==f) { error(GL_INVALID_VALUE); return; }
  glTranslatef(-(r+l)/(r-l),-(t+b)/(t-b),-(f+n)/(f-n));
  glScalef(2/(r-l),2/(t-b),-2/(f-n));
}
static int client_read(const PS4GLClientArray *a, uint32_t index, GLfloat *v) {
  if(!a->enabled) return 1;
  if(a->type!=GL_FLOAT || a->size<1 || a->size>4) {error(GL_INVALID_OPERATION);return 0;}
  size_t width=(size_t)a->size*4, stride=a->stride ? (size_t)a->stride : width;
  if(index && stride>SIZE_MAX/index) {error(GL_INVALID_OPERATION);return 0;}
  size_t offset=(size_t)index*stride;
  const unsigned char *base=a->pointer;
  if(a->buffer) {
    PS4GLBuffer *buffer=NULL;
    for(unsigned i=0;i<PS4GL_MAX_BUFFERS;++i) if(current->buffers[i].name==a->buffer) {buffer=&current->buffers[i];break;}
    size_t start=(uintptr_t)a->pointer;
    if(!buffer || start>buffer->size || offset>buffer->size-start || width>buffer->size-start-offset) {error(GL_INVALID_OPERATION);return 0;}
    base=buffer->data+start;
  } else if(!base || offset>UINTPTR_MAX-width || (uintptr_t)base>UINTPTR_MAX-offset-width) {error(GL_INVALID_OPERATION);return 0;}
  memcpy(v,base+offset,width); return 1;
}
static const char *client_attributes[]={"ps4gl_client_vertex","ps4gl_client_color","ps4gl_client_normal","ps4gl_client_uv0","ps4gl_client_uv1"};
static int client_uses_arrays(void) {
  if(!current->program) return current->fixed.clients[0].enabled;
  for(unsigned i=0;i<5;++i) if(glGetAttribLocation(current->program,client_attributes[i])>=0) return 1;
  return 0;
}
static void client_shader_draw(GLenum mode, GLint first, GLsizei count, GLenum type, const unsigned char *ids) {
  size_t n=mode==GL_QUADS ? (size_t)(count/4)*6 : (size_t)count;
  if(n>PS4GL_MAX_BUFFER_BYTES/(5*16)) {error(GL_OUT_OF_MEMORY);return;}
  GLfloat *data=malloc(n*5*16);
  if(!data) {error(GL_OUT_OF_MEMORY);return;}
  GLint locations[5];for(unsigned j=0;j<5;++j) locations[j]=glGetAttribLocation(current->program,client_attributes[j]);
  const unsigned quad[]={0,1,2,0,2,3};
  for(size_t i=0;i<n;++i) {
    size_t source=mode==GL_QUADS ? (i/6)*4+quad[i%6] : i;
    uint32_t index=type ? element_index(ids,type,source) : (uint32_t)first+(uint32_t)source;
    for(unsigned j=0;j<5;++j) {
      GLfloat *v=data+(i*5+j)*4;v[0]=v[1]=v[2]=0;v[3]=1;
      if(j==1) {memcpy(v,current->fixed.color,16);if(current->fixed.clients[j].enabled) v[3]=1;}
      if(j==2) v[2]=1;
      if(locations[j]>=0 && !client_read(&current->fixed.clients[j],index,v)) {free(data);return;}
    }
  }
  PS4GLAttrib saved[PS4GL_MAX_ATTRIBS];memcpy(saved,current->attribs,sizeof(saved));
  GLuint previous=current->array_buffer,buffer=0;glGenBuffers(1,&buffer);
  if(buffer) {
    glBindBuffer(GL_ARRAY_BUFFER,buffer);glBufferData(GL_ARRAY_BUFFER,n*5*16,data,GL_STREAM_DRAW);
    PS4GLBuffer *b=bound_buffer(GL_ARRAY_BUFFER);
    if(b && b->size==n*5*16) {
      for(unsigned j=0;j<5;++j) if(locations[j]>=0) {glVertexAttribPointer(locations[j],4,GL_FLOAT,GL_FALSE,5*16,(void *)(uintptr_t)(j*16));glEnableVertexAttribArray(locations[j]);}
      draw(GL_TRIANGLES,0,n,0,NULL);
    }
    glDeleteBuffers(1,&buffer);
  }
  glBindBuffer(GL_ARRAY_BUFFER,previous);memcpy(current->attribs,saved,sizeof(saved));free(data);
}
static void client_draw(GLenum mode, GLint first, GLsizei count, GLenum type, const void *indices) {
  if(!fixed_outside()) return;
  if(first<0 || count<0 || (unsigned)count>PS4GL_MAX_BUFFER_BYTES/sizeof(PS4GLFixedVertex)) {error(GL_INVALID_VALUE);return;}
  if(mode!=GL_TRIANGLES && mode!=GL_QUADS) {error(GL_INVALID_ENUM);return;}
  if(!count) return;
  const unsigned char *ids=indices;
  if(type) {
    size_t width=type==GL_UNSIGNED_INT ? 4 : 2;
    if(current->element_buffer) {
      PS4GLBuffer *b=bound_buffer(GL_ELEMENT_ARRAY_BUFFER);size_t off=(uintptr_t)indices;
      if(!b || off>b->size || (size_t)count>(b->size-off)/width) {error(GL_INVALID_OPERATION);return;}
      ids=b->data+off;
    } else if(!ids || (uintptr_t)ids>UINTPTR_MAX-(size_t)count*width) {error(GL_INVALID_OPERATION);return;}
  }
  if(current->program) {client_shader_draw(mode,first,count,type,ids);return;}
  PS4GLFixedVertex *vertices=malloc((size_t)count*sizeof(*vertices));
  if(!vertices) {error(GL_OUT_OF_MEMORY);return;}
  for(GLsizei i=0;i<count;++i) {
    uint32_t index=type ? element_index(ids,type,i) : (uint32_t)first+(uint32_t)i;
    GLfloat p[4]={0,0,0,1},c[4],uv[4]={0,0,0,1};
    memcpy(c,current->fixed.color,sizeof(c));if(current->fixed.clients[1].enabled) c[3]=1;memcpy(uv,current->fixed.uv,sizeof(current->fixed.uv));
    if(!client_read(&current->fixed.clients[0],index,p) || !client_read(&current->fixed.clients[1],index,c) || !client_read(&current->fixed.clients[3],index,uv)) {free(vertices);return;}
    if(p[3]!=1) {free(vertices);error(GL_INVALID_OPERATION);return;} /* no silent homogeneous-position loss */
    memcpy(vertices[i].position,p,12);memcpy(vertices[i].color,c,16);memcpy(vertices[i].uv,uv,12);
  }
  GLfloat saved_color[4],saved_uv[3];memcpy(saved_color,current->fixed.color,16);memcpy(saved_uv,current->fixed.uv,12);
  glBegin(mode);
  for(GLsizei i=0;i<count;++i) {PS4GLFixedVertex *v=&vertices[i];glColor4f(v->color[0],v->color[1],v->color[2],v->color[3]);glTexCoord3f(v->uv[0],v->uv[1],v->uv[2]);glVertex3f(v->position[0],v->position[1],v->position[2]);}
  glEnd();memcpy(current->fixed.color,saved_color,16);memcpy(current->fixed.uv,saved_uv,12);free(vertices);
}
