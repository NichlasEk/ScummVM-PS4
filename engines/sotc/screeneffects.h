// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_SCREENEFFECTS_H
#define SOTC_SCREENEFFECTS_H
#include <stdint.h>
namespace Sotc {
// DOS 0x10360, 0x102e7 and 0x10274. Three-times zoom repeats its final
// source column twice, then clears the two bottom lines, just like the EXE.
inline bool zoomScreen(const uint8_t *src,uint8_t *dst,unsigned mode,int x,int y) {
 if(!src || !dst || mode<1 || mode>3)return false;
 unsigned scale=mode+1, width=320/scale,height=200/scale;
 int left=x-int(width/2),top=y-(mode==1 ? 80 : mode==2 ? 50 : 44);
 int maxX=mode==1 ? 159 : mode==2 ? 213 : 239,maxY=mode==1 ? 99 : mode==2 ? 133 : 149;
 if(left<0)left=0;
 if(left>maxX)left=maxX;
 if(top<0)top=0;
 if(top>maxY)top=maxY;
 for(unsigned j=0;j<200;++j)for(unsigned i=0;i<320;++i) {
  unsigned col=i/scale;if(col>=width)col=width-1;
  dst[j*320+i]=j>=height*scale ? 0 : src[(top+j/scale)*320+left+col];
 }
 return true;
}
// Original 0xcdcf: clamp an approach point using the actor footprint and
// the two terrain edge columns, before starting the ordinary scripted move.
inline bool projectApproach(const uint8_t *terrain,int &x,int &y,unsigned halfWidth,unsigned footHeight) {
 if(!terrain || halfWidth>158 || footHeight>255)return false;
 if(x-int(halfWidth)<0)x=halfWidth+1;
 if(x+int(halfWidth)>319)x=318-halfWidth;
 if(y-int(footHeight)<=terrain[x-halfWidth])y=terrain[x-halfWidth]+footHeight+2;
 if(y-int(footHeight)<=terrain[x+halfWidth])y=terrain[x+halfWidth]+footHeight+2;
 if(y>199)y=199;
 return true;
}
inline int effectSigned16(unsigned n){n&=65535;return n<32768 ? int(n) : int(n)-65536;}
// Input/output are the original 8-bit palette components; 0x1073c and 0x10453.
inline void paletteEffect(const uint8_t *src,uint8_t *dst,unsigned mode,unsigned amount) {
 for(unsigned i=0;i<256;++i) {
  int r=src[i*3],g=src[i*3+1],b=src[i*3+2];
  if(mode==1){r=r*amount/256;g=g*amount/256;b=b*amount/256;}
  else if(mode==2){
   int ax=(effectSigned16(((r+g+b+100)/4-r)*amount)>>4)+r;
   r=ax&255;
   ax=effectSigned16(((unsigned(ax)&0xff00)|unsigned(g))*(16-int(amount)))>>4;g=ax&255;
   ax=effectSigned16(((unsigned(ax)&0xff00)|unsigned(b))*(16-int(amount)))>>4;b=ax&255;
  }
  else if(mode==3){r+=(256-r)/16*amount;g+=(256-g)/16*amount;b+=(256-b)/16*amount;}
  dst[i*3]=r;dst[i*3+1]=g;dst[i*3+2]=b;
 }
}
}
#endif
