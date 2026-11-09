#include "ps4gl.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned destroyed;
static void destroy_texture_snapshot(VkDevice d, PS4GLTextureSnapshot *t) {
  (void)d;
  if(!t->borrowed_snapshot && t->sampler) ++destroyed;
  *t=(PS4GLTextureSnapshot){0};
}
#include "ps4gl-texture-cache.h"
static PS4GLContext ctx;
int main(void) {
  unsigned char pixel=0;
  PS4GLTexture *s=&ctx.textures[0];
  *s=(PS4GLTexture){.name=1,.revision=7,.pixels=&pixel,.min_filter=GL_NEAREST,.mag_filter=GL_LINEAR,.wrap_s=GL_REPEAT,.wrap_t=GL_REPEAT};
  PS4GLTextureSnapshot t={.sampler=(VkSampler)(uintptr_t)1,.source_name=1,.source_revision=7,
    .allocation_bytes=4096,.min_filter=GL_NEAREST,.mag_filter=GL_LINEAR,.wrap_s=GL_REPEAT,.wrap_t=GL_REPEAT};
  cache_retired_texture(&ctx,&t); assert(t.borrowed_snapshot && ctx.texture_cache_bytes==4096);
  destroy_texture_snapshot(0,&t); assert(destroyed==0);
  for(unsigned i=0;i<100;++i) {
    retire_texture_cache(&ctx,0);
    assert(texture_snapshot_matches(&ctx.texture_cache[0],s));
    t=ctx.texture_cache[0]; t.borrowed_snapshot=1;
    cache_retired_texture(&ctx,&t); destroy_texture_snapshot(0,&t);
    assert(ctx.texture_cache_bytes==4096 && destroyed==0);
  }
  s->wrap_s=GL_MIRRORED_REPEAT;
  assert(!texture_snapshot_matches(&ctx.texture_cache[0],s));
  s->wrap_s=GL_REPEAT;
  s->wrap_t=GL_MIRRORED_REPEAT;
  assert(!texture_snapshot_matches(&ctx.texture_cache[0],s));
  s->wrap_t=GL_REPEAT;
  // Old in-flight image remains valid until retirement, never mutated in place.
  ++s->revision; assert(!texture_snapshot_matches(&ctx.texture_cache[0],s));
  assert(destroyed==0); retire_texture_cache(&ctx,0); assert(destroyed==1 && !ctx.texture_cache_bytes);
  t=(PS4GLTextureSnapshot){.sampler=(VkSampler)(uintptr_t)2,.source_name=1,.source_revision=8,
    .allocation_bytes=4096,.min_filter=GL_NEAREST,.mag_filter=GL_LINEAR,.wrap_s=GL_REPEAT,.wrap_t=GL_REPEAT};
  PS4GLTextureSnapshot original=t;
  cache_retired_texture(&ctx,&t); s->wrap_s=GL_CLAMP_TO_EDGE;
  assert(!texture_snapshot_matches(&ctx.texture_cache[0],s));
  retire_texture_cache(&ctx,0); assert(destroyed==2);
  s->wrap_s=GL_REPEAT; t=original; t.allocation_bytes=PS4GL_TEXTURE_CACHE_BUDGET+1;
  cache_retired_texture(&ctx,&t); assert(!t.borrowed_snapshot && !ctx.texture_cache_bytes);
  t=original; t.source_target=1; t.borrowed_view=1; s->render_target=1;
  cache_retired_texture(&ctx,&t); assert(!t.borrowed_snapshot); s->render_target=0;
  t=original; cache_retired_texture(&ctx,&t); s->deleted=1;
  retire_texture_cache(&ctx,0); assert(destroyed==3 && !ctx.texture_cache_bytes);
  s->deleted=0; ++s->revision; assert(!texture_snapshot_matches(&original,s));
  original.source_revision=s->revision;
  for(unsigned i=0;i<PS4GL_TEXTURE_CACHE_SLOTS;++i) {t=original;cache_retired_texture(&ctx,&t);assert(t.borrowed_snapshot);}
  t=original;cache_retired_texture(&ctx,&t);assert(!t.borrowed_snapshot); // full-cache fallback
  retire_texture_cache(&ctx,1); assert(ctx.texture_cache_bytes==0 && destroyed==3+PS4GL_TEXTURE_CACHE_SLOTS);
  const PS4GLTexture *fallback=unbound_texture_2d();
  assert(!fallback->name && fallback->width==1 && fallback->height==1 &&
         !memcmp(fallback->pixels,"\0\0\0\377",4));
  t=(PS4GLTextureSnapshot){.sampler=(VkSampler)(uintptr_t)9,.source_revision=1,
    .allocation_bytes=4096,.min_filter=GL_NEAREST,.mag_filter=GL_NEAREST,
    .wrap_s=GL_CLAMP_TO_EDGE,.wrap_t=GL_CLAMP_TO_EDGE};
  cache_retired_texture(&ctx,&t); assert(t.borrowed_snapshot);
  for(unsigned i=0;i<100;++i) {retire_texture_cache(&ctx,0); assert(ctx.texture_cache_bytes==4096);}
  assert(texture_snapshot_matches(&ctx.texture_cache[0],fallback));
  retire_texture_cache(&ctx,1); assert(!ctx.texture_cache_bytes);
  puts("PASS persistent texture cache: retirement ownership, 100-frame reuse, content/sampler invalidation, deleted names, target exclusion, bounds and teardown");
}
