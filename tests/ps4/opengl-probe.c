// SPDX-License-Identifier: GPL-3.0-or-later
/* A host run validates the test oracle, never PS4 graphics support. The PS4
 * binary dynamically loads Piglet and fails explicitly if it is unavailable. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __PS4__
#include "piglet-context.h"
#else
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <dlfcn.h>
#endif

#define GL_FUNCTIONS(X) \
 X(GetString) X(GetBooleanv) X(GetError) X(GetIntegerv) \
 X(CreateShader) X(ShaderSource) X(CompileShader) X(GetShaderiv) X(GetShaderInfoLog) X(DeleteShader) \
 X(CreateProgram) X(AttachShader) X(BindAttribLocation) X(LinkProgram) X(GetProgramiv) \
 X(GetProgramInfoLog) X(UseProgram) X(GetUniformLocation) X(Uniform1i) X(Uniform1f) X(Uniform4f) X(DeleteProgram) \
 X(GenTextures) X(BindTexture) X(TexParameteri) X(TexImage2D) X(DeleteTextures) X(ActiveTexture) \
 X(GenFramebuffers) X(BindFramebuffer) X(FramebufferTexture2D) X(CheckFramebufferStatus) X(DeleteFramebuffers) \
 X(GenRenderbuffers) X(BindRenderbuffer) X(RenderbufferStorage) X(FramebufferRenderbuffer) X(DeleteRenderbuffers) \
 X(GenBuffers) X(BindBuffer) X(BufferData) X(DeleteBuffers) X(VertexAttribPointer) X(EnableVertexAttribArray) \
 X(Viewport) X(ClearColor) X(ClearDepthf) X(Clear) X(DrawArrays) X(ReadPixels) X(Finish) \
 X(Enable) X(Disable) X(BlendFunc) X(DepthFunc)
static struct {
#define FIELD(name) __typeof__(&gl##name) name;
    GL_FUNCTIONS(FIELD)
#undef FIELD
} gl;
static FILE *logfile;

static int load_gl(void *(*resolve)(void *, const char *), void *context) {
#define LOAD(name) gl.name = (__typeof__(gl.name))resolve(context, "gl" #name); \
    if (!gl.name) { fprintf(logfile, "MISSING gl%s\n", #name); return 0; }
    GL_FUNCTIONS(LOAD)
#undef LOAD
    return 1;
}
static GLuint shader(GLenum type, const char *source, int expected_success) {
    GLuint object = gl.CreateShader(type);
    if (!object) return 0;
    gl.ShaderSource(object, 1, &source, NULL);
    gl.CompileShader(object);
    GLint success = 0;
    gl.GetShaderiv(object, GL_COMPILE_STATUS, &success);
    char message[4096]; GLsizei length = 0;
    gl.GetShaderInfoLog(object, sizeof(message), &length, message);
    fprintf(logfile, "SHADER type=0x%x compiled=%d expected=%d log=%.*s\n", type, success, expected_success, length, message);
    if (!success || !expected_success) { gl.DeleteShader(object); return success == expected_success ? (expected_success ? 0 : 1) : 0; }
    return object;
}
static GLuint texture(int width, int height, const unsigned char *data) {
    GLuint object = 0;
    gl.GenTextures(1, &object); gl.BindTexture(GL_TEXTURE_2D, object);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    return object;
}
static int pixels(const char *name, const unsigned char expected[4][4], unsigned tolerance) {
    unsigned char data[8 * 8 * 4];
    memset(data, 0xcd, sizeof(data));
    gl.Finish(); gl.ReadPixels(0, 0, 8, 8, GL_RGBA, GL_UNSIGNED_BYTE, data);
    GLenum error = gl.GetError();
    if (error) { fprintf(logfile, "FAIL %s GL error=0x%x\n", name, error); return 0; }
    for (unsigned y = 0; y < 8; ++y) for (unsigned x = 0; x < 8; ++x) for (unsigned c = 0; c < 4; ++c) {
        int actual = data[(y * 8 + x) * 4 + c];
        int wanted = expected[(y >= 4) * 2 + (x >= 4)][c];
        if ((unsigned)abs(actual - wanted) > tolerance) {
            fprintf(logfile, "FAIL %s (%u,%u) channel=%u expected=%d actual=%d\n", name, x, y, c, wanted, actual);
            return 0;
        }
    }
    fprintf(logfile, "PASS %s: 64 RGBA pixels\n", name); return 1;
}

static int render_tests(void) {
    const char *version = (const char *)gl.GetString(GL_VERSION);
    const char *vendor = (const char *)gl.GetString(GL_VENDOR);
    const char *renderer = (const char *)gl.GetString(GL_RENDERER);
    const char *language = (const char *)gl.GetString(GL_SHADING_LANGUAGE_VERSION);
    const char *extensions = (const char *)gl.GetString(GL_EXTENSIONS);
    fprintf(logfile, "GL_VERSION=%s\nGL_VENDOR=%s\nGL_RENDERER=%s\nGLSL=%s\nEXTENSIONS=%s\n",
        version ? version : "NULL", vendor ? vendor : "NULL", renderer ? renderer : "NULL",
        language ? language : "NULL", extensions ? extensions : "NULL");
    if (!version || !renderer) return 0;
    GLboolean compiler = GL_FALSE;
    gl.GetBooleanv(GL_SHADER_COMPILER, &compiler);
    fprintf(logfile, "GL_SHADER_COMPILER=%u\n", compiler);
    if (!compiler) {
        fprintf(logfile, "BLOCKED: no source shader compiler; an offline shader route is required.\n");
        return 0;
    }
    GLint texture_limit = 0, attributes = 0;
    gl.GetIntegerv(GL_MAX_TEXTURE_SIZE, &texture_limit);
    gl.GetIntegerv(GL_MAX_VERTEX_ATTRIBS, &attributes);
    fprintf(logfile, "MAX_TEXTURE_SIZE=%d MAX_VERTEX_ATTRIBS=%d\n", texture_limit, attributes);
    const char *vertex = "attribute vec2 a_position; varying vec2 v_uv; uniform float u_z;"
        "void main(){v_uv=a_position*.5+.5;gl_Position=vec4(a_position,u_z,1.0);}";
    const char *fragment = "precision mediump float; varying vec2 v_uv; uniform sampler2D u_texture;"
        "uniform vec4 u_tint; void main(){gl_FragColor=texture2D(u_texture,v_uv)*u_tint;}";
    GLuint vs = 0, fs = 0, program = 0, input = 0, target[2] = {0}, fbo[2] = {0}, depth = 0, buffer = 0;
    int passed = 0;
    vs = shader(GL_VERTEX_SHADER, vertex, 1); if (!vs) goto done;
    fs = shader(GL_FRAGMENT_SHADER, fragment, 1); if (!fs) goto done;
    program = gl.CreateProgram(); gl.AttachShader(program, vs); gl.AttachShader(program, fs);
    gl.BindAttribLocation(program, 0, "a_position"); gl.LinkProgram(program);
    GLint linked = 0; gl.GetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char message[4096]; GLsizei length = 0;
        gl.GetProgramInfoLog(program, sizeof(message), &length, message);
        fprintf(logfile, "FAIL link: %.*s\n", length, message); goto done;
    }
    gl.UseProgram(program);
    GLint tint = gl.GetUniformLocation(program, "u_tint"), z = gl.GetUniformLocation(program, "u_z");
    GLint sampler = gl.GetUniformLocation(program, "u_texture");
    if (tint < 0 || z < 0 || sampler < 0) { fprintf(logfile, "FAIL uniform lookup\n"); goto done; }
    gl.Uniform1i(sampler, 0); gl.Uniform1f(z, 0); gl.Uniform4f(tint, 1, 1, 1, 1);
    gl.ActiveTexture(GL_TEXTURE0); gl.Disable(GL_DITHER); gl.Disable(GL_BLEND); gl.Disable(GL_DEPTH_TEST);
    gl.GenFramebuffers(2, fbo);
    for (unsigned i = 0; i < 2; ++i) {
        target[i] = texture(8, 8, NULL);
        gl.BindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
        gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target[i], 0);
        if (gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            fprintf(logfile, "FAIL color FBO %u\n", i); goto done;
        }
    }
    const unsigned char colors[4][4] = {{255,0,0,255}, {0,255,0,255}, {0,0,255,255}, {255,255,0,255}};
    input = texture(2, 2, &colors[0][0]);
    const GLfloat vertices[] = {-1,-1, 1,-1, -1,1, 1,1};
    gl.GenBuffers(1, &buffer); gl.BindBuffer(GL_ARRAY_BUFFER, buffer);
    gl.BufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    gl.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL); gl.EnableVertexAttribArray(0);
    gl.Viewport(0, 0, 8, 8); gl.BindFramebuffer(GL_FRAMEBUFFER, fbo[0]);
    gl.ClearColor(0, 0, 0, 0); gl.Clear(GL_COLOR_BUFFER_BIT); gl.DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    if (!pixels("textured quad + VBO + FBO", colors, 0)) goto done;
    gl.BindFramebuffer(GL_FRAMEBUFFER, fbo[1]); gl.BindTexture(GL_TEXTURE_2D, target[0]);
    gl.Clear(GL_COLOR_BUFFER_BIT); gl.DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    if (!pixels("render-to-texture sampling", colors, 0)) goto done;
    const unsigned char white[4] = {255,255,255,255};
    gl.BindTexture(GL_TEXTURE_2D, input);
    gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    gl.ClearColor(0, 0, 1, 1); gl.Clear(GL_COLOR_BUFFER_BIT);
    gl.Enable(GL_BLEND); gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl.Uniform4f(tint, 1, 0, 0, .5f); gl.DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    const unsigned char blended[4][4] = {{128,0,127,191},{128,0,127,191},{128,0,127,191},{128,0,127,191}};
    if (!pixels("alpha blending", blended, 1)) goto done;
    gl.Disable(GL_BLEND); gl.Enable(GL_DEPTH_TEST); gl.DepthFunc(GL_LESS);
    gl.GenRenderbuffers(1, &depth); gl.BindRenderbuffer(GL_RENDERBUFFER, depth);
    gl.RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, 8, 8);
    gl.FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    if (gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(logfile, "FAIL depth FBO\n"); goto done;
    }
    gl.ClearDepthf(1); gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    gl.Uniform1f(z, -.5f); gl.Uniform4f(tint, 1, 0, 0, 1); gl.DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    gl.Uniform1f(z, .5f); gl.Uniform4f(tint, 0, 1, 0, 1); gl.DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    const unsigned char red[4][4] = {{255,0,0,255},{255,0,0,255},{255,0,0,255},{255,0,0,255}};
    if (!pixels("depth rejection", red, 0)) goto done;
    if (!shader(GL_FRAGMENT_SHADER, "not valid GLSL", 0)) {
        fprintf(logfile, "FAIL invalid shader accepted\n"); goto done;
    }
    /* Show the four-color result on the native surface too. */
    gl.BindFramebuffer(GL_FRAMEBUFFER, 0); gl.Disable(GL_DEPTH_TEST);
    gl.Viewport(0, 0, 1280, 720); gl.BindTexture(GL_TEXTURE_2D, target[0]);
    gl.Uniform1f(z, 0); gl.Uniform4f(tint, 1, 1, 1, 1); gl.DrawArrays(GL_TRIANGLE_STRIP, 0, 4); gl.Finish();
    passed = gl.GetError() == GL_NO_ERROR;
done:
    gl.UseProgram(0); gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
    gl.DeleteBuffers(1, &buffer); gl.DeleteTextures(1, &input); gl.DeleteTextures(2, target);
    gl.DeleteFramebuffers(2, fbo); gl.DeleteRenderbuffers(1, &depth);
    if (program) gl.DeleteProgram(program);
    if (vs) gl.DeleteShader(vs);
    if (fs) gl.DeleteShader(fs);
    return passed;
}

#ifdef __PS4__
static void *resolve(void *context, const char *name) { return ps4_piglet_proc(context, name); }
#else
static void *resolve(void *context, const char *name) {
    (void)context;
    void *p = (void *)eglGetProcAddress(name);
    return p ? p : dlsym(RTLD_DEFAULT, name);
}
#endif
int main(int argc, char **argv) {
    (void)argc; (void)argv;
#ifdef __PS4__
    logfile = fopen("/data/scummvm-opengl-probe.log", "w");
#else
    logfile = stdout;
#endif
    if (!logfile) return 1;
    setvbuf(logfile, NULL, _IONBF, 0);
    int passed = 0;
#ifdef __PS4__
    fprintf(logfile, "PS4 Piglet/GLES2 probe v1; no software fallback\n");
    Ps4Piglet *context = ps4_piglet_load(logfile);
    if (context && ps4_piglet_create(context, 1280, 720) && load_gl(resolve, context)) {
        passed = render_tests();
        if (passed) passed = ps4_piglet_swap(context);
    }
    ps4_piglet_close(context);
#else
    fprintf(logfile, "HOST oracle validation ONLY; not evidence of PS4 rendering\n");
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLConfig config;
    EGLint count = 0;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    int initialized = 0;
    const EGLint attrs[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE};
    const EGLint pbufattrs[] = {EGL_WIDTH, 1280, EGL_HEIGHT, 720, EGL_NONE};
    const EGLint ctxattrs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    if (display == EGL_NO_DISPLAY || !eglInitialize(display, NULL, NULL)) goto cleanup;
    initialized = 1;
    if (!eglBindAPI(EGL_OPENGL_ES_API) || !eglChooseConfig(display, attrs, &config, 1, &count) || !count) goto cleanup;
    surface = eglCreatePbufferSurface(display, config, pbufattrs);
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctxattrs);
    if (surface == EGL_NO_SURFACE || context == EGL_NO_CONTEXT || !eglMakeCurrent(display, surface, surface, context)) goto cleanup;
    if (load_gl(resolve, NULL)) passed = render_tests();
cleanup:
    if (!passed) fprintf(logfile, "HOST EGL error=0x%04x\n", eglGetError());
    if (initialized) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context) eglDestroyContext(display, context);
        if (surface) eglDestroySurface(display, surface);
        eglTerminate(display);
    }
#endif
    fprintf(logfile, "%s\n", passed ? "PASS: context, GLSL, textures, FBO, blending, depth and pixels" : "NOT READY: see failed stage above");
    if (logfile != stdout) fclose(logfile);
    return passed ? 0 : 2;
}
