/* Native packet and counter decoding contract, including completed zero. */
#include "../../.tools/opengnm-stack/vulkan-ps4/src/vk_ps4_query.c"
#include <assert.h>
#include <stdio.h>
int main(void) {
  uint64_t storage[34]={0}, result[2]={99,99};
  VkPs4QueryPool pool={0};
  pool.result_buffer=storage;pool.create_info.queryType=VK_QUERY_TYPE_OCCLUSION;pool.create_info.queryCount=1;
  VkQueryResultFlags flags=VK_QUERY_RESULT_64_BIT|VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
  assert(occlusion_results(&pool,0,1,sizeof(result),result,sizeof(result),flags)==VK_NOT_READY);
  assert(result[0]==99 && result[1]==0);
  for(unsigned rb=0;rb<8;++rb) storage[rb*2]=storage[rb*2+1]=OCCLUSION_VALID|100;
  assert(occlusion_results(&pool,0,1,sizeof(result),result,sizeof(result),flags)==VK_NOT_READY);
  storage[32]=1;
  assert(occlusion_results(&pool,0,1,sizeof(result),result,sizeof(result),flags)==VK_SUCCESS);
  assert(result[0]==0 && result[1]==1);
  storage[3]+=7;storage[15]+=11;
  assert(occlusion_results(&pool,0,1,sizeof(result),result,sizeof(result),flags)==VK_SUCCESS && result[0]==18);
  storage[15]&=~OCCLUSION_VALID;
  assert(occlusion_results(&pool,0,1,sizeof(result),result,sizeof(result),flags)==VK_NOT_READY && result[1]==0);
  assert(occlusion_results(&pool,0,1,8,result,16,flags)==VK_ERROR_OUT_OF_HOST_MEMORY);
  uint32_t words[128];memset(words,0xcc,sizeof(words));
  VkPs4CommandBuffer cmd={0};cmd.gnm_cmd.cmdptr=words;cmd.gnm_cmd.endptr=words+128;
  occlusion_reset(&cmd,UINT64_C(0x123456780));
  assert(cmd.gnm_cmd.cmdptr==words+72 && words[0]==0xc0463700u);
  assert(words[2]==0x23456780 && words[3]==1);
  for(unsigned i=4;i<72;++i) assert(!words[i]);
  occlusion_sample(&cmd,UINT64_C(0x123456788));
  assert(words[72]==0xc0024600u && words[73]==0x115 && words[74]==0x23456788 && words[75]==1);
  puts("PASS occlusion packets, pending/completed-zero distinction, RB deltas, invalid counter and output bounds");
}
