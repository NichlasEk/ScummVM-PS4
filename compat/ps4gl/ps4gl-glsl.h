#ifndef PS4GL_GLSL_H
#define PS4GL_GLSL_H
#include <stddef.h>
#include <stdint.h>
/* Single-threaded compiler owner. Explicit Vulkan GLSL, not legacy GL source.
 * Output is caller-owned malloc storage; failure returns NULL and zero bytes. */
int ps4glGLSLInitialize(void);
void ps4glGLSLFinalize(void);
int ps4glCompileVulkanGLSL(int fragment, const char *source, uint32_t **words,
                          size_t *bytes, char *log, size_t log_capacity);
int ps4glLinkVulkanGLSL(const char *, const char *, uint32_t **, size_t *, uint32_t **, size_t *, char *, size_t);
#endif
