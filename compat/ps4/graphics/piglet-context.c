// SPDX-License-Identifier: GPL-3.0-or-later
#include "piglet-context.h"
#include <orbis/libkernel.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define EGL_FUNCTIONS(X) \
 X(eglGetDisplay) X(eglInitialize) X(eglBindAPI) X(eglChooseConfig) \
 X(eglCreateWindowSurface) X(eglCreateContext) X(eglMakeCurrent) \
 X(eglSwapBuffers) X(eglDestroySurface) X(eglDestroyContext) \
 X(eglTerminate) X(eglGetError) X(eglGetProcAddress)
struct Ps4Piglet {
    FILE *log;
    int module;
    int initialized;
    EGLDisplay display;
    EGLContext context;
    EGLSurface surface;
    OrbisPglWindow window;
    __typeof__(&scePigletSetConfigurationVSH) configure;
#define FIELD(name) __typeof__(&name) name;
    EGL_FUNCTIONS(FIELD)
#undef FIELD
};

static int resolve(Ps4Piglet *p, const char *name, void **address) {
    *address = NULL;
    int result = sceKernelDlsym(p->module, name, address);
    if (result || !*address) {
        fprintf(p->log, "SYMBOL %s result=0x%08x present=%d\n", name, (unsigned)result, *address != NULL);
        return 0;
    }
    return 1;
}

Ps4Piglet *ps4_piglet_load(FILE *log) {
    Ps4Piglet *p = calloc(1, sizeof(*p));
    if (!p) return NULL;
    p->log = log;
    p->module = -1;
    char sandbox_path[256] = {0};
    const char *sandbox = sceKernelGetFsSandboxRandomWord();
    if (sandbox && *sandbox)
        snprintf(sandbox_path, sizeof(sandbox_path), "/%s/common/lib/libScePigletv2VSH.sprx", sandbox);
    const char *paths[] = {sandbox_path, "/common/lib/libScePigletv2VSH.sprx",
        "/system/common/lib/libScePigletv2VSH.sprx", "/app0/sce_module/libScePigletv2VSH.sprx"};
    for (unsigned i = 0; i < sizeof(paths) / sizeof(*paths); ++i) {
        if (!*paths[i]) continue;
        struct stat st;
        if (stat(paths[i], &st) || !S_ISREG(st.st_mode)) {
            fprintf(log, "MODULE %s unavailable\n", paths[i]);
            continue;
        }
        int start_result = 0;
        int module = (int)sceKernelLoadStartModule(paths[i], 0, NULL, 0, NULL, &start_result);
        fprintf(log, "MODULE %s handle=0x%08x start=0x%08x\n", paths[i], (unsigned)module, (unsigned)start_result);
        if (module >= 0) { p->module = module; break; }
    }
    if (p->module < 0) {
        fprintf(log, "BLOCKED: Piglet runtime module is unavailable; no GL test was run.\n");
        free(p); return NULL;
    }
    if (!resolve(p, "scePigletSetConfigurationVSH", (void **)&p->configure)) goto failed;
#define RESOLVE(name) if (!resolve(p, #name, (void **)&p->name)) goto failed;
    EGL_FUNCTIONS(RESOLVE)
#undef RESOLVE
    return p;
failed:
    fprintf(log, "BLOCKED: required Piglet/EGL exports could not be resolved.\n");
    ps4_piglet_close(p); return NULL;
}

int ps4_piglet_create(Ps4Piglet *p, unsigned width, unsigned height) {
    if (!p || p->initialized || !width || !height || width > 1920 || height > 1080) return 0;
    OrbisPglConfig cfg = {0};
    cfg.size = sizeof(cfg);
    /* Match OpenOrbis's Piglet sample for the initial capability test. */
    cfg.flags = ORBIS_PGL_FLAGS_USE_COMPOSITE_EXT | ORBIS_PGL_FLAGS_USE_FLEXIBLE_MEMORY | 0x60;
    cfg.processOrder = 1;
    cfg.systemSharedMemorySize = 250 * 1024 * 1024;
    cfg.videoSharedMemorySize = 512 * 1024 * 1024;
    cfg.maxMappedFlexibleMemory = 170 * 1024 * 1024;
    cfg.drawCommandBufferSize = 1024 * 1024;
    cfg.lcueResourceBufferSize = 1024 * 1024;
    cfg.dbgPosCmd_0x40 = width;
    cfg.dbgPosCmd_0x44 = height;
    cfg.unk_0x5C = 2;
    if (!p->configure(&cfg)) {
        fprintf(p->log, "FAIL: scePigletSetConfigurationVSH\n"); return 0;
    }
    p->display = p->eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major = 0, minor = 0, count = 0;
    EGLConfig config;
    const EGLint attrs[] = {EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 16, EGL_STENCIL_SIZE, 0,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE};
    const EGLint ctxattrs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    const EGLint winattrs[] = {EGL_RENDER_BUFFER, EGL_BACK_BUFFER, EGL_NONE};
    if (p->display == EGL_NO_DISPLAY || !p->eglInitialize(p->display, &major, &minor)) goto failed;
    p->initialized = 1;
    fprintf(p->log, "EGL %d.%d\n", major, minor);
    if (!p->eglBindAPI(EGL_OPENGL_ES_API)) goto failed;
    if (!p->eglChooseConfig(p->display, attrs, &config, 1, &count) || count < 1) goto failed;
    p->window = (OrbisPglWindow){0, width, height, 0};
    p->surface = p->eglCreateWindowSurface(p->display, config, &p->window, winattrs);
    if (p->surface == EGL_NO_SURFACE) goto failed;
    p->context = p->eglCreateContext(p->display, config, EGL_NO_CONTEXT, ctxattrs);
    if (p->context == EGL_NO_CONTEXT) goto failed;
    if (!p->eglMakeCurrent(p->display, p->surface, p->surface, p->context)) goto failed;
    return 1;
failed:
    fprintf(p->log, "FAIL: EGL context setup error=0x%04x\n", p->eglGetError());
    return 0;
}

void *ps4_piglet_proc(Ps4Piglet *p, const char *name) {
    if (!p || !name) return NULL;
    void *address = (void *)p->eglGetProcAddress(name);
    if (!address) resolve(p, name, &address);
    return address;
}
int ps4_piglet_swap(Ps4Piglet *p) {
    if (p && p->surface && p->eglSwapBuffers(p->display, p->surface)) return 1;
    if (p) fprintf(p->log, "FAIL: eglSwapBuffers error=0x%04x\n", p->eglGetError());
    return 0;
}
void ps4_piglet_close(Ps4Piglet *p) {
    if (!p) return;
    if (p->initialized) {
        p->eglMakeCurrent(p->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (p->context) p->eglDestroyContext(p->display, p->context);
        if (p->surface) p->eglDestroySurface(p->display, p->surface);
        p->eglTerminate(p->display);
    }
    if (p->module >= 0) {
        int result = 0;
        int rc = sceKernelStopUnloadModule(p->module, 0, NULL, 0, NULL, &result);
        fprintf(p->log, "MODULE unload=0x%08x stop=0x%08x\n", (unsigned)rc, (unsigned)result);
    }
    free(p);
}
