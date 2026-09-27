#include "ps4gl.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
/* Deliberately single-threaded: SDL/thread ownership is a later integration gate. */
static PS4GLContext *current;
static void trace_error(GLenum value, int line) {
  static unsigned char seen[2048]; if(line>=0 && line<2048 && !seen[line]) {seen[line]=1;fprintf(stderr,"PS4GL error %x line %d\n",value,line);}
  if (current && !current->error) current->error = value;
}
#define error(v) trace_error(v,__LINE__)
int ps4glInit(PS4GLContext *ctx, GLint width, GLint height) {
  if (!ctx || width <= 0 || height <= 0) return 0;
  memset(ctx, 0, sizeof(*ctx));
  ctx->blend=(PS4GLBlend){.src_rgb=GL_ONE,.src_alpha=GL_ONE,.equation_rgb=GL_FUNC_ADD,.equation_alpha=GL_FUNC_ADD};
  ctx->depth_write_mask=GL_TRUE; ctx->raster=(PS4GLRaster)PS4GL_RASTER_DEFAULT; ctx->clear_depth=1.f;
  ctx->fixed.color[0]=ctx->fixed.color[1]=ctx->fixed.color[2]=ctx->fixed.color[3]=1.f;
  ctx->fixed.alpha_func=GL_ALWAYS; ctx->fixed.cull_face=GL_BACK; ctx->fixed.front_face=GL_CCW;
  for(unsigned m=0;m<2;++m) for(unsigned i=0;i<4;++i) ctx->fixed.matrix[m][0][i*5]=1.f;
  for(unsigned i=0;i<PS4GL_TEXTURE_UNITS;++i) {
    ctx->fixed.env_mode[i]=ctx->fixed.env_rgb[i]=ctx->fixed.env_alpha[i]=GL_MODULATE;
    for(unsigned a=0;a<3;++a) {
      ctx->fixed.env_source_rgb[i][a]=ctx->fixed.env_source_alpha[i][a]=a==0 ? GL_TEXTURE : a==1 ? GL_PREVIOUS : GL_CONSTANT;
      ctx->fixed.env_operand_rgb[i][a]=a==2 ? GL_SRC_ALPHA : GL_SRC_COLOR;
      ctx->fixed.env_operand_alpha[i][a]=GL_SRC_ALPHA;
    }
  }
  ctx->next_query=1; ctx->next_shader=65536;
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
/* Raster features that are still unavailable in the bounded backend.
 * Disabling these features is supported; enabling them must remain explicit
 * failure until the corresponding rendering behavior is implemented. */
static int fixed_disabled_cap(GLenum cap) {
  return cap==GL_CULL_FACE || cap==GL_DITHER;
}
void glEnable(GLenum cap) {
  if(current && (cap==GL_TEXTURE_2D || cap==GL_ALPHA_TEST)) {
    if(current->fixed.active) { error(GL_INVALID_OPERATION); return; }
    if(cap==GL_TEXTURE_2D) current->fixed.texture_enabled[current->active_texture]=1; else current->fixed.alpha_enabled=1; return;
  }
  if(current && cap==GL_DEPTH_TEST) { current->raster.depth_enabled=1; return; }
  if(current && cap==GL_STENCIL_TEST) { current->raster.stencil_enabled=1; return; }
  if(current && cap==GL_POLYGON_OFFSET_FILL) { current->polygon_offset_fill=1; return; }
  if(fixed_disabled_cap(cap)) { error(GL_INVALID_OPERATION); return; }
  if(cap!=GL_SCISSOR_TEST && cap!=GL_BLEND) { error(GL_INVALID_ENUM); return; }
  if(current) { if(cap==GL_BLEND) current->blend.enabled=1; else current->scissor_enabled=GL_TRUE; }
}
void glDisable(GLenum cap) {
  if(current && (cap==GL_TEXTURE_2D || cap==GL_ALPHA_TEST)) {
    if(current->fixed.active) { error(GL_INVALID_OPERATION); return; }
    if(cap==GL_TEXTURE_2D) current->fixed.texture_enabled[current->active_texture]=0; else current->fixed.alpha_enabled=0; return;
  }
  if(current && cap==GL_DEPTH_TEST) { current->raster.depth_enabled=0; return; }
  if(current && cap==GL_STENCIL_TEST) { current->raster.stencil_enabled=0; return; }
  if(current && cap==GL_POLYGON_OFFSET_FILL) { current->polygon_offset_fill=0; return; }
  if(fixed_disabled_cap(cap)) return;
  if(cap!=GL_SCISSOR_TEST && cap!=GL_BLEND) { error(GL_INVALID_ENUM); return; }
  if(current) { if(cap==GL_BLEND) current->blend.enabled=0; else current->scissor_enabled=GL_FALSE; }
}
GLboolean glIsEnabled(GLenum cap) {
  if(current && cap==GL_TEXTURE_2D) return current->fixed.texture_enabled[current->active_texture];
  if(current && cap==GL_ALPHA_TEST) return current->fixed.alpha_enabled;
  if(current && cap==GL_DEPTH_TEST) return current->raster.depth_enabled;
  if(current && cap==GL_STENCIL_TEST) return current->raster.stencil_enabled;
  if(current && cap==GL_POLYGON_OFFSET_FILL) return current->polygon_offset_fill;
  if(fixed_disabled_cap(cap)) return GL_FALSE;
  if(cap!=GL_SCISSOR_TEST && cap!=GL_BLEND) { error(GL_INVALID_ENUM); return GL_FALSE; }
  return current ? (cap==GL_BLEND ? current->blend.enabled : current->scissor_enabled) : GL_FALSE;
}
/* Depth writes are applied only when the active target owns a depth plane. */
void glDepthMask(GLboolean enabled) {
  if(current) { current->depth_write_mask=enabled ? GL_TRUE : GL_FALSE; current->raster.depth_write=!!enabled; }
}
void glColorMask(GLboolean r,GLboolean g,GLboolean b,GLboolean a) {
  if(current) current->raster.color_mask= (!!r) | (!!g<<1) | (!!b<<2) | (!!a<<3);
}
void glClearDepth(double value) { if(current) current->clear_depth=value>0 ? (value<1 ? (float)value : 1.f) : 0.f; }
void glClearStencil(GLint value) { if(current) current->clear_stencil=value; }
static int compare_func(GLenum f) { return f>=GL_NEVER && f<=GL_ALWAYS; }
void glDepthFunc(GLenum f) { if(!compare_func(f)) { error(GL_INVALID_ENUM); return; } if(current) current->raster.depth_func=f; }
static int stencil_face(GLenum face) { return face==GL_FRONT || face==GL_BACK || face==GL_FRONT_AND_BACK; }
void glStencilFuncSeparate(GLenum face,GLenum f,GLint ref,GLuint mask) {
  if(!stencil_face(face) || !compare_func(f)) { error(GL_INVALID_ENUM); return; }
  if(!current) return;
  unsigned reference=ref<0?0:ref>255?255:(unsigned)ref;
  if(face!=GL_BACK) { current->raster.stencil_func=f; current->raster.stencil_ref=reference; current->raster.stencil_mask=mask&255; }
  if(face!=GL_FRONT) { current->raster.back_stencil_func=f; current->raster.back_stencil_ref=reference; current->raster.back_stencil_mask=mask&255; }
}
void glStencilFunc(GLenum f,GLint ref,GLuint mask) { glStencilFuncSeparate(GL_FRONT_AND_BACK,f,ref,mask); }
static int stencil_op(GLenum op) { return op==GL_KEEP || op==GL_ZERO || op==GL_REPLACE || op==GL_INCR || op==GL_DECR || op==GL_INVERT || op==GL_INCR_WRAP || op==GL_DECR_WRAP; }
void glStencilOpSeparate(GLenum face,GLenum fail,GLenum depth_fail,GLenum pass) {
  if(!stencil_face(face) || !stencil_op(fail) || !stencil_op(depth_fail) || !stencil_op(pass)) { error(GL_INVALID_ENUM); return; }
  if(!current) return;
  if(face!=GL_BACK) { current->raster.stencil_fail=fail; current->raster.stencil_depth_fail=depth_fail; current->raster.stencil_pass=pass; }
  if(face!=GL_FRONT) { current->raster.back_stencil_fail=fail; current->raster.back_stencil_depth_fail=depth_fail; current->raster.back_stencil_pass=pass; }
}
void glStencilOp(GLenum fail,GLenum depth_fail,GLenum pass) { glStencilOpSeparate(GL_FRONT_AND_BACK,fail,depth_fail,pass); }
void glStencilMaskSeparate(GLenum face,GLuint mask) {
  if(!stencil_face(face)) { error(GL_INVALID_ENUM); return; }
  if(!current) return;
  if(face!=GL_BACK) current->raster.stencil_write_mask=mask&255;
  if(face!=GL_FRONT) current->raster.back_stencil_write_mask=mask&255;
}
void glStencilMask(GLuint mask) { glStencilMaskSeparate(GL_FRONT_AND_BACK,mask); }
void glGetBooleanv(GLenum pname, GLboolean *values) {
  if(!current) return;
  if(!values) { error(GL_INVALID_VALUE); return; }
  if(pname==GL_COLOR_WRITEMASK) { for(unsigned i=0;i<4;++i) values[i]=!!(current->raster.color_mask&(1u<<i)); }
  else if(pname==GL_DEPTH_WRITEMASK) *values=current->depth_write_mask;
  else error(GL_INVALID_ENUM);
}
void glGetIntegerv(GLenum pname, GLint *values) {
  if (!current) return;
  if (!values) { error(GL_INVALID_VALUE); return; }
  if (pname==GL_SCISSOR_BOX || pname==GL_VIEWPORT)
    memcpy(values,pname==GL_SCISSOR_BOX ? current->scissor : current->viewport,4*sizeof(GLint));
  else if(pname==GL_DEPTH_WRITEMASK) *values=current->depth_write_mask;
  else if(pname==GL_MATRIX_MODE) *values=current->fixed.matrix_mode ? GL_PROJECTION : GL_MODELVIEW;
  else if(pname==GL_FRAMEBUFFER_BINDING) *values=current->framebuffer;
  else if(pname==GL_ACTIVE_TEXTURE) *values=GL_TEXTURE0+current->active_texture;
  else if(pname==GL_TEXTURE_BINDING_CUBE_MAP) *values=current->cube_bindings[current->active_texture];
  else if(pname==GL_TEXTURE_BINDING_1D) *values=current->one_dimensional_bindings[current->active_texture];
  else if(pname==GL_TEXTURE_BINDING_RECTANGLE) *values=current->rectangle_bindings[current->active_texture];
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
  else if(pname==GL_MAX_TEXTURE_IMAGE_UNITS || pname==GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS) *values=PS4GL_MAX_SAMPLERS;
  else if(pname==GL_RED_BITS || pname==GL_GREEN_BITS || pname==GL_BLUE_BITS || pname==GL_ALPHA_BITS) *values=8;
  else if(pname==GL_DEPTH_BITS || pname==GL_STENCIL_BITS) *values=(!current->framebuffer && current->default_depth_stencil) ? (pname==GL_DEPTH_BITS ? 32 : 8) : 0;
  else if(pname==GL_STEREO || pname==GL_SAMPLE_BUFFERS || pname==GL_SAMPLES) *values=0;
  else if(pname==GL_DOUBLEBUFFER) *values=!current->framebuffer;
  else if(pname==GL_DEPTH_TEST) *values=current->raster.depth_enabled;
  else if(pname==GL_STENCIL_TEST) *values=current->raster.stencil_enabled;
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
  if(pname==GL_MODELVIEW_MATRIX || pname==GL_PROJECTION_MATRIX) {
    unsigned mode=pname==GL_PROJECTION_MATRIX; memcpy(values,current->fixed.matrix[mode][current->fixed.top[mode]],16*sizeof(*values));
  }
  else if (pname==GL_COLOR_CLEAR_VALUE) memcpy(values,current->clear,sizeof(current->clear));
  else error(GL_INVALID_ENUM);
}
void glClear(GLbitfield mask) {
  if (!current) return;
  if (mask & ~(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT)) {
    error(GL_INVALID_VALUE); return;
  }
  /* Offscreen color targets have no depth/stencil attachment; GL ignores
   * clear bits for absent buffers. The default display owns both planes. */
  if(!current->framebuffer && current->default_depth_stencil) {
    if(!current->depth_write_mask) mask&=~GL_DEPTH_BUFFER_BIT;
    if(!current->raster.stencil_write_mask) mask&=~GL_STENCIL_BUFFER_BIT;
    else if((mask&GL_STENCIL_BUFFER_BIT) && current->raster.stencil_write_mask!=255) { error(GL_INVALID_OPERATION); return; }
  } else mask&=~(GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
  if(!current->raster.color_mask) mask&=~GL_COLOR_BUFFER_BIT;
  else if((mask&GL_COLOR_BUFFER_BIT) && current->raster.color_mask!=15) { error(GL_INVALID_OPERATION); return; }
  if(!mask) return;
  if (!current->command && current->begin_frame && !current->begin_frame(current)) { error(GL_INVALID_OPERATION); return; }
  if (!current->command) { error(GL_INVALID_OPERATION); return; }
  /* A complete color clear overwrites every texel. A new offscreen target
   * can skip importing its CPU shadow only for this operation. */
  current->discard_target_contents=(mask&GL_COLOR_BUFFER_BIT) && !current->scissor_enabled;
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
  if(mask&GL_COLOR_BUFFER_BIT) vkCmdClearAttachments(current->command,1,&clear,1,&rect);
  clear.aspectMask=((mask&GL_DEPTH_BUFFER_BIT)?VK_IMAGE_ASPECT_DEPTH_BIT:0)|((mask&GL_STENCIL_BUFFER_BIT)?VK_IMAGE_ASPECT_STENCIL_BIT:0);
  clear.clearValue.depthStencil.depth=current->clear_depth;
  clear.clearValue.depthStencil.stencil=(unsigned)current->clear_stencil&255;
  if(clear.aspectMask) vkCmdClearAttachments(current->command,1,&clear,1,&rect);
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
  if((unsigned)n>free_slots) { fprintf(stderr,"PS4GL buffer record limit: requested=%d available=%u limit=%u\n",n,free_slots,(unsigned)PS4GL_MAX_BUFFERS); error(GL_OUT_OF_MEMORY); return; }
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
  if(!strncmp(uniform,"ps4gl_",6)) return -1;
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
  if(current->blend.enabled || current->managed_frame || current->default_depth_stencil || current->raster.color_mask!=15) {
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
  /* GL window coordinates have a bottom-left origin. Render-to-texture
   * already uses GL-oriented rows, whereas the swapchain uses top-left rows. */
  for(unsigned u=0;u<p->uniform_count;++u) if(!strcmp(p->uniforms[u].name,"ps4gl_fragcoord_y")) {
    const GLfloat transform[2]={current->active_target ? 1.f : -1.f,
                               current->active_target ? 0.f : (GLfloat)current->height};
    memcpy(p->uniform_data[1]+p->uniforms[u].offset[1],transform,sizeof(transform));
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
static void client_draw(GLenum, GLint, GLsizei, GLenum, const void *);
static int client_uses_arrays(void);
static void client_shader_draw(GLenum, GLint, GLsizei, GLenum, const unsigned char *);
void glDrawArrays(GLenum mode, GLint first, GLsizei count) {
  if(current && client_uses_arrays()) { client_draw(mode,first,count,0,NULL); return; }
  draw(mode,first,count,0,NULL);
}
void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices) {
  if(!current) return;
  if(type!=GL_UNSIGNED_SHORT && type!=GL_UNSIGNED_INT) { error(GL_INVALID_ENUM); return; }
  if(client_uses_arrays()) { client_draw(mode,0,count,type,indices); return; }
  draw(mode,0,count,type,indices);
}
#include "ps4gl-fixed.inc"
#include "ps4gl-client.inc"
#include "ps4gl-query.inc"

PS4GLProc ps4glGetProcAddress(const char *name) {
  if (!name) return NULL;
#define PROC(fn) if(!strcmp(name,#fn)) return (PS4GLProc)fn
  PROC(glGenQueries); PROC(glDeleteQueries); PROC(glIsQuery); PROC(glBeginQuery); PROC(glEndQuery); PROC(glGetQueryObjectiv); PROC(glGetQueryObjectuiv);
  PROC(glHint); PROC(glEnableClientState); PROC(glDisableClientState); PROC(glClientActiveTexture);
  if(!strcmp(name,"glClientActiveTextureARB")) return (PS4GLProc)glClientActiveTexture;
  PROC(glVertexPointer); PROC(glColorPointer); PROC(glNormalPointer); PROC(glTexCoordPointer); PROC(glOrtho);
  PROC(glCompressedTexImage2D);
  PROC(glMatrixMode); PROC(glLoadIdentity); PROC(glLoadMatrixf); PROC(glPushMatrix); PROC(glPopMatrix);
  PROC(glScalef); PROC(glTranslatef); PROC(glBegin); PROC(glEnd); PROC(glVertex3f); PROC(glColor3f); PROC(glColor4f);
  PROC(glMultiTexCoord3f);
  if(!strcmp(name,"glMultiTexCoord3fARB")) return (PS4GLProc)glMultiTexCoord3f;
  PROC(glTexCoord3f); PROC(glTexCoord2f); PROC(glAlphaFunc); PROC(glShadeModel); PROC(glCullFace); PROC(glFrontFace); PROC(glFlush);
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
  PROC(glTexEnvi); PROC(glTexEnvfv); PROC(glActiveTexture); PROC(glPixelStorei); PROC(glReadPixels); PROC(glTexParameteri); PROC(glCopyTexSubImage2D); PROC(glTexImage1D); PROC(glTexImage2D); PROC(glTexSubImage2D);
  PROC(glUniform1i); PROC(glUniform1iv);
  PROC(glColorMask); PROC(glClearDepth); PROC(glClearStencil); PROC(glDepthFunc); PROC(glStencilFunc); PROC(glStencilOp); PROC(glStencilMask); PROC(glStencilFuncSeparate); PROC(glStencilOpSeparate); PROC(glStencilMaskSeparate);
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
  if((unsigned)count>available) { fprintf(stderr,"PS4GL texture record limit: requested=%d available=%u bytes=%zu\n",count,available,current->texture_bytes); error(GL_OUT_OF_MEMORY); return; }
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
const PS4GLTexture *ps4glCubeAtUnit(PS4GLContext *ctx, unsigned unit) {
  return ctx && unit<PS4GL_TEXTURE_UNITS ? texture_name(ctx,ctx->cube_bindings[unit]) : NULL;
}
const PS4GLTexture *ps4glOneDimensionalAtUnit(PS4GLContext *ctx, unsigned unit) {
  return ctx && unit<PS4GL_TEXTURE_UNITS ? texture_name(ctx,ctx->one_dimensional_bindings[unit]) : NULL;
}
const PS4GLTexture *ps4glRectangleAtUnit(PS4GLContext *ctx, unsigned unit) {
  return ctx && unit<PS4GL_TEXTURE_UNITS ? texture_name(ctx,ctx->rectangle_bindings[unit]) : NULL;
}
void glBindTexture(GLenum target, GLuint name) {
  if(!current) return;
  if(target!=GL_TEXTURE_CUBE_MAP && target!=GL_TEXTURE_1D && target!=GL_TEXTURE_2D && target!=GL_TEXTURE_RECTANGLE) { error(GL_INVALID_ENUM); return; }
  if(name) {
    PS4GLTexture *t=texture_name(current,name);
    if(!t) for(unsigned i=0;i<PS4GL_MAX_TEXTURES;++i) if(!current->textures[i].name) {
      t=&current->textures[i]; t->name=name; break;
    }
    if(!t) { error(GL_OUT_OF_MEMORY); return; }
    if(t->created && t->target!=target) {error(GL_INVALID_OPERATION);return;}
    if(!t->created) {
      t->target=target;t->created=1; t->min_filter=GL_NEAREST_MIPMAP_LINEAR; t->mag_filter=GL_LINEAR;
      t->wrap_r=t->wrap_s=t->wrap_t=GL_REPEAT;
      if(target==GL_TEXTURE_RECTANGLE) {t->min_filter=GL_LINEAR;t->wrap_s=t->wrap_t=GL_CLAMP_TO_EDGE;}
    }
  }
  if(target==GL_TEXTURE_CUBE_MAP) current->cube_bindings[current->active_texture]=name;
  else if(target==GL_TEXTURE_1D) current->one_dimensional_bindings[current->active_texture]=name;
  else if(target==GL_TEXTURE_RECTANGLE) current->rectangle_bindings[current->active_texture]=name;
  else current->texture_bindings[current->active_texture]=name;
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
    for(unsigned u=0;u<PS4GL_TEXTURE_UNITS;++u) {
      if(current->texture_bindings[u]==names[n]) current->texture_bindings[u]=0;
      if(current->cube_bindings[u]==names[n]) current->cube_bindings[u]=0;
      if(current->one_dimensional_bindings[u]==names[n]) current->one_dimensional_bindings[u]=0;
      if(current->rectangle_bindings[u]==names[n]) current->rectangle_bindings[u]=0;
    }
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
  if(target!=GL_TEXTURE_CUBE_MAP && target!=GL_TEXTURE_1D && target!=GL_TEXTURE_2D && target!=GL_TEXTURE_RECTANGLE) { error(GL_INVALID_ENUM); return NULL; }
  PS4GLTexture *t=texture_name(current,target==GL_TEXTURE_CUBE_MAP ? current->cube_bindings[current->active_texture] : target==GL_TEXTURE_1D ? current->one_dimensional_bindings[current->active_texture] : target==GL_TEXTURE_RECTANGLE ? current->rectangle_bindings[current->active_texture] : current->texture_bindings[current->active_texture]);
  if(!t) error(GL_INVALID_OPERATION);
  return t;
}
void glCopyTexSubImage2D(GLenum target, GLint level, GLint dx, GLint dy, GLint x, GLint y, GLsizei width, GLsizei height) {
  if(!fixed_outside()) return;
  if(target!=GL_TEXTURE_2D && target!=GL_TEXTURE_RECTANGLE) {error(GL_INVALID_ENUM);return;}
  PS4GLTexture *t=bound_texture(target);if(!t) return;
  if(level || dx<0 || dy<0 || x<0 || y<0 || width<0 || height<0 || dx>t->width || dy>t->height || width>t->width-dx || height>t->height-dy) {error(GL_INVALID_VALUE);return;}
  if(!width || !height) return;
  if(!current->copy_texture || (t->internal_format!=GL_RGBA && t->internal_format!=GL_RGBA8) || t==ps4glFramebufferTextureObject(current)) {error(GL_INVALID_OPERATION);return;}
  if(!current->command && current->begin_frame && !current->begin_frame(current)) {error(GL_INVALID_OPERATION);return;}
  if(!current->command || !current->copy_texture(current,t,dx,dy,x,y,width,height)) {if(!current->error) error(GL_INVALID_OPERATION);}
}
void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *data) {
  if(!current) return;
  if(format!=GL_RGBA || type!=GL_UNSIGNED_BYTE) { error(GL_INVALID_ENUM); return; }
  if(x<0 || y<0 || width<0 || height<0) { error(GL_INVALID_VALUE); return; }
  if(!width || !height) return;
  if(!data || !current->read_pixels || !current->framebuffer || current->command) { error(GL_INVALID_OPERATION); return; }
  if(!current->read_pixels(current,x,y,width,height,data) && !current->error) error(GL_INVALID_OPERATION);
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
  if(target==GL_TEXTURE_RECTANGLE && ((pname==GL_TEXTURE_MIN_FILTER && value!=GL_NEAREST && value!=GL_LINEAR) || ((pname==GL_TEXTURE_WRAP_S || pname==GL_TEXTURE_WRAP_T) && value!=GL_CLAMP_TO_EDGE))) {error(GL_INVALID_ENUM);return;}
  if(pname==GL_TEXTURE_MIN_FILTER || pname==GL_TEXTURE_MAG_FILTER) {
    if(value!=GL_NEAREST && value!=GL_LINEAR && !(pname==GL_TEXTURE_MIN_FILTER && value==GL_NEAREST_MIPMAP_LINEAR)) {
      error(GL_INVALID_ENUM); return;
    }
    if(pname==GL_TEXTURE_MIN_FILTER) t->min_filter=value; else t->mag_filter=value;
  } else if(pname==GL_TEXTURE_WRAP_S || pname==GL_TEXTURE_WRAP_T || pname==GL_TEXTURE_WRAP_R) {
    if(value!=GL_REPEAT && value!=GL_CLAMP_TO_EDGE) { error(GL_INVALID_ENUM); return; }
    if(pname==GL_TEXTURE_WRAP_S) t->wrap_s=value; else if(pname==GL_TEXTURE_WRAP_T) t->wrap_t=value; else t->wrap_r=value;
  } else error(GL_INVALID_ENUM);
}
/* CPU uploads are normalized to the immutable RGBA8 draw snapshot format. */
static unsigned texture_pixel_bytes(GLenum format, GLenum type) {
  if(format!=GL_BGR && format!=GL_BGRA && format!=GL_RED && format!=GL_RG && format!=GL_RGB && format!=GL_RGBA && format!=GL_ALPHA && format!=GL_LUMINANCE && format!=GL_LUMINANCE_ALPHA) { error(GL_INVALID_ENUM); return 0; }
  if(type==GL_UNSIGNED_BYTE) return (format==GL_RGB || format==GL_BGR) ? 3 : (format==GL_RGBA || format==GL_BGRA) ? 4 : (format==GL_LUMINANCE_ALPHA || format==GL_RG) ? 2 : 1;
  if(type==GL_UNSIGNED_SHORT_5_6_5) {
    if(format==GL_RGB) return 2;
    error(GL_INVALID_OPERATION); return 0;
  }
  error(GL_INVALID_ENUM); return 0;
}
static void texture_copy(unsigned char *dst, size_t dst_stride, const void *pixels,
                         GLsizei width, GLsizei height, unsigned bpp, GLenum format, GLenum internal) {
  GLboolean rgb=internal==GL_RGB || internal==GL_RGB8;
  size_t row=(size_t)width*bpp;
  size_t stride=(row+current->unpack_alignment-1)&~(size_t)(current->unpack_alignment-1);
  if(format==GL_RGBA && bpp==4 && (internal==GL_RGBA || internal==GL_RGBA8)) {
    for(GLsizei y=0;y<height;++y)
      memcpy(dst+(size_t)y*dst_stride,(const unsigned char *)pixels+(size_t)y*stride,row);
    return;
  }
  for(GLsizei y=0;y<height;++y) for(GLsizei x=0;x<width;++x) {
    const unsigned char *src=(const unsigned char *)pixels+(size_t)y*stride+(size_t)x*bpp;
    unsigned char *out=dst+(size_t)y*dst_stride+(size_t)x*4;
    if(format==GL_BGR || format==GL_BGRA) {out[0]=src[2];out[1]=src[1];out[2]=src[0];out[3]=format==GL_BGRA ? src[3] : 255;}
    else if(format==GL_RED || format==GL_RG) {out[0]=src[0];out[1]=format==GL_RG ? src[1] : 0;out[2]=0;out[3]=255;}
    else if(format==GL_ALPHA) { out[0]=out[1]=out[2]=0; out[3]=src[0]; }
    else if(format==GL_LUMINANCE || format==GL_LUMINANCE_ALPHA) {
      out[0]=out[1]=out[2]=src[0]; out[3]=format==GL_LUMINANCE_ALPHA ? src[1] : 255;
    } else if(bpp==2) {
      uint16_t packed; memcpy(&packed,src,2); /* Unaligned client pointers are valid. */
      out[0]=(unsigned char)((((packed>>11)&31)*255+15)/31);
      out[1]=(unsigned char)((((packed>>5)&63)*255+31)/63);
      out[2]=(unsigned char)(((packed&31)*255+15)/31);
    } else memcpy(out,src,3);
    if(format==GL_RGB || format==GL_RGBA) out[3]=bpp==4 ? src[3] : 255;
    if(rgb) out[3]=255;
    if(internal==GL_RED || internal==GL_RG) {if(internal==GL_RED) out[1]=0;out[2]=0;out[3]=255;}
    if(internal==GL_ALPHA) out[0]=out[1]=out[2]=0;
    if(internal==GL_LUMINANCE || internal==GL_LUMINANCE_ALPHA) out[1]=out[2]=out[0];
    if(internal==GL_LUMINANCE) out[3]=255;
  }
}
static void texture_image(GLenum target, GLint level, GLint internal, GLsizei width, GLsizei height,
                  GLint border, GLenum format, GLenum type, const void *pixels) {
  if(!current) return;
  int face=target>=GL_TEXTURE_CUBE_MAP_POSITIVE_X && target<GL_TEXTURE_CUBE_MAP_POSITIVE_X+6 ? (int)(target-GL_TEXTURE_CUBE_MAP_POSITIVE_X) : -1;
  PS4GLTexture *t=bound_texture(face>=0 ? GL_TEXTURE_CUBE_MAP : target); if(!t) return;
  if(level || border || width<0 || height<0 || width>PS4GL_TEXTURE_LIMIT || height>PS4GL_TEXTURE_LIMIT) { error(GL_INVALID_VALUE); return; }
  if(internal!=GL_RED && internal!=GL_RG && internal!=GL_RGB && internal!=GL_RGB8 && internal!=GL_RGBA && internal!=GL_RGBA8 && internal!=GL_ALPHA && internal!=GL_LUMINANCE && internal!=GL_LUMINANCE_ALPHA) { error(GL_INVALID_ENUM); return; }
  if(face>=0 && (width<=0 || width!=height || height>PS4GL_TEXTURE_LIMIT/6 || (t->pixels && (t->width!=width || t->internal_format!=(GLenum)internal)))) {error(GL_INVALID_VALUE);return;}
  unsigned bpp=texture_pixel_bytes(format,type); if(!bpp) return;
  GLboolean rgb=internal==GL_RGB || internal==GL_RGB8;
  GLsizei storage_height=face>=0 ? height*6 : height;
  size_t bytes=(size_t)width*storage_height*4, old=(size_t)t->width*t->height*4;
  if(bytes>PS4GL_TEXTURE_BUDGET-(current->texture_bytes-old)) { fprintf(stderr,"PS4GL texture budget: texture=%u size=%dx%d requested=%zu resident=%zu limit=%u\n",t->name,width,height,bytes,current->texture_bytes,(unsigned)PS4GL_TEXTURE_BUDGET); error(GL_OUT_OF_MEMORY); return; }
  if(current->next_texture_revision==UINT64_MAX) { error(GL_OUT_OF_MEMORY); return; }
  unsigned char *copy=bytes ? calloc(1,bytes) : NULL;
  if(bytes && !copy) { error(GL_OUT_OF_MEMORY); return; }
  if(face>=0 && t->pixels) memcpy(copy,t->pixels,bytes);
  unsigned char *destination=copy ? copy+(face>=0 ? (size_t)face*width*height*4 : 0) : NULL;
  if(pixels && bytes) texture_copy(destination,(size_t)width*4,pixels,width,height,bpp,format,internal);
  else if(rgb) for(size_t i=3;i<(size_t)width*height*4;i+=4) destination[i]=255;
  end_target(); t->render_target=0; t->revision=++current->next_texture_revision;
  free(t->pixels); t->cpu_contents_defined=pixels!=NULL; t->rgb_storage=rgb; t->internal_format=internal; t->pixels=copy; t->width=width; t->height=storage_height;if(face>=0)t->cube_faces|=1u<<face; current->texture_bytes+=bytes; current->texture_bytes-=old;
}
void glTexImage1D(GLenum target, GLint level, GLint internal, GLsizei width, GLint border, GLenum format, GLenum type, const void *pixels) {
  if(target!=GL_TEXTURE_1D) {error(GL_INVALID_ENUM);return;}
  texture_image(target,level,internal,width,1,border,format,type,pixels);
}
void glTexImage2D(GLenum target, GLint level, GLint internal, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels) {
  if(target==GL_TEXTURE_1D || target==GL_TEXTURE_CUBE_MAP) {error(GL_INVALID_ENUM);return;}
  texture_image(target,level,internal,width,height,border,format,type,pixels);
}
void glTexSubImage2D(GLenum target, GLint level, GLint x, GLint y, GLsizei width, GLsizei height,
                     GLenum format, GLenum type, const void *pixels) {
  if(!current) return;
  if(target!=GL_TEXTURE_2D && target!=GL_TEXTURE_RECTANGLE) { error(GL_INVALID_ENUM); return; }
  PS4GLTexture *t=bound_texture(target); if(!t) return;
  unsigned bpp=texture_pixel_bytes(format,type); if(!bpp) return;
  if(level || x<0 || y<0 || width<0 || height<0 || x>t->width || y>t->height || width>t->width-x || height>t->height-y) { error(GL_INVALID_VALUE); return; }
  if(!width || !height) return;
  if(t->render_target) { error(GL_INVALID_OPERATION); return; }
  if(!pixels || !t->pixels) { error(GL_INVALID_OPERATION); return; }
  if(current->next_texture_revision==UINT64_MAX) { error(GL_OUT_OF_MEMORY); return; }
  t->revision=++current->next_texture_revision; t->cpu_contents_defined=GL_TRUE;
  texture_copy(t->pixels+((size_t)y*t->width+x)*4,(size_t)t->width*4,pixels,width,height,bpp,format,t->internal_format);
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
  if((unsigned)count>available) { fprintf(stderr,"PS4GL framebuffer record limit: requested=%d available=%u bytes=%zu\n",count,available,current->texture_bytes); error(GL_OUT_OF_MEMORY); return; }
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

#include "ps4gl-s3tc.inc"
