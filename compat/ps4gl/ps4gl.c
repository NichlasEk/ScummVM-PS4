#include "ps4gl.h"
#include <string.h>
#include <stdlib.h>
/* Deliberately single-threaded: SDL/thread ownership is a later integration gate. */
static PS4GLContext *current;
static void error(GLenum value) {
  if (current && !current->error) current->error = value;
}
int ps4glInit(PS4GLContext *ctx, GLint width, GLint height) {
  if (!ctx || width <= 0 || height <= 0) return 0;
  memset(ctx, 0, sizeof(*ctx));
  ctx->next_buffer=1;
  for(unsigned i=0;i<PS4GL_MAX_ATTRIBS;++i) ctx->attribs[i].size=4;
  ctx->width=width; ctx->height=height;
  ctx->scissor[2]=ctx->viewport[2]=width;
  ctx->scissor[3]=ctx->viewport[3]=height;
  return 1;
}
PS4GLContext *ps4glCurrentContext(void) { return current; }
void ps4glMakeCurrent(PS4GLContext *ctx) { current=ctx; }
void ps4glBeginPass(VkCommandBuffer command) {
  if (!current) return;
  if (!command || current->command) { error(GL_INVALID_OPERATION); return; }
  current->command=command;
}
void ps4glEndPass(void) { if (current) current->command=VK_NULL_HANDLE; }
GLenum glGetError(void) {
  if (!current) return GL_INVALID_OPERATION;
  GLenum result=current->error; current->error=GL_NO_ERROR; return result;
}
void glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
  if (!current) return;
  const GLfloat values[]={r,g,b,a};
  for (unsigned i=0;i<4;++i)
    current->clear[i]=!(values[i]>0) ? 0 : values[i]>1 ? 1 : values[i];
}
static void box(GLint *dst, GLint x, GLint y, GLsizei width, GLsizei height) {
  if (width<0 || height<0) { error(GL_INVALID_VALUE); return; }
  dst[0]=x; dst[1]=y; dst[2]=width; dst[3]=height;
}
void glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
  if (current) box(current->scissor,x,y,width,height);
}
void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
  if (current) box(current->viewport,x,y,width,height);
}
void glEnable(GLenum cap) {
  if (cap!=GL_SCISSOR_TEST) { error(GL_INVALID_ENUM); return; }
  if (current) current->scissor_enabled=GL_TRUE;
}
void glDisable(GLenum cap) {
  if (cap!=GL_SCISSOR_TEST) { error(GL_INVALID_ENUM); return; }
  if (current) current->scissor_enabled=GL_FALSE;
}
GLboolean glIsEnabled(GLenum cap) {
  if (cap!=GL_SCISSOR_TEST) { error(GL_INVALID_ENUM); return GL_FALSE; }
  return current ? current->scissor_enabled : GL_FALSE;
}
void glGetIntegerv(GLenum pname, GLint *values) {
  if (!current) return;
  if (!values) { error(GL_INVALID_VALUE); return; }
  if (pname==GL_SCISSOR_BOX || pname==GL_VIEWPORT)
    memcpy(values,pname==GL_SCISSOR_BOX ? current->scissor : current->viewport,4*sizeof(GLint));
  else if (pname==GL_ARRAY_BUFFER_BINDING) *values=(GLint)current->array_buffer;
  else if (pname==GL_CURRENT_PROGRAM) *values=current->program;
  else if (pname==GL_SCISSOR_TEST) *values=current->scissor_enabled;
  else error(GL_INVALID_ENUM);
}
void glGetFloatv(GLenum pname, GLfloat *values) {
  if (!current) return;
  if (!values) { error(GL_INVALID_VALUE); return; }
  if (pname==GL_COLOR_CLEAR_VALUE) memcpy(values,current->clear,sizeof(current->clear));
  else error(GL_INVALID_ENUM);
}
void glClear(GLbitfield mask) {
  if (!current) return;
  if (mask & ~(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT)) {
    error(GL_INVALID_VALUE); return;
  }
  /* This color-only target has no depth/stencil attachment: GL ignores those
   * bits rather than rejecting a valid request for an absent buffer. */
  if (!(mask & GL_COLOR_BUFFER_BIT)) return;
  if (!current->command) { error(GL_INVALID_OPERATION); return; }
  int64_t x0=0,y0=0,x1=current->width,y1=current->height;
  if (current->scissor_enabled) {
    x0=current->scissor[0]; y0=current->scissor[1];
    x1=x0+(int64_t)current->scissor[2]; y1=y0+(int64_t)current->scissor[3];
    if(x0<0) x0=0;
    if(y0<0) y0=0;
    if(x1>current->width) x1=current->width;
    if(y1>current->height) y1=current->height;
  }
  if(x1<=x0 || y1<=y0) return;
  VkClearAttachment clear={.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,.colorAttachment=0};
  memcpy(clear.clearValue.color.float32,current->clear,sizeof(current->clear));
  /* GL is bottom-left; the native attachment's rows are top-left. */
  VkClearRect rect={{{(int32_t)x0,(int32_t)(current->height-y1)},
                    {(uint32_t)(x1-x0),(uint32_t)(y1-y0)}},0,1};
  vkCmdClearAttachments(current->command,1,&clear,1,&rect);
}
static PS4GLBuffer *find_buffer(PS4GLContext *ctx, GLuint name) {
  if(!name) return NULL;
  for(unsigned i=0;i<PS4GL_MAX_BUFFERS;++i) if(ctx->buffers[i].name==name) return &ctx->buffers[i];
  return NULL;
}
void glGenBuffers(GLsizei n, GLuint *names) {
  if(!current) return;
  if(n<0 || (n && !names)) { error(GL_INVALID_VALUE); return; }
  unsigned free_slots=0;
  for(unsigned i=0;i<PS4GL_MAX_BUFFERS;++i) free_slots+=!current->buffers[i].name;
  if((unsigned)n>free_slots) { error(GL_OUT_OF_MEMORY); return; }
  for(GLsizei j=0;j<n;++j) {
    while(current->next_buffer && find_buffer(current,current->next_buffer)) ++current->next_buffer;
    if(!current->next_buffer) { error(GL_OUT_OF_MEMORY); return; }
    for(unsigned i=0;i<PS4GL_MAX_BUFFERS;++i) if(!current->buffers[i].name) {
      names[j]=current->buffers[i].name=current->next_buffer++;
      current->buffers[i].usage=GL_STATIC_DRAW;
      break;
    }
  }
}
void glBindBuffer(GLenum target, GLuint name) {
  if(!current) return;
  if(target!=GL_ARRAY_BUFFER) { error(GL_INVALID_ENUM); return; }
  if(name) {
    PS4GLBuffer *b=find_buffer(current,name);
    if(!b) for(unsigned i=0;i<PS4GL_MAX_BUFFERS;++i) if(!current->buffers[i].name) {
      b=&current->buffers[i]; b->name=name; b->usage=GL_STATIC_DRAW; break;
    }
    if(!b) { error(GL_OUT_OF_MEMORY); return; }
    b->created=GL_TRUE;
  }
  current->array_buffer=name;
}
GLboolean glIsBuffer(GLuint name) {
  PS4GLBuffer *b=current ? find_buffer(current,name) : NULL;
  return b && b->created;
}
void glDeleteBuffers(GLsizei n, const GLuint *names) {
  if(!current) return;
  if(n<0 || (n && !names)) { error(GL_INVALID_VALUE); return; }
  for(GLsizei j=0;j<n;++j) {
    PS4GLBuffer *b=find_buffer(current,names[j]);
    if(!b) continue;
    if(current->array_buffer==names[j]) current->array_buffer=0;
    for(unsigned a=0;a<PS4GL_MAX_ATTRIBS;++a)
      if(current->attribs[a].buffer==names[j]) current->attribs[a].buffer=0;
    free(b->data); *b=(PS4GLBuffer){0};
  }
}
static PS4GLBuffer *bound_buffer(GLenum target) {
  if(target!=GL_ARRAY_BUFFER) { error(GL_INVALID_ENUM); return NULL; }
  PS4GLBuffer *b=find_buffer(current,current->array_buffer);
  if(!b || !b->created) { error(GL_INVALID_OPERATION); return NULL; }
  return b;
}
void glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
  if(!current) return;
  PS4GLBuffer *b=bound_buffer(target); if(!b) return;
  if(size<0) { error(GL_INVALID_VALUE); return; }
  if(usage!=GL_STATIC_DRAW && usage!=GL_DYNAMIC_DRAW && usage!=GL_STREAM_DRAW) { error(GL_INVALID_ENUM); return; }
  if((size_t)size>PS4GL_MAX_BUFFER_BYTES) { error(GL_OUT_OF_MEMORY); return; }
  unsigned char *copy=size ? malloc((size_t)size) : NULL;
  if(size && !copy) { error(GL_OUT_OF_MEMORY); return; }
  if(size) { if(data) memcpy(copy,data,(size_t)size); else memset(copy,0,(size_t)size); }
  free(b->data); b->data=copy; b->size=(size_t)size; b->usage=usage;
}
void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) {
  if(!current) return;
  PS4GLBuffer *b=bound_buffer(target); if(!b) return;
  if(offset<0 || size<0 || (size && !data) || (size_t)offset>b->size ||
     (size_t)size>b->size-(size_t)offset) { error(GL_INVALID_VALUE); return; }
  if(size) memcpy(b->data+(size_t)offset,data,(size_t)size);
}
void glGetBufferParameteriv(GLenum target, GLenum pname, GLint *value) {
  if(!current) return;
  PS4GLBuffer *b=bound_buffer(target); if(!b) return;
  if(!value) { error(GL_INVALID_VALUE); return; }
  if(pname==GL_BUFFER_SIZE) *value=(GLint)b->size;
  else if(pname==GL_BUFFER_USAGE) *value=(GLint)b->usage;
  else error(GL_INVALID_ENUM);
}
void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized,
                           GLsizei stride, const void *pointer) {
  (void)normalized; /* Floating point inputs are never normalized. */
  if(!current) return;
  if(index>=PS4GL_MAX_ATTRIBS || size<1 || size>4 || stride<0) { error(GL_INVALID_VALUE); return; }
  if(type!=GL_FLOAT) { error(GL_INVALID_ENUM); return; }
  if(!current->array_buffer) { error(GL_INVALID_OPERATION); return; }
  PS4GLAttrib *a=&current->attribs[index];
  a->buffer=current->array_buffer; a->size=size; a->stride=stride;
  a->offset=(uintptr_t)pointer;
}
void glEnableVertexAttribArray(GLuint index) {
  if(!current) return;
  if(index>=PS4GL_MAX_ATTRIBS) { error(GL_INVALID_VALUE); return; }
  current->attribs[index].enabled=GL_TRUE;
}
void glDisableVertexAttribArray(GLuint index) {
  if(!current) return;
  if(index>=PS4GL_MAX_ATTRIBS) { error(GL_INVALID_VALUE); return; }
  current->attribs[index].enabled=GL_FALSE;
}
int ps4glPackVertices(PS4GLContext *ctx, const PS4GLProgram *p, GLsizei count, void *dest) {
  if(!ctx || !p || count<0 || (count && !dest)) return 0;
  /* Validate every source range before writing the output; subtraction avoids overflow. */
  for(unsigned i=0;i<PS4GL_MAX_ATTRIBS;++i) if(p->components[i] && ctx->attribs[i].enabled && count) {
    PS4GLAttrib *a=&ctx->attribs[i]; PS4GLBuffer *b=find_buffer(ctx,a->buffer);
    size_t width=(size_t)a->size*4, stride=a->stride ? (size_t)a->stride : width;
    if(!b || !b->created || a->offset>b->size || width>b->size-a->offset ||
       (size_t)(count-1)>(b->size-a->offset-width)/stride) {
      if(!ctx->error) ctx->error=GL_INVALID_OPERATION;
      return 0;
    }
  }
  for(GLsizei v=0;v<count;++v) {
    unsigned char *out=(unsigned char *)dest+(size_t)v*p->vertex_stride;
    for(unsigned i=0;i<PS4GL_MAX_ATTRIBS;++i) if(p->components[i]) {
      float value[4]={0,0,0,1}; PS4GLAttrib *a=&ctx->attribs[i];
      if(a->enabled) {
        PS4GLBuffer *b=find_buffer(ctx,a->buffer);
        size_t stride=a->stride ? (size_t)a->stride : (size_t)a->size*4;
        memcpy(value,b->data+a->offset+(size_t)v*stride,(size_t)a->size*4);
      }
      memcpy(out,value,p->components[i]*4); out+=p->components[i]*4;
    }
  }
  return 1;
}
void ps4glDestroyBuffers(PS4GLContext *ctx) {
  if(!ctx) return;
  for(unsigned i=0;i<PS4GL_MAX_BUFFERS;++i) { free(ctx->buffers[i].data); ctx->buffers[i]=(PS4GLBuffer){0}; }
  ctx->array_buffer=0;
  for(unsigned i=0;i<PS4GL_MAX_ATTRIBS;++i) ctx->attribs[i]=(PS4GLAttrib){.size=4};
}
static PS4GLProgram *program(GLuint name) {
  if (!current || !name || name>PS4GL_MAX_PROGRAMS) return NULL;
  PS4GLProgram *p=&current->programs[name-1];
  return p->pipeline && !p->retired ? p : NULL;
}
GLboolean glIsProgram(GLuint name) { return program(name) ? GL_TRUE : GL_FALSE; }
void glUseProgram(GLuint name) {
  if (!current) return;
  if (name && !program(name)) { error(GL_INVALID_VALUE); return; }
  if (current->program==name) return;
  PS4GLProgram *old=program(current->program);
  if (old && old->deleted) old->retired=GL_TRUE;
  current->program=name;
}
void glDeleteProgram(GLuint name) {
  if (!current || !name) return;
  PS4GLProgram *p=program(name);
  if (!p) { error(GL_INVALID_VALUE); return; }
  p->deleted=GL_TRUE;
  if (current->program!=name) p->retired=GL_TRUE;
}
void glGetProgramiv(GLuint name, GLenum pname, GLint *value) {
  if (!current) return;
  PS4GLProgram *p=program(name);
  if (!p || !value) { error(GL_INVALID_VALUE); return; }
  if (pname==GL_LINK_STATUS) *value=GL_TRUE;
  else if (pname==GL_DELETE_STATUS) *value=p->deleted;
  else error(GL_INVALID_ENUM);
}
void glDrawArrays(GLenum mode, GLint first, GLsizei count) {
  if (!current) return;
  if (mode!=GL_TRIANGLES) { error(GL_INVALID_ENUM); return; }
  if (count<0 || first<0) { error(GL_INVALID_VALUE); return; }
  PS4GLProgram *p=program(current->program);
  if (!p || !current->command || first) { error(GL_INVALID_OPERATION); return; }
  GLint *v=current->viewport;
  /* The initial native subset deliberately rejects out-of-target viewports. */
  if(v[0]<0 || v[1]<0 || (int64_t)v[0]+v[2]>current->width ||
      (int64_t)v[1]+v[3]>current->height) { error(GL_INVALID_OPERATION); return; }
  if(!count || !v[2] || !v[3]) return;
  int64_t x0=0,y0=0,x1=current->width,y1=current->height;
  if(current->scissor_enabled) {
    x0=current->scissor[0]; y0=current->scissor[1];
    x1=x0+(int64_t)current->scissor[2]; y1=y0+(int64_t)current->scissor[3];
    if(x0<0) x0=0;
    if(y0<0) y0=0;
    if(x1>current->width) x1=current->width;
    if(y1>current->height) y1=current->height;
  }
  if(x1<=x0 || y1<=y0) return;
  VkViewport vp={.x=v[0],.y=current->height-v[1]-v[3],.width=v[2],.height=v[3],.maxDepth=1};
  VkRect2D sc={{(int32_t)x0,(int32_t)(current->height-y1)},
              {(uint32_t)(x1-x0),(uint32_t)(y1-y0)}};
  if(p->vertex_stride) {
    if(!current->upload_vertices) { error(GL_INVALID_OPERATION); return; }
    if((size_t)count>PS4GL_MAX_BUFFER_BYTES/p->vertex_stride) { error(GL_OUT_OF_MEMORY); return; }
    size_t bytes=(size_t)count*p->vertex_stride;
    void *packed=malloc(bytes);
    if(!packed) { error(GL_OUT_OF_MEMORY); return; }
    int ok=ps4glPackVertices(current,p,count,packed);
    if(ok) ok=current->upload_vertices(current,packed,bytes);
    free(packed);
    if(!ok) return;
  }
  vkCmdBindPipeline(current->command,VK_PIPELINE_BIND_POINT_GRAPHICS,p->pipeline);
  vkCmdSetViewport(current->command,0,1,&vp);
  vkCmdSetScissor(current->command,0,1,&sc);
  vkCmdDraw(current->command,(uint32_t)count,1,0,0);
}
PS4GLProc ps4glGetProcAddress(const char *name) {
  if (!name) return NULL;
#define PROC(fn) if(!strcmp(name,#fn)) return (PS4GLProc)fn
  PROC(glClearColor); PROC(glClear); PROC(glScissor); PROC(glViewport);
  PROC(glEnable); PROC(glDisable); PROC(glIsEnabled); PROC(glGetError);
  PROC(glGetIntegerv); PROC(glGetFloatv);
  PROC(glUseProgram); PROC(glDeleteProgram); PROC(glIsProgram);
  PROC(glGetProgramiv); PROC(glDrawArrays);
  PROC(glGenBuffers); PROC(glBindBuffer); PROC(glDeleteBuffers); PROC(glIsBuffer);
  PROC(glBufferData); PROC(glBufferSubData); PROC(glGetBufferParameteriv);
  PROC(glVertexAttribPointer); PROC(glEnableVertexAttribArray); PROC(glDisableVertexAttribArray);
#undef PROC
  return NULL;
}
