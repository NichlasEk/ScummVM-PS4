#ifndef PS4GL_H
#define PS4GL_H
/* Experimental single-thread GL state/draw subset, not a complete GL context.
 * Owner supplies a full-target color render pass. Clears require RGBA8 UNORM;
 * imported draws also work with the tested RGBA8 SRGB presentation pass.
 * Programs own Vulkan pipelines; SDL and a GL version are not advertised. */
#include <vulkan/vulkan.h>
#include <stdint.h>
#include <stddef.h>
typedef unsigned int GLuint;
typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef int GLint;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLboolean;
#define GL_TRIANGLES 0x0004
#define GL_CURRENT_PROGRAM 0x8b8d
#define GL_LINK_STATUS 0x8b82
#define GL_DELETE_STATUS 0x8b80
#define GL_OUT_OF_MEMORY 0x0505
#define PS4GL_MAX_PROGRAMS 16
/* Slots are retained through context teardown: recorded GPU commands may still
 * refer to programs after glDeleteProgram. Names are never recycled. */
typedef struct PS4GLProgram {
  VkPipeline pipeline;
  VkPipelineLayout layout;
  GLboolean deleted, retired;
} PS4GLProgram;
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_NO_ERROR 0
#define GL_INVALID_ENUM 0x0500
#define GL_INVALID_VALUE 0x0501
#define GL_INVALID_OPERATION 0x0502
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_DEPTH_BUFFER_BIT 0x0100
#define GL_STENCIL_BUFFER_BIT 0x0400
#define GL_SCISSOR_TEST 0x0c11
#define GL_SCISSOR_BOX 0x0c10
#define GL_VIEWPORT 0x0ba2
#define GL_COLOR_CLEAR_VALUE 0x0c22
typedef struct PS4GLContext {
  VkCommandBuffer command;
  GLint width, height, scissor[4], viewport[4];
  GLfloat clear[4];
  GLenum error;
  GLboolean scissor_enabled;
  GLuint program;
  VkDevice program_device;
  VkRenderPass program_pass;
  PS4GLProgram programs[PS4GL_MAX_PROGRAMS];
} PS4GLContext;
typedef void (*PS4GLProc)(void);
int ps4glInit(PS4GLContext *, GLint width, GLint height);
void ps4glMakeCurrent(PS4GLContext *);
/* Attach/detach only while the caller owns the command buffer's recording. */
void ps4glBeginPass(VkCommandBuffer);
void ps4glEndPass(void);
PS4GLProc ps4glGetProcAddress(const char *);
PS4GLContext *ps4glCurrentContext(void);
/* Explicit Vulkan-SPIR-V import, not glCompileShader/glLinkProgram. No resources
 * or vertex attributes in this first draw subset. Clip coordinates must already
 * use Vulkan conventions (Y flipped and depth mapped to [0,1]). */
GLuint ps4glCreateProgramSPIRV(VkDevice, VkRenderPass,
                             const uint32_t *, size_t, const uint32_t *, size_t);
/* Owner must end recording and retire ALL commands using these objects first. */
void ps4glDestroyPrograms(VkDevice, PS4GLContext *);
void glUseProgram(GLuint);
void glDeleteProgram(GLuint);
GLboolean glIsProgram(GLuint);
void glGetProgramiv(GLuint, GLenum, GLint *);
void glDrawArrays(GLenum, GLint, GLsizei);
void glClearColor(GLfloat, GLfloat, GLfloat, GLfloat);
void glClear(GLbitfield);
void glScissor(GLint, GLint, GLsizei, GLsizei);
void glViewport(GLint, GLint, GLsizei, GLsizei);
void glEnable(GLenum);
void glDisable(GLenum);
GLboolean glIsEnabled(GLenum);
GLenum glGetError(void);
void glGetIntegerv(GLenum, GLint *);
void glGetFloatv(GLenum, GLfloat *);
#endif
