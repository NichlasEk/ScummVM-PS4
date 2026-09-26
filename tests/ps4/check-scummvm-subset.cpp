/* Exercise the injected loader against ScummVM's actual GLAD declarations. */
#define GLAD_GL_IMPLEMENTATION
#include "graphics/opengl/glad.h"
#include <cassert>
#include <climits>
#include <cstring>
#include <string>
#include <stdexcept>
namespace Common { using String=std::string; }
static const char *missing=nullptr;
static bool wrong_renderer=false;
static GLint texture_limit=1024;
static void unused() {}
static const GLubyte *identity(GLenum) {
 return reinterpret_cast<const GLubyte *>(wrong_renderer ? "wrong" : "OpenGNM Vulkan bridge (experimental)");
}
static void limit(GLenum key, GLint *out) { assert(key==GL_MAX_TEXTURE_SIZE); *out=texture_limit; }
static GLenum no_error() { return GL_NO_ERROR; }
static GLADapiproc loadFunc(const char *name) {
 assert(!strncmp(name,"gl",2) && strncmp(name,"glad",4));
 if(missing && !strcmp(name,missing)) return nullptr;
 if(!strcmp(name,"glGetString")) return reinterpret_cast<GLADapiproc>(identity);
 if(!strcmp(name,"glGetIntegerv")) return reinterpret_cast<GLADapiproc>(limit);
 if(!strcmp(name,"glGetError")) return reinterpret_cast<GLADapiproc>(no_error);
 return unused;
}
[[noreturn]] static void error(const char *message, ...) { throw std::runtime_error(message); }
static void debug(int,const char *) {}
struct Context {
 enum { kContextGL=1 };
 int type=kContextGL, majorVersion=0, minorVersion=0, glslVersion=0;
 unsigned maxTextureSize=0,textureLookupPrecision=0;
 bool shadersSupported=false,NPOTSupported=false,textureEdgeClampSupported=false;
 bool enginesShadersSupported=false,framebufferObjectSupported=false,multitextureSupported=false;
 void initialize() {
#include "scummvm-context.inc"
 }
};
int main() {
 Context ok; ok.initialize();
 assert(ok.shadersSupported && ok.NPOTSupported && ok.textureEdgeClampSupported && ok.maxTextureSize==1024);
 assert(!ok.majorVersion && !ok.minorVersion && ok.enginesShadersSupported && !ok.framebufferObjectSupported && ok.multitextureSupported && ok.glslVersion==120);
 assert(!GLAD_GL_VERSION_2_0 && !GLAD_GL_ARB_framebuffer_object && glReadPixels);
 for(int failure=0;failure<4;++failure) {
  Context c;
  missing=failure==0 ? "glDrawArrays" : nullptr;
  wrong_renderer=failure==1; texture_limit=failure==2 ? 0 : 1024;
  if(failure==3) c.type=99;
  bool rejected=false;
  try { c.initialize(); } catch(const std::runtime_error &) { rejected=true; }
  assert(rejected && !c.shadersSupported);
 }
 puts("PASS explicit GLAD subset, missing entry, renderer identity, limit and context rejection; explicit engine shader subset without GL version promotion");
}
