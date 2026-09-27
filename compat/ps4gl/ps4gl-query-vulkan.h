static int query_command(PS4GLContext *c, PS4GLQuery *q, int operation) {
  if(operation==3) {if(q->pool) vkDestroyQueryPool(c->program_device,q->pool,NULL);q->pool=VK_NULL_HANDLE;return 1;}
  if(!c->command || !c->managed_frame) return 0;
  if(operation==1) {
    if(!q->pool) {
      VkQueryPoolCreateInfo info={.sType=VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,.queryType=VK_QUERY_TYPE_OCCLUSION,.queryCount=1};
      if(vkCreateQueryPool(c->program_device,&info,NULL,&q->pool)!=VK_SUCCESS) return 0;
    }
    end_render_target(c);
    vkCmdResetQueryPool(c->command,q->pool,0,1);
    if(!prepare_render_target(c)) return 0;
    vkCmdBeginQuery(c->command,q->pool,0,VK_QUERY_CONTROL_PRECISE_BIT);
    return 1;
  }
  if(operation==2 && c->pass_open) {vkCmdEndQuery(c->command,q->pool,0);return 1;}
  return 0;
}
static void complete_queries(PS4GLContext *c) {
  for(unsigned i=0;i<PS4GL_MAX_QUERIES;++i) {
    PS4GLQuery *q=&c->queries[i];
    if(!q->pending) continue;
    uint64_t values[2]={0,0};
    VkResult r=vkGetQueryPoolResults(c->program_device,q->pool,0,1,sizeof(values),values,sizeof(values),
                                    VK_QUERY_RESULT_64_BIT|VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
    q->available=r==VK_SUCCESS && values[1]!=0;
    if(q->available) q->result=values[0]>UINT32_MAX ? UINT32_MAX : (GLuint)values[0];
    q->pending=0;
  }
}
/* Called only after the owner resets/discards its command buffer. */
static void release_queries(PS4GLContext *c) {
  for(unsigned i=0;i<PS4GL_MAX_QUERIES;++i) {
    PS4GLQuery *q=&c->queries[i];
    q->recorded=0;q->pending=0;
    if(q->deleted) {query_command(c,q,3);*q=(PS4GLQuery){0};}
  }
}
