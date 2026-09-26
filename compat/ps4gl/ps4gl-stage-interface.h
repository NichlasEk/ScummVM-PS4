/* Bounded generated-SPIR-V interface check. glslang Vulkan linking permits
 * absent producers; GL program linking must reject those before native import.
 * This is not a general SPIR-V validator. Inputs here come from glslang. */
static const uint32_t *stage_instruction(const uint32_t *code, size_t words, unsigned op, uint32_t id) {
  for(size_t i=5;i<words;i+=code[i]>>16)
    if((code[i]&0xffff)==op && (code[i]>>16)>=2 && code[i+1]==id) return &code[i];
  return NULL;
}
static int stage_locations(const uint32_t *code, size_t bytes, unsigned storage, unsigned locations[16]) {
  size_t words=bytes/4;
  if(bytes%4 || words<5 || code[0]!=0x07230203) return 0;
  for(size_t i=5;i<words;) { unsigned count=code[i]>>16; if(!count || count>words-i) return 0; i+=count; }
  for(size_t i=5;i<words;i+=code[i]>>16) {
    if((code[i]&0xffff)!=59 || (code[i]>>16)<4 || code[i+3]!=storage) continue;
    uint32_t id=code[i+2]; unsigned location=16; int builtin=0, component=0;
    for(size_t j=5;j<words;j+=code[j]>>16) if((code[j]&0xffff)==71 && (code[j]>>16)>=4 && code[j+1]==id) {
      if(code[j+2]==11) builtin=1;
      if(code[j+2]==30) location=code[j+3];
      if(code[j+2]==31) component=code[j+3]!=0;
    }
    if(builtin) continue;
    const uint32_t *pointer=stage_instruction(code,words,32,code[i+1]);
    if(!pointer || (pointer[0]>>16)!=4) return 0;
    if(location==16) {
      /* The vertex gl_PerVertex block carries member BuiltIn decorations. */
      const uint32_t *block=stage_instruction(code,words,30,pointer[3]);
      if(storage!=3 || !block) return 0;
      unsigned members=(block[0]>>16)-2;
      for(unsigned member=0;member<members;++member) {
        int decorated=0;
        for(size_t j=5;j<words;j+=code[j]>>16) if((code[j]&0xffff)==72 && (code[j]>>16)>=5 &&
            code[j+1]==pointer[3] && code[j+2]==member && code[j+3]==11) decorated=1;
        if(!decorated) return 0;
      }
      continue;
    }
    if(location>=16 || component || locations[location]) return 0;
    uint32_t scalar=pointer[3]; unsigned width=1;
    const uint32_t *vector=stage_instruction(code,words,23,scalar);
    if(vector) { if((vector[0]>>16)!=4) return 0; scalar=vector[2]; width=vector[3]; }
    const uint32_t *type=stage_instruction(code,words,22,scalar);
    if(!type || (type[0]>>16)!=3 || type[2]!=32 || !width || width>4) return 0;
    locations[location]=width;
  }
  return 1;
}
static int compatible_stages(const uint32_t *vs, size_t vb, const uint32_t *fs, size_t fb) {
  unsigned outputs[16]={0}, inputs[16]={0};
  if(!stage_locations(vs,vb,3,outputs) || !stage_locations(fs,fb,1,inputs)) return 0;
  for(unsigned i=0;i<16;++i) if(inputs[i] && inputs[i]!=outputs[i]) return 0;
  return 1;
}
