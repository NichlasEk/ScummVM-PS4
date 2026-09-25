/* libpng callback decoding, with independent generated pixel oracles.
 * Includes all legal color/depth combinations, Adam7, filters and bad inputs. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <png.h>

struct state {
    FILE *input;
    unsigned char *pixels;
    png_bytep *rows;
};
static void read_bytes(png_structp png, png_bytep bytes, png_size_t length) {
    struct state *s = png_get_io_ptr(png);
    if (fread(bytes, 1, length, s->input) != length)
        png_error(png, "short stream");
}
static void fail(png_structp png, png_const_charp message) {
    (void)message;
    png_longjmp(png, 1);
}
static void warning(png_structp png, png_const_charp message) {
    (void)png; (void)message;
}
static int decode(const char *path, const char *name, FILE *out, int expect_bad) {
    struct state *s = calloc(1, sizeof(*s));
    if (!s) return 1;
    s->input = fopen(path, "rb");
    if (!s->input) { free(s); return 1; }
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, fail, warning);
    png_infop info = png ? png_create_info_struct(png) : NULL;
    if (!info) {
        if (png) png_destroy_read_struct(&png, NULL, NULL);
        fclose(s->input); free(s); return 1;
    }
    if (setjmp(png_jmpbuf(png))) {
        free(s->rows); free(s->pixels); fclose(s->input); free(s);
        png_destroy_read_struct(&png, &info, NULL);
        fprintf(out, "%s %s\n", name, expect_bad ? "rejected" : "DECODE FAILED");
        return !expect_bad;
    }
    png_set_read_fn(png, s, read_bytes);
    png_read_info(png, info);
    png_uint_32 width = png_get_image_width(png, info), height = png_get_image_height(png, info);
    int color = png_get_color_type(png, info), depth = png_get_bit_depth(png, info);
    if (!width || !height || width > 1024 || height > 1024) png_error(png, "dimensions");
    int transparency = png_get_valid(png, info, PNG_INFO_tRNS);
    if (depth == 16) png_set_strip_16(png);
    if (color == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (color == PNG_COLOR_TYPE_GRAY && depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (transparency) png_set_tRNS_to_alpha(png);
    if (color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png);
    if (!(color & PNG_COLOR_MASK_ALPHA) && !transparency) png_set_filler(png, 255, PNG_FILLER_AFTER);
    png_set_interlace_handling(png);
    png_read_update_info(png, info);
    if (png_get_rowbytes(png, info) != width * 4) png_error(png, "stride");
    s->pixels = calloc(height, width * 4);
    s->rows = malloc(height * sizeof(*s->rows));
    if (!s->pixels || !s->rows) png_error(png, "allocation");
    for (unsigned y = 0; y < height; ++y) s->rows[y] = s->pixels + y * width * 4;
    png_read_image(png, s->rows);
    png_read_end(png, info);
    uint32_t hash = 2166136261u;
    for (unsigned i = 0; i < width * height * 4; ++i) hash = (hash ^ s->pixels[i]) * 16777619u;
    fprintf(out, "%s %ux%u RGBA fnv1a=%08x\n", name, width, height, hash);
    free(s->rows); free(s->pixels); fclose(s->input); free(s);
    png_destroy_read_struct(&png, &info, NULL);
    return expect_bad;
}
int main(int argc, char **argv) {
#ifdef __PS4__
    const char *directory = "/app0", *output = "/data/png-smoke.txt";
#else
    if (argc != 3) return 2;
    const char *directory = argv[1], *output = argv[2];
#endif
    FILE *out = fopen(output, "w");
    if (!out) return 2;
    char path[1024], name[128];
    snprintf(path, sizeof(path), "%s/files.txt", directory);
    FILE *list = fopen(path, "r");
    if (!list) { fclose(out); return 2; }
    unsigned count = 0;
    int result = 0;
    while (fgets(name, sizeof(name), list)) {
        name[strcspn(name, "\r\n")] = 0;
        snprintf(path, sizeof(path), "%s/%s", directory, name);
        result |= decode(path, name, out, 0);
        ++count;
    }
    fclose(list);
    const char *bad[] = {"bad-crc.png", "truncated.png", "invalid.png"};
    for (unsigned i = 0; i < 3; ++i) {
        snprintf(path, sizeof(path), "%s/%s", directory, bad[i]);
        result |= decode(path, bad[i], out, 1);
    }
    result |= count != 30;
    fprintf(out, "%s\n", result ? "FAIL" : "PASS");
    fclose(out);
    return result;
}
