/* Host-side state/coordinate checks; native probe verifies resulting pixels. */
#include "ps4gl.h"
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned calls;
static VkClearRect last;
static VkClearAttachment color;
VKAPI_ATTR void VKAPI_CALL vkCmdClearAttachments(VkCommandBuffer cmd, uint32_t nc,
 const VkClearAttachment *c, uint32_t nr, const VkClearRect *r) {
  assert(cmd && nc==1 && nr==1); ++calls; last=*r; color=*c;
}
static unsigned draws, expected_vertices=3, expected_first;
static VkViewport viewport;
VKAPI_ATTR void VKAPI_CALL vkCmdBindPipeline(VkCommandBuffer cmd, VkPipelineBindPoint point, VkPipeline p) {
  assert(cmd && point==VK_PIPELINE_BIND_POINT_GRAPHICS && p);
}
VKAPI_ATTR void VKAPI_CALL vkCmdSetViewport(VkCommandBuffer cmd, uint32_t first, uint32_t count, const VkViewport *v) {
  assert(cmd && !first && count==1); viewport=*v;
}
VKAPI_ATTR void VKAPI_CALL vkCmdSetScissor(VkCommandBuffer cmd, uint32_t first, uint32_t count, const VkRect2D *r) {
  assert(cmd && !first && count==1); last.rect=*r;
}
VKAPI_ATTR void VKAPI_CALL vkCmdDraw(VkCommandBuffer cmd, uint32_t count, uint32_t instances, uint32_t first, uint32_t instance) {
  assert(cmd && count==expected_vertices && instances==1 && first==expected_first && !instance); ++draws;
}
static unsigned indexed_draws, uploads, expected_indices=3;
static unsigned stream_vertices;
static int check_stream;
static unsigned char snapshot[PS4GL_FIXED_VERTICES*sizeof(PS4GLFixedVertex)];
static size_t snapshot_size, snapshot_vertex_bytes;
static GLenum snapshot_type;
static int upload(PS4GLContext *ctx, const void *data, size_t size, size_t vb, GLenum type) {
  assert(ctx && size<=sizeof(snapshot)); ++uploads;
  if(check_stream) {
    assert(vb==size && type==0 && size%sizeof(PS4GLFixedVertex)==0);
    const PS4GLFixedVertex *v=data;
    for(size_t i=0;i<size/sizeof(*v);++i) {
      assert(v[i].position[0]==(float)stream_vertices++);
      assert(v[i].position[1]==0 && v[i].position[2]==0);
      assert(v[i].color[0]==.2f && v[i].color[1]==.3f && v[i].color[2]==.4f && v[i].color[3]==1.f);
      assert(v[i].uv[0]==3.f && v[i].uv[1]==.5f);
    }
  }
  memcpy(snapshot,data,size); snapshot_size=size; snapshot_vertex_bytes=vb; snapshot_type=type;
  return 1;
}
VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexed(VkCommandBuffer cmd, uint32_t count,
 uint32_t instances, uint32_t first, int32_t base, uint32_t instance) {
  assert(cmd && count==expected_indices && instances==1 && !first && !base && !instance); ++indexed_draws;
}
static int compile_source(int fragment, const char *source, uint32_t **out, size_t *bytes, char *log, size_t capacity) {
  (void)fragment; *out=NULL; *bytes=0;
  if(strstr(source,"bad")) { snprintf(log,capacity,"rejected source"); return 0; }
  *out=malloc(4); assert(*out); **out=1; *bytes=4; return 1;
}
static int link_sources(const char *vs, const char *fs, const PS4GLAttributeBinding *bindings, size_t binding_count, uint32_t **v, size_t *vb, uint32_t **f, size_t *fb, char *log, size_t cap) {
  assert(binding_count==1 && !strcmp(bindings[0].name,"position") && bindings[0].index==4);
  assert(!strcmp(vs,"vertex") && !strcmp(fs,"fragment"));
  return compile_source(0,vs,v,vb,log,cap) && compile_source(1,fs,f,fb,log,cap);
}
static GLuint import_program(VkDevice device, VkRenderPass pass, const uint32_t *vs, size_t vb, const uint32_t *fs, size_t fb) {
  assert(device && pass && vs && fs && vb==4 && fb==4);
  PS4GLContext *c=ps4glCurrentContext(); GLuint name=c->link_destination; assert(name);
  c->programs[name-1]=(PS4GLProgram){.pipeline=(VkPipeline)(uintptr_t)1};
  c->programs[name-1].components[4]=2; strcpy(c->programs[name-1].attribute_names[4],"position"); return name;
}
static unsigned lazy_calls;
static int lazy_fail;
static int begin_lazy(PS4GLContext *ctx) {
  assert(ctx==ps4glCurrentContext()); ++lazy_calls;
  if(lazy_fail) return 0;
  ps4glBeginPass((VkCommandBuffer)(uintptr_t)1); return 1;
}
static void check_rgb_uploads(void) {
  static PS4GLContext ctx; assert(ps4glInit(&ctx,80,60)); ps4glMakeCurrent(&ctx);
  GLuint name; glGenTextures(1,&name); glBindTexture(GL_TEXTURE_2D,name);
  const PS4GLTexture *t=ps4glTextureAtUnit(&ctx,0);
  for(unsigned align=1;align<=8;align*=2) for(unsigned width=1;width<=3;width+=2) {
    unsigned char storage[65]; memset(storage,0xee,sizeof(storage));
    unsigned char *src=storage+1; /* Deliberately unaligned, with poisoned padding. */
    size_t stride=(width*3+align-1)&~(size_t)(align-1);
    for(unsigned y=0;y<2;++y) for(unsigned x=0;x<width;++x) {
      src[y*stride+x*3]=11+x; src[y*stride+x*3+1]=25+y; src[y*stride+x*3+2]=71;
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT,align);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,2,0,GL_RGB,GL_UNSIGNED_BYTE,src);
    for(unsigned y=0;y<2;++y) for(unsigned x=0;x<width;++x) {
      const unsigned char expected[]={11+x,25+y,71,255};
      assert(!memcmp(t->pixels+(y*width+x)*4,expected,4));
    }
    stride=(width*2+align-1)&~(size_t)(align-1);
    memset(storage,0xee,sizeof(storage));
    for(unsigned y=0;y<2;++y) for(unsigned x=0;x<width;++x) {
      uint16_t packed=y ? 0x8410 : 0xf800; memcpy(src+y*stride+x*2,&packed,2);
    }
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,width,2,0,GL_RGB,GL_UNSIGNED_SHORT_5_6_5,src);
    for(unsigned x=0;x<width;++x) {
      const unsigned char red[]={255,0,0,255}, middle[]={132,130,132,255};
      assert(!memcmp(t->pixels+x*4,red,4));
      assert(!memcmp(t->pixels+(width+x)*4,middle,4));
    }
    assert(!glGetError() && ctx.texture_bytes==width*2*4);
  }
  glPixelStorei(GL_UNPACK_ALIGNMENT,4);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGB8,3,3,0,GL_RGB,GL_UNSIGNED_BYTE,NULL);
  unsigned char rgba[]={9,8,7,0};
  glTexSubImage2D(GL_TEXTURE_2D,0,1,1,1,1,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
  assert(t->pixels[16]==9 && t->pixels[19]==255 && t->pixels[15]==255 && !t->pixels[20]);
  uint16_t green=0x07e0;
  glTexSubImage2D(GL_TEXTURE_2D,0,2,2,1,1,GL_RGB,GL_UNSIGNED_SHORT_5_6_5,&green);
  const unsigned char expected_green[]={0,255,0,255}; assert(!memcmp(t->pixels+32,expected_green,4));
  unsigned char rows[]={31,32,33,99,41,42,43};
  glTexSubImage2D(GL_TEXTURE_2D,0,1,0,1,2,GL_RGB,GL_UNSIGNED_BYTE,rows);
  assert(t->pixels[4]==31 && t->pixels[16]==41 && t->pixels[7]==255 && t->pixels[19]==255);
  assert(!t->pixels[0] && !t->pixels[8] && !memcmp(t->pixels+32,expected_green,4));
  unsigned char before[36]; memcpy(before,t->pixels,36);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_SHORT_5_6_5,&green);
  assert(glGetError()==GL_INVALID_OPERATION && t->rgb_storage && ctx.texture_bytes==36);
  glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1,1,GL_RGB,GL_FLOAT,rgba);
  assert(glGetError()==GL_INVALID_ENUM && !memcmp(before,t->pixels,36));
  glTexSubImage2D(GL_TEXTURE_2D,0,3,3,0,0,GL_RGB,GL_UNSIGNED_BYTE,NULL);
  assert(!glGetError());
  GLuint fbo; glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_FRAMEBUFFER,fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,name,0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_UNSUPPORTED);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
  assert(!t->rgb_storage && t->pixels[3]==0 && ctx.texture_bytes==4);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
  glDeleteFramebuffers(1,&fbo); ps4glDestroyTextures(&ctx); ps4glMakeCurrent(NULL);
}
static void check_luminance_alpha(void) {
  static PS4GLContext c; assert(ps4glInit(&c,80,60)); ps4glMakeCurrent(&c);
  GLuint t; glGenTextures(1,&t); glBindTexture(GL_TEXTURE_2D,t);
  const unsigned char la[]={7,128,99,99,33,255,99,99};
  glTexImage2D(GL_TEXTURE_2D,0,GL_LUMINANCE_ALPHA,1,2,0,GL_LUMINANCE_ALPHA,GL_UNSIGNED_BYTE,la);
  const unsigned char expected[]={7,7,7,128,33,33,33,255};
  assert(!glGetError() && !memcmp(c.textures[0].pixels,expected,8));
  const unsigned char sub[]={64,192};
  glTexSubImage2D(GL_TEXTURE_2D,0,0,1,1,1,GL_LUMINANCE_ALPHA,GL_UNSIGNED_BYTE,sub);
  assert(!glGetError() && c.textures[0].pixels[4]==64 && c.textures[0].pixels[7]==192);
  glTexImage2D(GL_TEXTURE_2D,0,GL_ALPHA,1,1,0,GL_ALPHA,GL_UNSIGNED_BYTE,sub);
  assert(!glGetError() && !c.textures[0].pixels[0] && c.textures[0].pixels[3]==64);
  ps4glDestroyTextures(&c); ps4glMakeCurrent(NULL);
}
static void check_launcher_state(void) {
  static PS4GLContext a,b; assert(ps4glInit(&a,80,60) && ps4glInit(&b,80,60));
  ps4glMakeCurrent(&a);
  GLboolean mask=99;
  glGetBooleanv(GL_DEPTH_WRITEMASK,&mask); assert(mask==GL_TRUE);
  glDepthMask(GL_FALSE); glGetBooleanv(GL_DEPTH_WRITEMASK,&mask); assert(mask==GL_FALSE);
  ps4glMakeCurrent(&b); glGetBooleanv(GL_DEPTH_WRITEMASK,&mask); assert(mask==GL_TRUE);
  ps4glMakeCurrent(&a); glGetBooleanv(GL_DEPTH_WRITEMASK,&mask); assert(mask==GL_FALSE);
  glDepthMask(7); glGetBooleanv(GL_DEPTH_WRITEMASK,&mask); assert(mask==GL_TRUE);
  GLint depth=-1; glGetIntegerv(GL_DEPTH_WRITEMASK,&depth); assert(depth==1);
  glGetBooleanv(GL_DEPTH_WRITEMASK,NULL); assert(glGetError()==GL_INVALID_VALUE);
  mask=99; glGetBooleanv(0xffff,&mask); assert(mask==99 && glGetError()==GL_INVALID_ENUM);
  const GLenum caps[]={GL_CULL_FACE,GL_DITHER};
  glEnable(GL_BLEND); glEnable(GL_SCISSOR_TEST);
  for(unsigned i=0;i<2;++i) {
    GLint value=-1; glDisable(caps[i]); glDisable(caps[i]);
    assert(!glIsEnabled(caps[i])); glGetIntegerv(caps[i],&value);
    assert(!value && glGetError()==GL_NO_ERROR);
    glEnable(caps[i]); assert(glGetError()==GL_INVALID_OPERATION);
    assert(!glIsEnabled(caps[i]) && glIsEnabled(GL_BLEND) && glIsEnabled(GL_SCISSOR_TEST));
  }
  glEnable(GL_DEPTH_TEST); glEnable(GL_STENCIL_TEST);
  assert(glIsEnabled(GL_DEPTH_TEST) && glIsEnabled(GL_STENCIL_TEST));
  glDepthFunc(GL_GEQUAL); glStencilFunc(GL_EQUAL,-9,~0u); glStencilOp(GL_REPLACE,GL_KEEP,GL_INCR_WRAP);
  assert(a.raster.depth_func==GL_GEQUAL && a.raster.stencil_ref==0 && a.raster.stencil_mask==255);
  assert(a.raster.back_stencil_func==GL_EQUAL && a.raster.back_stencil_pass==GL_INCR_WRAP);
  glStencilFuncSeparate(GL_BACK,GL_ALWAYS,999,15); glStencilOpSeparate(GL_BACK,GL_KEEP,GL_DECR_WRAP,GL_KEEP);
  glStencilMaskSeparate(GL_BACK,7);
  assert(!glGetError() && a.raster.back_stencil_ref==255 && a.raster.back_stencil_mask==15 && a.raster.stencil_mask==255);
  assert(a.raster.back_stencil_depth_fail==GL_DECR_WRAP && a.raster.stencil_depth_fail==GL_KEEP && a.raster.stencil_pass==GL_INCR_WRAP);
  assert(a.raster.back_stencil_write_mask==7 && a.raster.stencil_write_mask==255);
  PS4GLRaster savedStencil=a.raster;
  glStencilFuncSeparate(0xffff,GL_ALWAYS,0,255); assert(glGetError()==GL_INVALID_ENUM && !memcmp(&savedStencil,&a.raster,sizeof(savedStencil)));
  glStencilOpSeparate(GL_BACK,GL_KEEP,0xffff,GL_KEEP); assert(glGetError()==GL_INVALID_ENUM && !memcmp(&savedStencil,&a.raster,sizeof(savedStencil)));
  glStencilMask(255); assert(a.raster.back_stencil_write_mask==255);
  assert(ps4glGetProcAddress("glStencilFuncSeparate") && ps4glGetProcAddress("glStencilOpSeparate"));
  PS4GLRaster state=a.raster; glStencilOp(GL_REPLACE,0xffff,GL_KEEP);
  assert(glGetError()==GL_INVALID_ENUM && !memcmp(&state,&a.raster,sizeof(state)));
  glColorMask(0,7,0,1); GLboolean rgba[4]; glGetBooleanv(GL_COLOR_WRITEMASK,rgba);
  assert(!rgba[0] && rgba[1] && !rgba[2] && rgba[3]); glColorMask(1,1,1,1);
  glDisable(GL_DEPTH_TEST); glDisable(GL_STENCIL_TEST);
  glDisable(0xffff); glEnable(GL_CULL_FACE);
  assert(glGetError()==GL_INVALID_ENUM && !glGetError());
  assert(!glIsEnabled(0xffff) && glGetError()==GL_INVALID_ENUM);
  GLint value=-1;
  glGetIntegerv(GL_PACK_ALIGNMENT,&value); assert(value==4);
  glGetIntegerv(GL_UNPACK_ALIGNMENT,&value); assert(value==4);
  glPixelStorei(GL_UNPACK_ALIGNMENT,2);
  for(unsigned align=1;align<=8;align*=2) {
    glPixelStorei(GL_PACK_ALIGNMENT,align);
    glGetIntegerv(GL_PACK_ALIGNMENT,&value); assert(value==(GLint)align);
    glGetIntegerv(GL_UNPACK_ALIGNMENT,&value); assert(value==2);
  }
  const GLint bad[]={-1,0,3,16};
  for(unsigned i=0;i<4;++i) {
    glPixelStorei(GL_PACK_ALIGNMENT,bad[i]); assert(glGetError()==GL_INVALID_VALUE);
    glGetIntegerv(GL_PACK_ALIGNMENT,&value); assert(value==8);
  }
  glPixelStorei(0xffff,4); assert(glGetError()==GL_INVALID_ENUM);
  ps4glMakeCurrent(&b);
  glGetIntegerv(GL_PACK_ALIGNMENT,&value); assert(value==4);
  glGetIntegerv(GL_UNPACK_ALIGNMENT,&value); assert(value==4);
  assert(!glIsEnabled(GL_BLEND) && !glIsEnabled(GL_SCISSOR_TEST));
  ps4glMakeCurrent(&a); assert(a.pack_alignment==8 && a.unpack_alignment==2 && !a.command);
  ps4glMakeCurrent(NULL);
}
static void check_large_textures(void) {
  static PS4GLContext ctx; assert(ps4glInit(&ctx,1280,720)); ps4glMakeCurrent(&ctx);
  enum { full_textures = PS4GL_TEXTURE_BUDGET / (2048u*2048u*4u) };
  GLuint ids[full_textures+1]; glGenTextures(full_textures+1,ids);
  for (unsigned i=0;i<full_textures;++i) {
    glBindTexture(GL_TEXTURE_2D,ids[i]);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2048,2048,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    assert(glGetError()==GL_NO_ERROR);
  }
  assert(ctx.texture_bytes==PS4GL_TEXTURE_BUDGET);
  glBindTexture(GL_TEXTURE_2D,ids[full_textures]);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  assert(glGetError()==GL_OUT_OF_MEMORY);
  glBindTexture(GL_TEXTURE_2D,ids[0]);
  const PS4GLTexture *t=ps4glTextureAtUnit(&ctx,0); assert(t);
  void *before=t->pixels;
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2049,720,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  assert(glGetError()==GL_INVALID_VALUE && t->pixels==before && t->width==2048);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1280,720,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  assert(glGetError()==GL_NO_ERROR && t->width==1280 && t->height==720);
  const unsigned char edge[]={7,23,201,255};
  glTexSubImage2D(GL_TEXTURE_2D,0,1279,719,1,1,GL_RGBA,GL_UNSIGNED_BYTE,edge);
  assert(glGetError()==GL_NO_ERROR && !memcmp(t->pixels+(1280*720-1)*4,edge,4));
  glDeleteTextures(full_textures+1,ids); assert(ctx.texture_bytes==0);
  ps4glMakeCurrent(NULL);
}

static void check_texture_records(void) {
  static PS4GLContext ctx; assert(ps4glInit(&ctx,80,60)); ps4glMakeCurrent(&ctx);
  GLuint names[PS4GL_MAX_TEXTURES]; glGenTextures(PS4GL_MAX_TEXTURES,names);
  assert(glGetError()==GL_NO_ERROR && ctx.texture_bytes==0);
  const unsigned char pixel[]={4,9,21,255};
  for(unsigned i=0;i<PS4GL_MAX_TEXTURES;++i) {
    glBindTexture(GL_TEXTURE_2D,names[i]);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    assert(glGetError()==GL_NO_ERROR);
  }
  GLuint rejected=0xabcdef; glGenTextures(1,&rejected);
  assert(glGetError()==GL_OUT_OF_MEMORY && rejected==0xabcdef);
  assert(ctx.texture_bytes==4*PS4GL_MAX_TEXTURES);
  glDeleteTextures(PS4GL_MAX_TEXTURES,names); assert(ctx.texture_bytes==0);
  glGenTextures(1,&rejected); assert(glGetError()==GL_NO_ERROR && rejected!=0xabcdef);
  glDeleteTextures(1,&rejected); ps4glMakeCurrent(NULL);
}

static void check_s3tc(void) {
  static PS4GLContext ctx; assert(ps4glInit(&ctx,80,60)); ps4glMakeCurrent(&ctx);
  GLuint t; glGenTextures(1,&t); glBindTexture(GL_TEXTURE_2D,t);
  /* Red/green endpoints; every row covers all four palette entries. */
  unsigned char bc1[]={0,248,224,7,228,228,228,228};
  glCompressedTexImage2D(GL_TEXTURE_2D,0,0x83f1,4,4,0,8,bc1); assert(!glGetError());
  const unsigned char row[]={255,0,0,255,0,255,0,255,170,85,0,255,85,170,0,255};
  const PS4GLTexture *tex=ps4glTextureAtUnit(&ctx,0);
  for(unsigned y=0;y<4;++y) assert(!memcmp(tex->pixels+y*16,row,16));
  /* BC1 three-color mode: transparent black, or opaque black for RGB. */
  unsigned char black[]={0,0,255,255,255,255,255,255};
  for(unsigned fmt=0x83f0;fmt<=0x83f1;++fmt) {
    glCompressedTexImage2D(GL_TEXTURE_2D,0,fmt,3,1,0,8,black); assert(!glGetError());
    for(unsigned x=0;x<3;++x) assert(tex->pixels[x*4]==0 && tex->pixels[x*4+3]==(fmt==0x83f0 ? 255 : 0));
  }
  unsigned char bc2[16]={0x10,0x32,0x54,0x76,0x98,0xba,0xdc,0xfe,0,0,255,255,255,255,255,255};
  glCompressedTexImage2D(GL_TEXTURE_2D,0,0x83f2,4,4,0,16,bc2); assert(!glGetError());
  for(unsigned i=0;i<16;++i) assert(tex->pixels[i*4]==170 && tex->pixels[i*4+3]==i*17);
  for(unsigned mode=0;mode<2;++mode) {
    unsigned char bc3[16]={0}; bc3[0]=mode ? 0 : 255; bc3[1]=mode ? 255 : 0;
    uint64_t bits=0; for(unsigned i=0;i<16;++i) bits|=(uint64_t)(i%8)<<(i*3);
    for(unsigned i=0;i<6;++i) bc3[i+2]=bits>>(8*i);
    memcpy(bc3+8,bc1,8);
    glCompressedTexImage2D(GL_TEXTURE_2D,0,0x83f3,4,4,0,16,bc3); assert(!glGetError());
    const unsigned char high[]={255,0,218,182,145,109,72,36}, low[]={0,255,51,102,153,204,0,255};
    for(unsigned i=0;i<16;++i) assert(tex->pixels[i*4+3]==(mode ? low : high)[i%8]);
  }
  unsigned char old[64]; memcpy(old,tex->pixels,64);
  glCompressedTexImage2D(GL_TEXTURE_2D,0,0x83f3,4,4,0,15,bc2); assert(glGetError()==GL_INVALID_VALUE);
  assert(!memcmp(old,tex->pixels,64));
  glCompressedTexImage2D(GL_TEXTURE_2D,0,0x83f3,INT_MAX,4,0,16,bc2); assert(glGetError()==GL_INVALID_VALUE);
  ps4glDestroyTextures(&ctx);
  puts("PASS BC1/BC2/BC3 palettes, both alpha modes, small edges, exact lengths and atomic rejection");
}
static void check_fixed(void) {
  static PS4GLContext ctx;
  assert(ps4glInit(&ctx,80,60)); ps4glMakeCurrent(&ctx);
  float m[16]; GLint mode;
  glGetIntegerv(GL_MATRIX_MODE,&mode); assert(mode==GL_MODELVIEW);
  glScalef(2,3,4); glTranslatef(5,6,7); glGetFloatv(GL_MODELVIEW_MATRIX,m);
  assert(m[0]==2 && m[5]==3 && m[10]==4 && m[12]==10 && m[13]==18 && m[14]==28);
  glPushMatrix(); glLoadIdentity(); glPopMatrix(); glGetFloatv(GL_MODELVIEW_MATRIX,m); assert(m[12]==10);
  glPopMatrix(); assert(glGetError()==GL_STACK_UNDERFLOW);
  for(unsigned i=1;i<PS4GL_MATRIX_DEPTH;++i) glPushMatrix();
  glPushMatrix(); assert(glGetError()==GL_STACK_OVERFLOW);
  for(unsigned i=1;i<PS4GL_MATRIX_DEPTH;++i) glPopMatrix();
  glMatrixMode(GL_PROJECTION); glGetFloatv(GL_PROJECTION_MATRIX,m); assert(m[0]==1 && m[12]==0);
  glEnable(GL_TEXTURE_2D); assert(glIsEnabled(GL_TEXTURE_2D));
  glActiveTexture(GL_TEXTURE0+1); assert(!glIsEnabled(GL_TEXTURE_2D));
  glEnable(GL_TEXTURE_2D); glActiveTexture(GL_TEXTURE0); glDisable(GL_TEXTURE_2D);
  glActiveTexture(GL_TEXTURE0+1); assert(glIsEnabled(GL_TEXTURE_2D));
  glDisable(GL_TEXTURE_2D); glActiveTexture(GL_TEXTURE0);
  assert(ctx.fixed.env_mode[0]==GL_MODULATE && ctx.fixed.env_alpha[0]==GL_MODULATE);
  glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_COMBINE);
  glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_ALPHA,GL_REPLACE);
  assert(ctx.fixed.env_alpha[0]==GL_REPLACE && ctx.fixed.env_alpha[1]==GL_MODULATE);
  glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_RGB,GL_ADD);
  glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND1_RGB,GL_ONE_MINUS_SRC_ALPHA);
  assert(ctx.fixed.env_rgb[0]==GL_ADD && ctx.fixed.env_operand_rgb[0][1]==GL_ONE_MINUS_SRC_ALPHA);
  glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_ALPHA,0xdead);
  assert(glGetError()==GL_INVALID_ENUM && ctx.fixed.env_alpha[0]==GL_REPLACE);
  assert(ps4glGetProcAddress("glTexEnvfv"));
  GLfloat env[]={-.5f,.25f,.75f,2.f};
  glTexEnvfv(GL_TEXTURE_ENV,GL_TEXTURE_ENV_COLOR,env);
  assert(!glGetError() && ctx.fixed.env_color[0][0]==0 && ctx.fixed.env_color[0][1]==.25f && ctx.fixed.env_color[0][3]==1);
  assert(ctx.fixed.env_color[1][1]==0);
  glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE1_RGB,GL_CONSTANT);
  glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_ALPHA,GL_CONSTANT);
  assert(!glGetError() && ctx.fixed.env_source_rgb[0][1]==GL_CONSTANT && ctx.fixed.env_source_alpha[0][0]==GL_CONSTANT);
  glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE1_RGB,0xdead);
  assert(glGetError()==GL_INVALID_ENUM && ctx.fixed.env_source_rgb[0][1]==GL_CONSTANT);
  glTexEnvfv(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,env);
  assert(glGetError()==GL_INVALID_ENUM && ctx.fixed.env_color[0][1]==.25f);
  glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE1_RGB,GL_PREVIOUS);
  glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_ALPHA,GL_TEXTURE);
  glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
  glAlphaFunc(GL_LESS,2); assert(ctx.fixed.alpha_ref==1);
  glAlphaFunc(GL_GREATER,-1); assert(ctx.fixed.alpha_ref==0);
  ctx.fixed.programs[0]=1; ctx.programs[0].pipeline=(VkPipeline)(uintptr_t)1;
  ctx.programs[0].vertex_stride=sizeof(PS4GLFixedVertex);
  ctx.programs[0].components[0]=3; ctx.programs[0].components[1]=4; ctx.programs[0].components[2]=3;
  ctx.upload_vertices=upload; ps4glBeginPass((VkCommandBuffer)(uintptr_t)1);
  GLuint sentinel; glGenBuffers(1,&sentinel); glBindBuffer(GL_ARRAY_BUFFER,sentinel);
  glMultiTexCoord3f(GL_TEXTURE0+1,.2f,.4f,.6f);
  assert(ctx.fixed.uv[0]==0 && ctx.fixed.extra_uv[0][2]==.6f);
  glMultiTexCoord3f(GL_TEXTURE0+PS4GL_TEXTURE_UNITS,1,1,1); assert(glGetError()==GL_INVALID_ENUM);
  assert(ps4glGetProcAddress("glMultiTexCoord3fARB")==ps4glGetProcAddress("glMultiTexCoord3f"));
  expected_vertices=6; glBegin(GL_QUADS); glBegin(GL_TRIANGLES); assert(glGetError()==GL_INVALID_OPERATION);
  glLoadIdentity(); assert(glGetError()==GL_INVALID_OPERATION);
  for(int i=0;i<4;++i) { glColor4f(i,0,1,i*.25f); glTexCoord2f(i,.5f); glVertex3f(i,2,3); }
  glEnd(); assert(!glGetError());
  assert(ctx.program==0 && ctx.array_buffer==sentinel && !ctx.attribs[0].enabled);
  PS4GLFixedVertex v[6]; memcpy(v,snapshot,sizeof(v));
  const unsigned order[]={0,1,2,0,2,3};
  for(unsigned i=0;i<6;++i) assert(v[i].position[0]==order[i] && v[i].color[0]==order[i] && v[i].color[3]==order[i]*.25f && v[i].uv[0]==order[i]);
  glColor3f(.2f,.3f,.4f); assert(ctx.fixed.color[3]==1.f);
  assert(ps4glGetProcAddress("glColor4f"));
  unsigned before=draws; glBegin(GL_TRIANGLES); glVertex3f(0,0,0); glEnd(); assert(draws==before);
  check_stream=1; stream_vertices=0;
  expected_vertices=PS4GL_FIXED_VERTICES; glBegin(GL_TRIANGLES);
  for(unsigned i=0;i<PS4GL_FIXED_VERTICES*2;++i) glVertex3f(i,0,0);
  glEnd(); assert(draws==before+2 && !glGetError());
  assert(stream_vertices==PS4GL_FIXED_VERTICES*2);
  expected_vertices=3; glBegin(GL_TRIANGLES);
  for(unsigned i=0;i<3;++i) glVertex3f(stream_vertices+i,0,0);
  glEnd(); assert(draws==before+3 && stream_vertices==PS4GL_FIXED_VERTICES*2+3);
  check_stream=0;
  glEnd(); assert(glGetError()==GL_INVALID_OPERATION);
  /* Client indices must dereference the declared arrays, not be treated as GPU offsets. */
  GLfloat positions[]={10,11,12,20,21,22,30,31,32};
  GLuint indices[]={2,0,1};
  glBindBuffer(GL_ARRAY_BUFFER,0);glVertexPointer(3,GL_FLOAT,0,positions);glEnableClientState(0x8074);
  before=draws;expected_vertices=3;glDrawElements(GL_TRIANGLES,3,GL_UNSIGNED_INT,indices);
  assert(!glGetError() && draws==before+1);memcpy(v,snapshot,3*sizeof(*v));
  assert(v[0].position[0]==30 && v[1].position[0]==10 && v[2].position[0]==20);
  assert(ctx.fixed.color[0]==.2f && ctx.array_buffer==0);
  glBindBuffer(GL_ARRAY_BUFFER,sentinel);glBufferData(GL_ARRAY_BUFFER,sizeof(positions),positions,GL_STATIC_DRAW);
  glVertexPointer(3,GL_FLOAT,0,0);glBindBuffer(GL_ARRAY_BUFFER,0);
  indices[2]=3;before=draws;glDrawElements(GL_TRIANGLES,3,GL_UNSIGNED_INT,indices);
  assert(glGetError()==GL_INVALID_OPERATION && draws==before);
  indices[2]=1;glDrawElements(GL_TRIANGLES,3,GL_UNSIGNED_INT,indices);assert(!glGetError() && draws==before+1);
  strcpy(ctx.programs[0].attribute_names[0],"ps4gl_client_vertex");
  strcpy(ctx.programs[0].attribute_names[1],"ps4gl_client_color");
  strcpy(ctx.programs[0].attribute_names[2],"ps4gl_client_uv0");
  glUseProgram(1);before=draws;glDrawElements(GL_TRIANGLES,3,GL_UNSIGNED_INT,indices);
  assert(!glGetError() && draws==before+1 && ctx.program==1 && ctx.array_buffer==0 && !ctx.attribs[0].enabled);
  memcpy(v,snapshot,3*sizeof(*v));assert(v[0].position[0]==30 && v[1].position[0]==10 && v[2].position[0]==20);
  assert(v[0].color[0]==.2f && v[0].color[3]==1);
  indices[2]=3;before=draws;glDrawElements(GL_TRIANGLES,3,GL_UNSIGNED_INT,indices);
  assert(glGetError()==GL_INVALID_OPERATION && draws==before && ctx.program==1);
  strcpy(ctx.programs[0].attribute_names[3],"ps4gl_client_uv1");ctx.programs[0].components[3]=3;
  before=draws;glBegin(GL_TRIANGLES);
  for(unsigned i=0;i<3;++i) {glTexCoord3f(i,2,3);glVertex3f(50+i,0,0);}glEnd();
  assert(!glGetError() && draws==before+1 && ctx.program==1 && ctx.fixed.clients[0].buffer==sentinel);
  memcpy(v,snapshot,3*sizeof(*v));assert(v[0].position[0]==50 && v[2].position[0]==52 && v[1].uv[2]==3);
  assert(v[0].extra_uv[0][0]==.2f && v[2].extra_uv[0][2]==.6f);
  glUseProgram(0);
  glDisableClientState(0x8074);
  glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,80,60,0,-1,1);glGetFloatv(GL_PROJECTION_MATRIX,m);
  assert(m[0]==.025f && m[12]==-1 && m[13]==1 && m[10]==-1);
  glHint(0x0c50,0x1102);assert(!glGetError());
  puts("PASS legacy client CPU indices, captured VBO binding, atomic bounds rejection, current color preservation and orthographic projection");
  ps4glEndPass(); ps4glDestroyBuffers(&ctx); expected_vertices=3;
  puts("PASS fixed matrices, stacks, errors, independent projection, quad winding, bounded batches and programmable state restoration");
}
static void check_rectangle_textures(void) {
  static PS4GLContext ctx;assert(ps4glInit(&ctx,80,60));ps4glMakeCurrent(&ctx);
  GLuint names[2];glGenTextures(2,names);glBindTexture(GL_TEXTURE_2D,names[0]);glBindTexture(GL_TEXTURE_RECTANGLE,names[1]);
  GLint value;glGetIntegerv(GL_TEXTURE_BINDING_2D,&value);assert(value==(GLint)names[0]);
  glGetIntegerv(GL_TEXTURE_BINDING_RECTANGLE,&value);assert(value==(GLint)names[1]);
  glBindTexture(GL_TEXTURE_RECTANGLE,names[0]);assert(glGetError()==GL_INVALID_OPERATION);
  glTexImage2D(GL_TEXTURE_RECTANGLE,0,GL_RGBA,2,3,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  const PS4GLTexture *rect=ps4glRectangleAtUnit(&ctx,0);assert(rect && rect->width==2 && rect->height==3 && rect->min_filter==GL_LINEAR);
  assert(ps4glTextureAtUnit(&ctx,0)->width==0 && !glGetError());
  glTexParameteri(GL_TEXTURE_RECTANGLE,GL_TEXTURE_WRAP_S,GL_REPEAT);assert(glGetError()==GL_INVALID_ENUM && rect->wrap_s==GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_RECTANGLE,GL_TEXTURE_MIN_FILTER,GL_NEAREST_MIPMAP_LINEAR);assert(glGetError()==GL_INVALID_ENUM);
  glTexImage2D(GL_TEXTURE_RECTANGLE,1,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);assert(glGetError()==GL_INVALID_VALUE && rect->width==2);
  glDeleteTextures(2,names);assert(!ps4glRectangleAtUnit(&ctx,0) && !ps4glTextureAtUnit(&ctx,0));ps4glDestroyTextures(&ctx);
  GLuint line;glGenTextures(1,&line);glBindTexture(GL_TEXTURE_1D,line);
  const unsigned char ramp[]={0,32,128,255};glTexImage1D(GL_TEXTURE_1D,0,GL_LUMINANCE,4,0,GL_LUMINANCE,GL_UNSIGNED_BYTE,ramp);
  const PS4GLTexture *one=ps4glOneDimensionalAtUnit(&ctx,0);assert(one && one->width==4 && one->height==1 && one->pixels[8]==128 && one->pixels[11]==255);
  glGetIntegerv(GL_TEXTURE_BINDING_1D,&value);assert(value==(GLint)line && !ps4glTextureAtUnit(&ctx,0) && !glGetError());
  glTexImage2D(GL_TEXTURE_1D,0,GL_RGBA,2,3,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);assert(glGetError()==GL_INVALID_ENUM && one->width==4);
  glBindTexture(GL_TEXTURE_2D,line);assert(glGetError()==GL_INVALID_OPERATION);
  const unsigned char rg[]={255,128,128,64,0,32,255,0};
  glTexImage1D(GL_TEXTURE_1D,0,GL_RG,4,0,GL_RG,GL_UNSIGNED_BYTE,rg);
  assert(!glGetError() && one->pixels[0]==255 && one->pixels[1]==128 && one->pixels[2]==0 && one->pixels[3]==255 && one->pixels[13]==0);
  glDeleteTextures(1,&line);assert(!ps4glOneDimensionalAtUnit(&ctx,0));ps4glDestroyTextures(&ctx);
  GLuint cube;glGenTextures(1,&cube);glBindTexture(GL_TEXTURE_CUBE_MAP,cube);
  for(unsigned face=0;face<6;++face) {
    unsigned char color[16];for(unsigned i=0;i<4;++i){color[i*4]=face; color[i*4+1]=20+face;color[i*4+2]=40+face;color[i*4+3]=100+face;}
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,0,GL_RGBA,2,2,0,GL_BGRA,GL_UNSIGNED_BYTE,color);
    const PS4GLTexture *t=ps4glCubeAtUnit(&ctx,0);assert(!glGetError() && t && t->cube_faces==((1u<<(face+1))-1) && t->width==2 && t->height==12);
    for(unsigned i=0;i<=face;++i) assert(t->pixels[i*16]==40+i && t->pixels[i*16+2]==i && t->pixels[i*16+3]==100+i);
  }
  const PS4GLTexture *t=ps4glCubeAtUnit(&ctx,0);uint64_t revision=t->revision;
  glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X,0,GL_RGBA,3,2,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  assert(glGetError()==GL_INVALID_VALUE && t->revision==revision && t->cube_faces==63);
  glBindTexture(GL_TEXTURE_2D,cube);assert(glGetError()==GL_INVALID_OPERATION);
  glDeleteTextures(1,&cube);assert(!ps4glCubeAtUnit(&ctx,0));ps4glDestroyTextures(&ctx);
  puts("PASS rectangle target isolation, immutable target type, default filtering, non-mipmap bounds and deletion");
}
static unsigned copy_calls;
static int copy_probe(PS4GLContext *c,PS4GLTexture *t,GLint dx,GLint dy,GLint x,GLint y,GLsizei w,GLsizei h) {
  assert(c && t && !dx && !dy && x==2 && y==3 && w==4 && h==5);
  assert(t->width==4 && t->height==5 && t->internal_format==GL_RGBA8);
  ++copy_calls; return 1;
}
static void check_copy_image(void) {
  static PS4GLContext c; assert(ps4glInit(&c,80,60)); ps4glMakeCurrent(&c);
  c.copy_texture=copy_probe; c.command=(VkCommandBuffer)1;
  GLuint t; glGenTextures(1,&t); glBindTexture(GL_TEXTURE_2D,t);
  glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,3,4,5,0);
  assert(!glGetError() && copy_calls==1 && ps4glTextureAtUnit(&c,0)->width==4);
  uint64_t revision=ps4glTextureAtUnit(&c,0)->revision;
  glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,79,3,4,5,0); assert(glGetError()==GL_INVALID_VALUE);
  glCopyTexImage2D(GL_TEXTURE_2D,1,GL_RGBA8,2,3,4,5,0); assert(glGetError()==GL_INVALID_VALUE);
  glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,3,4,5,1); assert(glGetError()==GL_INVALID_VALUE);
  glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB,2,3,4,5,0); assert(glGetError()==GL_INVALID_ENUM);
  assert(copy_calls==1 && ps4glTextureAtUnit(&c,0)->revision==revision);
  assert(ps4glGetProcAddress("glCopyTexImage2D"));
  c.command=0; ps4glDestroyTextures(&c);
  puts("PASS framebuffer copy allocation, GPU dispatch, format/bounds rejection and unchanged storage on invalid calls");
}
int main(void) {
  check_rectangle_textures();
  check_texture_records();
  check_large_textures();
  check_luminance_alpha();
  check_launcher_state();
  static PS4GLContext a,b;
  assert(ps4glInit(&a,80,60) && ps4glInit(&b,20,10));
  assert(!ps4glInit(NULL,80,60) && !ps4glInit(&b,0,60));
  ps4glMakeCurrent(&a);
  assert(!strncmp((const char *)glGetString(GL_VERSION),"0.0 ",4));
  assert(!*glGetString(GL_EXTENSIONS));
  assert(!glGetString(0xdead) && glGetError()==GL_INVALID_ENUM);
  GLint capability=0; glGetIntegerv(GL_MAX_TEXTURE_SIZE,&capability); assert(capability==PS4GL_TEXTURE_LIMIT);
  glGetIntegerv(GL_MAX_VERTEX_ATTRIBS,&capability); assert(capability==PS4GL_MAX_ATTRIBS);
  assert(ps4glGetProcAddress("glGetString"));
  assert(ps4glGetProcAddress("glClear") && ps4glGetProcAddress("glDrawArrays") && ps4glGetProcAddress("glShaderSource"));
  glClear(GL_COLOR_BUFFER_BIT); assert(glGetError()==GL_INVALID_OPERATION);
  ps4glBeginPass((VkCommandBuffer)(uintptr_t)1);
  glViewport(1,2,3,4);
  glClearColor(-1,.25f,2,.5f); glClear(GL_COLOR_BUFFER_BIT);
  assert(calls==1 && last.rect.extent.width==80 && last.rect.extent.height==60);
  assert(color.clearValue.color.float32[0]==0 && color.clearValue.color.float32[2]==1);
  glEnable(GL_SCISSOR_TEST); glScissor(-5,55,10,20); glClear(GL_COLOR_BUFFER_BIT);
  assert(calls==2 && last.rect.offset.x==0 && last.rect.offset.y==0);
  assert(last.rect.extent.width==5 && last.rect.extent.height==5);
  glScissor(6,39,11,13); glClear(GL_COLOR_BUFFER_BIT);
  assert(calls==3 && last.rect.offset.x==6 && last.rect.offset.y==8);
  glScissor(0,0,-1,1); glEnable(0xffff);
  assert(glGetError()==GL_INVALID_VALUE && glGetError()==GL_NO_ERROR);
  GLint box[4]; glGetIntegerv(GL_SCISSOR_BOX,box);
  assert(box[0]==6 && box[1]==39 && box[2]==11 && box[3]==13);
  glScissor(INT_MAX,INT_MAX,INT_MAX,INT_MAX); glClear(GL_COLOR_BUFFER_BIT);
  glScissor(INT_MIN,INT_MIN,INT_MAX,INT_MAX); glClear(GL_COLOR_BUFFER_BIT);
  glScissor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
  assert(calls==3);
  glClear(1); assert(glGetError()==GL_INVALID_VALUE);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  assert(glGetError()==GL_NO_ERROR && calls==3);
  ps4glMakeCurrent(&b); assert(!glIsEnabled(GL_SCISSOR_TEST));
  GLfloat v[4]={1,1,1,1}; glGetFloatv(GL_COLOR_CLEAR_VALUE,v);
  assert(!v[0] && !v[3]);
  ps4glMakeCurrent(&a); assert(glIsEnabled(GL_SCISSOR_TEST));
  glDisable(GL_SCISSOR_TEST); glClear(GL_COLOR_BUFFER_BIT); assert(calls==4);
  a.programs[0].pipeline=(VkPipeline)(uintptr_t)5;
  a.programs[1].pipeline=(VkPipeline)(uintptr_t)6;
  glDrawArrays(GL_TRIANGLES,0,3); assert(glGetError()==GL_INVALID_OPERATION && !draws);
  glUseProgram(1); glViewport(10,15,30,20);
  glDrawArrays(GL_TRIANGLES,0,3); assert(draws==1 && viewport.y==25 && viewport.x==10);
  assert(last.rect.extent.width==80 && last.rect.extent.height==60);
  glEnable(GL_SCISSOR_TEST); glScissor(-2,50,12,20);
  glDrawArrays(GL_TRIANGLES,0,3); assert(draws==2 && last.rect.offset.y==0 && last.rect.extent.width==10);
  glDrawArrays(GL_TRIANGLES,-1,3); assert(glGetError()==GL_INVALID_VALUE && draws==2);
  glDrawArrays(GL_TRIANGLES,0,-1); assert(glGetError()==GL_INVALID_VALUE);
  glDrawArrays(0xffff,0,3); assert(glGetError()==GL_INVALID_ENUM);
  glDeleteProgram(1); assert(glIsProgram(1));
  GLint value; glGetProgramiv(1,GL_DELETE_STATUS,&value); assert(value);
  glDrawArrays(GL_TRIANGLES,0,3); assert(draws==3);
  glUseProgram(2); assert(!glIsProgram(1));
  glUseProgram(1); assert(glGetError()==GL_INVALID_VALUE);
  glGetIntegerv(GL_CURRENT_PROGRAM,&value); assert(value==2);
  glViewport(-1,0,80,60); glDrawArrays(GL_TRIANGLES,0,3);
  assert(glGetError()==GL_INVALID_OPERATION && draws==3);
  glDeleteProgram(2); glUseProgram(0); assert(!glIsProgram(2));
  ps4glEndPass(); glClear(GL_COLOR_BUFFER_BIT); assert(glGetError()==GL_INVALID_OPERATION);
  static PS4GLContext vertices_ctx;
  assert(ps4glInit(&vertices_ctx,80,60)); ps4glMakeCurrent(&vertices_ctx);
  GLuint buffers[2]; glGenBuffers(2,buffers);
  assert(!glIsBuffer(buffers[0]));
  float data[20]={99,99,99,99, 1,2, .2f,.3f,.4f,77, .6f,.7f,
                                  3,4, .4f,.5f,.6f,88, .8f,.9f};
  glBindBuffer(GL_ARRAY_BUFFER,buffers[0]); assert(glIsBuffer(buffers[0]));
  glBufferData(GL_ARRAY_BUFFER,sizeof(data),data,GL_STREAM_DRAW);
  glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,32,(const void *)16);
  glVertexAttribPointer(1,3,GL_FLOAT,GL_TRUE,32,(const void *)24);
  glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,32,(const void *)40);
  for(unsigned i=0;i<3;++i) glEnableVertexAttribArray(i);
  glBindBuffer(GL_ARRAY_BUFFER,buffers[1]);
  glBufferData(GL_ARRAY_BUFFER,sizeof(data),NULL,GL_STATIC_DRAW);
  PS4GLProgram attributes={.components={2,4,2},.vertex_stride=32};
  float packed[16];
  const GLfloat ignored_constant[]={9,8,7,6};
  glVertexAttrib4fv(1,ignored_constant); /* Enabled arrays override constants, including default W. */
  assert(ps4glPackVertices(&vertices_ctx,&attributes,2,packed));
  assert(packed[0]==1 && packed[1]==2 && packed[5]==1 && packed[6]==.6f && packed[8]==3 && packed[13]==1);
  float separate_uv[]={.11f,.22f,.33f,.44f};
  glBufferData(GL_ARRAY_BUFFER,sizeof(separate_uv),separate_uv,GL_STATIC_DRAW);
  glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,0,NULL);
  assert(ps4glPackVertices(&vertices_ctx,&attributes,2,packed));
  assert(packed[0]==1 && packed[6]==.11f && packed[14]==.33f);
  glBindBuffer(GL_ARRAY_BUFFER,buffers[0]);
  float replacement=9; glBufferSubData(GL_ARRAY_BUFFER,16,4,&replacement);
  assert(ps4glPackVertices(&vertices_ctx,&attributes,2,packed) && packed[0]==9);
  glBufferSubData(GL_ARRAY_BUFFER,PTRDIFF_MAX,1,&replacement); assert(glGetError()==GL_INVALID_VALUE);
  glBufferData(GL_ARRAY_BUFFER,-1,NULL,GL_STREAM_DRAW); assert(glGetError()==GL_INVALID_VALUE);
  glGetBufferParameteriv(GL_ARRAY_BUFFER,GL_BUFFER_SIZE,&value); assert(value==sizeof(data));
  glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,32,(const void *)UINTPTR_MAX);
  packed[0]=123;
  assert(!ps4glPackVertices(&vertices_ctx,&attributes,2,packed) && packed[0]==123);
  assert(glGetError()==GL_INVALID_OPERATION);
  glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,INT_MAX,(const void *)16);
  assert(!ps4glPackVertices(&vertices_ctx,&attributes,2,packed)); assert(glGetError()==GL_INVALID_OPERATION);
  glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,32,(const void *)16);
  glBufferData(GL_ARRAY_BUFFER,32,NULL,GL_STREAM_DRAW);
  assert(!ps4glPackVertices(&vertices_ctx,&attributes,2,packed)); assert(glGetError()==GL_INVALID_OPERATION);
  glDeleteBuffers(2,buffers); assert(!glIsBuffer(buffers[0]));
  assert(!ps4glPackVertices(&vertices_ctx,&attributes,2,packed)); assert(glGetError()==GL_INVALID_OPERATION);
  const GLfloat default_constant[]={0,0,0,1}; glVertexAttrib4fv(1,default_constant);
  for(unsigned i=0;i<3;++i) glDisableVertexAttribArray(i);
  assert(ps4glPackVertices(&vertices_ctx,&attributes,2,packed));
  assert(packed[0]==0 && packed[5]==1 && packed[6]==0);
  const GLfloat constant[]={.2f,.4f,.6f,.8f};
  glVertexAttrib4fv(1,constant);
  assert(ps4glPackVertices(&vertices_ctx,&attributes,2,packed));
  assert(!memcmp(packed+2,constant,sizeof(constant)) && !memcmp(packed+10,constant,sizeof(constant)));
  glVertexAttrib3fv(1,constant);
  assert(ps4glPackVertices(&vertices_ctx,&attributes,2,packed));
  assert(packed[4]==.6f && packed[5]==1);
  glVertexAttrib2fv(1,constant);
  glVertexAttrib4fv(PS4GL_MAX_ATTRIBS,constant); assert(glGetError()==GL_INVALID_VALUE);
  glVertexAttrib3fv(1,NULL); assert(glGetError()==GL_INVALID_VALUE);
  assert(ps4glPackVertices(&vertices_ctx,&attributes,2,packed));
  assert(packed[2]==.2f && packed[3]==.4f && packed[4]==0 && packed[5]==1);
  ps4glMakeCurrent(&a); assert(a.attribs[1].constant[0]==0 && a.attribs[1].constant[3]==1);
  ps4glMakeCurrent(&vertices_ctx);
  assert(ps4glGetProcAddress("glVertexAttrib2fv") && ps4glGetProcAddress("glVertexAttrib3fv") && ps4glGetProcAddress("glVertexAttrib4fv"));
  GLuint too_many[PS4GL_MAX_BUFFERS+1]; glGenBuffers(PS4GL_MAX_BUFFERS+1,too_many);
  assert(glGetError()==GL_OUT_OF_MEMORY);
  glBindBuffer(GL_ARRAY_BUFFER,1000); assert(glIsBuffer(1000));
  ps4glDestroyBuffers(&vertices_ctx);
  assert(vertices_ctx.attribs[1].constant[0]==0 && vertices_ctx.attribs[1].constant[3]==1);
  static PS4GLContext indexed;
  assert(ps4glInit(&indexed,80,60)); ps4glMakeCurrent(&indexed);
  indexed.programs[0]=(PS4GLProgram){.pipeline=(VkPipeline)(uintptr_t)5,
      .components={2},.vertex_stride=8};
  indexed.upload_vertices=upload;
  glUseProgram(1); ps4glBeginPass((VkCommandBuffer)(uintptr_t)1);
  glBindBuffer(GL_ARRAY_BUFFER,10);
  float positions[]={99,99, 1,2, 3,4, 5,6};
  glBufferData(GL_ARRAY_BUFFER,sizeof(positions),positions,GL_STATIC_DRAW);
  glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,NULL); glEnableVertexAttribArray(0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,11);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&value); assert(value==10);
  glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING,&value); assert(value==11);
  uint16_t ix16[]={65535,2,3,1};
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(ix16),ix16,GL_STREAM_DRAW);
  glDrawElements(GL_TRIANGLES,3,GL_UNSIGNED_SHORT,(void *)2);
  assert(!glGetError() && indexed_draws==1 && uploads==1);
  assert(snapshot_size==38 && snapshot_vertex_bytes==32 && snapshot_type==GL_UNSIGNED_SHORT);
  assert(!memcmp(snapshot,positions,32) && !memcmp(snapshot+32,ix16+1,6));
  uint32_t ix32[]={UINT32_MAX,3,1,2};
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(ix32),ix32,GL_DYNAMIC_DRAW);
  glDrawElements(GL_TRIANGLES,3,GL_UNSIGNED_INT,(void *)4);
  assert(!glGetError() && indexed_draws==2 && uploads==2);
  assert(snapshot_size==44 && snapshot_type==GL_UNSIGNED_INT && !memcmp(snapshot+32,ix32+1,12));
  glDrawElements(GL_TRIANGLES,3,GL_UNSIGNED_INT,(void *)UINTPTR_MAX);
  assert(glGetError()==GL_INVALID_OPERATION);
  glDrawElements(GL_TRIANGLES,4,GL_UNSIGNED_INT,(void *)4);
  assert(glGetError()==GL_INVALID_OPERATION);
  glDrawElements(GL_TRIANGLES,-1,GL_UNSIGNED_INT,NULL); assert(glGetError()==GL_INVALID_VALUE);
  glDrawElements(GL_TRIANGLES,3,GL_FLOAT,NULL); assert(glGetError()==GL_INVALID_ENUM);
  glDrawElements(GL_TRIANGLES,3,GL_UNSIGNED_INT,NULL); assert(glGetError()==GL_INVALID_OPERATION);
  uint32_t outside=4; glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,4,4,&outside);
  glDrawElements(GL_TRIANGLES,3,GL_UNSIGNED_INT,(void *)4); assert(glGetError()==GL_INVALID_OPERATION);
  outside=PS4GL_MAX_BUFFER_BYTES/8;
  glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,4,4,&outside);
  glDrawElements(GL_TRIANGLES,3,GL_UNSIGNED_INT,(void *)4); assert(glGetError()==GL_OUT_OF_MEMORY);
  glDrawElements(GL_TRIANGLES,0,GL_UNSIGNED_INT,NULL); assert(!glGetError());
  assert(uploads==2 && indexed_draws==2);
  GLuint deleted=11; glDeleteBuffers(1,&deleted);
  glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING,&value); assert(!value);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&value); assert(value==10);
  glDrawElements(GL_TRIANGLES,3,GL_UNSIGNED_SHORT,NULL); assert(glGetError()==GL_INVALID_OPERATION);
  assert(uploads==2 && indexed_draws==2);
  assert(ps4glGetProcAddress("glDrawElements"));
  ps4glEndPass(); ps4glDestroyBuffers(&indexed);
  static PS4GLContext uniforms;
  assert(ps4glInit(&uniforms,80,60)); ps4glMakeCurrent(&uniforms);
  PS4GLProgram *up=&uniforms.programs[0];
  up->pipeline=(VkPipeline)(uintptr_t)5; up->uniform_count=5;
  up->uniforms[0]=(PS4GLUniform){.name="gain",.width=1,.offset={64,16}};
  up->uniforms[1]=(PS4GLUniform){.name="tint",.width=4,.offset={-1,0}};
  up->uniforms[2]=(PS4GLUniform){.name="transform",.width=16,.offset={0,-1}};
  up->uniforms[3]=(PS4GLUniform){.name="offset",.width=2,.offset={80,-1}};
  up->uniforms[4]=(PS4GLUniform){.name="light",.width=3,.offset={96,-1}};
  glUseProgram(1);
  glUniform2f(3,7,8); glUniform3f(4,4,5,6);
  float pair[]={7,8}, triple[]={4,5,6};
  assert(!memcmp(up->uniform_data[0]+80,pair,8) && !memcmp(up->uniform_data[0]+96,triple,12));
  assert(glGetUniformLocation(1,"gain")==0 && glGetUniformLocation(1,"transform")==2);
  assert(glGetUniformLocation(1,"absent")==-1 && glGetError()==GL_NO_ERROR);
  glUniform1f(0,.5f); float captured;
  memcpy(&captured,up->uniform_data[0]+64,4); assert(captured==.5f);
  memcpy(&captured,up->uniform_data[1]+16,4); assert(captured==.5f);
  glUniform4f(1,1,2,3,4);
  float tint_values[]={1,2,3,4}; assert(!memcmp(up->uniform_data[1],tint_values,16));
  float matrix_values[16]; for(unsigned i=0;i<16;++i) matrix_values[i]=(float)i;
  glUniformMatrix4fv(2,1,GL_TRUE,matrix_values);
  float matrix_copy[16]; memcpy(matrix_copy,up->uniform_data[0],64);
  for(unsigned c=0;c<4;++c) for(unsigned r=0;r<4;++r) assert(matrix_copy[c*4+r]==matrix_values[r*4+c]);
  glUniformMatrix4fv(2,1,GL_FALSE,matrix_values); assert(!memcmp(up->uniform_data[0],matrix_values,64));
  glUniform1f(1,9); assert(glGetError()==GL_INVALID_OPERATION);
  glUniform4fv(1,2,matrix_values); assert(glGetError()==GL_INVALID_OPERATION);
  glUniform4fv(1,-1,matrix_values); assert(glGetError()==GL_INVALID_VALUE);
  glUniform4fv(1,1,NULL); assert(glGetError()==GL_INVALID_VALUE);
  glUniform4fv(1,0,NULL); glUniform4fv(-1,1,NULL); assert(!glGetError());
  glUniform1f(9,1); assert(glGetError()==GL_INVALID_OPERATION);
  assert(!memcmp(up->uniform_data[1],tint_values,16));
  uniforms.programs[1]=*up; glUseProgram(2); glUniform1f(0,7);
  memcpy(&captured,up->uniform_data[0]+64,4); assert(captured==.5f);
  glUseProgram(0); glUniform1f(0,1); assert(glGetError()==GL_INVALID_OPERATION);
  glGetUniformLocation(0,"gain"); assert(glGetError()==GL_INVALID_VALUE);
  assert(ps4glGetProcAddress("glUniformMatrix4fv") && ps4glGetProcAddress("glUniform4f"));
  static PS4GLContext arrays;
  assert(ps4glInit(&arrays,80,60)); ps4glMakeCurrent(&arrays);
  PS4GLProgram *ap=&arrays.programs[0]; ap->pipeline=(VkPipeline)(uintptr_t)1; ap->uniform_count=3;
  ap->uniforms[0]=(PS4GLUniform){.name="RandomValue",.width=1,.offset={0,16},.array_length=5,.array_stride={16,32}};
  ap->uniforms[1]=(PS4GLUniform){.name="matrix",.width=16,.offset={80,-1},.array_length=2,.array_stride={64,0}};
  ap->uniforms[2]=(PS4GLUniform){.name="color",.width=3,.offset={-1,192},.array_length=2,.array_stride={0,16}};
  glUseProgram(1);
  GLint base=glGetUniformLocation(1,"RandomValue");
  assert(base==glGetUniformLocation(1,"RandomValue[0]"));
  const char *bad_names[]={"RandomValue[5]","RandomValue[-1]","RandomValue[]","RandomValue[1]x","RandomValue[9999999999999999999999999]","RandomValue[1][0]"};
  for(unsigned n=0;n<sizeof(bad_names)/sizeof(*bad_names);++n) assert(glGetUniformLocation(1,bad_names[n])==-1);
  float initial[]={1,2,3,4,5}; glUniform1fv(base,5,initial);
  for(unsigned j=0;j<5;++j) {
    memcpy(&captured,ap->uniform_data[0]+16*j,4); assert(captured==initial[j]);
    memcpy(&captured,ap->uniform_data[1]+16+32*j,4); assert(captured==initial[j]);
  }
  float tail[]={8,9}; glUniform1fv(glGetUniformLocation(1,"RandomValue[3]"),2,tail);
  memcpy(&captured,ap->uniform_data[0]+32,4); assert(captured==3);
  memcpy(&captured,ap->uniform_data[0]+48,4); assert(captured==8);
  memcpy(&captured,ap->uniform_data[0]+64,4); assert(captured==9);
  /* Excess count must be truncated before reading past the supplied final value. */
  float last_value=7; glUniform1fv(glGetUniformLocation(1,"RandomValue[4]"),INT_MAX,&last_value);
  memcpy(&captured,ap->uniform_data[1]+144,4); assert(captured==7 && !glGetError());
  float matrices[32]; for(unsigned j=0;j<32;++j) matrices[j]=(float)j;
  glUniformMatrix4fv(glGetUniformLocation(1,"matrix"),2,GL_TRUE,matrices);
  for(unsigned j=0;j<2;++j) for(unsigned c=0;c<4;++c) for(unsigned r=0;r<4;++r) {
    memcpy(&captured,ap->uniform_data[0]+80+64*j+4*(c*4+r),4); assert(captured==matrices[j*16+r*4+c]);
  }
  float colors[]={1,2,3,4,5,6}; glUniform3fv(glGetUniformLocation(1,"color"),2,colors);
  assert(!memcmp(ap->uniform_data[1]+192,colors,12) && !memcmp(ap->uniform_data[1]+208,colors+3,12));
  assert(ap->uniform_data[1][204]==0);
  glUniform4fv(base,1,colors); assert(glGetError()==GL_INVALID_OPERATION);
  glUniform1f(PS4GL_MAX_UNIFORMS*5,1); assert(glGetError()==GL_INVALID_OPERATION);
  check_rgb_uploads();
  static PS4GLContext texctx;
  assert(ps4glInit(&texctx,80,60)); ps4glMakeCurrent(&texctx);
  GLuint tex[2]; glGenTextures(2,tex); assert(!glIsTexture(tex[0]));
  glBindTexture(GL_TEXTURE_2D,tex[0]); assert(glIsTexture(tex[0]));
  glPixelStorei(GL_UNPACK_ALIGNMENT,8);
  unsigned char pixels[]={1,2,3,4, 99,99,99,99, 5,6,7,8};
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,2,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  const PS4GLTexture *t=ps4glTextureAtUnit(&texctx,0);
  assert(t && t->width==1 && t->height==2 && t->pixels[4]==5 && texctx.texture_bytes==8);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,INT_MAX,2,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  assert(glGetError()==GL_INVALID_VALUE && t->width==1 && t->pixels[4]==5);
  glTexSubImage2D(GL_TEXTURE_2D,0,INT_MAX,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  assert(glGetError()==GL_INVALID_VALUE && t->pixels[0]==1);
  unsigned char replacement_pixel[]={9,10,11,12};
  glTexSubImage2D(GL_TEXTURE_2D,0,0,1,1,1,GL_RGBA,GL_UNSIGNED_BYTE,replacement_pixel);
  assert(t->pixels[0]==1 && t->pixels[4]==9);
  glActiveTexture(GL_TEXTURE0+1); glBindTexture(GL_TEXTURE_2D,tex[1]);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,1,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  assert(ps4glTextureAtUnit(&texctx,1)->pixels[0]==0 && t->pixels[4]==9);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  assert(ps4glTextureAtUnit(&texctx,1)->min_filter==GL_NEAREST && ps4glTextureAtUnit(&texctx,1)->wrap_s==GL_CLAMP_TO_EDGE);
  glActiveTexture(GL_TEXTURE0+PS4GL_TEXTURE_UNITS); assert(glGetError()==GL_INVALID_ENUM && texctx.active_texture==1);
  glPixelStorei(GL_UNPACK_ALIGNMENT,3); assert(glGetError()==GL_INVALID_VALUE && texctx.unpack_alignment==8);
  texctx.programs[0].pipeline=(VkPipeline)(uintptr_t)1; texctx.programs[0].uniform_count=1;
  texctx.programs[0].uniforms[0]=(PS4GLUniform){.name="image",.sampler_binding=2};
  glUseProgram(1); glUniform1i(glGetUniformLocation(1,"image"),1);
  assert(texctx.programs[0].sampler_units[0]==1);
  glUniform1i(0,PS4GL_TEXTURE_UNITS); assert(glGetError()==GL_INVALID_VALUE && texctx.programs[0].sampler_units[0]==1);
  glUniform1f(0,1); assert(glGetError()==GL_INVALID_OPERATION);
  glUniform1iv(0,0,NULL); assert(!glGetError());
  glActiveTexture(GL_TEXTURE0+2); glBindTexture(GL_TEXTURE_2D,tex[0]);
  glDeleteTextures(1,tex); assert(!ps4glTextureAtUnit(&texctx,0) && !ps4glTextureAtUnit(&texctx,2));
  assert(ps4glTextureAtUnit(&texctx,1) && texctx.texture_bytes==8);
  ps4glDestroyTextures(&texctx); assert(texctx.texture_bytes==0 && !glIsTexture(tex[1]));
  assert(ps4glGetProcAddress("glTexImage2D") && ps4glGetProcAddress("glUniform1i"));
  static PS4GLContext blend_ctx;
  assert(ps4glInit(&blend_ctx,80,60)); ps4glMakeCurrent(&blend_ctx);
  assert(!glIsEnabled(GL_BLEND));
  glGetIntegerv(GL_BLEND_SRC_RGB,&value); assert(value==GL_ONE);
  glGetIntegerv(GL_BLEND_DST_ALPHA,&value); assert(value==GL_ZERO);
  glGetIntegerv(GL_BLEND_EQUATION_RGB,&value); assert(value==GL_FUNC_ADD);
  glEnable(GL_BLEND); assert(glIsEnabled(GL_BLEND));
  glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ZERO);
  glBlendEquationSeparate(GL_FUNC_SUBTRACT,GL_MAX);
  glGetIntegerv(GL_BLEND_SRC_RGB,&value); assert(value==GL_SRC_ALPHA);
  glGetIntegerv(GL_BLEND_DST_RGB,&value); assert(value==GL_ONE_MINUS_SRC_ALPHA);
  glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&value); assert(value==GL_MAX);
  PS4GLBlend previous=blend_ctx.blend;
  glBlendFuncSeparate(GL_ONE,GL_ZERO,0xdead,GL_ZERO); assert(glGetError()==GL_INVALID_ENUM);
  assert(!memcmp(&previous,&blend_ctx.blend,sizeof(previous)));
  glBlendEquationSeparate(GL_MIN,0xdead); assert(glGetError()==GL_INVALID_ENUM);
  assert(!memcmp(&previous,&blend_ctx.blend,sizeof(previous)));
  glBlendFunc(GL_DST_COLOR,GL_ONE_MINUS_DST_COLOR); glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
  assert(blend_ctx.blend.src_rgb==blend_ctx.blend.src_alpha && blend_ctx.blend.equation_alpha==GL_FUNC_REVERSE_SUBTRACT);
  ps4glBeginPass((VkCommandBuffer)(uintptr_t)1);
  unsigned clears_before=calls; glClear(GL_COLOR_BUFFER_BIT); assert(calls==clears_before+1 && !glGetError());
  ps4glEndPass(); glDisable(GL_BLEND); assert(!glIsEnabled(GL_BLEND));
  assert(ps4glGetProcAddress("glBlendFuncSeparate") && ps4glGetProcAddress("glBlendEquationSeparate"));
  static PS4GLContext fboctx;
  assert(ps4glInit(&fboctx,80,60)); ps4glMakeCurrent(&fboctx);
  GLuint fbo, ft;
  glGenFramebuffers(1,&fbo); assert(!glIsFramebuffer(fbo));
  glBindFramebuffer(GL_FRAMEBUFFER,fbo); assert(glIsFramebuffer(fbo));
  glGetIntegerv(GL_FRAMEBUFFER_BINDING,&value); assert(value==(GLint)fbo);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT);
  glGenTextures(1,&ft);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,ft,0);
  assert(glGetError()==GL_INVALID_OPERATION); /* reserved texture is not created */
  glBindTexture(GL_TEXTURE_2D,ft);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,ft,0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,8,6,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);

  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,ft,1);
  assert(glGetError()==GL_INVALID_VALUE && ps4glFramebufferTexture(&fboctx)==ft);
  glBindFramebuffer(0xdead,0); assert(glGetError()==GL_INVALID_ENUM && fboctx.framebuffer==fbo);
  ps4glBeginPass((VkCommandBuffer)(uintptr_t)1);
  glClear(GL_COLOR_BUFFER_BIT); assert(glGetError()==GL_INVALID_OPERATION); /* borrowed pass cannot switch */
  ps4glEndPass();
  glBindFramebuffer(GL_FRAMEBUFFER,0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
  size_t retained=fboctx.texture_bytes;
  glDeleteTextures(1,&ft); assert(!glGetError() && !glIsTexture(ft) && retained==fboctx.texture_bytes);
  glBindFramebuffer(GL_FRAMEBUFFER,fbo);
  PS4GLTexture *orphan=ps4glFramebufferTextureObject(&fboctx);
  assert(orphan && orphan->deleted && glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
  glBindTexture(GL_TEXTURE_2D,ft); /* Same name now denotes a different object. */
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  assert(ps4glTextureAtUnit(&fboctx,0)!=orphan && orphan->width==8);
  glDeleteTextures(1,&ft); /* Must not detach the old object with the same name. */
  assert(ps4glFramebufferTextureObject(&fboctx)==orphan && fboctx.texture_bytes==retained);
  glDeleteFramebuffers(1,&fbo); assert(fboctx.texture_bytes==0);
  glBindFramebuffer(GL_FRAMEBUFFER,fbo); glBindTexture(GL_TEXTURE_2D,ft);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,ft,0);
  glDeleteTextures(1,&ft); /* Bound FBO is detached automatically. */
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT && fboctx.texture_bytes==0);
  glDeleteFramebuffers(1,&fbo);
  assert(!glIsFramebuffer(fbo) && !glIsTexture(ft));
  GLuint full[PS4GL_MAX_FRAMEBUFFERS]; glGenFramebuffers(PS4GL_MAX_FRAMEBUFFERS,full);
  GLuint unchanged=123; glGenFramebuffers(1,&unchanged);
  assert(glGetError()==GL_OUT_OF_MEMORY && unchanged==123);
  glDeleteFramebuffers(PS4GL_MAX_FRAMEBUFFERS,full);
  glBindFramebuffer(GL_FRAMEBUFFER,345); glDeleteFramebuffers(1,(GLuint[]){345});
  assert(!fboctx.framebuffer && !glGetError());
  ps4glDestroyFramebuffers(&fboctx); ps4glDestroyTextures(&fboctx);
  assert(ps4glGetProcAddress("glBindFramebuffer") && ps4glGetProcAddress("glCheckFramebufferStatus"));
  static PS4GLContext ints; assert(ps4glInit(&ints,80,60)); ps4glMakeCurrent(&ints);
  PS4GLProgram *ip=&ints.programs[0];
  ip->pipeline=(VkPipeline)(uintptr_t)1; ip->uniform_count=2;
  ip->uniforms[0]=(PS4GLUniform){.name="mode",.width=1,.integer=1,.offset={0,16}};
  ip->uniforms[1]=(PS4GLUniform){.name="modes",.width=1,.integer=1,.offset={16,32},.array_length=3,.array_stride={16,32}};
  glUseProgram(1); GLint iv=0, values[]={INT32_MIN,INT32_MAX,-7};
  glUniform1i(0,-17); glUniform1iv(1,3,values);
  for(unsigned st=0;st<2;++st) {
    memcpy(&iv,ip->uniform_data[st]+ip->uniforms[0].offset[st],4); assert(iv==-17);
    for(unsigned j=0;j<3;++j) { memcpy(&iv,ip->uniform_data[st]+ip->uniforms[1].offset[st]+j*ip->uniforms[1].array_stride[st],4); assert(iv==values[j]); }
  }
  GLint integer_tail=glGetUniformLocation(1,"modes[2]"); glUniform1iv(integer_tail,3,values);
  memcpy(&iv,ip->uniform_data[1]+96,4); assert(iv==INT32_MIN); /* clipped */
  unsigned char before[sizeof(ip->uniform_data)]; memcpy(before,ip->uniform_data,sizeof(before));
  glUniform1f(0,2); assert(glGetError()==GL_INVALID_OPERATION);
  ip->uniforms[0].integer=0;
  glUniform1i(0,2); assert(glGetError()==GL_INVALID_OPERATION);
  ip->uniforms[0].integer=1;
  glUniform1iv(0,2,values); assert(glGetError()==GL_INVALID_OPERATION);
  glUniform1iv(0,1,NULL); assert(glGetError()==GL_INVALID_VALUE);
  glUniform1iv(0,-1,values); assert(glGetError()==GL_INVALID_VALUE);
  glUniform1iv(0,0,NULL); glUniform1iv(-1,1,NULL); assert(!glGetError());
  assert(!memcmp(before,ip->uniform_data,sizeof(before)));
  ip->uniforms[0].integer=2;
  glUniform1i(0,-17); assert(!glGetError());
  uint32_t boolean_bits=0; memcpy(&boolean_bits,ip->uniform_data[0],4); assert(boolean_bits==1);
  glUniform1f(0,0.f); assert(!glGetError());
  memcpy(&boolean_bits,ip->uniform_data[0],4); assert(boolean_bits==0);
  glUniform1f(0,-.25f); assert(!glGetError());
  memcpy(&boolean_bits,ip->uniform_data[0],4); assert(boolean_bits==1);
  static PS4GLContext shaderctx;
  assert(ps4glInit(&shaderctx,80,60)); ps4glMakeCurrent(&shaderctx);
  shaderctx.program_device=(VkDevice)(uintptr_t)1; shaderctx.import_program=import_program;
  ps4glConfigureShaderCompiler((VkRenderPass)(uintptr_t)1,compile_source,link_sources);
  assert(!glCreateShader(GL_TRIANGLES) && glGetError()==GL_INVALID_ENUM);
  GLuint vs=glCreateShader(GL_VERTEX_SHADER), fs=glCreateShader(GL_FRAGMENT_SHADER), prog=glCreateProgram();
  assert(vs!=prog && fs!=prog && glIsProgram(prog) && glIsShader(vs));
  assert(glGetAttribLocation(prog,"position")==-1 && glGetError()==GL_INVALID_OPERATION);
  glBindAttribLocation(prog,8,"position"); assert(glGetError()==GL_INVALID_VALUE);
  glBindAttribLocation(prog,1,"gl_reserved"); assert(glGetError()==GL_INVALID_OPERATION);
  glBindAttribLocation(prog,1,"position"); glBindAttribLocation(prog,4,"position");
  glGetProgramiv(prog,GL_LINK_STATUS,&value); assert(!value);
  glUseProgram(prog); assert(glGetError()==GL_INVALID_OPERATION);
  glLinkProgram(prog); assert(!glGetError());
  glGetProgramiv(prog,GL_INFO_LOG_LENGTH,&value); assert(value>1);
  char src[]="vertex--tail"; const char *pieces[]={src,""}; GLint lengths[]={6,0};
  glShaderSource(vs,2,pieces,lengths); src[0]='x';
  char textbuf[32]; GLsizei written=99;
  glGetShaderSource(vs,sizeof(textbuf),&written,textbuf); assert(written==6 && !strcmp(textbuf,"vertex"));
  glShaderSource(vs,-1,NULL,NULL); assert(glGetError()==GL_INVALID_VALUE);
  const char embedded[]={'a',0,'b'}; const char *embed[]={embedded}; GLint three=3;
  glShaderSource(vs,1,embed,&three); assert(glGetError()==GL_INVALID_VALUE);
  glGetShaderSource(vs,4,&written,textbuf); assert(written==3 && !strcmp(textbuf,"ver"));
  glGetShaderSource(vs,0,&written,NULL); assert(written==0);
  glCompileShader(vs); glGetShaderiv(vs,GL_COMPILE_STATUS,&value); assert(value);
  const char *bad="bad", *fragment="fragment";
  glShaderSource(vs,1,&bad,NULL); glGetShaderiv(vs,GL_COMPILE_STATUS,&value); assert(value);
  glShaderSource(fs,1,&bad,NULL); glCompileShader(fs);
  glGetShaderiv(fs,GL_COMPILE_STATUS,&value); assert(!value && !glGetError());
  glGetShaderInfoLog(fs,5,&written,textbuf); assert(written==4 && !strcmp(textbuf,"reje"));
  glShaderSource(fs,1,&fragment,NULL); glCompileShader(fs);
  glAttachShader(prog,vs); glAttachShader(prog,fs);
  glAttachShader(prog,vs); assert(glGetError()==GL_INVALID_OPERATION);
  glDeleteShader(vs); glDeleteShader(fs); assert(glIsShader(vs) && glIsShader(fs));
  shaderctx.command=(VkCommandBuffer)(uintptr_t)1;
  glEnable(0xdead); glLinkProgram(prog); assert(glGetError()==GL_INVALID_ENUM);
  glGetProgramiv(prog,GL_LINK_STATUS,&value); assert(value);
  assert(shaderctx.programs[prog-1].binding_count==1 && shaderctx.programs[prog-1].bindings[0].index==4);
  assert(glGetAttribLocation(prog,"position")==4 && glGetAttribLocation(prog,"absent")==-1);
  glBindAttribLocation(prog,2,"position"); assert(glGetAttribLocation(prog,"position")==4);
  assert(ps4glGetProcAddress("glBindAttribLocation") && ps4glGetProcAddress("glGetAttribLocation"));
  glGetProgramiv(prog,GL_INFO_LOG_LENGTH,&value); assert(!value);
  glGetProgramiv(prog,GL_ATTACHED_SHADERS,&value); assert(value==2);
  glLinkProgram(prog); assert(glGetError()==GL_INVALID_OPERATION); /* retained executable */
  shaderctx.command=VK_NULL_HANDLE;
  glUseProgram(prog); glDeleteProgram(prog); assert(glIsProgram(prog) && glIsShader(vs));
  glUseProgram(0); assert(!glIsProgram(prog) && !glIsShader(vs) && !glIsShader(fs));
  GLuint shared=glCreateShader(GL_VERTEX_SHADER), p1=glCreateProgram(), p2=glCreateProgram();
  glAttachShader(p1,shared); glAttachShader(p2,shared); glDeleteShader(shared);
  glDeleteProgram(p1); assert(glIsShader(shared)); glDetachShader(p2,shared); assert(!glIsShader(shared)); glDeleteProgram(p2);
  GLuint allshaders[PS4GL_MAX_SHADERS];
  for(unsigned i=0;i<PS4GL_MAX_SHADERS;++i) allshaders[i]=glCreateShader(GL_VERTEX_SHADER);
  assert(!glCreateShader(GL_VERTEX_SHADER) && glGetError()==GL_OUT_OF_MEMORY);
  for(unsigned i=0;i<PS4GL_MAX_SHADERS;++i) glDeleteShader(allshaders[i]);
  ps4glDestroyShaderObjects(&shaderctx); assert(!glGetError());
  static PS4GLContext capacity;
  assert(ps4glInit(&capacity,80,60)); ps4glMakeCurrent(&capacity);
  for(unsigned i=0;i<PS4GL_MAX_PROGRAMS;++i) {
    GLuint name=glCreateProgram(); assert(name==i+1 && glIsProgram(name));
    // Retired slots stay reserved until teardown to protect queued GPU work.
    glDeleteProgram(name); assert(!glIsProgram(name));
  }
  assert(!glCreateProgram() && glGetError()==GL_OUT_OF_MEMORY);
  ps4glDestroyShaderObjects(&capacity);
  static PS4GLContext lazy; assert(ps4glInit(&lazy,80,60)); ps4glMakeCurrent(&lazy);
  lazy.begin_frame=begin_lazy; lazy.programs[0].pipeline=(VkPipeline)(uintptr_t)1; glUseProgram(1);
  glClear(0); assert(!lazy_calls);
  lazy_fail=1; glClear(GL_COLOR_BUFFER_BIT); assert(glGetError()==GL_INVALID_OPERATION && lazy_calls==1 && !lazy.command);
  lazy_fail=0; glDrawArrays(GL_TRIANGLES,0,0); assert(!glGetError() && lazy_calls==2 && lazy.command);
  glDrawArrays(GL_TRIANGLES,0,0); assert(!glGetError() && lazy_calls==2);
  ps4glEndPass(); glDrawArrays(0xdead,0,0); assert(glGetError()==GL_INVALID_ENUM && lazy_calls==2);

  static PS4GLContext strips; assert(ps4glInit(&strips,80,60)); ps4glMakeCurrent(&strips);
  strips.programs[0]=(PS4GLProgram){.pipeline=(VkPipeline)(uintptr_t)1,.components={2},.vertex_stride=8};
  strips.upload_vertices=upload; glUseProgram(1); ps4glBeginPass((VkCommandBuffer)(uintptr_t)1);
  float strip_vertices[]={0,0, 1,0, 0,1, 1,1, 0,2};
  glBindBuffer(GL_ARRAY_BUFFER,1); glBufferData(GL_ARRAY_BUFFER,sizeof(strip_vertices),strip_vertices,GL_STREAM_DRAW);
  glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,NULL); glEnableVertexAttribArray(0);
  expected_indices=9;
  glDrawArrays(GL_TRIANGLE_STRIP,0,5); assert(!glGetError());
  const uint32_t ordered[]={0,1,2, 2,1,3, 2,3,4};
  assert(snapshot_type==GL_UNSIGNED_INT && snapshot_size==76 && snapshot_vertex_bytes==40);
  assert(!memcmp(snapshot,strip_vertices,40) && !memcmp(snapshot+40,ordered,sizeof(ordered)));
  expected_indices=6;
  glDrawArrays(GL_TRIANGLE_STRIP,1,4); assert(!glGetError());
  const uint32_t offset_strip[]={1,2,3,3,2,4};
  assert(snapshot_vertex_bytes==40 && snapshot_size==64 && !memcmp(snapshot+40,offset_strip,sizeof(offset_strip)));
  expected_vertices=3; expected_indices=3;
  glDrawArrays(GL_TRIANGLES,2,3); assert(!glGetError() && snapshot_vertex_bytes==40);
  const uint32_t offset_triangles[]={2,3,4};
  assert(snapshot_type==GL_UNSIGNED_INT && snapshot_size==52 &&
         !memcmp(snapshot+40,offset_triangles,sizeof(offset_triangles)));
  glDrawArrays(GL_TRIANGLE_STRIP,2,4); assert(glGetError()==GL_INVALID_OPERATION);
  glDrawArrays(GL_TRIANGLES,INT_MAX,3); assert(glGetError()==GL_INVALID_VALUE);
  expected_indices=9;
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,2);
  uint16_t strip16[]={65535,4,1,1,3,2};
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(strip16),strip16,GL_STREAM_DRAW);
  glDrawElements(GL_TRIANGLE_STRIP,5,GL_UNSIGNED_SHORT,(void *)2); assert(!glGetError());
  const uint32_t degenerate[]={4,1,1, 1,1,3, 1,3,2};
  assert(snapshot_size==76 && !memcmp(snapshot+40,degenerate,sizeof(degenerate)));
  uint32_t strip32[]={UINT32_MAX,4,1,1,3,2};
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(strip32),strip32,GL_STREAM_DRAW);
  glDrawElements(GL_TRIANGLE_STRIP,5,GL_UNSIGNED_INT,(void *)4); assert(!glGetError());
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,0,NULL,GL_STATIC_DRAW);
  glBufferData(GL_ARRAY_BUFFER,0,NULL,GL_STATIC_DRAW);
  assert(!memcmp(snapshot+40,degenerate,sizeof(degenerate))); /* Captured before both stores changed. */
  /* Attribute-free shaders must retain gl_VertexIndex and ignore the bound EBO
   * for DrawArrays, even when that EBO has no storage. */
  strips.programs[0].vertex_stride=0; strips.programs[0].components[0]=0;
  expected_indices=6; glDrawArrays(GL_TRIANGLE_STRIP,0,4); assert(!glGetError());
  assert(snapshot_vertex_bytes==0 && snapshot_size==24 && !memcmp(snapshot,ordered,24));
  glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING,&value); assert(value==2);
  strips.programs[0].vertex_stride=8; strips.programs[0].components[0]=2;
  unsigned old_uploads=uploads, old_draws=indexed_draws;
  for(int n=0;n<3;++n) { glDrawArrays(GL_TRIANGLE_STRIP,0,n); assert(!glGetError()); }
  glDrawArrays(GL_TRIANGLE_STRIP,0,INT_MAX); assert(glGetError()==GL_OUT_OF_MEMORY);
  glDrawArrays(GL_TRIANGLE_STRIP,0,-1); assert(glGetError()==GL_INVALID_VALUE);
  glDrawElements(GL_TRIANGLE_STRIP,4,GL_UNSIGNED_SHORT,(void *)UINTPTR_MAX); assert(glGetError()==GL_INVALID_OPERATION);
  assert(uploads==old_uploads && indexed_draws==old_draws);
  ps4glDestroyBuffers(&strips);
  check_copy_image();
  check_s3tc();
  check_fixed();
  ps4glMakeCurrent(NULL);
  puts("PASS launcher fixed-disabled state, independent pack alignment and isolation; triangle strips, winding, degenerates, index widths, bounds and snapshots; constant vertex attributes, array precedence, context isolation and atomic errors; signed integer uniforms, array strides, shared stages and atomic type errors; shader objects, source snapshots, deferred/shared deletion and status; framebuffer namespace, completeness, attachment errors and limits; blend defaults, queries, atomic errors and clear independence; RGB8/RGB565 conversion, opaque RGB storage, padded rows and subimages; texture state, upload bounds, row alignment, updates and sampler units; uniform arrays, partial updates and truncation; GL uniform types, shared stages, transpose, isolation and errors; GL state/draw and vertex buffer capture, packing, bounds, deletion and overflow checks; indexed draws, widths, offsets and rejected ranges");
}
