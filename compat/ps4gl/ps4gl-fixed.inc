#include "ps4gl-fixed-shaders.h"
/* Immediate triangles/quads lowered to the existing bounded shader draw path.
 * Not a complete fixed-function implementation: lighting, fog, texgen and
 * additional texture units remain unsupported. No general GL version claimed. */
static int fixed_outside(void) {
  if(!current) return 0;
  if(current->fixed.active) { error(GL_INVALID_OPERATION); return 0; }
  return 1;
}
static GLfloat *fixed_matrix(void) {
  PS4GLFixed *f=&current->fixed; return f->matrix[f->matrix_mode][f->top[f->matrix_mode]];
}
void glMatrixMode(GLenum mode) {
  if(!fixed_outside()) return;
  if(mode!=GL_MODELVIEW && mode!=GL_PROJECTION) { error(GL_INVALID_ENUM); return; }
  current->fixed.matrix_mode=mode==GL_PROJECTION;
}
void glLoadIdentity(void) {
  if(!fixed_outside()) return;
  GLfloat *m=fixed_matrix(); memset(m,0,16*sizeof(*m));
  for(unsigned i=0;i<4;++i) m[i*5]=1.f;
}
void glLoadMatrixf(const GLfloat *m) {
  if(!fixed_outside()) return;
  if(!m) { error(GL_INVALID_VALUE); return; }
  memcpy(fixed_matrix(),m,16*sizeof(*m));
}
void glPushMatrix(void) {
  if(!fixed_outside()) return;
  PS4GLFixed *f=&current->fixed; unsigned mode=f->matrix_mode, top=f->top[mode];
  if(top+1==PS4GL_MATRIX_DEPTH) { error(GL_STACK_OVERFLOW); return; }
  memcpy(f->matrix[mode][top+1],f->matrix[mode][top],sizeof(f->matrix[mode][0])); ++f->top[mode];
}
void glPopMatrix(void) {
  if(!fixed_outside()) return;
  unsigned *top=&current->fixed.top[current->fixed.matrix_mode];
  if(!*top) { error(GL_STACK_UNDERFLOW); return; } --*top;
}
void glScalef(GLfloat x, GLfloat y, GLfloat z) {
  if(!fixed_outside()) return;
  GLfloat *m=fixed_matrix();
  for(unsigned i=0;i<4;++i) { m[i]*=x; m[4+i]*=y; m[8+i]*=z; }
}
void glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
  if(!fixed_outside()) return;
  GLfloat *m=fixed_matrix();
  for(unsigned i=0;i<4;++i) m[12+i]+=m[i]*x+m[4+i]*y+m[8+i]*z;
}
void glColor3f(GLfloat r, GLfloat g, GLfloat b) {
  if(!current) return;
  GLfloat *c=current->fixed.color; c[0]=r; c[1]=g; c[2]=b; c[3]=1.f;
}
void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
  if(!current) return;
  GLfloat *c=current->fixed.color; c[0]=r; c[1]=g; c[2]=b; c[3]=a;
}
void glTexCoord2f(GLfloat u, GLfloat v) {
  if(current) { current->fixed.uv[0]=u; current->fixed.uv[1]=v; current->fixed.uv[2]=0; }
}
void glTexCoord3f(GLfloat u, GLfloat v, GLfloat r) {
  glTexCoord2f(u,v);if(current) current->fixed.uv[2]=r;
}
void glAlphaFunc(GLenum func, GLfloat ref) {
  if(!fixed_outside()) return;
  if(!compare_func(func)) { error(GL_INVALID_ENUM); return; }
  current->fixed.alpha_func=func;
  current->fixed.alpha_ref=ref>0 ? (ref<1 ? ref : 1) : 0;
}
void glShadeModel(GLenum mode) {
  if(!fixed_outside()) return;
  if(mode!=GL_SMOOTH) error(GL_INVALID_ENUM);
}
void glCullFace(GLenum mode) {
  if(!fixed_outside()) return;
  if(mode!=GL_BACK && mode!=GL_FRONT && mode!=GL_FRONT_AND_BACK) { error(GL_INVALID_ENUM); return; }
  current->fixed.cull_face=mode; /* Enabling culling still fails explicitly. */
}
void glFrontFace(GLenum mode) {
  if(!fixed_outside()) return;
  if(mode!=GL_CW && mode!=GL_CCW) { error(GL_INVALID_ENUM); return; }
  current->fixed.front_face=mode;
}
void glFlush(void) {
  /* Draws have already entered the owner's ordered command buffer. Submission
   * remains with the frame owner, as for the other bridge draw entrypoints. */
  (void)fixed_outside();
}
void glTexEnvi(GLenum target, GLenum pname, GLint value) {
  if(!fixed_outside()) return;
  if(target!=GL_TEXTURE_ENV) { error(GL_INVALID_ENUM); return; }
  PS4GLFixed *f=&current->fixed; unsigned u=current->active_texture;
  GLenum *dst=NULL;
  if(pname==GL_TEXTURE_ENV_MODE) {
    if(value!=GL_MODULATE && value!=GL_REPLACE && value!=GL_COMBINE) {error(GL_INVALID_ENUM);return;}
    dst=&f->env_mode[u];
  } else if(pname==GL_COMBINE_RGB || pname==GL_COMBINE_ALPHA) {
    if(value!=GL_MODULATE && value!=GL_REPLACE && value!=GL_ADD) {error(GL_INVALID_ENUM);return;}
    dst=pname==GL_COMBINE_RGB ? &f->env_rgb[u] : &f->env_alpha[u];
  } else if(pname==GL_OPERAND1_RGB) {
    if(value!=GL_SRC_COLOR && value!=GL_ONE_MINUS_SRC_COLOR && value!=GL_SRC_ALPHA && value!=GL_ONE_MINUS_SRC_ALPHA) {error(GL_INVALID_ENUM);return;}
    dst=&f->env_operand1[u];
  } else {error(GL_INVALID_ENUM);return;}
  *dst=value;
}
static GLuint fixed_program(int textured) {
  PS4GLFixed *f=&current->fixed;
  if(f->programs[textured]) return f->programs[textured];
  const char *vs=ps4gl_fixed_vertex; char fs[2048];
  ps4glFixedFragment(textured,fs,sizeof(fs));
  GLuint shaders[2]={glCreateShader(GL_VERTEX_SHADER),glCreateShader(GL_FRAGMENT_SHADER)};
  GLuint p=glCreateProgram(); GLint ok=0;
  for(unsigned i=0;i<2;++i) {
    const char *source=i ? fs : vs;
    glShaderSource(shaders[i],1,&source,NULL); glCompileShader(shaders[i]);
    glGetShaderiv(shaders[i],GL_COMPILE_STATUS,&ok);
    if(!ok) goto failed;
    glAttachShader(p,shaders[i]);
  }
  glBindAttribLocation(p,0,"position"); glBindAttribLocation(p,1,"color"); glBindAttribLocation(p,2,"uv");
  glLinkProgram(p); glGetProgramiv(p,GL_LINK_STATUS,&ok);
  if(!ok) goto failed;
  for(unsigned i=0;i<2;++i) { glDetachShader(p,shaders[i]); glDeleteShader(shaders[i]); }
  f->programs[textured]=p; return p;
failed:
  { char log[2048]; glGetProgramInfoLog(p,sizeof(log),NULL,log);
    fprintf(stderr,"PS4GL fixed program failed: %s\n",log); }
  glDeleteProgram(p); for(unsigned i=0;i<2;++i) glDeleteShader(shaders[i]);
  error(GL_INVALID_OPERATION); return 0;
}
static void fixed_draw(void) {
  PS4GLFixed *f=&current->fixed;
  if(!f->count) return;
  if(current->program && client_uses_arrays()) {
    PS4GLClientArray saved[10];memcpy(saved,f->clients,sizeof(saved));memset(f->clients,0,sizeof(f->clients));
    f->clients[0]=(PS4GLClientArray){.size=3,.type=GL_FLOAT,.stride=sizeof(f->vertices[0]),.pointer=f->vertices[0].position,.enabled=1};
    f->clients[1]=(PS4GLClientArray){.size=4,.type=GL_FLOAT,.stride=sizeof(f->vertices[0]),.pointer=f->vertices[0].color,.enabled=1};
    f->clients[3]=(PS4GLClientArray){.size=3,.type=GL_FLOAT,.stride=sizeof(f->vertices[0]),.pointer=f->vertices[0].uv,.enabled=1};
    client_shader_draw(GL_TRIANGLES,0,f->count,0,NULL);memcpy(f->clients,saved,sizeof(saved));f->count=0;return;
  }
  const PS4GLTexture *texture=ps4glTextureAtUnit(current,0);
  /* Fixed-function GL ignores incomplete texture units (unlike shader
   * sampling). In particular a bind of texture zero must render vertex color. */
  int textured=f->texture_enabled[0] && texture && texture->pixels && texture->width>0 && texture->height>0 &&
    (texture->min_filter==GL_NEAREST || texture->min_filter==GL_LINEAR);
  GLuint p=fixed_program(textured);
  if(!p) { f->count=0; return; }
  GLuint saved_program=current->program, saved_buffer=current->array_buffer;
  PS4GLAttrib saved_attribs[PS4GL_MAX_ATTRIBS]; memcpy(saved_attribs,current->attribs,sizeof(saved_attribs));
  glUseProgram(p);
  glUniformMatrix4fv(glGetUniformLocation(p,"projection"),1,GL_FALSE,f->matrix[1][f->top[1]]);
  glUniformMatrix4fv(glGetUniformLocation(p,"modelview"),1,GL_FALSE,f->matrix[0][f->top[0]]);
  glUniform1i(glGetUniformLocation(p,"alphaOp"),f->alpha_enabled ? (GLint)(f->alpha_func-GL_NEVER) : 7);
  glUniform1f(glGetUniformLocation(p,"alphaRef"),f->alpha_ref);
  if(textured) {
    glUniform1i(glGetUniformLocation(p,"image"),0);
    GLenum rgb=f->env_mode[0]==GL_COMBINE ? f->env_rgb[0] : f->env_mode[0];
    GLenum alpha=f->env_mode[0]==GL_COMBINE ? f->env_alpha[0] : f->env_mode[0];
    glUniform1i(glGetUniformLocation(p,"envRgb"),rgb==GL_REPLACE ? 1 : rgb==GL_ADD ? 2 : 0);
    glUniform1i(glGetUniformLocation(p,"envAlpha"),alpha==GL_REPLACE ? 1 : alpha==GL_ADD ? 2 : 0);
    glUniform1i(glGetUniformLocation(p,"envOperand"),f->env_mode[0]==GL_COMBINE ? f->env_operand1[0]-GL_SRC_COLOR : 0);
  }
  if(!f->buffer) glGenBuffers(1,&f->buffer);
  glBindBuffer(GL_ARRAY_BUFFER,f->buffer);
  glBufferData(GL_ARRAY_BUFFER,f->count*sizeof(f->vertices[0]),f->vertices,GL_DYNAMIC_DRAW);
  glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(f->vertices[0]),(const void *)offsetof(PS4GLFixedVertex,position));
  glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,sizeof(f->vertices[0]),(const void *)offsetof(PS4GLFixedVertex,color));
  glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(f->vertices[0]),(const void *)offsetof(PS4GLFixedVertex,uv));
  for(unsigned i=0;i<3;++i) glEnableVertexAttribArray(i);
  glDrawArrays(GL_TRIANGLES,0,f->count);
  /* Internal lowering must not overwrite the caller's programmable state. */
  glUseProgram(saved_program); glBindBuffer(GL_ARRAY_BUFFER,saved_buffer);
  memcpy(current->attribs,saved_attribs,sizeof(saved_attribs)); f->count=0;
}
void glBegin(GLenum mode) {
  if(!fixed_outside()) return;
  if(mode!=GL_TRIANGLES && mode!=GL_QUADS) { error(GL_INVALID_ENUM); return; }
  current->fixed.active=1; current->fixed.primitive=mode;
  current->fixed.count=current->fixed.pending_count=0;
}
void glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
  if(!current) return;
  PS4GLFixed *f=&current->fixed;
  if(!f->active) { error(GL_INVALID_OPERATION); return; }
  PS4GLFixedVertex v={{x,y,z},{0},{0}};
  memcpy(v.color,f->color,sizeof(v.color)); memcpy(v.uv,f->uv,sizeof(v.uv));
  f->pending[f->pending_count++]=v;
  unsigned group=f->primitive==GL_QUADS ? 4 : 3;
  if(f->pending_count!=group) return;
  static const unsigned quad[]={0,1,2,0,2,3};
  unsigned count=group==4 ? 6 : 3;
  for(unsigned i=0;i<count;++i) f->vertices[f->count++]=f->pending[group==4 ? quad[i] : i];
  f->pending_count=0;
  if(f->count==PS4GL_FIXED_VERTICES) fixed_draw();
}
void glEnd(void) {
  if(!current) return;
  if(!current->fixed.active) { error(GL_INVALID_OPERATION); return; }
  current->fixed.active=0; fixed_draw(); current->fixed.pending_count=0;
}
