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
      unsigned width=1, alignment=4, length=0, stride=0;
      const uint32_t *array=gl_spv_type(s,n,28,type);
      if(array) {
        if((array[0]>>16)!=4) return 0;
        const uint32_t *length_constant=NULL;
        for(size_t j=5;j<n;j+=s[j]>>16)
          if((s[j]&65535)==43 && (s[j]>>16)==4 && s[j+2]==array[3]) length_constant=s+j;
        if(!length_constant) return 0;
        const uint32_t *integer=gl_spv_type(s,n,21,length_constant[1]);
        if(!integer || (integer[0]>>16)!=4 || integer[2]!=32 || integer[3]>1) return 0;
        length=length_constant[3];
        if(!length || length>PS4GL_UNIFORM_BYTES/16) return 0;
        int declared_stride=gl_spv_dec(s,n,type,-1,6);
        if(declared_stride<=0 || declared_stride%16) return 0;
        stride=(unsigned)declared_stride;
        type=array[2]; scalar=type;
      }
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
      const uint32_t *integer=gl_spv_type(s,n,21,scalar);
      unsigned is_integer=integer && (integer[0]>>16)==4 && integer[2]==32 && integer[3]==1;
      if(is_integer) { if(width!=1 || mat || vec) return 0; }
      else if(!f || (f[0]>>16)!=3 || f[2]!=32) return 0;
      if(array) alignment=16;
      if(array && (stride<width*4 || stride>PS4GL_UNIFORM_BYTES/length)) return 0;
      unsigned occupied_bytes=array ? stride*length : width*4;
      int offset=gl_spv_dec(s,n,ptr[3],m,35);
      if(offset<0 || (unsigned)offset%alignment || (unsigned)offset>PS4GL_UNIFORM_BYTES-occupied_bytes) return 0;
      for(unsigned b=offset;b<(unsigned)offset+occupied_bytes;++b) { if(occupied[b]) return 0; occupied[b]=1; }
      const char *name=NULL; char field_name[64];
      for(size_t j=5;j<n;j+=s[j]>>16) if((s[j]&65535)==6 && (s[j]>>16)>=4 && s[j+1]==ptr[3] && s[j+2]==m) {
        const char *candidate=(const char *)(s+j+3);
        const char *end=memchr(candidate,0,((s[j]>>16)-3)*4);
        if(!end) return 0;
        if(!strncmp(candidate,"ps4gl_bool_",11)) {
          if(!is_integer || width!=1 || array) return 0;
          is_integer=2; candidate+=11;
        }
        if(end==candidate) return 0;
        if(!strncmp(candidate,"ps4gl_field_",12)) {
          const char *digits=candidate+12; unsigned length=0;
          if(*digits<'1' || *digits>'9') return 0;
          while(*digits>='0' && *digits<='9') {
            length=length*10+(unsigned)(*digits++-'0'); if(length>=64) return 0;
          }
          if(*digits++!='_' || (size_t)(end-digits)<=length || end-digits+1>=64) return 0;
          memcpy(field_name,digits,length); field_name[length]='.';
          strcpy(field_name+length+1,digits+length); candidate=field_name;
        }
        if(strlen(candidate)>=64) return 0;
        name=candidate;
      }
      if(!name) return 0;
      if(!strcmp(name,"ps4gl_fragcoord_y") && (stage!=1 || width!=2 || is_integer || length)) return 0;
      if(!strncmp(name,"ps4gl_cube_size",15)) {
        if(stage!=1 || width!=2 || is_integer || length || name[15]<'0' || name[15]>='0'+PS4GL_MAX_SAMPLERS || name[16]) return 0;
        p->cube_sampler_mask|=1u<<(name[15]-'0');
      }
      if(!strncmp(name,"ps4gl_rect_size",15)) {
        if(stage!=1 || width!=2 || is_integer || length || name[15]<'0' || name[15]>='0'+PS4GL_MAX_SAMPLERS || name[16]) return 0;
        p->rectangle_sampler_mask|=1u<<(name[15]-'0');
      }
      unsigned u;
      for(u=0;u<p->uniform_count;++u) if(!strcmp(p->uniforms[u].name,name)) break;
      if(u==p->uniform_count) {
        if(u==PS4GL_MAX_UNIFORMS) return 0;
        p->uniform_count++;
        p->uniforms[u]=(PS4GLUniform){.width=width,.integer=is_integer,.offset={-1,-1},.array_length=length};
        strcpy(p->uniforms[u].name,name);
      }
      if(p->uniforms[u].integer!=is_integer || p->uniforms[u].width!=width || p->uniforms[u].array_length!=length || p->uniforms[u].offset[stage]>=0) return 0;
      p->uniforms[u].offset[stage]=offset; p->uniforms[u].array_stride[stage]=stride;
    }
    p->uniform_stages|=1u<<stage;
  }
  return 1;
}
/* Fragment-only, non-array, float sampler2D at set 0 bindings 2..9. */
static inline int gl_reflect_samplers(const uint32_t *s, size_t bytes, unsigned stage, PS4GLProgram *p) {
  size_t n=bytes/4;
  for(size_t i=5;i<n;) { unsigned wc=s[i]>>16; if(!wc || wc>n-i) return 0; i+=wc; }
  for(size_t i=5;i<n;i+=s[i]>>16) {
    if((s[i]&65535)!=59) continue;
    if((s[i]>>16)<4) return 0;
    if(s[i+3]!=0) continue;
    if(stage!=1 || gl_spv_dec(s,n,s[i+2],-1,34)!=0) return 0;
    int binding=gl_spv_dec(s,n,s[i+2],-1,33);
    if(binding<2 || binding>=2+PS4GL_MAX_SAMPLERS || (p->sampler_mask&(1u<<(binding-2)))) return 0;
    const uint32_t *ptr=gl_spv_type(s,n,32,s[i+1]);
    if(!ptr || (ptr[0]>>16)!=4 || ptr[2]!=0) return 0;
    const uint32_t *sampled=gl_spv_type(s,n,27,ptr[3]);
    if(!sampled || (sampled[0]>>16)!=3) return 0;
    const uint32_t *image=gl_spv_type(s,n,25,sampled[2]);
    if(!image || (image[0]>>16)!=9 || image[3]!=1 || image[4] || image[5] || image[6] || image[7]!=1 || image[8]) return 0;
    const uint32_t *scalar=gl_spv_type(s,n,22,image[2]);
    if(!scalar || (scalar[0]>>16)!=3 || scalar[2]!=32) return 0;
    const char *name=NULL;
    for(size_t j=5;j<n;j+=s[j]>>16) if((s[j]&65535)==5 && (s[j]>>16)>=3 && s[j+1]==s[i+2]) {
      const char *candidate=(const char *)(s+j+2), *end=memchr(candidate,0,((s[j]>>16)-2)*4);
      if(!end || end==candidate || end-candidate>=64) return 0;
      name=candidate;
    }
    if(!name || p->uniform_count==PS4GL_MAX_UNIFORMS) return 0;
    if(!strncmp(name,"ps4gl_cube_",11)) {name+=11;if(!*name)return 0;}
    if(!strncmp(name,"ps4gl_1d_",9)) {
      name+=9;if(!*name) return 0;p->one_dimensional_sampler_mask|=1u<<(binding-2);
    }
    for(unsigned u=0;u<p->uniform_count;++u) if(!strcmp(p->uniforms[u].name,name)) return 0;
    PS4GLUniform *u=&p->uniforms[p->uniform_count++];
    *u=(PS4GLUniform){.offset={-1,-1},.sampler_binding=(unsigned)binding}; strcpy(u->name,name);
    p->sampler_mask|=1u<<(binding-2);
  }
  return 1;
}
