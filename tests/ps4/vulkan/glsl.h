#include "ps4gl-glsl.c"
#include "glsl-sources.h"
static GLuint compile_glsl_program(VkDevice device, VkRenderPass pass, const char *vertex, const char *fragment) {
  (void)device;
  ps4glConfigureShaderCompiler(pass,ps4glCompileVulkanGLSL,ps4glLinkVulkanGLSL);
  GLuint shaders[]={glCreateShader(GL_VERTEX_SHADER),glCreateShader(GL_FRAGMENT_SHADER)};
  const char *sources[]={vertex,fragment}; GLint status; char log[2048];
  GLuint result=glCreateProgram();
  for(unsigned i=0;i<2;++i) {
    /* Explicit lengths, multiple strings, and independent copied storage. */
    const char *pieces[]={sources[i],sources[i]+13}; GLint lengths[]={13,(GLint)strlen(sources[i]+13)};
    glShaderSource(shaders[i],2,pieces,lengths); glCompileShader(shaders[i]);
    glGetShaderiv(shaders[i],GL_COMPILE_STATUS,&status);
    if(!status) { glGetShaderInfoLog(shaders[i],sizeof(log),NULL,log); fprintf(report,"Compile failed: %s\n",log); return 0; }
    glAttachShader(result,shaders[i]); glDeleteShader(shaders[i]);
    glGetShaderiv(shaders[i],GL_DELETE_STATUS,&status); if(!status) return 0;
    /* Uncompiled replacement must not change the compiled source used by link. */
    const char *replacement="invalid replacement, deliberately not compiled";
    glShaderSource(shaders[i],1,&replacement,NULL);
  }
  glLinkProgram(result); glGetProgramiv(result,GL_LINK_STATUS,&status);
  if(!status) { glGetProgramInfoLog(result,sizeof(log),NULL,log); fprintf(report,"Link failed: %s\n",log); return 0; }
  for(unsigned i=0;i<2;++i) { glDetachShader(result,shaders[i]); if(glIsShader(shaders[i])) return 0; }
  glGetProgramiv(result,GL_ATTACHED_SHADERS,&status); if(status || glGetError()) return 0;
  fprintf(report,"GLSL runtime program=%u via shader objects and cross-stage link\n",result);
  fprintf(report,"PASS GL shader objects copied source and deferred deletion\n");
  return result;
}

static int reject_glsl(void) {
  const char *bad[]={"#version 450\nvoid main( {", "#version 450\nvoid main(){gl_Position=missing;}",
    "#version 450\n#extension GL_GOOGLE_include_directive : require\n#include \"absent.glsl\"\nvoid main(){}"};
  for(unsigned i=0;i<3;++i) {
    uint32_t *words=(uint32_t *)(uintptr_t)1; size_t bytes=99; char log[256];
    if(ps4glCompileVulkanGLSL(0,bad[i],&words,&bytes,log,sizeof(log)) || words || bytes || !log[0]) return 2;
    fprintf(report,"PASS GLSL runtime rejection %u: %s\n",i,log);
  }
  GLuint vs=glCreateShader(GL_VERTEX_SHADER), fs=glCreateShader(GL_FRAGMENT_SHADER), p=glCreateProgram();
  const char *v="#version 450\nlayout(location=0) out vec2 value;void main(){value=vec2(1);gl_Position=vec4(0,0,0,1);}";
  const char *f="#version 450\nlayout(location=0) in vec3 value;layout(location=0) out vec4 result;void main(){result=vec4(value,1);}";
  glShaderSource(vs,1,&v,NULL); glCompileShader(vs);
  glShaderSource(fs,1,&f,NULL); glCompileShader(fs);
  GLint status; glGetShaderiv(vs,GL_COMPILE_STATUS,&status); if(!status) return 2;
  glGetShaderiv(fs,GL_COMPILE_STATUS,&status); if(!status) return 2;
  glAttachShader(p,vs); glAttachShader(p,fs); glLinkProgram(p);
  glGetProgramiv(p,GL_LINK_STATUS,&status); if(status || glGetError()) return 2;
  char log[256]; glGetProgramInfoLog(p,sizeof(log),NULL,log); if(!log[0]) return 2;
  glUseProgram(p); if(glGetError()!=GL_INVALID_OPERATION) return 2;
  const char *unsupported="#version 450\nlayout(set=0,binding=1) uniform Defaults{int count;};layout(location=0) out vec4 result;void main(){result=vec4(float(count));}";
  glShaderSource(fs,1,&unsupported,NULL); glCompileShader(fs);
  glGetShaderiv(fs,GL_COMPILE_STATUS,&status); if(!status) return 2;
  glLinkProgram(p); glGetProgramiv(p,GL_LINK_STATUS,&status);
  if(status || glGetError()) return 2;
  char native_log[256]; glGetProgramInfoLog(p,sizeof(native_log),NULL,native_log);
  if(!strstr(native_log,"Native program import rejected")) return 2;
  fprintf(report,"PASS GL shader objects native interface rejection\n");
  glDeleteShader(vs); glDeleteShader(fs); glDeleteProgram(p);
  if(glIsShader(vs) || glIsShader(fs)) return 2;
  fprintf(report,"PASS GL shader objects reject cross-stage mismatch: %s\n",log);
  return 0;
}
