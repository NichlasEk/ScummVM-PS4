/* Decode upstream baseline and arithmetic JPEG fixtures on the
 * host and PS4. Identical RGB hashes exercise the target ABI and decoder. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <setjmp.h>
#include <jpeglib.h>

struct errors { struct jpeg_error_mgr base; jmp_buf jump; };
static void fail(j_common_ptr c) {
    struct errors *e = (struct errors *)c->err;
    longjmp(e->jump, 1);
}
static int decode(const char *path, FILE *out) {
    FILE *in = fopen(path, "rb");
    if (!in) { fprintf(out, "OPEN FAILED: %s\n", path); return 1; }
    struct jpeg_decompress_struct c = {0};
    struct errors e;
    c.err = jpeg_std_error(&e.base);
    e.base.error_exit = fail;
    if (setjmp(e.jump)) {
        fprintf(out, "DECODE FAILED\n");
        jpeg_destroy_decompress(&c); fclose(in); return 1;
    }
    jpeg_create_decompress(&c);
    jpeg_stdio_src(&c, in);
    jpeg_read_header(&c, TRUE);
    c.out_color_space = JCS_RGB;
    c.dct_method = JDCT_ISLOW;
    jpeg_start_decompress(&c);
    unsigned width = c.output_width, height = c.output_height;
    unsigned stride = width * c.output_components;
    JSAMPARRAY row = (*c.mem->alloc_sarray)((j_common_ptr)&c, JPOOL_IMAGE, stride, 1);
    uint32_t hash = 2166136261u;
    while (c.output_scanline < c.output_height) {
        jpeg_read_scanlines(&c, row, 1);
        for (unsigned i = 0; i < stride; ++i) hash = (hash ^ row[0][i]) * 16777619u;
    }
    jpeg_finish_decompress(&c);
    fprintf(out, "%ux%u RGB fnv1a=%08x\n", width, height, hash);
    jpeg_destroy_decompress(&c); fclose(in); return 0;
}
int main(int argc, char **argv) {
#ifdef __PS4__
    const char *directory = "/app0";
    const char *output = "/data/jpeg-smoke.txt";
#else
    if (argc != 3) return 2;
    const char *directory = argv[1], *output = argv[2];
#endif
    FILE *out = fopen(output, "w");
    if (!out) return 2;
    const char *names[] = {"testorig.jpg", "testimgint.jpg", "testimgari.jpg"};
    int result = 0;
    for (unsigned i = 0; i < 3; ++i) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", directory, names[i]);
        fprintf(out, "%s ", names[i]);
        result |= decode(path, out);
    }
    fprintf(out, "%s\n", result ? "FAIL" : "PASS");
    fclose(out);
    return result;
}
