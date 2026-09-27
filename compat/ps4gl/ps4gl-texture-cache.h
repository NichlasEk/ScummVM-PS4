/* Call retirement helpers only after the frame's GPU work has completed. */
static int texture_snapshot_matches(const PS4GLTextureSnapshot *t, const PS4GLTexture *s) {
  return t->sampler && !s->deleted && t->source_name==s->name &&
    t->source_revision==s->revision && t->source_target==s->render_target &&
    t->min_filter==s->min_filter && t->mag_filter==s->mag_filter &&
    t->wrap_s==s->wrap_s && t->wrap_t==s->wrap_t;
}
static int texture_snapshot_current(const PS4GLContext *ctx, const PS4GLTextureSnapshot *t) {
  if(t->source_target || t->borrowed_view) return 0;
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
