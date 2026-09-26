#ifndef PS4GL_H
#define PS4GL_H
/* Experimental single-thread GL state/draw subset, not a complete GL context.
 * Owner supplies a full-target color render pass. Clears require RGBA8 UNORM;
 * imported draws also work with the tested RGBA8 SRGB presentation pass.
 * Programs own Vulkan pipelines; SDL and a GL version are not advertised. */
#include <vulkan/vulkan.h>
#include <stdint.h>
#include <stddef.h>
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;
typedef unsigned int GLuint;
typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef int GLint;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLboolean;
#define GL_TEXTURE_2D 0x0de1
#define GL_TEXTURE0 0x84c0
#define GL_ACTIVE_TEXTURE 0x84e0
#define GL_TEXTURE_BINDING_2D 0x8069
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#define GL_REPEAT 0x2901
#define GL_CLAMP_TO_EDGE 0x812f
#define GL_RGBA 0x1908
#define GL_RGBA8 0x8058
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNPACK_ALIGNMENT 0x0cf5
#define PS4GL_TEXTURE_UNITS 4
#define PS4GL_MAX_TEXTURES 32
#define PS4GL_TEXTURE_LIMIT 1024
#define PS4GL_TEXTURE_BUDGET (64u*1024u*1024u)
typedef struct PS4GLTexture {
  GLuint name;
  GLboolean created;
  unsigned char *pixels;
  GLsizei width,height;
  GLenum min_filter,mag_filter,wrap_s,wrap_t;
} PS4GLTexture;
typedef struct PS4GLTextureSnapshot {
  VkImage image;
  VkDeviceMemory memory;
  VkImageView view;
  VkSampler sampler;
} PS4GLTextureSnapshot;
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_ELEMENT_ARRAY_BUFFER_BINDING 0x8895
#define GL_UNSIGNED_SHORT 0x1403
#define GL_UNSIGNED_INT 0x1405
#define GL_ARRAY_BUFFER 0x8892
#define GL_ARRAY_BUFFER_BINDING 0x8894
#define GL_BUFFER_SIZE 0x8764
#define GL_BUFFER_USAGE 0x8765
#define GL_STATIC_DRAW 0x88e4
#define GL_DYNAMIC_DRAW 0x88e8
#define GL_STREAM_DRAW 0x88e0
#define GL_FLOAT 0x1406
#define PS4GL_MAX_BUFFERS 32
#define PS4GL_MAX_ATTRIBS 8
#define PS4GL_MAX_SNAPSHOTS 64
#define PS4GL_MAX_BUFFER_BYTES (16u*1024u*1024u)
typedef struct PS4GLBuffer {
  GLuint name;
  unsigned char *data;
  size_t size;
  GLenum usage;
  GLboolean created;
} PS4GLBuffer;
typedef struct PS4GLAttrib {
  GLuint buffer;
  GLint size;
  GLsizei stride;
  uintptr_t offset;
  GLboolean enabled;
} PS4GLAttrib;
typedef struct PS4GLSnapshot {
  VkBuffer buffer;
  VkDeviceMemory memory;
  VkDescriptorPool pool;
  PS4GLTextureSnapshot textures[2];
} PS4GLSnapshot;
#define GL_TRIANGLES 0x0004
#define GL_CURRENT_PROGRAM 0x8b8d
#define GL_LINK_STATUS 0x8b82
#define GL_DELETE_STATUS 0x8b80
#define GL_OUT_OF_MEMORY 0x0505
#define PS4GL_MAX_PROGRAMS 16
/* Slots are retained through context teardown: recorded GPU commands may still
 * refer to programs after glDeleteProgram. Names are never recycled. */
#define PS4GL_MAX_UNIFORMS 16
#define PS4GL_UNIFORM_BYTES 256
/* Imported default-uniform blocks: set 0, binding 0 vertex / 1 fragment. */
typedef struct PS4GLUniform {
  char name[64];
  unsigned width; /* 1..4 floats or 16 for column-major mat4 */
  int offset[2];
  unsigned sampler_binding; /* 0 for float uniforms, 2/3 for sampler2D */
  unsigned array_length, array_stride[2]; /* zero length means non-array */
} PS4GLUniform;
typedef struct PS4GLProgram {
  VkPipeline pipeline;
  VkPipelineLayout layout;
  VkDescriptorSetLayout descriptors;
  unsigned uniform_stages, uniform_count, sampler_mask;
  GLint sampler_units[2];
  PS4GLUniform uniforms[PS4GL_MAX_UNIFORMS];
  unsigned char uniform_data[2][PS4GL_UNIFORM_BYTES];
  GLboolean deleted, retired;
  unsigned components[PS4GL_MAX_ATTRIBS], vertex_stride;
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
  PS4GLBuffer buffers[PS4GL_MAX_BUFFERS];
  PS4GLAttrib attribs[PS4GL_MAX_ATTRIBS];
  GLuint array_buffer, element_buffer, next_buffer;
  PS4GLTexture textures[PS4GL_MAX_TEXTURES];
  GLuint texture_bindings[PS4GL_TEXTURE_UNITS], next_texture;
  unsigned active_texture, unpack_alignment;
  size_t texture_bytes;
  VkPhysicalDevice physical;
  PS4GLSnapshot snapshots[PS4GL_MAX_SNAPSHOTS];
  unsigned snapshot_count;
  size_t snapshot_bytes;
  /* Immutable payload: packed vertices followed by indices; index type 0 for arrays. */
  int (*upload_uniforms)(struct PS4GLContext *, const PS4GLProgram *);
  int (*upload_vertices)(struct PS4GLContext *, const void *, size_t, size_t, GLenum);
} PS4GLContext;
typedef void (*PS4GLProc)(void);
int ps4glInit(PS4GLContext *, GLint width, GLint height);
void ps4glMakeCurrent(PS4GLContext *);
/* Attach/detach only while the caller owns the command buffer's recording. */
void ps4glBeginPass(VkCommandBuffer);
void ps4glEndPass(void);
PS4GLProc ps4glGetProcAddress(const char *);
PS4GLContext *ps4glCurrentContext(void);
/* Explicit Vulkan-SPIR-V import, not glCompileShader/glLinkProgram. Bounded float default-uniform blocks;
 * plain float vertex inputs at locations 0..7 are supported. Clip coordinates must already
 * use Vulkan conventions (Y flipped and depth mapped to [0,1]). */
GLuint ps4glCreateProgramSPIRV(VkDevice, VkRenderPass,
                             const uint32_t *, size_t, const uint32_t *, size_t);
/* Owner must end recording and retire ALL commands using these objects first. */
void ps4glDestroyPrograms(VkDevice, PS4GLContext *);
void glGenBuffers(GLsizei, GLuint *);
void glBindBuffer(GLenum, GLuint);
void glDeleteBuffers(GLsizei, const GLuint *);
GLboolean glIsBuffer(GLuint);
void glBufferData(GLenum, GLsizeiptr, const void *, GLenum);
void glBufferSubData(GLenum, GLintptr, GLsizeiptr, const void *);
void glGetBufferParameteriv(GLenum, GLenum, GLint *);
void glVertexAttribPointer(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
void glEnableVertexAttribArray(GLuint);
void glDisableVertexAttribArray(GLuint);
/* Pack active inputs into the imported program's canonical float layout. */
int ps4glPackVertices(PS4GLContext *, const PS4GLProgram *, GLsizei, void *);
void ps4glDestroyBuffers(PS4GLContext *);
void ps4glConfigureVertexDevice(VkPhysicalDevice, VkDevice);
/* Only after fences complete AND all referencing command buffers are reset. */
void ps4glReleaseVertexSnapshots(PS4GLContext *);
void glUseProgram(GLuint);
void glDeleteProgram(GLuint);
GLboolean glIsProgram(GLuint);
void glGetProgramiv(GLuint, GLenum, GLint *);
GLint glGetUniformLocation(GLuint, const char *);
void glUniform1i(GLint, GLint);
void glUniform1iv(GLint, GLsizei, const GLint *);
void glGenTextures(GLsizei, GLuint *);
void glBindTexture(GLenum, GLuint);
void glDeleteTextures(GLsizei, const GLuint *);
GLboolean glIsTexture(GLuint);
void glActiveTexture(GLenum);
void glPixelStorei(GLenum, GLint);
void glTexParameteri(GLenum, GLenum, GLint);
void glTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *);
void glTexSubImage2D(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *);
const PS4GLTexture *ps4glTextureAtUnit(PS4GLContext *, unsigned);
void ps4glDestroyTextures(PS4GLContext *);
void glUniform1f(GLint, GLfloat);
void glUniform2f(GLint, GLfloat, GLfloat);
void glUniform3f(GLint, GLfloat, GLfloat, GLfloat);
void glUniform4f(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
void glUniform1fv(GLint, GLsizei, const GLfloat *);
void glUniform2fv(GLint, GLsizei, const GLfloat *);
void glUniform3fv(GLint, GLsizei, const GLfloat *);
void glUniform4fv(GLint, GLsizei, const GLfloat *);
void glUniformMatrix4fv(GLint, GLsizei, GLboolean, const GLfloat *);
void glDrawArrays(GLenum, GLint, GLsizei);
void glDrawElements(GLenum, GLsizei, GLenum, const void *);
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
