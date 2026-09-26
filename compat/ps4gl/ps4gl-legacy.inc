/* Operates on glslang-preprocessed text, not raw macros. Deliberately bounded
 * global declarations; complex interface forms fail rather than being guessed. */
#include <ctype.h>
#include <stdarg.h>
typedef struct LegacyDecl { char kind[16],type[16],name[64]; unsigned count; size_t begin,end; } LegacyDecl;
typedef struct LegacySource { char *text; LegacyDecl decl[64]; unsigned count; } LegacySource;
typedef struct LegacyOutput { char *text; size_t length; int failed; } LegacyOutput;
#define LEGACY_OUTPUT_LIMIT (512u*1024u)
static void legacy_emit(LegacyOutput *out, const char *format, ...) {
  if(out->failed) return;
  va_list args; va_start(args,format);
  int n=vsnprintf(out->text+out->length,LEGACY_OUTPUT_LIMIT-out->length,format,args); va_end(args);
  if(n<0 || (size_t)n>=LEGACY_OUTPUT_LIMIT-out->length) out->failed=1; else out->length+=(size_t)n;
}
static int legacy_token(const char *text, size_t *at, char *token, size_t capacity) {
  while(isspace((unsigned char)text[*at])) ++*at;
  size_t start=*at;
  if(isalpha((unsigned char)text[*at]) || text[*at]=='_') {
    do { ++*at; } while(isalnum((unsigned char)text[*at]) || text[*at]=='_');
  } else if(isdigit((unsigned char)text[*at])) {
    do { ++*at; } while(isdigit((unsigned char)text[*at]));
  } else if(text[*at]) ++*at;
  size_t n=*at-start;
  if(n>=capacity) return 0;
  memcpy(token,text+start,n); token[n]=0; return n!=0;
}
static int legacy_type(const char *type) {
  return !strcmp(type,"float") || !strcmp(type,"int") || !strcmp(type,"bool") || !strcmp(type,"vec2") ||
    !strcmp(type,"vec3") || !strcmp(type,"vec4") || !strcmp(type,"mat4") || !strcmp(type,"sampler2D");
}
static int legacy_preprocess(int fragment, const char *source, LegacySource *out, char *log, size_t cap) {
  if(!initialized || !source) { diagnostic(log,cap,"Invalid legacy compiler state"); return 0; }
  size_t length=0; while(length<=256u*1024u && source[length]) ++length;
  if(!length || length>256u*1024u) { diagnostic(log,cap,"Legacy source exceeds size limit"); return 0; }
  glslang_input_t input={.language=GLSLANG_SOURCE_GLSL,.stage=fragment ? GLSLANG_STAGE_FRAGMENT : GLSLANG_STAGE_VERTEX,
    .client=GLSLANG_CLIENT_NONE,.target_language=GLSLANG_TARGET_NONE,.code=source,.default_version=110,
    .default_profile=GLSLANG_NO_PROFILE,.resource=glslang_default_resource(),
    .callbacks={.include_local=reject_include,.include_system=reject_include}};
  glslang_shader_t *shader=glslang_shader_create(&input); if(!shader) return 0;
  if(!glslang_shader_preprocess(shader,&input)) { diagnostic(log,cap,glslang_shader_get_info_log(shader)); glslang_shader_delete(shader); return 0; }
  input.code=glslang_shader_get_preprocessed_code(shader);
  /* Validate original language before rewriting, including reserved identifiers. */
  if(!glslang_shader_parse(shader,&input)) { diagnostic(log,cap,glslang_shader_get_info_log(shader)); glslang_shader_delete(shader); return 0; }
  length=strlen(input.code);
  if(length>=LEGACY_OUTPUT_LIMIT) { diagnostic(log,cap,"Expanded legacy source exceeds limit"); glslang_shader_delete(shader); return 0; }
  out->text=malloc(length+1);
  if(out->text) memcpy(out->text,input.code,length+1);
  glslang_shader_delete(shader);
  if(!out->text) { diagnostic(log,cap,"Cannot allocate legacy source"); return 0; }
  /* Preserve offsets while removing version/line directives. Other directives
   * (especially extensions) need explicit support rather than silent removal. */
  for(char *line=out->text; *line;) {
    char *end=strchr(line,'\n'); if(!end) end=line+strlen(line);
    char *p=line; while(p<end && isspace((unsigned char)*p)) ++p;
    if(p<end && *p=='#') {
      ++p; while(p<end && isspace((unsigned char)*p)) ++p;
      if(strncmp(p,"version",7) && strncmp(p,"line",4)) { diagnostic(log,cap,"Unsupported legacy directive"); return 0; }
      memset(line,' ',(size_t)(end-line));
    }
    line=*end ? end+1 : end;
  }
  size_t at=0; int depth=0; char token[128];
  while(out->text[at]) {
    size_t begin=at;
    if(!legacy_token(out->text,&at,token,sizeof(token))) break;
    if(!strncmp(token,"ps4gl_",6)) { diagnostic(log,cap,"Reserved bridge identifier"); return 0; }
    if(!strncmp(token,"gl_",3) && strcmp(token,"gl_Position") && strcmp(token,"gl_FragColor")) {
      diagnostic(log,cap,"Unsupported legacy builtin (only gl_Position/gl_FragColor are translated)"); return 0;
    }
    if(!strcmp(token,"{")) ++depth;
    if(!strcmp(token,"}")) --depth;
    if(depth) continue;
    if(!strcmp(token,"precision")) {
      while(out->text[at] && out->text[at]!=';') ++at;
      if(out->text[at]) ++at;
      memset(out->text+begin,' ',at-begin); continue;
    }
    if(strcmp(token,"uniform") && strcmp(token,"attribute") && strcmp(token,"varying")) continue;
    if(out->count==64) goto unsupported;
    LegacyDecl *d=&out->decl[out->count]; strcpy(d->kind,token); d->begin=begin;
    if(!legacy_token(out->text,&at,d->type,sizeof(d->type))) goto unsupported;
    if(!strcmp(d->type,"highp") || !strcmp(d->type,"mediump") || !strcmp(d->type,"lowp"))
      if(!legacy_token(out->text,&at,d->type,sizeof(d->type))) goto unsupported;
    if(!legacy_type(d->type) || !legacy_token(out->text,&at,d->name,sizeof(d->name)) ||
       !strncmp(d->name,"ps4gl_",6) || !(isalpha((unsigned char)d->name[0]) || d->name[0]=='_')) goto unsupported;
    if(!legacy_token(out->text,&at,token,sizeof(token))) goto unsupported;
    if(!strcmp(token,"[")) {
      if(!legacy_token(out->text,&at,token,sizeof(token))) goto unsupported;
      char *end; unsigned long count=strtoul(token,&end,10);
      if(*end || !count || count>64) goto unsupported;
      d->count=(unsigned)count;
      if(!legacy_token(out->text,&at,token,sizeof(token)) || strcmp(token,"]") ||
         !legacy_token(out->text,&at,token,sizeof(token))) goto unsupported;
    }
    if(strcmp(token,";") || (strcmp(d->kind,"uniform") && (d->count || !strcmp(d->type,"mat4") || !strcmp(d->type,"sampler2D") || !strcmp(d->type,"int") || !strcmp(d->type,"bool")))) goto unsupported;
    if(!strcmp(d->type,"bool") && d->count) goto unsupported;
    if(!strcmp(d->type,"sampler2D") && (d->count || !fragment || strcmp(d->kind,"uniform"))) goto unsupported;
    if(fragment && !strcmp(d->kind,"attribute")) goto unsupported;
    for(unsigned i=0;i<out->count;++i) if(!strcmp(out->decl[i].name,d->name)) goto unsupported;
    d->end=at; ++out->count;
  }
  return 1;
unsupported:
  diagnostic(log,cap,"Unsupported legacy declaration (single named scalar/vector/mat4 uniform or float attribute/varying required)"); return 0;
}
static int legacy_find(const LegacySource *source, const char *kind, const char *name) {
  for(unsigned i=0;i<source->count;++i) if(!strcmp(source->decl[i].kind,kind) && !strcmp(source->decl[i].name,name)) return (int)i;
  return -1;
}
static int legacy_location(const LegacySource *vertex, const LegacySource *fragment, const LegacyDecl *d,
                           const PS4GLAttributeBinding *bindings, size_t count) {
  if(!strcmp(d->kind,"attribute")) {
    for(size_t i=0;i<count;++i) if(!strcmp(bindings[i].name,d->name)) return bindings[i].index<8 ? (int)bindings[i].index : -1;
    unsigned used=0;
    for(size_t i=0;i<count;++i) if(bindings[i].index<8 && legacy_find(vertex,"attribute",bindings[i].name)>=0) used|=1u<<bindings[i].index;
    for(unsigned i=0;i<vertex->count;++i) if(!strcmp(vertex->decl[i].kind,"attribute")) {
      int bound=0; for(size_t j=0;j<count;++j) if(!strcmp(bindings[j].name,vertex->decl[i].name)) bound=1;
      if(bound) continue;
      unsigned slot=0; while(slot<8 && (used&(1u<<slot))) ++slot;
      if(slot==8) return -1;
      used|=1u<<slot; if(!strcmp(vertex->decl[i].name,d->name)) return (int)slot;
    }
    return -1;
  }
  const LegacySource *source=!strcmp(d->type,"sampler2D") ? fragment : vertex;
  int ordinal=0;
  for(unsigned i=0;i<source->count;++i) {
    const LegacyDecl *candidate=&source->decl[i];
    int match=!strcmp(d->type,"sampler2D") ? !strcmp(candidate->type,"sampler2D") : !strcmp(candidate->kind,"varying");
    if(match) { if(!strcmp(candidate->name,d->name)) return ordinal; ++ordinal; }
  }
  return -1;
}
static char *legacy_rewrite(const LegacySource *source, int fragment, const LegacySource *vs, const LegacySource *fs,
                            const PS4GLAttributeBinding *bindings, size_t count, char *log, size_t cap) {
  LegacyOutput out={.text=malloc(LEGACY_OUTPUT_LIMIT)}; if(!out.text) return NULL;
  out.text[0]=0; legacy_emit(&out,"#version 450\n");
  int uniforms=0; unsigned used_attributes=0;
  for(unsigned i=0;i<source->count;++i) {
    const LegacyDecl *d=&source->decl[i];
    if(!strcmp(d->kind,"uniform") && strcmp(d->type,"sampler2D")) { uniforms=1; continue; }
    int location=legacy_location(vs,fs,d,bindings,count);
    int sampler=!strcmp(d->type,"sampler2D"), attribute=!strcmp(d->kind,"attribute");
    if(location<0 || location>=(sampler ? 2 : attribute ? 8 : 16)) goto fail;
    if(attribute) { if(used_attributes&(1u<<location)) goto fail; used_attributes|=1u<<location; }
    if(sampler) legacy_emit(&out,"layout(set=0,binding=%d) uniform sampler2D %s;\n",location+2,d->name);
    else legacy_emit(&out,"layout(location=%d) %s %s %s;\n",location,attribute || fragment ? "in" : "out",d->type,d->name);
  }
  if(uniforms) {
    legacy_emit(&out,"layout(std140,set=0,binding=%d) uniform ps4gl_%s_defaults {\n",fragment ? 1 : 0,fragment ? "fragment" : "vertex");
    for(unsigned i=0;i<source->count;++i) { const LegacyDecl *d=&source->decl[i];
      if(!strcmp(d->kind,"uniform") && strcmp(d->type,"sampler2D")) {
        if(!strcmp(d->type,"bool")) legacy_emit(&out,"int ps4gl_bool_%s",d->name);
        else legacy_emit(&out,"%s %s",d->type,d->name);
        if(d->count) legacy_emit(&out,"[%u]",d->count);
        legacy_emit(&out,";\n");
      }
    }
    legacy_emit(&out,"};\n");
  }
  if(fragment) legacy_emit(&out,"layout(location=0) out vec4 ps4gl_fragment_color;\n");
  size_t at=0; unsigned declaration=0; char token[128];
  while(source->text[at]) {
    if(declaration<source->count && at==source->decl[declaration].begin) { at=source->decl[declaration++].end; continue; }
    if(isalpha((unsigned char)source->text[at]) || source->text[at]=='_') {
      if(!legacy_token(source->text,&at,token,sizeof(token))) goto fail;
      const char *replacement=!strcmp(token,"main") ? "ps4gl_main" : !strcmp(token,"texture2D") ? "texture" :
        !strcmp(token,"gl_FragColor") ? "ps4gl_fragment_color" : token;
      const LegacyDecl *boolean=NULL;
      for(unsigned i=0;i<source->count;++i)
        if(!strcmp(source->decl[i].kind,"uniform") && !strcmp(source->decl[i].type,"bool") &&
           !strcmp(token,source->decl[i].name)) { boolean=&source->decl[i]; break; }
      if(boolean) legacy_emit(&out,"(ps4gl_bool_%s != 0)",boolean->name);
      else legacy_emit(&out,"%s",replacement);
    } else legacy_emit(&out,"%c",source->text[at++]);
  }
  legacy_emit(&out,"\nvoid main(){ps4gl_main();%s}\n",fragment ? "" : "gl_Position.y=-gl_Position.y;gl_Position.z=(gl_Position.z+gl_Position.w)*0.5;");
  if(out.failed) goto fail;
  return out.text;
fail:
  free(out.text); diagnostic(log,cap,"Unsupported/missing legacy interface, aliased attributes or output limit"); return NULL;
}
static int legacy_pair(const char *v, const char *f, const PS4GLAttributeBinding *bindings, size_t count,
                       char **vo, char **fo, char *log, size_t cap) {
  LegacySource vs={0},fs={0}; int ok=0; *vo=NULL; *fo=NULL;
  if(count>8 || (count && !bindings)) { diagnostic(log,cap,"Invalid attribute bindings"); return 0; }
  if(!legacy_preprocess(0,v,&vs,log,cap) || !legacy_preprocess(1,f,&fs,log,cap)) goto done;
  for(unsigned i=0;i<fs.count;++i) {
    LegacyDecl *d=&fs.decl[i]; int match=legacy_find(&vs,d->kind,d->name);
    if((!strcmp(d->kind,"varying") && match<0) || (match>=0 &&
       (strcmp(d->type,vs.decl[match].type) || d->count!=vs.decl[match].count))) {
      diagnostic(log,cap,"Legacy varying/uniform types do not match across stages"); goto done;
    }
  }
  *vo=legacy_rewrite(&vs,0,&vs,&fs,bindings,count,log,cap);
  *fo=legacy_rewrite(&fs,1,&vs,&fs,bindings,count,log,cap);
  ok=*vo && *fo;
done:
  free(vs.text); free(fs.text);
  if(!ok) { free(*vo); free(*fo); *vo=NULL; *fo=NULL; }
  return ok;
}
