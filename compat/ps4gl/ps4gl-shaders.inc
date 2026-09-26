/* Shader names occupy a disjoint range; program names retain native slots.
 * Linked executables and their command references remain owned until teardown. */
static PS4GLShader *shader(GLuint name) {
  if(current) for(unsigned i=0;i<PS4GL_MAX_SHADERS;++i) if(current->shaders[i].name==name && name) return &current->shaders[i];
  return NULL;
}
static void release_shader(PS4GLShader *s) {
  if(s && s->deleted && !s->references) { free(s->source); free(s->compiled_source); *s=(PS4GLShader){0}; }
}
static void detach_all(PS4GLProgram *p) {
  for(unsigned i=0;i<2;++i) if(p->attached[i]) {
    PS4GLShader *s=shader(p->attached[i]);
    if(s) { --s->references; release_shader(s); }
    p->attached[i]=0;
  }
}
static void copy_text(const char *source, GLsizei capacity, GLsizei *length, char *out) {
  if(capacity<0 || (capacity && !out)) { error(GL_INVALID_VALUE); return; }
  size_t n=source ? strlen(source) : 0;
  if(!capacity) n=0; else if(n>=(size_t)capacity) n=(size_t)capacity-1;
  if(capacity) { if(n) memcpy(out,source,n); out[n]=0; }
  if(length) *length=(GLsizei)n;
}
GLuint glCreateShader(GLenum type) {
  if(!current) return 0;
  if(type!=GL_VERTEX_SHADER && type!=GL_FRAGMENT_SHADER) { error(GL_INVALID_ENUM); return 0; }
  if(!current->next_shader) { error(GL_OUT_OF_MEMORY); return 0; }
  for(unsigned i=0;i<PS4GL_MAX_SHADERS;++i) if(!current->shaders[i].name) {
    current->shaders[i]=(PS4GLShader){.name=current->next_shader++,.type=type}; return current->shaders[i].name;
  }
  error(GL_OUT_OF_MEMORY); return 0;
}
GLboolean glIsShader(GLuint name) { return shader(name)!=NULL; }
void glShaderSource(GLuint name, GLsizei count, const char *const *strings, const GLint *lengths) {
  if(!current) return;
  PS4GLShader *s=shader(name); if(!s) { error(GL_INVALID_VALUE); return; }
  if(count<0 || (count && !strings)) { error(GL_INVALID_VALUE); return; }
  size_t bytes=0;
  for(GLsizei i=0;i<count;++i) {
    if(!strings[i]) { error(GL_INVALID_VALUE); return; }
    size_t n=0;
    if(lengths && lengths[i]>=0) n=(size_t)lengths[i];
    else while(n<=PS4GL_SHADER_SOURCE_LIMIT && strings[i][n]) ++n;
    if(n>PS4GL_SHADER_SOURCE_LIMIT-bytes) { error(GL_OUT_OF_MEMORY); return; }
    /* NUL within an explicitly sized fragment is outside this text-only subset. */
    if(memchr(strings[i],0,n)) { error(GL_INVALID_VALUE); return; }
    bytes+=n;
  }
  char *copy=malloc(bytes+1); if(!copy) { error(GL_OUT_OF_MEMORY); return; }
  size_t offset=0;
  for(GLsizei i=0;i<count;++i) {
    size_t n=lengths && lengths[i]>=0 ? (size_t)lengths[i] : strlen(strings[i]);
    memcpy(copy+offset,strings[i],n); offset+=n;
  }
  copy[offset]=0; free(s->source); s->source=copy;
  /* GL source replacement does not change the last compilation until Compile. */
}
void glCompileShader(GLuint name) {
  if(!current) return;
  PS4GLShader *s=shader(name); if(!s) { error(GL_INVALID_VALUE); return; }
  s->compiled=0; free(s->compiled_source); s->compiled_source=NULL; s->log[0]=0;
  if(!current->compile_source) { snprintf(s->log,sizeof(s->log),"No shader compiler configured"); return; }
  uint32_t *words=NULL; size_t bytes=0;
  if(!current->compile_source(s->type==GL_FRAGMENT_SHADER,s->source ? s->source : "",&words,&bytes,s->log,sizeof(s->log))) { free(words); return; }
  free(words);
  size_t length=strlen(s->source ? s->source : "");
  s->compiled_source=malloc(length+1);
  if(!s->compiled_source) { error(GL_OUT_OF_MEMORY); snprintf(s->log,sizeof(s->log),"Cannot retain compiled source"); return; }
  memcpy(s->compiled_source,s->source ? s->source : "",length+1); s->compiled=1;
}
void glDeleteShader(GLuint name) {
  if(!current || !name) return;
  PS4GLShader *s=shader(name); if(!s) { error(GL_INVALID_VALUE); return; }
  s->deleted=1; release_shader(s);
}
void glGetShaderiv(GLuint name, GLenum pname, GLint *out) {
  if(!current) return;
  PS4GLShader *s=shader(name); if(!s || !out) { error(GL_INVALID_VALUE); return; }
  switch(pname) {
    case GL_SHADER_TYPE:*out=s->type; break;
    case GL_COMPILE_STATUS:*out=s->compiled; break;
    case GL_DELETE_STATUS:*out=s->deleted; break;
    case GL_INFO_LOG_LENGTH:*out=s->log[0] ? (GLint)strlen(s->log)+1 : 0; break;
    case GL_SHADER_SOURCE_LENGTH:*out=s->source ? (GLint)strlen(s->source)+1 : 0; break;
    default:error(GL_INVALID_ENUM);
  }
}
void glGetShaderInfoLog(GLuint name, GLsizei cap, GLsizei *length, char *out) {
  if(!current) return;
  PS4GLShader *s=shader(name); if(!s) { error(GL_INVALID_VALUE); return; } copy_text(s->log,cap,length,out);
}
void glGetShaderSource(GLuint name, GLsizei cap, GLsizei *length, char *out) {
  if(!current) return;
  PS4GLShader *s=shader(name); if(!s) { error(GL_INVALID_VALUE); return; } copy_text(s->source,cap,length,out);
}
GLuint glCreateProgram(void) {
  if(!current) return 0;
  for(unsigned i=0;i<PS4GL_MAX_PROGRAMS;++i) if(!current->programs[i].pipeline && !current->programs[i].allocated) {
    current->programs[i].allocated=1; return i+1;
  }
  error(GL_OUT_OF_MEMORY); return 0;
}
void glAttachShader(GLuint name, GLuint shader_name) {
  if(!current) return;
  PS4GLProgram *p=program(name); PS4GLShader *s=shader(shader_name);
  if(!p || !s) { error(GL_INVALID_VALUE); return; }
  unsigned stage=s->type==GL_FRAGMENT_SHADER;
  if(p->attached[stage]) { error(GL_INVALID_OPERATION); return; }
  p->attached[stage]=shader_name; ++s->references;
}
void glDetachShader(GLuint name, GLuint shader_name) {
  if(!current) return;
  PS4GLProgram *p=program(name); PS4GLShader *s=shader(shader_name);
  if(!p || !s) { error(GL_INVALID_VALUE); return; }
  unsigned stage=s->type==GL_FRAGMENT_SHADER;
  if(p->attached[stage]!=shader_name) { error(GL_INVALID_OPERATION); return; }
  p->attached[stage]=0; --s->references; release_shader(s);
}
void glGetProgramInfoLog(GLuint name, GLsizei cap, GLsizei *length, char *out) {
  if(!current) return;
  PS4GLProgram *p=program(name); if(!p) { error(GL_INVALID_VALUE); return; } copy_text(p->log,cap,length,out);
}
void glLinkProgram(GLuint name) {
  if(!current) return;
  PS4GLProgram *p=program(name); if(!p) { error(GL_INVALID_VALUE); return; }
  /* Relinking live native executables needs versioned GPU storage, a later gate. */
  if(p->pipeline) { error(GL_INVALID_OPERATION); return; }
  p->log[0]=0;
  PS4GLShader *vs=shader(p->attached[0]), *fs=shader(p->attached[1]);
  if(!vs || !fs || !vs->compiled || !fs->compiled) { snprintf(p->log,sizeof(p->log),"One compiled vertex and fragment shader required"); return; }
  if(!current->link_sources || !current->import_program) { snprintf(p->log,sizeof(p->log),"No program linker configured"); return; }
  uint32_t *v=NULL,*f=NULL; size_t vb=0,fb=0;
  if(!current->link_sources(vs->compiled_source,fs->compiled_source,p->bindings,p->binding_count,&v,&vb,&f,&fb,p->log,sizeof(p->log))) { free(v); free(f); return; }
  GLuint attached[2]={p->attached[0],p->attached[1]}; GLboolean deleted=p->deleted;
  PS4GLAttributeBinding bindings[8]; memcpy(bindings,p->bindings,sizeof(bindings)); unsigned binding_count=p->binding_count;
  GLenum previous=current->error; current->error=GL_NO_ERROR;
  current->link_destination=name;
  GLuint imported=current->import_program(current->program_device,current->program_pass,v,vb,f,fb);
  current->link_destination=0;
  free(v); free(f);
  if(!imported) {
    GLenum native_error=current->error; current->error=previous;
    if(native_error==GL_OUT_OF_MEMORY) error(GL_OUT_OF_MEMORY);
    snprintf(p->log,sizeof(p->log),"Native program import rejected linked interface (0x%x)",native_error); return;
  }
  current->error=previous;
  if(imported!=name) { *p=current->programs[imported-1]; current->programs[imported-1]=(PS4GLProgram){0}; }
  memcpy(p->bindings,bindings,sizeof(bindings)); p->binding_count=binding_count;
  p->allocated=1; p->deleted=deleted; p->attached[0]=attached[0]; p->attached[1]=attached[1];
}
void ps4glDestroyShaderObjects(PS4GLContext *ctx) {
  if(!ctx) return;
  if(ctx->command) { if(!ctx->error) ctx->error=GL_INVALID_OPERATION; return; }
  for(unsigned i=0;i<PS4GL_MAX_SHADERS;++i) {
    free(ctx->shaders[i].source); free(ctx->shaders[i].compiled_source); ctx->shaders[i]=(PS4GLShader){0};
  }
  for(unsigned i=0;i<PS4GL_MAX_PROGRAMS;++i) ctx->programs[i].attached[0]=ctx->programs[i].attached[1]=0;
  ctx->compile_source=NULL; ctx->link_sources=NULL;
}

void ps4glConfigureShaderCompiler(VkRenderPass pass, PS4GLCompileSource compile, PS4GLLinkSources link) {
  if(!current) return;
  if(!pass || !compile || !link || current->command || !current->program_device || !current->import_program ||
      (current->program_pass && current->program_pass!=pass)) { error(GL_INVALID_OPERATION); return; }
  current->program_pass=pass; current->compile_source=compile; current->link_sources=link;
}

void glBindAttribLocation(GLuint name, GLuint index, const GLchar *attribute) {
  if(!current) return;
  PS4GLProgram *p=program(name);
  if(!p || !attribute || index>=PS4GL_MAX_ATTRIBS) { error(GL_INVALID_VALUE); return; }
  if(!strncmp(attribute,"gl_",3)) { error(GL_INVALID_OPERATION); return; }
  size_t length=strlen(attribute);
  if(!length || length>=64) { error(GL_INVALID_VALUE); return; }
  unsigned slot;
  for(slot=0;slot<p->binding_count;++slot) if(!strcmp(p->bindings[slot].name,attribute)) break;
  if(slot==8) { error(GL_OUT_OF_MEMORY); return; }
  if(slot==p->binding_count) ++p->binding_count;
  strcpy(p->bindings[slot].name,attribute); p->bindings[slot].index=index;
}
GLint glGetAttribLocation(GLuint name, const GLchar *attribute) {
  if(!current) return -1;
  PS4GLProgram *p=program(name);
  if(!p || !attribute) { error(GL_INVALID_VALUE); return -1; }
  if(!p->pipeline) { error(GL_INVALID_OPERATION); return -1; }
  for(unsigned i=0;i<PS4GL_MAX_ATTRIBS;++i) if(p->components[i] && !strcmp(p->attribute_names[i],attribute)) return (GLint)i;
  return -1;
}
