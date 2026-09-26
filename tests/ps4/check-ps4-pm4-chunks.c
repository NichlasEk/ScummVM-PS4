#include "vk_ps4_pm4_chunks.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    uint32_t offsets[8], sizes[8], count;
    /* The middle word looks like an invalid header; it is payload. */
    uint32_t packets[]={0xc0011000,0x40000000,0,0x80000000,0xc0001000,42};
    assert(vk_ps4_pm4_chunks(packets,6,4,offsets,sizes,&count));
    assert(count==2 && offsets[0]==0 && sizes[0]==4 && offsets[1]==4 && sizes[1]==2);
    assert(!vk_ps4_pm4_chunks(packets,5,4,offsets,sizes,&count) && count==0);
    assert(!vk_ps4_pm4_chunks(packets,6,2,offsets,sizes,&count) && count==0);
    uint32_t bad=0x40000000;
    assert(!vk_ps4_pm4_chunks(&bad,1,4,offsets,sizes,&count) && count==0);
    uint32_t tiny[9]; for(unsigned i=0;i<9;++i) tiny[i]=0x80000000;
    assert(!vk_ps4_pm4_chunks(tiny,9,1,offsets,sizes,&count) && count==0);
    assert(vk_ps4_pm4_chunks(tiny,0,4,offsets,sizes,&count) && count==0);
    uint32_t words=VK_PS4_PM4_RECORDING_BYTES/4;
    uint32_t *large=malloc((size_t)words*4); assert(large);
    for(uint32_t i=0;i<words;++i) large[i]=0x80000000;
    assert(vk_ps4_pm4_chunks(large,words,VK_PS4_PM4_SUBMIT_DWORDS,offsets,sizes,&count));
    assert(count==5);
    uint32_t end=0;
    for(uint32_t i=0;i<count;++i) {
        assert(offsets[i]==end && sizes[i] && sizes[i]<=VK_PS4_PM4_SUBMIT_DWORDS);
        end+=sizes[i];
    }
    assert(end==words);
    assert(!vk_ps4_pm4_chunks(large,words+1,VK_PS4_PM4_SUBMIT_DWORDS,offsets,sizes,&count));
    free(large);
    puts("PASS PM4 packet boundaries, malformed streams, bounded chunks and 16 MiB recording");
}
