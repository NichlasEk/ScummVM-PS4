static int draw_gl_blend(void (*draw_gl)(GLenum,GLint,GLsizei)) {
  GLint program; glGetIntegerv(GL_CURRENT_PROGRAM,&program);
  GLint color=glGetUniformLocation(program,"color"); if(color<0) return 2;
  for(unsigned tile=0;tile<8;++tile) {
    glViewport(tile*160,0,160,720); glDisable(GL_BLEND);
    glUniform4f(color,.2f,.4f,.6f,.25f); draw_gl(GL_TRIANGLES,0,3);
    glEnable(GL_BLEND); glBlendEquation(GL_FUNC_ADD);
    if(tile==0) glDisable(GL_BLEND);
    if(tile==1) glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    if(tile==2) glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    if(tile>=3 && tile<=5) glBlendFunc(GL_ONE,GL_ONE);
    if(tile==4) glBlendEquation(GL_FUNC_SUBTRACT);
    if(tile==5) glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
    if(tile>=6) { glBlendFunc(GL_ZERO,GL_ZERO); glBlendEquation(tile==6 ? GL_MIN : GL_MAX); }
    if(tile==7) glBlendEquationSeparate(GL_MAX,GL_MIN);
    glUniform4f(color,.8f,.2f,.1f,.5f); draw_gl(GL_TRIANGLES,0,3);
    /* Expose stored destination alpha as RGB in the bottom half. */
    glViewport(tile*160,0,160,360); glEnable(GL_BLEND); glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_DST_ALPHA,GL_ZERO,GL_ONE,GL_ZERO);
    glUniform4f(color,1,1,1,1); draw_gl(GL_TRIANGLES,0,3);
  }
  glDisable(GL_BLEND); glUniform4f(color,0,0,0,0);
  if(glGetError()!=GL_NO_ERROR) return 2;
  PS4GLProgram *p=&ps4glCurrentContext()->programs[program-1];
  if(p->variant_count!=8) return 2;
  fprintf(report,"PASS GL blend variants=8 and state snapshots\n");
  return 0;
}
