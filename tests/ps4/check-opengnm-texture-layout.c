/* Host regression for the actual GNM texture descriptor/layout implementation. */
#include <assert.h>
#include <stdio.h>
#include <gnm/texture.h>

int main(void) {
    GnmTextureCreateInfo ci = {
        .texturetype = GNM_TEXTURE_2D, .width = 2, .height = 2, .depth = 1,
        .nummiplevels = 1, .numslices = 1, .numfragments = 1,
        .format = GNM_FMT_R8G8B8A8_UNORM,
        .tilemodehint = GNM_TM_DISPLAY_LINEAR_ALIGNED, .mingpumode = GNM_GPU_BASE,
    };
    GnmTexture texture = {0};
    assert(gnmCreateTexture(&texture, &ci) == GNM_ERROR_OK);
    /* 64-byte pitch alignment plus 256-byte slice alignment at 1 sample:
       two rows require 32 RGBA texels per row, not 16 (the old 2-sample bug). */
    assert(gnmTexGetPitch(&texture) == 32);
    assert(texture.basearray == 0 && texture.lastarray == 0);
    assert(gnmTexGetNumArraySlices(&texture) == 1);
    uint64_t size = 0;
    uint32_t alignment = 0;
    assert(gnmTexCalcByteSize(&size, &alignment, &texture) == GNM_ERROR_OK);
    assert(size == 256 && alignment == 256);

    ci.texturetype = GNM_TEXTURE_2D_ARRAY;
    ci.numslices = 3;
    assert(gnmCreateTexture(&texture, &ci) == GNM_ERROR_OK);
    assert(texture.lastarray == 2 && gnmTexGetNumArraySlices(&texture) == 3);
    assert(gnmTexCalcByteSize(&size, &alignment, &texture) == GNM_ERROR_OK);
    assert(size == 768);
    puts("PASS: single-sample texture pitch, byte size and inclusive array bounds");
    return 0;
}
