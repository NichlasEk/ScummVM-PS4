// SPDX-License-Identifier: GPL-3.0-or-later
#include "backends/graphics/surfacesdl/ps4-framebuffer.h"
#include <assert.h>
#include <vector>
#include <stdio.h>
int main() {
 std::vector<uint32_t> colors(65536);
 for (unsigned i=0; i<colors.size(); ++i) colors[i]=0xff000000|i;
 for (int sw=1;sw<=9;++sw) for (int sh=1;sh<=7;++sh)
 for (int vw=1;vw<=13;++vw) for (int vh=1;vh<=11;++vh) {
  int sp=sw*2+2, dw=vw+4, dh=vh+4, dp=dw*4+4;
  std::vector<uint8_t> src(sp*sh,0), dst(dp*dh+16,0xcd);
  for(int y=0;y<sh;++y) for(int x=0;x<sw;++x) {
   uint16_t v=y*sw+x; memcpy(&src[y*sp+x*2],&v,2);
  }
  assert(ps4Scale565(src.data(),sw,sh,sp,dst.data(),dw,dh,dp,2,2,vw,vh,colors.data()));
  for(int y=0;y<dh;++y) for(int x=0;x<dw;++x) {
   uint32_t actual,expected=0; memcpy(&actual,&dst[y*dp+x*4],4);
   if(x>=2&&x<2+vw&&y>=2&&y<2+vh) {
    int sx=int((x-2+0.5)*sw/vw),sy=int((y-2+0.5)*sh/vh);
    expected=colors[sy*sw+sx];
   }
   assert(actual==expected);
  }
  for(int y=0;y<dh;++y) for(int x=dw*4;x<dp;++x) assert(dst[y*dp+x]==0xcd);
  for(unsigned i=dp*dh;i<dst.size();++i) assert(dst[i]==0xcd);
 }
 uint16_t src=0; uint32_t dst=0;
 assert(!ps4Scale565(&src,1,1,2,&dst,1,1,4,-1,0,1,1,colors.data()));
 assert(!ps4Scale565(&src,1,1,2,&dst,1,1,4,0,0,2,1,colors.data()));
 assert(!ps4Scale565(&src,1,1,1,&dst,1,1,4,0,0,1,1,colors.data()));
 assert(!ps4Scale565(nullptr,1,1,2,&dst,1,1,4,0,0,1,1,colors.data()));
 puts("PS4 framebuffer: 9009 scale/letterbox/pitch cases PASS");
}
