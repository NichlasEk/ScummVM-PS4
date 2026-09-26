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
  ps4glMakeCurrent(NULL);
  puts("PASS GL state isolation, clipping, origin, overflow, error and proc lookup checks");
}
