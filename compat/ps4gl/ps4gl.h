#ifndef PS4GL_H
#define PS4GL_H
/* Experimental single-thread GL state/draw subset, not a complete GL context.
 * Owner supplies a full-target color render pass. Clears require RGBA8 UNORM;
 * imported draws also work with the tested RGBA8 SRGB presentation pass.
 * Programs own Vulkan pipelines; SDL and a GL version are not advertised. */
#include <vulkan/vulkan.h>
#include <stdint.h>
#include <stddef.h>
#include "ps4gl-bindings.h"
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_EXTENSIONS 0x1F03
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#define GL_TEXTURE_RECTANGLE 0x84f5
#define GL_TEXTURE_BINDING_RECTANGLE 0x84f6
#define GL_MAX_TEXTURE_SIZE 0x0D33
#define GL_MAX_TEXTURE_IMAGE_UNITS 0x8872
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS 0x8B4D
#define GL_MAX_VERTEX_ATTRIBS 0x8869
#define GL_RED_BITS 0x0D52
#define GL_GREEN_BITS 0x0D53
#define GL_BLUE_BITS 0x0D54
#define GL_ALPHA 0x1906
#define GL_LUMINANCE 0x1909
#define GL_LUMINANCE_ALPHA 0x190A
#define GL_ALPHA_BITS 0x0D55
#define GL_DEPTH_BITS 0x0D56
#define GL_STENCIL_BITS 0x0D57
#define GL_DOUBLEBUFFER 0x0C32
#define GL_STEREO 0x0C33
#define GL_SAMPLE_BUFFERS 0x80A8
#define GL_SAMPLES 0x80A9
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;
typedef unsigned int GLuint;
typedef unsigned int GLenum;
const unsigned char *glGetString(GLenum);
typedef unsigned int GLbitfield;
typedef int GLint;
typedef int GLsizei;
typedef float GLfloat;
typedef char GLchar;
typedef unsigned char GLboolean;
#define GL_FRAMEBUFFER 0x8d40
#define GL_FRAMEBUFFER_BINDING 0x8ca6
#define GL_COLOR_ATTACHMENT0 0x8ce0
#define GL_FRAMEBUFFER_COMPLETE 0x8cd5
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT 0x8cd6
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT 0x8cd7
#define GL_INVALID_FRAMEBUFFER_OPERATION 0x0506
#define PS4GL_MAX_FRAMEBUFFERS 16
#define PS4GL_MAX_TARGETS 32
typedef struct PS4GLFramebuffer { GLuint name, texture; unsigned texture_slot; GLboolean created; } PS4GLFramebuffer;
typedef struct PS4GLRenderTarget {
  VkImage image; VkDeviceMemory memory; VkImageView view; VkFramebuffer framebuffer;
  GLsizei width,height;
  VkImageLayout layout;
  VkImage depth_image; VkDeviceMemory depth_memory; VkImageView depth_view;
  size_t depth_bytes;
} PS4GLRenderTarget;
#define GL_BLEND 0x0be2
#define GL_ZERO 0
#define GL_ONE 1
#define GL_SRC_COLOR 0x0300
#define GL_ONE_MINUS_SRC_COLOR 0x0301
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_DST_ALPHA 0x0304
#define GL_ONE_MINUS_DST_ALPHA 0x0305
#define GL_DST_COLOR 0x0306
#define GL_ONE_MINUS_DST_COLOR 0x0307
#define GL_FUNC_ADD 0x8006
#define GL_MIN 0x8007
#define GL_MAX 0x8008
#define GL_FUNC_SUBTRACT 0x800a
#define GL_FUNC_REVERSE_SUBTRACT 0x800b
#define GL_BLEND_SRC_RGB 0x80c9
#define GL_BLEND_DST_RGB 0x80c8
#define GL_BLEND_SRC_ALPHA 0x80cb
#define GL_BLEND_DST_ALPHA 0x80ca
#define GL_BLEND_EQUATION_RGB 0x8009
#define GL_BLEND_EQUATION_ALPHA 0x883d
#define PS4GL_BLEND_VARIANTS 16
typedef struct PS4GLBlend {
  unsigned enabled;
  GLenum src_rgb,dst_rgb,src_alpha,dst_alpha,equation_rgb,equation_alpha;
} PS4GLBlend;
#define GL_STENCIL_TEST 0x0b90
#define GL_POLYGON_OFFSET_FILL 0x8037
#define GL_COLOR_WRITEMASK 0x0c23
#define GL_NEVER 0x0200
#define GL_LESS 0x0201
#define GL_EQUAL 0x0202
#define GL_LEQUAL 0x0203
#define GL_GREATER 0x0204
#define GL_NOTEQUAL 0x0205
#define GL_GEQUAL 0x0206
#define GL_ALWAYS 0x0207
#define GL_KEEP 0x1e00
#define GL_REPLACE 0x1e01
#define GL_TEXTURE_ENV 0x2300
#define GL_TEXTURE_ENV_MODE 0x2200
#define GL_MODULATE 0x2100
#define GL_ADD 0x0104
#define GL_COMBINE 0x8570
#define GL_COMBINE_RGB 0x8571
#define GL_COMBINE_ALPHA 0x8572
#define GL_TEXTURE 0x1702
#define GL_TEXTURE_ENV_COLOR 0x2201
#define GL_CONSTANT 0x8576
#define GL_PRIMARY_COLOR 0x8577
#define GL_PREVIOUS 0x8578
#define GL_INTERPOLATE 0x8575
#define GL_SOURCE0_RGB 0x8580
#define GL_SOURCE1_RGB 0x8581
#define GL_SOURCE2_RGB 0x8582
#define GL_SOURCE0_ALPHA 0x8588
#define GL_SOURCE1_ALPHA 0x8589
#define GL_SOURCE2_ALPHA 0x858a
#define GL_OPERAND0_RGB 0x8590
#define GL_OPERAND1_RGB 0x8591
#define GL_OPERAND2_RGB 0x8592
#define GL_OPERAND0_ALPHA 0x8598
#define GL_OPERAND1_ALPHA 0x8599
#define GL_OPERAND2_ALPHA 0x859a
#define GL_INCR 0x1e02
#define GL_DECR 0x1e03
#define GL_INVERT 0x150a
#define GL_INCR_WRAP 0x8507
#define GL_DECR_WRAP 0x8508
typedef struct PS4GLRaster {
  unsigned color_mask,depth_enabled,depth_write,stencil_enabled;
  GLenum depth_func,stencil_func,stencil_fail,stencil_depth_fail,stencil_pass;
  unsigned stencil_ref,stencil_mask,stencil_write_mask;
  GLenum back_stencil_func,back_stencil_fail,back_stencil_depth_fail,back_stencil_pass;
  unsigned back_stencil_ref,back_stencil_mask,back_stencil_write_mask;
} PS4GLRaster;
#define PS4GL_RASTER_DEFAULT { .color_mask=15,.depth_write=1,.depth_func=GL_LESS, .stencil_func=GL_ALWAYS, .stencil_fail=GL_KEEP,.stencil_depth_fail=GL_KEEP,.stencil_pass=GL_KEEP,.stencil_mask=255,.stencil_write_mask=255, .back_stencil_func=GL_ALWAYS,.back_stencil_fail=GL_KEEP,.back_stencil_depth_fail=GL_KEEP,.back_stencil_pass=GL_KEEP,.back_stencil_mask=255,.back_stencil_write_mask=255 }
typedef struct PS4GLBlendVariant { PS4GLBlend state; PS4GLRaster raster; VkPipeline pipeline; VkRenderPass pass; } PS4GLBlendVariant;
#define GL_BGR 0x80e0
#define GL_BGRA 0x80e1
#define GL_RED 0x1903
#define GL_RG 0x8227
#define GL_TEXTURE_CUBE_MAP 0x8513
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X 0x8515
#define GL_TEXTURE_BINDING_CUBE_MAP 0x8514
#define GL_TEXTURE_1D 0x0de0
#define GL_TEXTURE_BINDING_1D 0x8068
#define GL_TEXTURE_2D 0x0de1
#define GL_TEXTURE0 0x84c0
#define GL_ACTIVE_TEXTURE 0x84e0
#define GL_TEXTURE_BINDING_2D 0x8069
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_TEXTURE_WRAP_R 0x8072
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#define GL_REPEAT 0x2901
#define GL_MIRRORED_REPEAT 0x8370
#define GL_CLAMP_TO_EDGE 0x812f
#define GL_FRAMEBUFFER_UNSUPPORTED 0x8cdd
#define GL_RGB 0x1907
#define GL_RGB8 0x8051
#define GL_UNSIGNED_SHORT_5_6_5 0x8363
#define GL_RGBA 0x1908
#define GL_RGBA8 0x8058
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNPACK_ALIGNMENT 0x0cf5
#define GL_PACK_ALIGNMENT 0x0d05
#define PS4GL_TEXTURE_UNITS PS4GL_MAX_SAMPLERS
#define PS4GL_MAX_TEXTURES 1024
#define PS4GL_TEXTURE_LIMIT 2048
#define PS4GL_TEXTURE_BUDGET (256u*1024u*1024u)
typedef struct PS4GLTexture {
  GLuint name;
  GLboolean created;
  unsigned char *pixels;
  GLsizei width,height;
  GLenum min_filter,mag_filter,wrap_s,wrap_t,wrap_r;
  GLboolean rgb_storage, cpu_contents_defined, owned_depth_stencil;
  GLenum internal_format, target;
  unsigned cube_faces; /* Six base-level faces packed vertically for the 2D-only native sampler. */
  unsigned render_target;
  uint64_t revision;
  GLboolean deleted;
} PS4GLTexture;
typedef struct PS4GLTextureSnapshot {
  VkImage image;
  VkDeviceMemory memory;
  VkImageView view;
  VkSampler sampler;
  GLboolean borrowed_view, borrowed_snapshot;
  size_t allocation_bytes;
  GLuint source_name;
  uint64_t source_revision;
  unsigned source_target;
  GLenum min_filter,mag_filter,wrap_s,wrap_t,wrap_r;
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
#define PS4GL_MAX_BUFFERS 4096
#define PS4GL_MAX_ATTRIBS 8
#define PS4GL_MAX_SNAPSHOTS 4096
#define PS4GL_UPLOAD_PAGE_BYTES (1024u*1024u)
#define PS4GL_UPLOAD_BUDGET (64u*1024u*1024u)
#define PS4GL_MAX_UPLOAD_PAGES 64
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
  GLfloat constant[4]; /* Current generic value when the array is disabled. */
} PS4GLAttrib;
typedef struct PS4GLUploadPage {
  VkBuffer buffer;
  VkDeviceMemory memory;
  void *mapped;
  size_t size,used,allocation_bytes;
} PS4GLUploadPage;
typedef struct PS4GLSnapshot {
  VkBuffer buffer;
  VkDescriptorPool pool;
  VkDeviceSize offset; /* Slice in a frame-owned upload page. */
  PS4GLTextureSnapshot textures[PS4GL_MAX_SAMPLERS];
} PS4GLSnapshot;
#define GL_TRIANGLES 0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_CURRENT_PROGRAM 0x8b8d
#define GL_VERTEX_SHADER 0x8b31
#define GL_FRAGMENT_SHADER 0x8b30
#define GL_COMPILE_STATUS 0x8b81
#define GL_INFO_LOG_LENGTH 0x8b84
#define GL_SHADER_SOURCE_LENGTH 0x8b88
#define GL_SHADER_TYPE 0x8b4f
#define GL_ATTACHED_SHADERS 0x8b85
#define PS4GL_MAX_SHADERS 32
#define PS4GL_SHADER_SOURCE_LIMIT (256u*1024u)
typedef int (*PS4GLCompileSource)(int, const char *, uint32_t **, size_t *, char *, size_t);
typedef int (*PS4GLLinkSources)(const char *, const char *, const PS4GLAttributeBinding *, size_t, uint32_t **, size_t *, uint32_t **, size_t *, char *, size_t);
typedef struct PS4GLShader {
  GLuint name;
  GLenum type;
  char *source, *compiled_source;
  char log[2048];
  unsigned references;
  GLboolean deleted, compiled;
} PS4GLShader;
#define GL_LINK_STATUS 0x8b82
#define GL_DELETE_STATUS 0x8b80
#define GL_OUT_OF_MEMORY 0x0505
#define GL_SAMPLES_PASSED 0x8914
#define GL_QUERY_RESULT 0x8866
#define GL_QUERY_RESULT_AVAILABLE 0x8867
#define PS4GL_MAX_QUERIES 128
typedef struct PS4GLQuery {
  GLuint name;
  VkQueryPool pool;
  GLboolean used, deleted, pending, available, recorded;
  GLuint result;
} PS4GLQuery;
#define PS4GL_MAX_PROGRAMS 128
/* Slots are retained through context teardown: recorded GPU commands may still
 * refer to programs after glDeleteProgram. Names are never recycled. */
#define PS4GL_MAX_UNIFORMS 64
#define PS4GL_UNIFORM_BYTES 8192
/* Imported default-uniform blocks: set 0, binding 0 vertex / 1 fragment. */
typedef struct PS4GLUniform {
  char name[64];
  unsigned width; /* 1..4 floats or 16 for column-major mat4 */
  unsigned integer; /* 0=float, 1=signed int32, 2=translated scalar bool */
  int offset[2];
  unsigned sampler_binding; /* 0 for block uniforms, 2..9 for texture samplers */
  unsigned array_length, array_stride[2]; /* zero length means non-array */
} PS4GLUniform;
typedef struct PS4GLProgram {
  GLboolean allocated;
  GLuint attached[2];
  PS4GLAttributeBinding bindings[8];
  unsigned binding_count;
  char attribute_names[8][64];
  char log[2048];
  VkPipeline pipeline;
  VkShaderModule modules[2];
  PS4GLBlendVariant variants[PS4GL_BLEND_VARIANTS];
  unsigned variant_count;
  VkPipelineLayout layout;
  VkDescriptorSetLayout descriptors;
  unsigned uniform_stages, uniform_count, sampler_mask, rectangle_sampler_mask, one_dimensional_sampler_mask, cube_sampler_mask;
  GLint sampler_units[PS4GL_MAX_SAMPLERS];
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
#define GL_CULL_FACE 0x0b44
#define GL_DEPTH_TEST 0x0b71
#define GL_DEPTH_WRITEMASK 0x0b72
#define GL_DITHER 0x0bd0
#define GL_SCISSOR_TEST 0x0c11
#define GL_SCISSOR_BOX 0x0c10
#define GL_VIEWPORT 0x0ba2
#define GL_COLOR_CLEAR_VALUE 0x0c22
/* Bounded immediate-mode adapter used by the experimental Watchmaker port. */
#define GL_MODELVIEW 0x1700
#define GL_PROJECTION 0x1701
#define GL_MATRIX_MODE 0x0ba0
#define GL_MODELVIEW_MATRIX 0x0ba6
#define GL_PROJECTION_MATRIX 0x0ba7
#define GL_QUADS 0x0007
#define GL_ALPHA_TEST 0x0bc0
#define GL_SMOOTH 0x1d01
#define GL_BACK 0x0405
#define GL_FRONT 0x0404
#define GL_FRONT_AND_BACK 0x0408
#define GL_CW 0x0900
#define GL_CCW 0x0901
#define GL_STACK_OVERFLOW 0x0503
#define GL_STACK_UNDERFLOW 0x0504
// Keep whole triangles/quads while amortizing draw/descriptor work.
#define PS4GL_FIXED_VERTICES 3072
#define PS4GL_MATRIX_DEPTH 32
typedef struct PS4GLFixedVertex { GLfloat position[3], color[4], uv[3], extra_uv[PS4GL_TEXTURE_UNITS-1][3]; } PS4GLFixedVertex;
typedef struct PS4GLClientArray {
  GLint size; GLenum type; GLsizei stride; const void *pointer; GLuint buffer; GLboolean enabled;
} PS4GLClientArray;
typedef struct PS4GLFixed {
  PS4GLClientArray clients[10]; /* position, color, normal, seven UV sets */
  unsigned client_texture;

  GLfloat matrix[2][PS4GL_MATRIX_DEPTH][16], color[4], uv[3], extra_uv[PS4GL_TEXTURE_UNITS-1][3], alpha_ref;
  unsigned top[2], matrix_mode, count, pending_count;
  GLenum primitive, alpha_func, cull_face, front_face;
  GLboolean active, texture_enabled[PS4GL_TEXTURE_UNITS], alpha_enabled;
  GLenum env_mode[PS4GL_TEXTURE_UNITS], env_rgb[PS4GL_TEXTURE_UNITS], env_alpha[PS4GL_TEXTURE_UNITS];
  GLenum env_source_rgb[PS4GL_TEXTURE_UNITS][3], env_source_alpha[PS4GL_TEXTURE_UNITS][3];
  GLenum env_operand_rgb[PS4GL_TEXTURE_UNITS][3], env_operand_alpha[PS4GL_TEXTURE_UNITS][3];
  GLfloat env_color[PS4GL_TEXTURE_UNITS][4];
  GLuint programs[2], buffer;
  PS4GLFixedVertex vertices[PS4GL_FIXED_VERTICES], pending[4];
} PS4GLFixed;
void glTexEnvi(GLenum, GLenum, GLint);
void glTexEnvfv(GLenum, GLenum, const GLfloat *);
void glGenQueries(GLsizei, GLuint *);
void glDeleteQueries(GLsizei, const GLuint *);
GLboolean glIsQuery(GLuint);
void glBeginQuery(GLenum, GLuint);
void glEndQuery(GLenum);
void glGetQueryObjectiv(GLuint, GLenum, GLint *);
void glGetQueryObjectuiv(GLuint, GLenum, GLuint *);
void glHint(GLenum, GLenum);
void glEnableClientState(GLenum);
void glDisableClientState(GLenum);
void glClientActiveTexture(GLenum);
void glVertexPointer(GLint, GLenum, GLsizei, const void *);
void glColorPointer(GLint, GLenum, GLsizei, const void *);
void glNormalPointer(GLenum, GLsizei, const void *);
void glTexCoordPointer(GLint, GLenum, GLsizei, const void *);
void glOrtho(double, double, double, double, double, double);
void glMatrixMode(GLenum);
void glLoadIdentity(void);
void glLoadMatrixf(const GLfloat *);
void glPushMatrix(void);
void glPopMatrix(void);
void glScalef(GLfloat, GLfloat, GLfloat);
void glTranslatef(GLfloat, GLfloat, GLfloat);
void glBegin(GLenum);
void glEnd(void);
void glVertex3f(GLfloat, GLfloat, GLfloat);
void glColor3f(GLfloat, GLfloat, GLfloat);
void glColor4f(GLfloat, GLfloat, GLfloat, GLfloat);
void glMultiTexCoord3f(GLenum, GLfloat, GLfloat, GLfloat);
void glTexCoord3f(GLfloat, GLfloat, GLfloat);
void glTexCoord2f(GLfloat, GLfloat);
void glAlphaFunc(GLenum, GLfloat);
void glShadeModel(GLenum);
void glCullFace(GLenum);
void glFrontFace(GLenum);
void glFlush(void);
void glCompressedTexImage2D(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void *);
typedef struct PS4GLContext {
  PS4GLFixed fixed;
  VkCommandBuffer command;
  GLint width, height, scissor[4], viewport[4];
  GLfloat clear[4];
  GLenum error;
  GLboolean scissor_enabled, depth_write_mask, default_depth_stencil, polygon_offset_fill;
  PS4GLRaster raster;
  GLfloat clear_depth;
  GLint clear_stencil;
  PS4GLBlend blend;
  GLuint program;
  VkDevice program_device;
  VkRenderPass program_pass;
  PS4GLQuery queries[PS4GL_MAX_QUERIES];
  GLuint next_query;
  unsigned active_query;
  int (*query_command)(struct PS4GLContext *, PS4GLQuery *, int);
  void (*complete_queries)(struct PS4GLContext *);
  PS4GLProgram programs[PS4GL_MAX_PROGRAMS];
  PS4GLShader shaders[PS4GL_MAX_SHADERS];
  GLuint next_shader, link_destination;
  PS4GLCompileSource compile_source;
  PS4GLLinkSources link_sources;
  GLuint (*import_program)(VkDevice, VkRenderPass, const uint32_t *, size_t, const uint32_t *, size_t);
  PS4GLBuffer buffers[PS4GL_MAX_BUFFERS];
  PS4GLAttrib attribs[PS4GL_MAX_ATTRIBS];
  GLuint array_buffer, element_buffer, next_buffer;
  PS4GLTexture textures[PS4GL_MAX_TEXTURES];
  PS4GLFramebuffer framebuffers[PS4GL_MAX_FRAMEBUFFERS];
  GLuint framebuffer, next_framebuffer;
  PS4GLRenderTarget targets[PS4GL_MAX_TARGETS];
  size_t target_bytes;
  VkRenderPass offscreen_pass, offscreen_depth_pass, active_pass, default_pass;
  VkFramebuffer default_framebuffer;
  VkImage default_image;
  GLint default_width,default_height;
  unsigned active_target;
  GLboolean managed_frame,pass_open,discard_target_contents;
  int (*prepare_target)(struct PS4GLContext *);
  void (*end_target)(struct PS4GLContext *);
  GLuint texture_bindings[PS4GL_TEXTURE_UNITS], rectangle_bindings[PS4GL_TEXTURE_UNITS], one_dimensional_bindings[PS4GL_TEXTURE_UNITS], cube_bindings[PS4GL_TEXTURE_UNITS], next_texture;
  unsigned active_texture, unpack_alignment, pack_alignment;
  size_t texture_bytes;
  uint64_t next_texture_revision;
#define PS4GL_TEXTURE_CACHE_SLOTS 128
// TWP room atlases exceed 32 MiB; keep them resident across frames.
#define PS4GL_TEXTURE_CACHE_BUDGET (128u*1024u*1024u)
  PS4GLTextureSnapshot texture_cache[PS4GL_TEXTURE_CACHE_SLOTS];
  size_t texture_cache_bytes;
  unsigned texture_snapshot_uploads, texture_snapshot_reuses;
  VkPhysicalDevice physical;
  PS4GLSnapshot *snapshots;
  unsigned snapshot_count,snapshot_capacity;
  PS4GLUploadPage upload_pages[PS4GL_MAX_UPLOAD_PAGES];
  unsigned upload_page_count;
  size_t upload_memory_bytes;
  const char *allocation_failure;
  size_t snapshot_bytes;
  /* Immutable payload: packed vertices followed by indices; index type 0 for arrays. */
  VkPipeline (*select_pipeline)(struct PS4GLContext *, PS4GLProgram *);
  int (*upload_uniforms)(struct PS4GLContext *, const PS4GLProgram *);
  int (*begin_frame)(struct PS4GLContext *); /* optional lazy owner: 1 success */
  int (*copy_texture)(struct PS4GLContext *, PS4GLTexture *, GLint, GLint, GLint, GLint, GLsizei, GLsizei);
  int (*read_pixels)(struct PS4GLContext *, GLint, GLint, GLsizei, GLsizei, void *);
  int (*upload_vertices)(struct PS4GLContext *, const void *, size_t, size_t, GLenum);
} PS4GLContext;
typedef void (*PS4GLProc)(void);
int ps4glInit(PS4GLContext *, GLint width, GLint height);
void ps4glMakeCurrent(PS4GLContext *);
/* Attach/detach only while the caller owns the command buffer's recording. */
void ps4glBeginPass(VkCommandBuffer);
void ps4glEndPass(void);
/* Managed frame: command recording is active, but no Vulkan render pass is open.
 * Default pass must LOAD/STORE in GENERAL; owner transitions default image.
 * Native backend must support signed viewport heights (OpenGNM does). */
void ps4glBeginFrame(VkCommandBuffer, VkRenderPass, VkFramebuffer, GLint, GLint);
void ps4glEndFrame(void);
/* Private attachment storage for the backend 3D target, not general renderbuffers. */
void ps4glFramebufferDepthStencil(void);
int ps4glHasDepthStencil(PS4GLContext *);
void glGenFramebuffers(GLsizei, GLuint *);
void glBindFramebuffer(GLenum, GLuint);
void glDeleteFramebuffers(GLsizei, const GLuint *);
GLboolean glIsFramebuffer(GLuint);
void glFramebufferTexture2D(GLenum, GLenum, GLenum, GLuint, GLint);
GLenum glCheckFramebufferStatus(GLenum);
GLuint ps4glFramebufferTexture(const PS4GLContext *);
PS4GLTexture *ps4glFramebufferTextureObject(PS4GLContext *);
void ps4glDestroyFramebuffers(PS4GLContext *);
void ps4glConfigureShaderCompiler(VkRenderPass, PS4GLCompileSource, PS4GLLinkSources);
GLuint glCreateShader(GLenum);
void glShaderSource(GLuint, GLsizei, const char *const *, const GLint *);
void glCompileShader(GLuint);
void glDeleteShader(GLuint);
GLboolean glIsShader(GLuint);
void glGetShaderiv(GLuint, GLenum, GLint *);
void glGetShaderInfoLog(GLuint, GLsizei, GLsizei *, char *);
void glGetShaderSource(GLuint, GLsizei, GLsizei *, char *);
GLuint glCreateProgram(void);
void glAttachShader(GLuint, GLuint);
void glDetachShader(GLuint, GLuint);
void glLinkProgram(GLuint);
void glBindAttribLocation(GLuint, GLuint, const GLchar *);
GLint glGetAttribLocation(GLuint, const GLchar *);
void glGetProgramInfoLog(GLuint, GLsizei, GLsizei *, char *);
void ps4glDestroyShaderObjects(PS4GLContext *);
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
void glVertexAttrib2fv(GLuint, const GLfloat *);
void glVertexAttrib3fv(GLuint, const GLfloat *);
void glVertexAttrib4fv(GLuint, const GLfloat *);
void glEnableVertexAttribArray(GLuint);
void glDisableVertexAttribArray(GLuint);
/* Pack active inputs into the imported program's canonical float layout. */
int ps4glPackVertices(PS4GLContext *, const PS4GLProgram *, GLsizei, void *);
void ps4glDestroyBuffers(PS4GLContext *);
void ps4glConfigureVertexDevice(VkPhysicalDevice, VkDevice);
/* Only after fences complete AND all referencing command buffers are reset. */
void ps4glReleaseVertexSnapshots(PS4GLContext *);
void ps4glRecycleVertexSnapshots(PS4GLContext *);
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
void glReadPixels(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *);
void glTexParameteri(GLenum, GLenum, GLint);
void glCopyTexImage2D(GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint);
void glCopyTexSubImage2D(GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei);
void glTexImage1D(GLenum, GLint, GLint, GLsizei, GLint, GLenum, GLenum, const void *);
void glTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *);
void glTexSubImage2D(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *);
const PS4GLTexture *ps4glCubeAtUnit(PS4GLContext *, unsigned);
const PS4GLTexture *ps4glOneDimensionalAtUnit(PS4GLContext *, unsigned);
const PS4GLTexture *ps4glRectangleAtUnit(PS4GLContext *, unsigned);
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
void glBlendFunc(GLenum, GLenum);
void glBlendFuncSeparate(GLenum, GLenum, GLenum, GLenum);
void glBlendEquation(GLenum);
void glBlendEquationSeparate(GLenum, GLenum);
void glEnable(GLenum);
void glDisable(GLenum);
GLboolean glIsEnabled(GLenum);
void glColorMask(GLboolean,GLboolean,GLboolean,GLboolean);
void glClearDepth(double);
void glClearStencil(GLint);
void glDepthFunc(GLenum);
void glStencilFuncSeparate(GLenum, GLenum, GLint, GLuint);
void glStencilOpSeparate(GLenum, GLenum, GLenum, GLenum);
void glStencilMaskSeparate(GLenum, GLuint);
void glStencilFunc(GLenum,GLint,GLuint);
void glStencilOp(GLenum,GLenum,GLenum);
void glStencilMask(GLuint);
void glDepthMask(GLboolean);
void glGetBooleanv(GLenum, GLboolean *);
GLenum glGetError(void);
void glGetIntegerv(GLenum, GLint *);
void glGetFloatv(GLenum, GLfloat *);
#endif
