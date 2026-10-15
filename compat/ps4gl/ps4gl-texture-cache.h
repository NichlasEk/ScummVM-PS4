/* A sampler still needs a valid native descriptor when its shader branch is
 * inactive. Texture name zero is reserved for this opaque-black 2D fallback. */
static const PS4GLTexture *unbound_texture_2d(void) {
  static unsigned char black[4]={0,0,0,255};
  static const PS4GLTexture texture={.name=0,.created=1,.pixels=black,
    .width=1,.height=1,.min_filter=GL_NEAREST,.mag_filter=GL_NEAREST,
    .wrap_s=GL_CLAMP_TO_EDGE,.wrap_t=GL_CLAMP_TO_EDGE,.wrap_r=GL_CLAMP_TO_EDGE,
    .cpu_contents_defined=1,.internal_format=GL_RGBA,.target=GL_TEXTURE_2D,.revision=1};
  return &texture;
}
/* Call retirement helpers only after the frame's GPU work has completed. */
static int texture_snapshot_matches(const PS4GLTextureSnapshot *t, const PS4GLTexture *s) {
  return t->sampler && !s->deleted && t->source_name==s->name &&
    t->source_revision==s->revision && t->source_target==s->render_target &&
    t->min_filter==s->min_filter && t->mag_filter==s->mag_filter &&
    t->wrap_s==s->wrap_s && t->wrap_t==s->wrap_t;
}
static int texture_snapshot_current(const PS4GLContext *ctx, const PS4GLTextureSnapshot *t) {
  if(t->source_target || t->borrowed_view) return 0;
  if(!t->source_name) return texture_snapshot_matches(t,unbound_texture_2d());
  for(unsigned i=0;i<PS4GL_MAX_TEXTURES;++i)
    if(ctx->textures[i].pixels && texture_snapshot_matches(t,&ctx->textures[i])) return 1;
  return 0;
}
static void retire_texture_cache(PS4GLContext *ctx, int all) {
  for(unsigned i=0;i<PS4GL_TEXTURE_CACHE_SLOTS;++i) {
    PS4GLTextureSnapshot *t=&ctx->texture_cache[i];
    if(t->sampler && (all || !texture_snapshot_current(ctx,t))) {
      ctx->texture_cache_bytes-=t->allocation_bytes;
      destroy_texture_snapshot(ctx->program_device,t);
    }
  }
}
static void cache_retired_texture(PS4GLContext *ctx, PS4GLTextureSnapshot *t) {
  if(!t->sampler || t->borrowed_snapshot || !texture_snapshot_current(ctx,t) ||
     !t->allocation_bytes || t->allocation_bytes>PS4GL_TEXTURE_CACHE_BUDGET-ctx->texture_cache_bytes) return;
  for(unsigned i=0;i<PS4GL_TEXTURE_CACHE_SLOTS;++i) if(!ctx->texture_cache[i].sampler) {
    ctx->texture_cache[i]=*t;
    ctx->texture_cache_bytes+=t->allocation_bytes;
    t->borrowed_snapshot=1; /* Ownership moved; retiring the frame must not free it. */
    return;
  }
}
