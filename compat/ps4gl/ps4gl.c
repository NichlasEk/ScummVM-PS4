#include "ps4gl.h"
#include <string.h>
/* Deliberately single-threaded: SDL/thread ownership is a later integration gate. */
static PS4GLContext *current;
static void error(GLenum value) {
  if (current && !current->error) current->error = value;
}
int ps4glInit(PS4GLContext *ctx, GLint width, GLint height) {
  if (!ctx || width <= 0 || height <= 0) return 0;
  memset(ctx, 0, sizeof(*ctx));
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
#undef PROC
  return NULL;
}
