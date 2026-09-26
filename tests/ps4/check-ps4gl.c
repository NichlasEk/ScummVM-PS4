/* Host-side state/coordinate checks; native probe verifies resulting pixels. */
#include "ps4gl.h"
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
static unsigned calls;
static VkClearRect last;
static VkClearAttachment color;
VKAPI_ATTR void VKAPI_CALL vkCmdClearAttachments(VkCommandBuffer cmd, uint32_t nc,
 const VkClearAttachment *c, uint32_t nr, const VkClearRect *r) {
  assert(cmd && nc==1 && nr==1); ++calls; last=*r; color=*c;
}
static unsigned draws;
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
  assert(cmd && count==3 && instances==1 && !first && !instance); ++draws;
}
static unsigned indexed_draws, uploads;
static unsigned char snapshot[128];
static size_t snapshot_size, snapshot_vertex_bytes;
static GLenum snapshot_type;
static int upload(PS4GLContext *ctx, const void *data, size_t size, size_t vb, GLenum type) {
  assert(ctx && size<=sizeof(snapshot)); ++uploads;
  memcpy(snapshot,data,size); snapshot_size=size; snapshot_vertex_bytes=vb; snapshot_type=type;
  return 1;
}
VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexed(VkCommandBuffer cmd, uint32_t count,
 uint32_t instances, uint32_t first, int32_t base, uint32_t instance) {
  assert(cmd && count==3 && instances==1 && !first && !base && !instance); ++indexed_draws;
}
int main(void) {
  PS4GLContext a,b;
  assert(ps4glInit(&a,80,60) && ps4glInit(&b,20,10));
  assert(!ps4glInit(NULL,80,60) && !ps4glInit(&b,0,60));
  ps4glMakeCurrent(&a);
  assert(ps4glGetProcAddress("glClear") && ps4glGetProcAddress("glDrawArrays") && !ps4glGetProcAddress("glShaderSource"));
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
  glDrawArrays(GL_TRIANGLES,1,3); assert(glGetError()==GL_INVALID_OPERATION && draws==2);
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
  PS4GLContext vertices_ctx;
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
  for(unsigned i=0;i<3;++i) glDisableVertexAttribArray(i);
  assert(ps4glPackVertices(&vertices_ctx,&attributes,2,packed));
  assert(packed[0]==0 && packed[5]==1 && packed[6]==0);
  GLuint too_many[PS4GL_MAX_BUFFERS+1]; glGenBuffers(PS4GL_MAX_BUFFERS+1,too_many);
  assert(glGetError()==GL_OUT_OF_MEMORY);
  glBindBuffer(GL_ARRAY_BUFFER,1000); assert(glIsBuffer(1000));
  ps4glDestroyBuffers(&vertices_ctx);
  PS4GLContext indexed;
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
  PS4GLContext uniforms;
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
  PS4GLContext arrays;
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
  PS4GLContext texctx;
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
  glActiveTexture(GL_TEXTURE0+4); assert(glGetError()==GL_INVALID_ENUM && texctx.active_texture==1);
  glPixelStorei(GL_UNPACK_ALIGNMENT,3); assert(glGetError()==GL_INVALID_VALUE && texctx.unpack_alignment==8);
  texctx.programs[0].pipeline=(VkPipeline)(uintptr_t)1; texctx.programs[0].uniform_count=1;
  texctx.programs[0].uniforms[0]=(PS4GLUniform){.name="image",.sampler_binding=2};
  glUseProgram(1); glUniform1i(glGetUniformLocation(1,"image"),1);
  assert(texctx.programs[0].sampler_units[0]==1);
  glUniform1i(0,4); assert(glGetError()==GL_INVALID_VALUE && texctx.programs[0].sampler_units[0]==1);
  glUniform1f(0,1); assert(glGetError()==GL_INVALID_OPERATION);
  glUniform1iv(0,0,NULL); assert(!glGetError());
  glActiveTexture(GL_TEXTURE0+2); glBindTexture(GL_TEXTURE_2D,tex[0]);
  glDeleteTextures(1,tex); assert(!ps4glTextureAtUnit(&texctx,0) && !ps4glTextureAtUnit(&texctx,2));
  assert(ps4glTextureAtUnit(&texctx,1) && texctx.texture_bytes==8);
  ps4glDestroyTextures(&texctx); assert(texctx.texture_bytes==0 && !glIsTexture(tex[1]));
  assert(ps4glGetProcAddress("glTexImage2D") && ps4glGetProcAddress("glUniform1i"));
  PS4GLContext blend_ctx;
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
  ps4glMakeCurrent(NULL);
  puts("PASS blend defaults, queries, atomic errors and clear independence; texture state, upload bounds, row alignment, updates and sampler units; uniform arrays, partial updates and truncation; GL uniform types, shared stages, transpose, isolation and errors; GL state/draw and vertex buffer capture, packing, bounds, deletion and overflow checks; indexed draws, widths, offsets and rejected ranges");
}
