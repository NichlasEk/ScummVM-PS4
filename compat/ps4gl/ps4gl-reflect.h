/* Bounded reflection for validated SPIR-V default-uniform bridge blocks.
 * Structural validation precedes every instruction walk. No general linker. */
static const uint32_t *gl_spv_type(const uint32_t *s, size_t n, unsigned op, uint32_t id) {
  for(size_t i=5;i<n;i+=s[i]>>16)
    if((s[i]&65535)==op && (s[i]>>16)>=2 && s[i+1]==id) return s+i;
  return NULL;
}
static int gl_spv_dec(const uint32_t *s, size_t n, uint32_t id, int member, unsigned dec) {
  for(size_t i=5;i<n;i+=s[i]>>16) {
    unsigned wc=s[i]>>16;
    if(member<0 && (s[i]&65535)==71 && wc>=3 && s[i+1]==id && s[i+2]==dec)
      return wc==4 && s[i+3]<=INT32_MAX ? (int)s[i+3] : wc==3 ? 1 : -1;
    if(member>=0 && (s[i]&65535)==72 && wc>=4 && s[i+1]==id && s[i+2]==(unsigned)member && s[i+3]==dec)
      return wc==5 && s[i+4]<=INT32_MAX ? (int)s[i+4] : wc==4 ? 1 : -1;
  }
  return -1;
}
static int gl_reflect_uniforms(const uint32_t *s, size_t bytes, unsigned stage, PS4GLProgram *p) {
  if(!s || bytes<20 || bytes%4 || stage>1) return 0;
  size_t n=bytes/4;
  for(size_t i=5;i<n;) { unsigned wc=s[i]>>16; if(!wc || wc>n-i) return 0; i+=wc; }
  for(size_t i=5;i<n;i+=s[i]>>16) {
    if((s[i]&65535)!=59) continue;
    if((s[i]>>16)<4) return 0;
    if(s[i+3]!=2) continue;
    if(p->uniform_stages&(1u<<stage)) return 0;
    if(gl_spv_dec(s,n,s[i+2],-1,34)!=0 || gl_spv_dec(s,n,s[i+2],-1,33)!=(int)stage) return 0;
    const uint32_t *ptr=gl_spv_type(s,n,32,s[i+1]);
    if(!ptr || (ptr[0]>>16)!=4 || ptr[2]!=2) return 0;
    const uint32_t *block=gl_spv_type(s,n,30,ptr[3]);
    if(!block || (block[0]>>16)<3 || gl_spv_dec(s,n,ptr[3],-1,2)!=1) return 0;
    unsigned char occupied[PS4GL_UNIFORM_BYTES]={0};
    for(unsigned m=0;m<(block[0]>>16)-2;++m) {
      uint32_t type=block[2+m], scalar=type;
      unsigned width=1, alignment=4;
      const uint32_t *mat=gl_spv_type(s,n,24,type), *vec;
      if(mat) {
        if((mat[0]>>16)!=4 || mat[3]!=4 || gl_spv_dec(s,n,ptr[3],m,7)!=16 ||
           gl_spv_dec(s,n,ptr[3],m,5)!=1 || gl_spv_dec(s,n,ptr[3],m,4)>=0) return 0;
        type=mat[2];
      }
      vec=gl_spv_type(s,n,23,type);
      if(vec) {
        if((vec[0]>>16)!=4 || vec[3]<2 || vec[3]>4) return 0;
        scalar=vec[2]; width=vec[3]; alignment=width==2 ? 8 : 16;
      }
      if(mat) { if(!vec || width!=4) return 0; width=16; }
      const uint32_t *f=gl_spv_type(s,n,22,scalar);
      if(!f || (f[0]>>16)!=3 || f[2]!=32) return 0;
      int offset=gl_spv_dec(s,n,ptr[3],m,35);
      if(offset<0 || (unsigned)offset%alignment || (unsigned)offset>PS4GL_UNIFORM_BYTES-width*4) return 0;
      for(unsigned b=offset;b<(unsigned)offset+width*4;++b) { if(occupied[b]) return 0; occupied[b]=1; }
      const char *name=NULL;
      for(size_t j=5;j<n;j+=s[j]>>16) if((s[j]&65535)==6 && (s[j]>>16)>=4 && s[j+1]==ptr[3] && s[j+2]==m) {
        const char *candidate=(const char *)(s+j+3);
        const char *end=memchr(candidate,0,((s[j]>>16)-3)*4);
        if(!end || end==candidate || end-candidate>=64) return 0;
        name=candidate;
      }
      if(!name) return 0;
      unsigned u;
      for(u=0;u<p->uniform_count;++u) if(!strcmp(p->uniforms[u].name,name)) break;
      if(u==p->uniform_count) {
        if(u==PS4GL_MAX_UNIFORMS) return 0;
        p->uniform_count++;
        p->uniforms[u]=(PS4GLUniform){.width=width,.offset={-1,-1}};
        strcpy(p->uniforms[u].name,name);
      }
      if(p->uniforms[u].width!=width || p->uniforms[u].offset[stage]>=0) return 0;
      p->uniforms[u].offset[stage]=offset;
    }
    p->uniform_stages|=1u<<stage;
  }
  return 1;
}
