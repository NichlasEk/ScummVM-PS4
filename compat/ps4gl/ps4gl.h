#ifndef PS4GL_H
#define PS4GL_H
/* Experimental single-thread GL clear/state subset, not a complete GL context.
 * The owner supplies an active render pass covering one RGBA8 UNORM attachment.
 * No Vulkan resource ownership, SDL context, draw API or version is advertised. */
#include <vulkan/vulkan.h>
#include <stdint.h>
typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef int GLint;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLboolean;
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
} PS4GLContext;
typedef void (*PS4GLProc)(void);
int ps4glInit(PS4GLContext *, GLint width, GLint height);
void ps4glMakeCurrent(PS4GLContext *);
/* Attach/detach only while the caller owns the command buffer's recording. */
void ps4glBeginPass(VkCommandBuffer);
void ps4glEndPass(void);
PS4GLProc ps4glGetProcAddress(const char *);
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
