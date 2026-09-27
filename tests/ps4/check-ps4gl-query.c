#include "ps4gl.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
static PS4GLContext *current;
#define error(v) do {if(!current->error)current->error=(v);}while(0)
static int fixed_outside(void) {return current && !current->fixed.active;}
static void end_render_target(PS4GLContext *c) {c->pass_open=0;}
static int prepare_render_target(PS4GLContext *c) {c->pass_open=1;return 1;}
static unsigned created,destroyed,begun,ended,reset;
static uint64_t samples,ready;
VKAPI_ATTR VkResult VKAPI_CALL vkCreateQueryPool(VkDevice d,const VkQueryPoolCreateInfo *i,const VkAllocationCallbacks *a,VkQueryPool *p) {
  assert(i->queryType==VK_QUERY_TYPE_OCCLUSION && i->queryCount==1);
  *p=(VkQueryPool)(uintptr_t)++created;return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL vkDestroyQueryPool(VkDevice d,VkQueryPool p,const VkAllocationCallbacks *a) {assert(p);++destroyed;}
VKAPI_ATTR void VKAPI_CALL vkCmdResetQueryPool(VkCommandBuffer c,VkQueryPool p,uint32_t f,uint32_t n) {assert(c && p && !current->pass_open && n==1);++reset;}
VKAPI_ATTR void VKAPI_CALL vkCmdBeginQuery(VkCommandBuffer c,VkQueryPool p,uint32_t q,VkQueryControlFlags f) {assert(current->pass_open && f==VK_QUERY_CONTROL_PRECISE_BIT);++begun;}
VKAPI_ATTR void VKAPI_CALL vkCmdEndQuery(VkCommandBuffer c,VkQueryPool p,uint32_t q) {assert(current->pass_open);++ended;}
VKAPI_ATTR VkResult VKAPI_CALL vkGetQueryPoolResults(VkDevice d,VkQueryPool p,uint32_t first,uint32_t count,size_t size,void *out,VkDeviceSize stride,VkQueryResultFlags flags) {
  assert(size==16 && count==1 && !(flags&VK_QUERY_RESULT_WAIT_BIT));
  uint64_t *v=out;v[0]=samples;v[1]=ready;return ready ? VK_SUCCESS : VK_NOT_READY;
}
#include "../../compat/ps4gl/ps4gl-query-vulkan.h"
#include "../../compat/ps4gl/ps4gl-query.inc"
int main(void) {
  static PS4GLContext ctx;
  current=&ctx;ctx.next_query=1;ctx.command=(VkCommandBuffer)1;ctx.managed_frame=1;ctx.query_command=query_command;
  GLuint name,value=77;glGenQueries(1,&name);assert(name && !glIsQuery(name));
  glBeginQuery(GL_SAMPLES_PASSED,name);assert(glIsQuery(name) && created==1 && begun==1 && reset==1);
  glBeginQuery(GL_SAMPLES_PASSED,name);assert(ctx.error==GL_INVALID_OPERATION);ctx.error=0;
  glEndQuery(GL_SAMPLES_PASSED);assert(ended==1);
  glGetQueryObjectuiv(name,GL_QUERY_RESULT_AVAILABLE,&value);assert(!value);
  ready=1;samples=0;complete_queries(&ctx);release_queries(&ctx);
  glGetQueryObjectuiv(name,GL_QUERY_RESULT_AVAILABLE,&value);assert(value==1);
  glGetQueryObjectuiv(name,GL_QUERY_RESULT,&value);assert(value==0);
  glBeginQuery(GL_SAMPLES_PASSED,name);glEndQuery(GL_SAMPLES_PASSED);
  glGetQueryObjectuiv(name,GL_QUERY_RESULT_AVAILABLE,&value);assert(value==0);
  samples=91;complete_queries(&ctx);release_queries(&ctx);glGetQueryObjectuiv(name,GL_QUERY_RESULT,&value);assert(value==91);
  glBeginQuery(GL_SAMPLES_PASSED,name);glEndQuery(GL_SAMPLES_PASSED);
  glDeleteQueries(1,&name);assert(!glIsQuery(name) && !destroyed);
  complete_queries(&ctx);assert(!destroyed);release_queries(&ctx);assert(destroyed==1);
  glGenQueries(1,&name);glBeginQuery(GL_SAMPLES_PASSED,name);glEndQuery(GL_SAMPLES_PASSED);
  ready=0;complete_queries(&ctx);release_queries(&ctx);glGetQueryObjectuiv(name,GL_QUERY_RESULT_AVAILABLE,&value);assert(!value);
  glDeleteQueries(1,&name);assert(destroyed==2 && !ctx.error);
  puts("PASS GL occlusion lifecycle, render-pass reset ordering, zero results, no stale results, deferred deletion and unavailable results");
}
