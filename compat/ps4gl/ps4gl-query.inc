/* Bounded occlusion names; GPU pools live until the submitting fence retires. */
static PS4GLQuery *query_name(GLuint name) {
  if(!current || !name) return NULL;
  for(unsigned i=0;i<PS4GL_MAX_QUERIES;++i)
    if(current->queries[i].name==name && !current->queries[i].deleted) return &current->queries[i];
  return NULL;
}
void glGenQueries(GLsizei n, GLuint *names) {
  if(!current) return;
  if(n<0 || (n && !names)) {error(GL_INVALID_VALUE);return;}
  unsigned free_slots=0;
  for(unsigned i=0;i<PS4GL_MAX_QUERIES;++i) free_slots+=!current->queries[i].name;
  if((unsigned)n>free_slots || (unsigned)n>UINT32_MAX-current->next_query) {error(GL_OUT_OF_MEMORY);return;}
  for(GLsizei j=0;j<n;++j) for(unsigned i=0;i<PS4GL_MAX_QUERIES;++i) if(!current->queries[i].name) {
    names[j]=current->queries[i].name=current->next_query++;break;
  }
}
GLboolean glIsQuery(GLuint name) {PS4GLQuery *q=query_name(name);return q && q->used;}
void glDeleteQueries(GLsizei n, const GLuint *names) {
  if(!current) return;
  if(n<0 || (n && !names)) {error(GL_INVALID_VALUE);return;}
  for(GLsizei j=0;j<n;++j) {
    PS4GLQuery *q=query_name(names[j]);if(!q) continue;
    if(current->active_query && q==&current->queries[current->active_query-1]) {error(GL_INVALID_OPERATION);return;}
    q->deleted=1;
    if(!q->pending && !q->recorded) {
      if(q->pool && current->query_command) current->query_command(current,q,3);
      *q=(PS4GLQuery){0};
    }
  }
}
void glBeginQuery(GLenum target, GLuint name) {
  if(!fixed_outside()) return;
  if(target!=GL_SAMPLES_PASSED) {error(GL_INVALID_ENUM);return;}
  PS4GLQuery *q=query_name(name);
  if(!q || current->active_query || q->pending || q->recorded || !current->query_command) {error(GL_INVALID_OPERATION);return;}
  if(!current->command && current->begin_frame && !current->begin_frame(current)) {error(GL_INVALID_OPERATION);return;}
  if(!current->query_command(current,q,1)) {error(GL_INVALID_OPERATION);return;}
  q->used=1;q->recorded=1;q->available=0;current->active_query=(unsigned)(q-current->queries)+1;
}
void glEndQuery(GLenum target) {
  if(!fixed_outside()) return;
  if(target!=GL_SAMPLES_PASSED) {error(GL_INVALID_ENUM);return;}
  if(!current->active_query || !current->query_command) {error(GL_INVALID_OPERATION);return;}
  PS4GLQuery *q=&current->queries[current->active_query-1];
  if(!current->query_command(current,q,2)) {error(GL_INVALID_OPERATION);return;}
  q->pending=1;current->active_query=0;
}
void glGetQueryObjectuiv(GLuint name, GLenum pname, GLuint *value) {
  if(!current) return;
  PS4GLQuery *q=query_name(name);
  if(!value) {error(GL_INVALID_VALUE);return;}
  if(!q || !q->used || (current->active_query && q==&current->queries[current->active_query-1])) {error(GL_INVALID_OPERATION);return;}
  if(pname==GL_QUERY_RESULT_AVAILABLE) {*value=q->available;return;}
  if(pname!=GL_QUERY_RESULT) {error(GL_INVALID_ENUM);return;}
  /* Bounded bridge requires result reads after submission completion. */
  if(!q->available) {error(GL_INVALID_OPERATION);return;}
  *value=q->result;
}
void glGetQueryObjectiv(GLuint name, GLenum pname, GLint *value) {
  GLuint v=0;
  if(!value) {error(GL_INVALID_VALUE);return;}
  glGetQueryObjectuiv(name,pname,&v);
  *value=v>INT32_MAX ? INT32_MAX : (GLint)v;
}
