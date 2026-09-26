#include "ps4gl.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
/* Deliberately single-threaded: SDL/thread ownership is a later integration gate. */
static PS4GLContext *current;
static void error(GLenum value) {
  if (current && !current->error) current->error = value;
}
int ps4glInit(PS4GLContext *ctx, GLint width, GLint height) {
  if (!ctx || width <= 0 || height <= 0) return 0;
  memset(ctx, 0, sizeof(*ctx));
  ctx->blend=(PS4GLBlend){.src_rgb=GL_ONE,.src_alpha=GL_ONE,.equation_rgb=GL_FUNC_ADD,.equation_alpha=GL_FUNC_ADD};
  ctx->depth_write_mask=GL_TRUE;
  ctx->next_shader=65536;
  ctx->next_framebuffer=1;
  ctx->next_buffer=1; ctx->next_texture=1; ctx->unpack_alignment=ctx->pack_alignment=4;
  for(unsigned i=0;i<PS4GL_MAX_ATTRIBS;++i) {
    ctx->attribs[i].size=4; ctx->attribs[i].constant[3]=1;
  }
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
  current->command=command; current->active_pass=current->program_pass;
}
void ps4glEndPass(void) {
  if(!current) return;
  if(current->managed_frame) { error(GL_INVALID_OPERATION); return; }
  current->command=VK_NULL_HANDLE;
}
static int prepare_target(void) {
  if(current->prepare_target) return current->prepare_target(current);
  if(current->framebuffer) { error(GL_INVALID_OPERATION); return 0; }
  return 1;
}
static void end_target(void) {
  if(current && current->managed_frame && current->end_target) current->end_target(current);
}
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
static int blend_factor(GLenum value) {
  return value==GL_ZERO || value==GL_ONE || (value>=GL_SRC_COLOR && value<=GL_ONE_MINUS_DST_COLOR);
}
static int blend_equation(GLenum value) {
  return value==GL_FUNC_ADD || value==GL_FUNC_SUBTRACT || value==GL_FUNC_REVERSE_SUBTRACT || value==GL_MIN || value==GL_MAX;
}
void glBlendFuncSeparate(GLenum sr, GLenum dr, GLenum sa, GLenum da) {
  if(!current) return;
  if(!blend_factor(sr) || !blend_factor(dr) || !blend_factor(sa) || !blend_factor(da)) { error(GL_INVALID_ENUM); return; }
  current->blend.src_rgb=sr; current->blend.dst_rgb=dr; current->blend.src_alpha=sa; current->blend.dst_alpha=da;
}
void glBlendFunc(GLenum source, GLenum destination) { glBlendFuncSeparate(source,destination,source,destination); }
void glBlendEquationSeparate(GLenum rgb, GLenum alpha) {
  if(!current) return;
  if(!blend_equation(rgb) || !blend_equation(alpha)) { error(GL_INVALID_ENUM); return; }
  current->blend.equation_rgb=rgb; current->blend.equation_alpha=alpha;
}
void glBlendEquation(GLenum equation) { glBlendEquationSeparate(equation,equation); }
/* Fixed raster policy of the bounded color-only backend, not general GL state.
 * Disabling these features is supported; enabling them must remain explicit
 * failure until the corresponding rendering behavior is implemented. */
static int fixed_disabled_cap(GLenum cap) {
  return cap==GL_CULL_FACE || cap==GL_DEPTH_TEST || cap==GL_DITHER;
}
void glEnable(GLenum cap) {
  if(fixed_disabled_cap(cap)) { error(GL_INVALID_OPERATION); return; }
  if(cap!=GL_SCISSOR_TEST && cap!=GL_BLEND) { error(GL_INVALID_ENUM); return; }
  if(current) { if(cap==GL_BLEND) current->blend.enabled=1; else current->scissor_enabled=GL_TRUE; }
}
void glDisable(GLenum cap) {
  if(fixed_disabled_cap(cap)) return;
  if(cap!=GL_SCISSOR_TEST && cap!=GL_BLEND) { error(GL_INVALID_ENUM); return; }
  if(current) { if(cap==GL_BLEND) current->blend.enabled=0; else current->scissor_enabled=GL_FALSE; }
}
GLboolean glIsEnabled(GLenum cap) {
  if(fixed_disabled_cap(cap)) return GL_FALSE;
  if(cap!=GL_SCISSOR_TEST && cap!=GL_BLEND) { error(GL_INVALID_ENUM); return GL_FALSE; }
  return current ? (cap==GL_BLEND ? current->blend.enabled : current->scissor_enabled) : GL_FALSE;
}
/* Depth writes are tracked for state restoration; color-only targets have no
 * depth attachment, and enabling depth testing remains unsupported. */
void glDepthMask(GLboolean enabled) {
  if(current) current->depth_write_mask=enabled ? GL_TRUE : GL_FALSE;
}
void glGetBooleanv(GLenum pname, GLboolean *values) {
  if(!current) return;
  if(!values) { error(GL_INVALID_VALUE); return; }
  if(pname==GL_DEPTH_WRITEMASK) *values=current->depth_write_mask;
  else error(GL_INVALID_ENUM);
}
void glGetIntegerv(GLenum pname, GLint *values) {
  if (!current) return;
  if (!values) { error(GL_INVALID_VALUE); return; }
  if (pname==GL_SCISSOR_BOX || pname==GL_VIEWPORT)
    memcpy(values,pname==GL_SCISSOR_BOX ? current->scissor : current->viewport,4*sizeof(GLint));
  else if(pname==GL_DEPTH_WRITEMASK) *values=current->depth_write_mask;
  else if(pname==GL_FRAMEBUFFER_BINDING) *values=current->framebuffer;
  else if(pname==GL_ACTIVE_TEXTURE) *values=GL_TEXTURE0+current->active_texture;
  else if(pname==GL_TEXTURE_BINDING_2D) *values=current->texture_bindings[current->active_texture];
  else if(pname==GL_UNPACK_ALIGNMENT) *values=current->unpack_alignment;
  else if(pname==GL_PACK_ALIGNMENT) *values=current->pack_alignment;
  else if(fixed_disabled_cap(pname)) *values=GL_FALSE;
  else if (pname==GL_ELEMENT_ARRAY_BUFFER_BINDING) *values=(GLint)current->element_buffer;
  else if (pname==GL_ARRAY_BUFFER_BINDING) *values=(GLint)current->array_buffer;
  else if (pname==GL_CURRENT_PROGRAM) *values=current->program;
  else if(pname==GL_BLEND) *values=current->blend.enabled;
  else if(pname==GL_BLEND_SRC_RGB) *values=current->blend.src_rgb;
  else if(pname==GL_BLEND_DST_RGB) *values=current->blend.dst_rgb;
  else if(pname==GL_BLEND_SRC_ALPHA) *values=current->blend.src_alpha;
  else if(pname==GL_BLEND_DST_ALPHA) *values=current->blend.dst_alpha;
  else if(pname==GL_BLEND_EQUATION_RGB) *values=current->blend.equation_rgb;
  else if(pname==GL_BLEND_EQUATION_ALPHA) *values=current->blend.equation_alpha;
  else if(pname==GL_MAX_TEXTURE_SIZE) *values=PS4GL_TEXTURE_LIMIT;
  else if(pname==GL_MAX_VERTEX_ATTRIBS) *values=PS4GL_MAX_ATTRIBS;
  else if(pname==GL_MAX_TEXTURE_IMAGE_UNITS || pname==GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS) *values=2;
  else if(pname==GL_RED_BITS || pname==GL_GREEN_BITS || pname==GL_BLUE_BITS || pname==GL_ALPHA_BITS) *values=8;
  else if(pname==GL_DEPTH_BITS || pname==GL_STENCIL_BITS || pname==GL_STEREO || pname==GL_SAMPLE_BUFFERS || pname==GL_SAMPLES) *values=0;
  else if(pname==GL_DOUBLEBUFFER) *values=!current->framebuffer;
  else if (pname==GL_SCISSOR_TEST) *values=current->scissor_enabled;
  else error(GL_INVALID_ENUM);
}
const unsigned char *glGetString(GLenum name) {
  if(!current) return NULL;
  switch(name) {
    case GL_VENDOR:return (const unsigned char *)"ScummVM-PS4";
    case GL_RENDERER:return (const unsigned char *)"OpenGNM Vulkan bridge (experimental)";
    /* A deliberate non-version: do not let engine autodetection infer a full
     * OpenGL 2.x implementation from this diagnostic subset. */
    case GL_VERSION:return (const unsigned char *)"0.0 PS4GL experimental subset";
    case GL_SHADING_LANGUAGE_VERSION:return (const unsigned char *)"1.10 bounded translator";
    case GL_EXTENSIONS:return (const unsigned char *)"";
    default:error(GL_INVALID_ENUM);return NULL;
  }
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
  if (!current->command && current->begin_frame && !current->begin_frame(current)) { error(GL_INVALID_OPERATION); return; }
  if (!current->command) { error(GL_INVALID_OPERATION); return; }
  /* A complete color clear overwrites every texel. A new offscreen target
   * can skip importing its CPU shadow only for this operation. */
  current->discard_target_contents=!current->scissor_enabled;
  int prepared=prepare_target(); current->discard_target_contents=GL_FALSE;
  if(!prepared) return;
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
  VkClearRect rect={{{(int32_t)x0,(int32_t)(current->active_target ? y0 : current->height-y1)},
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
  if(target!=GL_ARRAY_BUFFER && target!=GL_ELEMENT_ARRAY_BUFFER) { error(GL_INVALID_ENUM); return; }
  if(name) {
    PS4GLBuffer *b=find_buffer(current,name);
    if(!b) for(unsigned i=0;i<PS4GL_MAX_BUFFERS;++i) if(!current->buffers[i].name) {
      b=&current->buffers[i]; b->name=name; b->usage=GL_STATIC_DRAW; break;
    }
    if(!b) { error(GL_OUT_OF_MEMORY); return; }
    b->created=GL_TRUE;
  }
  if(target==GL_ARRAY_BUFFER) current->array_buffer=name;
  else current->element_buffer=name;
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
    if(current->element_buffer==names[j]) current->element_buffer=0;
    for(unsigned a=0;a<PS4GL_MAX_ATTRIBS;++a)
      if(current->attribs[a].buffer==names[j]) current->attribs[a].buffer=0;
    free(b->data); *b=(PS4GLBuffer){0};
  }
}
static PS4GLBuffer *bound_buffer(GLenum target) {
  if(target!=GL_ARRAY_BUFFER && target!=GL_ELEMENT_ARRAY_BUFFER) { error(GL_INVALID_ENUM); return NULL; }
  PS4GLBuffer *b=find_buffer(current,target==GL_ARRAY_BUFFER ? current->array_buffer : current->element_buffer);
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
static void constant_attrib(GLuint index, const GLfloat *value, unsigned size) {
  if(!current) return;
  if(index>=PS4GL_MAX_ATTRIBS || !value) { error(GL_INVALID_VALUE); return; }
  GLfloat copy[4]={0,0,0,1};
  memcpy(copy,value,size*sizeof(*value));
  memcpy(current->attribs[index].constant,copy,sizeof(copy));
}
void glVertexAttrib2fv(GLuint index, const GLfloat *value) { constant_attrib(index,value,2); }
void glVertexAttrib3fv(GLuint index, const GLfloat *value) { constant_attrib(index,value,3); }
void glVertexAttrib4fv(GLuint index, const GLfloat *value) { constant_attrib(index,value,4); }
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
      } else memcpy(value,a->constant,sizeof(value));
      memcpy(out,value,p->components[i]*4); out+=p->components[i]*4;
    }
  }
  return 1;
}
void ps4glDestroyBuffers(PS4GLContext *ctx) {
  if(!ctx) return;
  for(unsigned i=0;i<PS4GL_MAX_BUFFERS;++i) { free(ctx->buffers[i].data); ctx->buffers[i]=(PS4GLBuffer){0}; }
  ctx->array_buffer=ctx->element_buffer=0;
  for(unsigned i=0;i<PS4GL_MAX_ATTRIBS;++i) ctx->attribs[i]=(PS4GLAttrib){.size=4,.constant={0,0,0,1}};
}
static PS4GLProgram *program(GLuint name) {
  if (!current || !name || name>PS4GL_MAX_PROGRAMS) return NULL;
  PS4GLProgram *p=&current->programs[name-1];
  return (p->pipeline || p->allocated) && !p->retired ? p : NULL;
}
#include "ps4gl-shaders.inc"
GLboolean glIsProgram(GLuint name) { return program(name) ? GL_TRUE : GL_FALSE; }
void glUseProgram(GLuint name) {
  if (!current) return;
  if (name && !program(name)) { error(GL_INVALID_VALUE); return; }
  if(name && !program(name)->pipeline) { error(GL_INVALID_OPERATION); return; }
  if (current->program==name) return;
  PS4GLProgram *old=program(current->program);
  if (old && old->deleted) { detach_all(old); old->retired=GL_TRUE; }
  current->program=name;
}
void glDeleteProgram(GLuint name) {
  if (!current || !name) return;
  PS4GLProgram *p=program(name);
  if (!p) { error(GL_INVALID_VALUE); return; }
  p->deleted=GL_TRUE;
  if (current->program!=name) { detach_all(p); p->retired=GL_TRUE; }
}
void glGetProgramiv(GLuint name, GLenum pname, GLint *value) {
  if (!current) return;
  PS4GLProgram *p=program(name);
  if (!p || !value) { error(GL_INVALID_VALUE); return; }
  if (pname==GL_LINK_STATUS) *value=p->pipeline!=VK_NULL_HANDLE;
  else if(pname==GL_INFO_LOG_LENGTH) *value=p->log[0] ? (GLint)strlen(p->log)+1 : 0;
  else if(pname==GL_ATTACHED_SHADERS) *value=!!p->attached[0]+!!p->attached[1];
  else if (pname==GL_DELETE_STATUS) *value=p->deleted;
  else error(GL_INVALID_ENUM);
}
GLint glGetUniformLocation(GLuint name, const char *uniform) {
  if(!current) return -1;
  PS4GLProgram *p=program(name);
  if(!p || !uniform) { error(GL_INVALID_VALUE); return -1; }
  if(!p->pipeline) { error(GL_INVALID_OPERATION); return -1; }
  for(unsigned i=0;i<p->uniform_count;++i) {
    PS4GLUniform *u=&p->uniforms[i]; size_t len=strlen(u->name);
    if(!strcmp(uniform,u->name)) return (GLint)i;
    if(u->array_length && !strncmp(uniform,u->name,len) && uniform[len]=='[') {
      const char *q=uniform+len+1; unsigned element=0;
      if(*q<'0' || *q>'9') continue;
      while(*q>='0' && *q<='9') {
        /* Bound before multiplying, including excessively long numeric names. */
        if(element>=u->array_length) return -1;
        element=element*10+(unsigned)(*q++-'0');
      }
      if(*q==']' && !q[1] && element<u->array_length)
        return (GLint)(i+element*PS4GL_MAX_UNIFORMS);
    }
  }
  return -1;
}
static void uniform(GLint location, GLsizei count, unsigned width, GLboolean transpose, const void *data, unsigned integer) {
  if(!current) return;
  PS4GLProgram *p=program(current->program);
  if(!p) { error(GL_INVALID_OPERATION); return; }
  if(count<0) { error(GL_INVALID_VALUE); return; }
  if(location==-1) return;
  unsigned member=(unsigned)location%PS4GL_MAX_UNIFORMS, element=(unsigned)location/PS4GL_MAX_UNIFORMS;
  if(location<0 || member>=p->uniform_count) { error(GL_INVALID_OPERATION); return; }
  PS4GLUniform *u=&p->uniforms[member];
  unsigned available=u->array_length ? u->array_length : 1;
  if(element>=available || (u->integer!=2 && u->integer!=integer) || u->sampler_binding || u->width!=width || (!u->array_length && count>1)) { error(GL_INVALID_OPERATION); return; }
  if(!count) return;
  if(!data) { error(GL_INVALID_VALUE); return; }
  /* GL ignores array writes beyond the last active element. */
  if((unsigned)count>available-element) count=(GLsizei)(available-element);
  for(GLsizei j=0;j<count;++j) {
    uint32_t value[16]; const unsigned char *input=(const unsigned char *)data+(size_t)j*width*4;
    if(u->integer==2) {
      if(integer) { GLint v; memcpy(&v,input,4); value[0]=v!=0; }
      else { GLfloat v; memcpy(&v,input,4); value[0]=v!=0.f; }
    } else if(width==16 && transpose) {
      for(unsigned c=0;c<4;++c) for(unsigned r=0;r<4;++r) memcpy(value+c*4+r,input+(r*4+c)*4,4);
    } else memcpy(value,input,width*4);
    for(unsigned stage=0;stage<2;++stage) if(u->offset[stage]>=0)
      memcpy(p->uniform_data[stage]+u->offset[stage]+(element+(unsigned)j)*u->array_stride[stage],value,width*4);
  }
}
void glUniform1iv(GLint location, GLsizei count, const GLint *value) {
  if(!current) return;
  PS4GLProgram *p=program(current->program);
  if(!p) { error(GL_INVALID_OPERATION); return; }
  if(count<0) { error(GL_INVALID_VALUE); return; }
  if(location==-1) return;
  if(location>=0 && (unsigned)location%PS4GL_MAX_UNIFORMS<p->uniform_count &&
     !p->uniforms[(unsigned)location%PS4GL_MAX_UNIFORMS].sampler_binding) {
    uniform(location,count,1,0,value,1); return;
  }
  if(location<0 || (unsigned)location>=p->uniform_count || !p->uniforms[location].sampler_binding || count>1) {
    error(GL_INVALID_OPERATION); return;
  }
  if(!count) return;
  if(!value || *value<0 || *value>=PS4GL_TEXTURE_UNITS) { error(GL_INVALID_VALUE); return; }
  p->sampler_units[p->uniforms[location].sampler_binding-2]=*value;
}
void glUniform1i(GLint location, GLint value) { glUniform1iv(location,1,&value); }
void glUniform1fv(GLint l, GLsizei n, const GLfloat *v) { uniform(l,n,1,0,v,0); }
void glUniform2fv(GLint l, GLsizei n, const GLfloat *v) { uniform(l,n,2,0,v,0); }
void glUniform3fv(GLint l, GLsizei n, const GLfloat *v) { uniform(l,n,3,0,v,0); }
void glUniform4fv(GLint l, GLsizei n, const GLfloat *v) { uniform(l,n,4,0,v,0); }
void glUniformMatrix4fv(GLint l, GLsizei n, GLboolean t, const GLfloat *v) { uniform(l,n,16,t,v,0); }
void glUniform1f(GLint l, GLfloat x) { glUniform1fv(l,1,&x); }
void glUniform2f(GLint l, GLfloat x, GLfloat y) { float v[]={x,y}; glUniform2fv(l,1,v); }
void glUniform3f(GLint l, GLfloat x, GLfloat y, GLfloat z) { float v[]={x,y,z}; glUniform3fv(l,1,v); }
void glUniform4f(GLint l, GLfloat x, GLfloat y, GLfloat z, GLfloat w) { float v[]={x,y,z,w}; glUniform4fv(l,1,v); }
static uint32_t element_index(const unsigned char *data, GLenum type, uint32_t i) {
  if(!type) return i;
  if(type==GL_UNSIGNED_SHORT) { uint16_t value; memcpy(&value,data+(size_t)i*2,2); return value; }
  uint32_t value; memcpy(&value,data+(size_t)i*4,4); return value;
}
static void draw(GLenum mode, GLint first, GLsizei count, GLenum type, const void *indices) {
  if (!current) return;
  if (mode!=GL_TRIANGLES && mode!=GL_TRIANGLE_STRIP) { error(GL_INVALID_ENUM); return; }
  if (count<0 || first<0) { error(GL_INVALID_VALUE); return; }
  PS4GLProgram *p=program(current->program);
  if(p && !first && !current->command && current->begin_frame && !current->begin_frame(current)) { error(GL_INVALID_OPERATION); return; }
  if (!p || !current->command || first) { error(GL_INVALID_OPERATION); return; }
  if(!prepare_target()) return;
  GLint *v=current->viewport;
  /* The initial native subset deliberately rejects out-of-target viewports. */
  if(v[0]<0 || v[1]<0 || (int64_t)v[0]+v[2]>current->width ||
      (int64_t)v[1]+v[3]>current->height) { error(GL_INVALID_OPERATION); return; }
  if(!count || (mode==GL_TRIANGLE_STRIP && count<3) || !v[2] || !v[3]) return;
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
  /* Offscreen row zero is GL texture v=0: signed viewport cancels imported VS Y flip. */
  if(current->active_target) { vp.y=v[1]+v[3]; vp.height=-v[3]; }
  VkRect2D sc={{(int32_t)x0,(int32_t)(current->active_target ? y0 : current->height-y1)},
              {(uint32_t)(x1-x0),(uint32_t)(y1-y0)}};
  VkPipeline pipeline=p->pipeline;
  if(current->blend.enabled || current->managed_frame) {
    if(!current->select_pipeline) { error(GL_INVALID_OPERATION); return; }
    pipeline=current->select_pipeline(current,p); if(!pipeline) return;
  }
  const int strip=mode==GL_TRIANGLE_STRIP;
  uint32_t draw_count=(uint32_t)count;
  GLenum draw_type=type;
  if(strip) {
    /* Triangulate into the existing triangle-list pipeline. Bound expansion
     * before multiplication/allocation; keep vertex IDs and provoking vertex. */
    if((uint32_t)(count-2)>PS4GL_MAX_BUFFER_BYTES/(3u*sizeof(uint32_t))) { error(GL_OUT_OF_MEMORY); return; }
    draw_count=(uint32_t)(count-2)*3; draw_type=GL_UNSIGNED_INT;
  }
  size_t index_bytes=0;
  const unsigned char *index_data=NULL;
  uint32_t vertices=(uint32_t)count;
  if(type) {
    PS4GLBuffer *ibo=bound_buffer(GL_ELEMENT_ARRAY_BUFFER);
    if(!ibo) return;
    size_t width=type==GL_UNSIGNED_SHORT ? 2 : 4;
    uintptr_t offset=(uintptr_t)indices;
    if(offset>ibo->size || (size_t)count>(ibo->size-offset)/width) {
      error(GL_INVALID_OPERATION); return;
    }
    index_bytes=(size_t)count*width; index_data=ibo->data+offset;
    uint32_t max_index=0;
    for(GLsizei i=0;i<count;++i) {
      uint32_t value=element_index(index_data,type,(uint32_t)i);
      if(value>max_index) max_index=value;
    }
    /* Pack a dense prefix, keeping the original gl_VertexIndex semantics. */
    if(p->vertex_stride && max_index>=INT32_MAX) { error(GL_INVALID_OPERATION); return; }
    vertices=p->vertex_stride ? max_index+1 : 0;
  }
  if(strip) index_bytes=(size_t)draw_count*sizeof(uint32_t);
  if(p->vertex_stride || draw_type) {
    if(!current->upload_vertices) { error(GL_INVALID_OPERATION); return; }
    if(p->vertex_stride && vertices>PS4GL_MAX_BUFFER_BYTES/p->vertex_stride) { error(GL_OUT_OF_MEMORY); return; }
    size_t vertex_bytes=(size_t)vertices*p->vertex_stride;
    if(index_bytes>PS4GL_MAX_BUFFER_BYTES-vertex_bytes) { error(GL_OUT_OF_MEMORY); return; }
    size_t bytes=vertex_bytes+index_bytes;
    void *packed=malloc(bytes);
    if(!packed) { error(GL_OUT_OF_MEMORY); return; }
    int ok=!p->vertex_stride || ps4glPackVertices(current,p,(GLsizei)vertices,packed);
    if(ok && strip) {
      uint32_t *out=(uint32_t *)((unsigned char *)packed+vertex_bytes);
      for(uint32_t i=0;i<(uint32_t)count-2;++i) {
        out[i*3]=element_index(index_data,type,i+(i&1));
        out[i*3+1]=element_index(index_data,type,i+1-(i&1));
        out[i*3+2]=element_index(index_data,type,i+2);
      }
    } else if(ok && index_bytes) memcpy((unsigned char *)packed+vertex_bytes,index_data,index_bytes);
    if(ok) ok=current->upload_vertices(current,packed,bytes,vertex_bytes,draw_type);
    free(packed);
    if(!ok) return;
  }
  if(p->uniform_stages || p->sampler_mask) {
    if(!current->upload_uniforms) { error(GL_INVALID_OPERATION); return; }
    if(!current->upload_uniforms(current,p)) return;
  }
  vkCmdBindPipeline(current->command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
  vkCmdSetViewport(current->command,0,1,&vp);
  vkCmdSetScissor(current->command,0,1,&sc);
  if(draw_type) vkCmdDrawIndexed(current->command,draw_count,1,0,0,0);
  else vkCmdDraw(current->command,draw_count,1,0,0);
}
void glDrawArrays(GLenum mode, GLint first, GLsizei count) {
  draw(mode,first,count,0,NULL);
}
void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices) {
  if(!current) return;
  if(type!=GL_UNSIGNED_SHORT && type!=GL_UNSIGNED_INT) { error(GL_INVALID_ENUM); return; }
  draw(mode,0,count,type,indices);
}
PS4GLProc ps4glGetProcAddress(const char *name) {
  if (!name) return NULL;
#define PROC(fn) if(!strcmp(name,#fn)) return (PS4GLProc)fn
  PROC(glBindAttribLocation); PROC(glGetAttribLocation);
  PROC(glCreateShader); PROC(glShaderSource); PROC(glCompileShader); PROC(glDeleteShader); PROC(glIsShader);
  PROC(glGetShaderiv); PROC(glGetShaderInfoLog); PROC(glGetShaderSource);
  PROC(glCreateProgram); PROC(glAttachShader); PROC(glDetachShader); PROC(glLinkProgram); PROC(glGetProgramInfoLog);
  PROC(glClearColor); PROC(glClear); PROC(glScissor); PROC(glViewport);
  PROC(glBlendFunc); PROC(glBlendFuncSeparate); PROC(glBlendEquation); PROC(glBlendEquationSeparate);
  PROC(glEnable); PROC(glDisable); PROC(glIsEnabled); PROC(glGetError);
  PROC(glDepthMask); PROC(glGetBooleanv); PROC(glGetIntegerv); PROC(glGetString); PROC(glGetFloatv);
  PROC(glUseProgram); PROC(glDeleteProgram); PROC(glIsProgram);
  PROC(glGenFramebuffers); PROC(glBindFramebuffer); PROC(glDeleteFramebuffers); PROC(glIsFramebuffer);
  PROC(glFramebufferTexture2D); PROC(glCheckFramebufferStatus);
  PROC(glGenTextures); PROC(glBindTexture); PROC(glDeleteTextures); PROC(glIsTexture);
  PROC(glActiveTexture); PROC(glPixelStorei); PROC(glTexParameteri); PROC(glTexImage2D); PROC(glTexSubImage2D);
  PROC(glUniform1i); PROC(glUniform1iv);
  PROC(glGetUniformLocation); PROC(glUniform1f); PROC(glUniform2f); PROC(glUniform3f); PROC(glUniform4f);
  PROC(glUniform1fv); PROC(glUniform2fv); PROC(glUniform3fv); PROC(glUniform4fv); PROC(glUniformMatrix4fv);
  PROC(glGetProgramiv); PROC(glDrawArrays); PROC(glDrawElements);
  PROC(glGenBuffers); PROC(glBindBuffer); PROC(glDeleteBuffers); PROC(glIsBuffer);
  PROC(glBufferData); PROC(glBufferSubData); PROC(glGetBufferParameteriv);
  PROC(glVertexAttrib2fv); PROC(glVertexAttrib3fv); PROC(glVertexAttrib4fv);
  PROC(glVertexAttribPointer); PROC(glEnableVertexAttribArray); PROC(glDisableVertexAttribArray);
#undef PROC
  return NULL;
}

static PS4GLTexture *texture_name(PS4GLContext *ctx, GLuint name) {
  if(!name) return NULL;
  for(unsigned i=0;i<PS4GL_MAX_TEXTURES;++i) if(ctx->textures[i].name==name && !ctx->textures[i].deleted) return &ctx->textures[i];
  return NULL;
}
const PS4GLTexture *ps4glTextureAtUnit(PS4GLContext *ctx, unsigned unit) {
  return ctx && unit<PS4GL_TEXTURE_UNITS ? texture_name(ctx,ctx->texture_bindings[unit]) : NULL;
}
void glGenTextures(GLsizei count, GLuint *names) {
  if(!current) return;
  if(count<0 || (count && !names)) { error(GL_INVALID_VALUE); return; }
  unsigned available=0;
  for(unsigned i=0;i<PS4GL_MAX_TEXTURES;++i) available+=!current->textures[i].name;
  if((unsigned)count>available) { error(GL_OUT_OF_MEMORY); return; }
  for(GLsizei n=0;n<count;++n) {
    while(current->next_texture && texture_name(current,current->next_texture)) ++current->next_texture;
    if(!current->next_texture) { error(GL_OUT_OF_MEMORY); return; }
    for(unsigned i=0;i<PS4GL_MAX_TEXTURES;++i) if(!current->textures[i].name) {
      names[n]=current->textures[i].name=current->next_texture++; break;
    }
  }
}
void glActiveTexture(GLenum unit) {
  if(!current) return;
  if(unit<GL_TEXTURE0 || unit>=GL_TEXTURE0+PS4GL_TEXTURE_UNITS) { error(GL_INVALID_ENUM); return; }
  current->active_texture=unit-GL_TEXTURE0;
}
void glBindTexture(GLenum target, GLuint name) {
  if(!current) return;
  if(target!=GL_TEXTURE_2D) { error(GL_INVALID_ENUM); return; }
  if(name) {
    PS4GLTexture *t=texture_name(current,name);
    if(!t) for(unsigned i=0;i<PS4GL_MAX_TEXTURES;++i) if(!current->textures[i].name) {
      t=&current->textures[i]; t->name=name; break;
    }
    if(!t) { error(GL_OUT_OF_MEMORY); return; }
    if(!t->created) {
      t->created=1; t->min_filter=GL_NEAREST_MIPMAP_LINEAR; t->mag_filter=GL_LINEAR;
      t->wrap_s=t->wrap_t=GL_REPEAT;
    }
  }
  current->texture_bindings[current->active_texture]=name;
}
GLboolean glIsTexture(GLuint name) {
  PS4GLTexture *t=current ? texture_name(current,name) : NULL; return t && t->created;
}
/* An unbound FBO retains the object, even after its name becomes reusable. */
static void collect_deleted_textures(PS4GLContext *ctx) {
  for(unsigned i=0;i<PS4GL_MAX_TEXTURES;++i) if(ctx->textures[i].deleted) {
    int attached=0;
    for(unsigned f=0;f<PS4GL_MAX_FRAMEBUFFERS;++f) if(ctx->framebuffers[f].texture_slot==i+1) attached=1;
    if(attached) continue;
    PS4GLTexture *t=&ctx->textures[i];
    ctx->texture_bytes-=(size_t)t->width*t->height*4;
    free(t->pixels); *t=(PS4GLTexture){0};
  }
}
void glDeleteTextures(GLsizei count, const GLuint *names) {
  if(!current) return;
  if(count<0 || (count && !names)) { error(GL_INVALID_VALUE); return; }
  for(GLsizei n=0;n<count;++n) {
    PS4GLTexture *t=texture_name(current,names[n]); if(!t) continue;
    unsigned slot=(unsigned)(t-current->textures)+1;
    for(unsigned f=0;f<PS4GL_MAX_FRAMEBUFFERS;++f) {
      PS4GLFramebuffer *fb=&current->framebuffers[f];
      if(fb->name==current->framebuffer && fb->texture_slot==slot) {
        end_target(); fb->texture=0; fb->texture_slot=0;
      }
    }
    for(unsigned u=0;u<PS4GL_TEXTURE_UNITS;++u) if(current->texture_bindings[u]==names[n]) current->texture_bindings[u]=0;
    t->deleted=GL_TRUE;
  }
  collect_deleted_textures(current);
}
void ps4glDestroyTextures(PS4GLContext *ctx) {
  if(!ctx) return;
  for(unsigned i=0;i<PS4GL_MAX_TEXTURES;++i) { free(ctx->textures[i].pixels); ctx->textures[i]=(PS4GLTexture){0}; }
  memset(ctx->texture_bindings,0,sizeof(ctx->texture_bindings)); ctx->texture_bytes=0;
}
static PS4GLTexture *bound_texture(GLenum target) {
  if(target!=GL_TEXTURE_2D) { error(GL_INVALID_ENUM); return NULL; }
  PS4GLTexture *t=texture_name(current,current->texture_bindings[current->active_texture]);
  if(!t) error(GL_INVALID_OPERATION);
  return t;
}
void glPixelStorei(GLenum pname, GLint value) {
  if(!current) return;
  if(pname!=GL_UNPACK_ALIGNMENT && pname!=GL_PACK_ALIGNMENT) { error(GL_INVALID_ENUM); return; }
  if(value!=1 && value!=2 && value!=4 && value!=8) { error(GL_INVALID_VALUE); return; }
  if(pname==GL_PACK_ALIGNMENT) current->pack_alignment=(unsigned)value;
  else current->unpack_alignment=(unsigned)value;
}
void glTexParameteri(GLenum target, GLenum pname, GLint value) {
  if(!current) return;
  PS4GLTexture *t=bound_texture(target); if(!t) return;
  if(pname==GL_TEXTURE_MIN_FILTER || pname==GL_TEXTURE_MAG_FILTER) {
    if(value!=GL_NEAREST && value!=GL_LINEAR && !(pname==GL_TEXTURE_MIN_FILTER && value==GL_NEAREST_MIPMAP_LINEAR)) {
      error(GL_INVALID_ENUM); return;
    }
    if(pname==GL_TEXTURE_MIN_FILTER) t->min_filter=value; else t->mag_filter=value;
  } else if(pname==GL_TEXTURE_WRAP_S || pname==GL_TEXTURE_WRAP_T) {
    if(value!=GL_REPEAT && value!=GL_CLAMP_TO_EDGE) { error(GL_INVALID_ENUM); return; }
    if(pname==GL_TEXTURE_WRAP_S) t->wrap_s=value; else t->wrap_t=value;
  } else error(GL_INVALID_ENUM);
}
/* CPU uploads are normalized to the immutable RGBA8 draw snapshot format. */
static unsigned texture_pixel_bytes(GLenum format, GLenum type) {
  if(format!=GL_RGB && format!=GL_RGBA) { error(GL_INVALID_ENUM); return 0; }
  if(type==GL_UNSIGNED_BYTE) return format==GL_RGB ? 3 : 4;
  if(type==GL_UNSIGNED_SHORT_5_6_5) {
    if(format==GL_RGB) return 2;
    error(GL_INVALID_OPERATION); return 0;
  }
  error(GL_INVALID_ENUM); return 0;
}
static void texture_copy(unsigned char *dst, size_t dst_stride, const void *pixels,
                         GLsizei width, GLsizei height, unsigned bpp, GLboolean rgb) {
  size_t row=(size_t)width*bpp;
  size_t stride=(row+current->unpack_alignment-1)&~(size_t)(current->unpack_alignment-1);
  if(bpp==4 && !rgb) {
    for(GLsizei y=0;y<height;++y)
      memcpy(dst+(size_t)y*dst_stride,(const unsigned char *)pixels+(size_t)y*stride,row);
    return;
  }
  for(GLsizei y=0;y<height;++y) for(GLsizei x=0;x<width;++x) {
    const unsigned char *src=(const unsigned char *)pixels+(size_t)y*stride+(size_t)x*bpp;
    unsigned char *out=dst+(size_t)y*dst_stride+(size_t)x*4;
    if(bpp==2) {
      uint16_t packed; memcpy(&packed,src,2); /* Unaligned client pointers are valid. */
      out[0]=(unsigned char)((((packed>>11)&31)*255+15)/31);
      out[1]=(unsigned char)((((packed>>5)&63)*255+31)/63);
      out[2]=(unsigned char)(((packed&31)*255+15)/31);
    } else memcpy(out,src,3);
    out[3]=rgb || bpp!=4 ? 255 : src[3];
  }
}
void glTexImage2D(GLenum target, GLint level, GLint internal, GLsizei width, GLsizei height,
                  GLint border, GLenum format, GLenum type, const void *pixels) {
  if(!current) return;
  PS4GLTexture *t=bound_texture(target); if(!t) return;
  if(level || border || width<0 || height<0 || width>PS4GL_TEXTURE_LIMIT || height>PS4GL_TEXTURE_LIMIT) { error(GL_INVALID_VALUE); return; }
  if(internal!=GL_RGB && internal!=GL_RGB8 && internal!=GL_RGBA && internal!=GL_RGBA8) { error(GL_INVALID_ENUM); return; }
  unsigned bpp=texture_pixel_bytes(format,type); if(!bpp) return;
  GLboolean rgb=internal==GL_RGB || internal==GL_RGB8;
  size_t bytes=(size_t)width*height*4, old=(size_t)t->width*t->height*4;
  if(bytes>PS4GL_TEXTURE_BUDGET-(current->texture_bytes-old)) { error(GL_OUT_OF_MEMORY); return; }
  if(current->next_texture_revision==UINT64_MAX) { error(GL_OUT_OF_MEMORY); return; }
  unsigned char *copy=bytes ? calloc(1,bytes) : NULL;
  if(bytes && !copy) { error(GL_OUT_OF_MEMORY); return; }
  if(pixels && bytes) texture_copy(copy,(size_t)width*4,pixels,width,height,bpp,rgb);
  else if(rgb) for(size_t i=3;i<bytes;i+=4) copy[i]=255;
  end_target(); t->render_target=0; t->revision=++current->next_texture_revision;
  free(t->pixels); t->cpu_contents_defined=pixels!=NULL; t->rgb_storage=rgb; t->pixels=copy; t->width=width; t->height=height; current->texture_bytes+=bytes; current->texture_bytes-=old;
}
void glTexSubImage2D(GLenum target, GLint level, GLint x, GLint y, GLsizei width, GLsizei height,
                     GLenum format, GLenum type, const void *pixels) {
  if(!current) return;
  PS4GLTexture *t=bound_texture(target); if(!t) return;
  unsigned bpp=texture_pixel_bytes(format,type); if(!bpp) return;
  if(level || x<0 || y<0 || width<0 || height<0 || x>t->width || y>t->height || width>t->width-x || height>t->height-y) { error(GL_INVALID_VALUE); return; }
  if(!width || !height) return;
  if(t->render_target) { error(GL_INVALID_OPERATION); return; }
  if(!pixels || !t->pixels) { error(GL_INVALID_OPERATION); return; }
  if(current->next_texture_revision==UINT64_MAX) { error(GL_OUT_OF_MEMORY); return; }
  t->revision=++current->next_texture_revision; t->cpu_contents_defined=GL_TRUE;
  texture_copy(t->pixels+((size_t)y*t->width+x)*4,(size_t)t->width*4,pixels,width,height,bpp,t->rgb_storage);
}

static PS4GLFramebuffer *framebuffer_name(PS4GLContext *ctx, GLuint name) {
  if(!name) return NULL;
  for(unsigned i=0;i<PS4GL_MAX_FRAMEBUFFERS;++i) if(ctx->framebuffers[i].name==name) return &ctx->framebuffers[i];
  return NULL;
}
PS4GLTexture *ps4glFramebufferTextureObject(PS4GLContext *ctx) {
  if(!ctx || !ctx->framebuffer) return NULL;
  for(unsigned i=0;i<PS4GL_MAX_FRAMEBUFFERS;++i) if(ctx->framebuffers[i].name==ctx->framebuffer) {
    unsigned slot=ctx->framebuffers[i].texture_slot;
    return slot && slot<=PS4GL_MAX_TEXTURES ? &ctx->textures[slot-1] : NULL;
  }
  return NULL;
}
GLuint ps4glFramebufferTexture(const PS4GLContext *ctx) {
  if(!ctx || !ctx->framebuffer) return 0;
  for(unsigned i=0;i<PS4GL_MAX_FRAMEBUFFERS;++i) if(ctx->framebuffers[i].name==ctx->framebuffer) return ctx->framebuffers[i].texture;
  return 0;
}
void glGenFramebuffers(GLsizei count, GLuint *names) {
  if(!current) return;
  if(count<0 || (count && !names)) { error(GL_INVALID_VALUE); return; }
  unsigned available=0;
  for(unsigned i=0;i<PS4GL_MAX_FRAMEBUFFERS;++i) available+=!current->framebuffers[i].name;
  if((unsigned)count>available) { error(GL_OUT_OF_MEMORY); return; }
  for(GLsizei n=0;n<count;++n) {
    while(current->next_framebuffer && framebuffer_name(current,current->next_framebuffer)) ++current->next_framebuffer;
    if(!current->next_framebuffer) { error(GL_OUT_OF_MEMORY); return; }
    for(unsigned i=0;i<PS4GL_MAX_FRAMEBUFFERS;++i) if(!current->framebuffers[i].name) {
      names[n]=current->framebuffers[i].name=current->next_framebuffer++; break;
    }
  }
}
void glBindFramebuffer(GLenum target, GLuint name) {
  if(!current) return;
  if(target!=GL_FRAMEBUFFER) { error(GL_INVALID_ENUM); return; }
  if(name) {
    PS4GLFramebuffer *f=framebuffer_name(current,name);
    if(!f) for(unsigned i=0;i<PS4GL_MAX_FRAMEBUFFERS;++i) if(!current->framebuffers[i].name) {
      f=&current->framebuffers[i]; f->name=name; break;
    }
    if(!f) { error(GL_OUT_OF_MEMORY); return; }
    f->created=1;
  }
  if(current->framebuffer!=name) end_target();
  current->framebuffer=name;
}
GLboolean glIsFramebuffer(GLuint name) {
  PS4GLFramebuffer *f=current ? framebuffer_name(current,name) : NULL; return f && f->created;
}
void glDeleteFramebuffers(GLsizei count, const GLuint *names) {
  if(!current) return;
  if(count<0 || (count && !names)) { error(GL_INVALID_VALUE); return; }
  for(GLsizei n=0;n<count;++n) {
    PS4GLFramebuffer *f=framebuffer_name(current,names[n]); if(!f) continue;
    if(current->framebuffer==names[n]) { end_target(); current->framebuffer=0; }
    *f=(PS4GLFramebuffer){0};
  }
  collect_deleted_textures(current);
}
void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
  if(!current) return;
  if(target!=GL_FRAMEBUFFER || attachment!=GL_COLOR_ATTACHMENT0 || textarget!=GL_TEXTURE_2D) { error(GL_INVALID_ENUM); return; }
  if(level) { error(GL_INVALID_VALUE); return; }
  PS4GLFramebuffer *f=framebuffer_name(current,current->framebuffer);
  if(!f || (texture && !glIsTexture(texture))) { error(GL_INVALID_OPERATION); return; }
  PS4GLTexture *t=texture ? texture_name(current,texture) : NULL;
  unsigned slot=t ? (unsigned)(t-current->textures)+1 : 0;
  if(f->texture_slot!=slot) end_target();
  f->texture=texture; f->texture_slot=slot;
  collect_deleted_textures(current);
}
GLenum glCheckFramebufferStatus(GLenum target) {
  if(!current) return 0;
  if(target!=GL_FRAMEBUFFER) { error(GL_INVALID_ENUM); return 0; }
  if(!current->framebuffer) return GL_FRAMEBUFFER_COMPLETE;
  GLuint name=ps4glFramebufferTexture(current);
  if(!name) return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
  PS4GLTexture *t=ps4glFramebufferTextureObject(current);
  if(!t || !t->pixels || t->width<=0 || t->height<=0) return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
  /* RGB render targets need alpha swizzling; keep this upload-only for now. */
  return t->rgb_storage ? GL_FRAMEBUFFER_UNSUPPORTED : GL_FRAMEBUFFER_COMPLETE;
}
void ps4glDestroyFramebuffers(PS4GLContext *ctx) {
  if(!ctx) return;
  if(ctx->command) { if(!ctx->error) ctx->error=GL_INVALID_OPERATION; return; }
  memset(ctx->framebuffers,0,sizeof(ctx->framebuffers)); ctx->framebuffer=0;
  collect_deleted_textures(ctx);
}
