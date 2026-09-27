/* Operates on glslang-preprocessed text, not raw macros. Deliberately bounded
 * global declarations; complex interface forms fail rather than being guessed. */
#include <ctype.h>
#include <stdarg.h>
typedef struct LegacyDecl { char kind[16],type[64],name[64]; unsigned count; size_t begin,end; } LegacyDecl;
typedef struct LegacyStruct { char name[64]; LegacyDecl fields[16]; unsigned count; size_t begin,end; } LegacyStruct;
typedef struct LegacySource { char *text; LegacyDecl decl[64]; unsigned count; LegacyStruct structs[8]; unsigned struct_count; int fragcoord; unsigned client_mask; char constant_names[16][64]; unsigned constants[16],constant_count; } LegacySource;
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
static int legacy_sampler(const char *type) {
  return !strcmp(type,"sampler2D") || !strcmp(type,"sampler2DRect") || !strcmp(type,"sampler1D") || !strcmp(type,"samplerCube");
}
static int legacy_type(const char *type) {
  return !strcmp(type,"float") || !strcmp(type,"int") || !strcmp(type,"bool") || !strcmp(type,"vec2") ||
    !strcmp(type,"vec3") || !strcmp(type,"vec4") || !strcmp(type,"mat4") || legacy_sampler(type);
}
static const LegacyStruct *legacy_struct(const LegacySource *source, const char *name) {
  for(unsigned i=0;i<source->struct_count;++i)
    if(!strcmp(source->structs[i].name,name)) return &source->structs[i];
  return NULL;
}
static void legacy_field_name(char *out, size_t size, const char *object, const char *field) {
  snprintf(out,size,"ps4gl_field_%zu_%s%s",strlen(object),object,field);
}
static const char *legacy_client_names[]={"gl_Vertex","gl_Color","gl_Normal","gl_MultiTexCoord0","gl_MultiTexCoord1"};
static const char *legacy_client_attributes[]={"ps4gl_client_vertex","ps4gl_client_color","ps4gl_client_normal","ps4gl_client_uv0","ps4gl_client_uv1"};
static int legacy_client(const char *name) {
  for(unsigned i=0;i<5;++i) if(!strcmp(name,legacy_client_names[i])) return (int)i;
  return -1;
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
      if(strncmp(p,"version",7) && strncmp(p,"line",4)) {
        char extension[80],behavior[16];
        if(sscanf(p,"extension %79s : %15s",extension,behavior)!=2 || strcmp(extension,"GL_ARB_texture_rectangle") ||
           (strcmp(behavior,"enable") && strcmp(behavior,"require"))) {diagnostic(log,cap,"Unsupported legacy directive");return 0;}
      }
      memset(line,' ',(size_t)(end-line));
    }
    line=*end ? end+1 : end;
  }
  size_t at=0; int depth=0; char token[128];
  while(out->text[at]) {
    size_t begin=at;
    if(!legacy_token(out->text,&at,token,sizeof(token))) break;
    if(!strncmp(token,"ps4gl_",6)) { diagnostic(log,cap,"Reserved bridge identifier"); return 0; }
    if(fragment && !strcmp(token,"gl_FragCoord")) { out->fragcoord=1; continue; }
    int client=fragment ? -1 : legacy_client(token);
    if(client>=0) { out->client_mask|=1u<<client; continue; }
    if(!strncmp(token,"gl_",3) && strcmp(token,"gl_Position") && strcmp(token,"gl_FragColor")) {
      diagnostic(log,cap,"Unsupported legacy builtin (only gl_Position/gl_FragColor are translated)"); return 0;
    }
    if(!depth && !strcmp(token,"const")) {
      size_t q=at; char type[16],name[64],equal[4],value[64],semi[4];
      if(legacy_token(out->text,&q,type,sizeof(type)) && !strcmp(type,"int") &&
         legacy_token(out->text,&q,name,sizeof(name)) && legacy_token(out->text,&q,equal,sizeof(equal)) && !strcmp(equal,"=") &&
         legacy_token(out->text,&q,value,sizeof(value)) && legacy_token(out->text,&q,semi,sizeof(semi)) && !strcmp(semi,";")) {
        char *end; unsigned long v=strtoul(value,&end,10);
        if(!*end && v && v<=64) {
          if(out->constant_count==16) goto unsupported;
          strcpy(out->constant_names[out->constant_count],name); out->constants[out->constant_count++]=(unsigned)v;
        }
      }
    }
    if(!depth && !strcmp(token,"struct")) {
      if(out->struct_count==8) goto unsupported;
      LegacyStruct *st=&out->structs[out->struct_count]; st->begin=begin;
      if(!legacy_token(out->text,&at,st->name,sizeof(st->name)) ||
         !strncmp(st->name,"ps4gl_",6) || legacy_struct(out,st->name) ||
         !legacy_token(out->text,&at,token,sizeof(token)) || strcmp(token,"{")) goto unsupported;
      for(;;) {
        if(!legacy_token(out->text,&at,token,sizeof(token))) goto unsupported;
        if(!strcmp(token,"}")) break;
        if(st->count==16) goto unsupported;
        LegacyDecl *field=&st->fields[st->count];
        if(!strcmp(token,"highp") || !strcmp(token,"mediump") || !strcmp(token,"lowp"))
          if(!legacy_token(out->text,&at,token,sizeof(token))) goto unsupported;
        if(!legacy_type(token) || legacy_sampler(token)) goto unsupported;
        strcpy(field->type,token);
        if(!legacy_token(out->text,&at,field->name,sizeof(field->name)) ||
           !strncmp(field->name,"ps4gl_",6) ||
           !legacy_token(out->text,&at,token,sizeof(token)) || strcmp(token,";")) goto unsupported;
        ++st->count;
      }
      if(!st->count || !legacy_token(out->text,&at,token,sizeof(token)) || strcmp(token,";")) goto unsupported;
      st->end=at; ++out->struct_count; continue;
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
    if((!legacy_type(d->type) && !legacy_struct(out,d->type)) || !legacy_token(out->text,&at,d->name,sizeof(d->name)) ||
       !strncmp(d->name,"ps4gl_",6) || !(isalpha((unsigned char)d->name[0]) || d->name[0]=='_')) goto unsupported;
    if(!legacy_token(out->text,&at,token,sizeof(token))) goto unsupported;
    if(!strcmp(token,"[")) {
      if(!legacy_token(out->text,&at,token,sizeof(token))) goto unsupported;
      char *end; unsigned long count=strtoul(token,&end,10);
      if(*end) {
        count=0;
        for(unsigned c=0;c<out->constant_count;++c) if(!strcmp(token,out->constant_names[c])) count=out->constants[c];
      }
      if(!count || count>64) goto unsupported;
      d->count=(unsigned)count;
      if(!legacy_token(out->text,&at,token,sizeof(token)) || strcmp(token,"]") ||
         !legacy_token(out->text,&at,token,sizeof(token))) goto unsupported;
    }
    if(strcmp(token,";") || (strcmp(d->kind,"uniform") && (d->count || !strcmp(d->type,"mat4") || legacy_sampler(d->type) || !strcmp(d->type,"int") || !strcmp(d->type,"bool")))) goto unsupported;
    if(legacy_struct(out,d->type) && (strcmp(d->kind,"uniform") || d->count)) goto unsupported;
    if(!strcmp(d->type,"bool") && d->count) goto unsupported;
    if(legacy_sampler(d->type) && (d->count || !fragment || strcmp(d->kind,"uniform"))) goto unsupported;
    if(fragment && !strcmp(d->kind,"attribute")) goto unsupported;
    for(unsigned i=0;i<out->count;++i) if(!strcmp(out->decl[i].name,d->name)) goto unsupported;
    d->end=at; ++out->count;
  }
  for(unsigned i=0;i<5;++i) if(out->client_mask&(1u<<i)) {
    if(out->count==64) goto unsupported;
    LegacyDecl *d=&out->decl[out->count++];
    strcpy(d->kind,"attribute"); strcpy(d->type,i==2 ? "vec3" : "vec4");
    strcpy(d->name,legacy_client_attributes[i]); d->begin=d->end=SIZE_MAX;
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
  const LegacySource *source=legacy_sampler(d->type) ? fragment : vertex;
  int ordinal=0;
  for(unsigned i=0;i<source->count;++i) {
    const LegacyDecl *candidate=&source->decl[i];
    int match=legacy_sampler(d->type) ? legacy_sampler(candidate->type) : !strcmp(candidate->kind,"varying");
    if(match) { if(!strcmp(candidate->name,d->name)) return ordinal; ++ordinal; }
  }
  return -1;
}
static char *legacy_rewrite(const LegacySource *source, int fragment, const LegacySource *vs, const LegacySource *fs,
                            const PS4GLAttributeBinding *bindings, size_t count, char *log, size_t cap) {
  LegacyOutput out={.text=malloc(LEGACY_OUTPUT_LIMIT)}; if(!out.text) return NULL;
  out.text[0]=0; legacy_emit(&out,"#version 450\n");
  /* Keep source struct types for constructors and local values; flatten only
   * uniform storage, so the existing checked std140 upload path stays valid. */
  for(unsigned i=0;i<source->struct_count;++i) {
    const LegacyStruct *st=&source->structs[i];
    legacy_emit(&out,"struct %s {",st->name);
    for(unsigned j=0;j<st->count;++j) legacy_emit(&out,"%s %s;",st->fields[j].type,st->fields[j].name);
    legacy_emit(&out,"};\n");
  }
  const char *instance=fragment ? "ps4gl_fragment_data" : "ps4gl_vertex_data";
  int uniforms=source->fragcoord; unsigned used_attributes=0;
  for(unsigned i=0;i<source->count;++i) if(!strcmp(source->decl[i].type,"sampler2DRect") || !strcmp(source->decl[i].type,"samplerCube")) uniforms=1;
  for(unsigned i=0;i<source->count;++i) {
    const LegacyDecl *d=&source->decl[i];
    if(!strcmp(d->kind,"uniform") && !legacy_sampler(d->type)) { uniforms=1; continue; }
    int location=legacy_location(vs,fs,d,bindings,count);
    int sampler=legacy_sampler(d->type), attribute=!strcmp(d->kind,"attribute");
    if(location<0 || location>=(sampler ? PS4GL_MAX_SAMPLERS : attribute ? 8 : 16)) goto fail;
    if(attribute) { if(used_attributes&(1u<<location)) goto fail; used_attributes|=1u<<location; }
    if(sampler) legacy_emit(&out,"layout(set=0,binding=%d) uniform sampler2D %s%s;\n",location+2,!strcmp(d->type,"sampler1D") ? "ps4gl_1d_" : !strcmp(d->type,"samplerCube") ? "ps4gl_cube_" : "",d->name);
    else legacy_emit(&out,"layout(location=%d) %s %s %s;\n",location,attribute || fragment ? "in" : "out",d->type,d->name);
  }
  if(uniforms) {
    legacy_emit(&out,"layout(std140,set=0,binding=%d) uniform ps4gl_%s_defaults {\n",fragment ? 1 : 0,fragment ? "fragment" : "vertex");
    if(source->fragcoord) legacy_emit(&out,"vec2 ps4gl_fragcoord_y;\n");
    for(unsigned i=0;i<source->count;++i) if(!strcmp(source->decl[i].type,"samplerCube"))
      legacy_emit(&out,"vec2 ps4gl_cube_size%d;\n",legacy_location(vs,fs,&source->decl[i],bindings,count));
    for(unsigned i=0;i<source->count;++i) if(!strcmp(source->decl[i].type,"sampler2DRect"))
      legacy_emit(&out,"vec2 ps4gl_rect_size%d;\n",legacy_location(vs,fs,&source->decl[i],bindings,count));
    for(unsigned i=0;i<source->count;++i) { const LegacyDecl *d=&source->decl[i];
      if(!strcmp(d->kind,"uniform") && !legacy_sampler(d->type)) {
        const LegacyStruct *st=legacy_struct(source,d->type);
        if(st) {
          for(unsigned j=0;j<st->count;++j) {
            char name[160]; legacy_field_name(name,sizeof(name),d->name,st->fields[j].name);
            if(!strcmp(st->fields[j].type,"bool")) legacy_emit(&out,"int ps4gl_bool_%s;\n",name);
            else legacy_emit(&out,"%s %s;\n",st->fields[j].type,name);
          }
          continue;
        }
        if(!strcmp(d->type,"bool")) legacy_emit(&out,"int ps4gl_bool_%s",d->name);
        else legacy_emit(&out,"%s %s",d->type,d->name);
        if(d->count) legacy_emit(&out,"[%u]",d->count);
        legacy_emit(&out,";\n");
      }
    }
    legacy_emit(&out,"} %s;\n",instance);
  }
  if(fragment) {
    legacy_emit(&out,"layout(location=0) out vec4 ps4gl_fragment_color;\n");
    for(unsigned i=0;i<source->count;++i) if(!strcmp(source->decl[i].type,"samplerCube")) {
      const LegacyDecl *d=&source->decl[i];int slot=legacy_location(vs,fs,d,bindings,count);
      legacy_emit(&out,"vec4 ps4gl_cube_sample%d(vec3 d){vec3 a=abs(d);vec2 uv;float face;float m;"
        "if(a.x>=a.y && a.x>=a.z){m=a.x;if(d.x>=0.0){face=0.0;uv=vec2(-d.z,-d.y);}else{face=1.0;uv=vec2(d.z,-d.y);}}"
        "else if(a.y>=a.z){m=a.y;if(d.y>=0.0){face=2.0;uv=vec2(d.x,d.z);}else{face=3.0;uv=vec2(d.x,-d.z);}}"
        "else{m=a.z;if(d.z>=0.0){face=4.0;uv=vec2(d.x,-d.y);}else{face=5.0;uv=vec2(-d.x,-d.y);}}"
        "if(m==0.0)return vec4(0.0);vec2 h=vec2(0.5)/%s.ps4gl_cube_size%d;uv=clamp(uv/m*0.5+0.5,h,vec2(1.0)-h);"
        "return texture(ps4gl_cube_%s,vec2(uv.x,(uv.y+face)/6.0));}\n",slot,instance,slot,d->name);
    }
    for(unsigned i=0;i<source->count;++i) if(!strcmp(source->decl[i].type,"sampler1D")) {
      const LegacyDecl *d=&source->decl[i];int slot=legacy_location(vs,fs,d,bindings,count);
      legacy_emit(&out,"vec4 ps4gl_1d_sample%d(float u){return texture(ps4gl_1d_%s,vec2(u,0.5));}\n",slot,d->name);
    }
    for(unsigned i=0;i<source->count;++i) if(!strcmp(source->decl[i].type,"sampler2DRect")) {
      const LegacyDecl *d=&source->decl[i];int slot=legacy_location(vs,fs,d,bindings,count);
      legacy_emit(&out,"vec4 ps4gl_rect_sample%d(vec2 uv){return texture(%s,uv/%s.ps4gl_rect_size%d);}\n",slot,d->name,instance,slot);
    }
  }
  size_t at=0; unsigned declaration=0, structure=0; char token[128];
  while(source->text[at]) {
    if(structure<source->struct_count && at==source->structs[structure].begin) { at=source->structs[structure++].end; continue; }
    if(declaration<source->count && at==source->decl[declaration].begin) { at=source->decl[declaration++].end; continue; }
    if(isalpha((unsigned char)source->text[at]) || source->text[at]=='_') {
      size_t previous=at; while(previous && isspace((unsigned char)source->text[previous-1])) --previous;
      int member=previous && source->text[previous-1]=='.';
      if(!legacy_token(source->text,&at,token,sizeof(token))) goto fail;
      if(member) { legacy_emit(&out,"%s",token); continue; }
      if(!strcmp(token,"texture2DRect") || !strcmp(token,"texture1D") || !strcmp(token,"textureCube")) {
        int cube=!strcmp(token,"textureCube");
        int one_dimensional=!strcmp(token,"texture1D");
        char punct[4],sampler[64];
        if(!legacy_token(source->text,&at,punct,sizeof(punct)) || strcmp(punct,"(") ||
           !legacy_token(source->text,&at,sampler,sizeof(sampler)) ||
           !legacy_token(source->text,&at,punct,sizeof(punct)) || strcmp(punct,",")) goto fail;
        int index=legacy_find(source,"uniform",sampler);
        if(index<0 || strcmp(source->decl[index].type,cube ? "samplerCube" : one_dimensional ? "sampler1D" : "sampler2DRect")) goto fail;
        legacy_emit(&out,cube ? "ps4gl_cube_sample%d(" : one_dimensional ? "ps4gl_1d_sample%d(" : "ps4gl_rect_sample%d(",legacy_location(vs,fs,&source->decl[index],bindings,count));continue;
      }
      int client=fragment ? -1 : legacy_client(token);
      const char *replacement=client>=0 ? legacy_client_attributes[client] : !strcmp(token,"main") ? "ps4gl_main" : !strcmp(token,"texture2D") ? "texture" :
        !strcmp(token,"gl_FragColor") ? "ps4gl_fragment_color" : token;
      const LegacyDecl *boolean=NULL;
      for(unsigned i=0;i<source->count;++i)
        if(!strcmp(source->decl[i].kind,"uniform") && !strcmp(source->decl[i].type,"bool") &&
           !strcmp(token,source->decl[i].name)) { boolean=&source->decl[i]; break; }
      int uniform=legacy_find(source,"uniform",token);
      const LegacyStruct *st=uniform<0 ? NULL : legacy_struct(source,source->decl[uniform].type);
      if(!strcmp(token,"gl_FragCoord") && source->fragcoord) {
        legacy_emit(&out,"vec4(gl_FragCoord.x,gl_FragCoord.y*%s.ps4gl_fragcoord_y.x+%s.ps4gl_fragcoord_y.y,gl_FragCoord.zw)",instance,instance);
      }
      else if(st) {
        legacy_emit(&out,"%s(",st->name);
        for(unsigned j=0;j<st->count;++j) {
          char name[160]; legacy_field_name(name,sizeof(name),token,st->fields[j].name);
          if(j) legacy_emit(&out,",");
          if(!strcmp(st->fields[j].type,"bool")) legacy_emit(&out,"(%s.ps4gl_bool_%s != 0)",instance,name);
          else legacy_emit(&out,"%s.%s",instance,name);
        }
        legacy_emit(&out,")");
      }
      else if(boolean) legacy_emit(&out,"(%s.ps4gl_bool_%s != 0)",instance,boolean->name);
      else if(uniform>=0 && !legacy_sampler(source->decl[uniform].type)) legacy_emit(&out,"%s.%s",instance,token);
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
    if(match>=0) {
      const LegacyStruct *a=legacy_struct(&fs,d->type), *b=legacy_struct(&vs,vs.decl[match].type);
      if((a!=NULL)!=(b!=NULL) || (a && a->count!=b->count)) {
        diagnostic(log,cap,"Legacy uniform struct types differ across stages"); goto done;
      }
      if(a) for(unsigned j=0;j<a->count;++j)
        if(strcmp(a->fields[j].name,b->fields[j].name) || strcmp(a->fields[j].type,b->fields[j].type)) {
          diagnostic(log,cap,"Legacy uniform struct fields differ across stages"); goto done;
        }
    }
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
