/* CPU expansion of base-level BC1/BC2/BC3 into the bridge's RGBA8 storage.
 * Format reference: Khronos EXT_texture_compression_s3tc, version 1.6.
 * https://registry.khronos.org/OpenGL/extensions/EXT/EXT_texture_compression_s3tc.txt
 * No extension string is advertised: compressed subimages/mipmaps are absent. */
static unsigned s3tc_u16(const unsigned char *p) { return p[0]|((unsigned)p[1]<<8); }
static void s3tc_rgb(unsigned value,unsigned char *out) {
  unsigned r=(value>>11)&31,g=(value>>5)&63,b=value&31;
  out[0]=(r<<3)|(r>>2); out[1]=(g<<2)|(g>>4); out[2]=(b<<3)|(b>>2); out[3]=255;
}
void glCompressedTexImage2D(GLenum target, GLint level, GLenum format, GLsizei width, GLsizei height,
                            GLint border, GLsizei size, const void *data) {
  if(!current) return;
  if(target!=GL_TEXTURE_2D || format<0x83f0 || format>0x83f3) { error(GL_INVALID_ENUM); return; }
  if(level || border) { error(GL_INVALID_OPERATION); return; }
  if(width<=0 || height<=0 || width>PS4GL_TEXTURE_LIMIT || height>PS4GL_TEXTURE_LIMIT || size<0) { error(GL_INVALID_VALUE); return; }
  unsigned stride=format<=0x83f1 ? 8 : 16;
  size_t blocks_x=((unsigned)width+3)/4, blocks_y=((unsigned)height+3)/4;
  if((size_t)size!=blocks_x*blocks_y*stride || !data) { error(GL_INVALID_VALUE); return; }
  if(!bound_texture(target)) return;
  size_t bytes=(size_t)width*height*4;
  if(bytes>PS4GL_TEXTURE_BUDGET) { error(GL_OUT_OF_MEMORY); return; }
  unsigned char *pixels=malloc(bytes);
  if(!pixels) { error(GL_OUT_OF_MEMORY); return; }
  const unsigned char *src=data;
  for(size_t by=0;by<blocks_y;++by) for(size_t bx=0;bx<blocks_x;++bx,src+=stride) {
    const unsigned char *rgb=src+(stride==16 ? 8 : 0);
    unsigned c0=s3tc_u16(rgb),c1=s3tc_u16(rgb+2); unsigned char colors[4][4]={{0}};
    s3tc_rgb(c0,colors[0]); s3tc_rgb(c1,colors[1]);
    int four=stride==16 || c0>c1;
    for(unsigned k=0;k<3;++k) {
      colors[2][k]=four ? (2*colors[0][k]+colors[1][k])/3 : (colors[0][k]+colors[1][k])/2;
      colors[3][k]=four ? (colors[0][k]+2*colors[1][k])/3 : 0;
    }
    colors[2][3]=255; colors[3][3]=(!four && format==0x83f1) ? 0 : 255;
    unsigned char alpha[8]={src[0],src[1],0,0,0,0,0,255};
    if(format==0x83f3) {
      unsigned n=alpha[0]>alpha[1] ? 7 : 5;
      for(unsigned i=1;i<n;++i) alpha[i+1]=((n-i)*alpha[0]+i*alpha[1])/n;
    }
    uint64_t alpha_bits=0;
    if(format==0x83f3) for(unsigned i=0;i<6;++i) alpha_bits|=(uint64_t)src[2+i]<<(8*i);
    for(unsigned y=0;y<4;++y) for(unsigned x=0;x<4;++x) {
      size_t px=bx*4+x,py=by*4+y;
      if(px>=(unsigned)width || py>=(unsigned)height) continue;
      unsigned i=y*4+x,index=(rgb[4+y]>>(x*2))&3;
      unsigned char *out=pixels+(py*width+px)*4; memcpy(out,colors[index],4);
      if(format==0x83f2) out[3]=((src[i/2]>>((i&1)*4))&15)*17;
      else if(format==0x83f3) out[3]=alpha[(alpha_bits>>(i*3))&7];
    }
  }
  unsigned align=current->unpack_alignment; current->unpack_alignment=1;
  glTexImage2D(target,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  current->unpack_alignment=align; free(pixels);
}
