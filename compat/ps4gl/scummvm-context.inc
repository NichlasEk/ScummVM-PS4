/* Injected only into the isolated experimental Context::initialize.
 * Load the explicit bridge subset without setting GLAD version/extension flags.
 * Engine shaders use the validated desktop subset; no general GL version is advertised.
 */
{
	if (type != kContextGL) error("PS4GL subset requires the desktop shader path");
#define PS4GL_LOAD(name) \
	name = reinterpret_cast<decltype(name)>(loadFunc(#name)); \
	if (!name) error("PS4GL missing required entry: %s", #name)
	PS4GL_LOAD(glBindAttribLocation);
	PS4GL_LOAD(glGetAttribLocation);
	PS4GL_LOAD(glCreateShader);
	PS4GL_LOAD(glShaderSource);
	PS4GL_LOAD(glCompileShader);
	PS4GL_LOAD(glDeleteShader);
	PS4GL_LOAD(glIsShader);
	PS4GL_LOAD(glGetShaderiv);
	PS4GL_LOAD(glGetShaderInfoLog);
	PS4GL_LOAD(glGetShaderSource);
	PS4GL_LOAD(glCreateProgram);
	PS4GL_LOAD(glAttachShader);
	PS4GL_LOAD(glDetachShader);
	PS4GL_LOAD(glLinkProgram);
	PS4GL_LOAD(glGetProgramInfoLog);
	PS4GL_LOAD(glClearColor);
	PS4GL_LOAD(glClear);
	PS4GL_LOAD(glScissor);
	PS4GL_LOAD(glViewport);
	PS4GL_LOAD(glBlendFunc);
	PS4GL_LOAD(glBlendFuncSeparate);
	PS4GL_LOAD(glBlendEquation);
	PS4GL_LOAD(glBlendEquationSeparate);
	PS4GL_LOAD(glEnable);
	PS4GL_LOAD(glDisable);
	PS4GL_LOAD(glIsEnabled);
	PS4GL_LOAD(glGetError);
	PS4GL_LOAD(glGetBooleanv);
	PS4GL_LOAD(glDepthMask);
	PS4GL_LOAD(glGetIntegerv);
	PS4GL_LOAD(glGetString);
	PS4GL_LOAD(glGetFloatv);
	PS4GL_LOAD(glUseProgram);
	PS4GL_LOAD(glDeleteProgram);
	PS4GL_LOAD(glIsProgram);
	PS4GL_LOAD(glGenFramebuffers);
	PS4GL_LOAD(glBindFramebuffer);
	PS4GL_LOAD(glDeleteFramebuffers);
	PS4GL_LOAD(glIsFramebuffer);
	PS4GL_LOAD(glFramebufferTexture2D);
	PS4GL_LOAD(glCheckFramebufferStatus);
	PS4GL_LOAD(glGenTextures);
	PS4GL_LOAD(glBindTexture);
	PS4GL_LOAD(glDeleteTextures);
	PS4GL_LOAD(glIsTexture);
	PS4GL_LOAD(glActiveTexture);
	PS4GL_LOAD(glPixelStorei);
	PS4GL_LOAD(glTexParameteri);
	PS4GL_LOAD(glTexImage2D);
	PS4GL_LOAD(glTexSubImage2D);
	PS4GL_LOAD(glUniform1i);
	PS4GL_LOAD(glUniform1iv);
	PS4GL_LOAD(glGetUniformLocation);
	PS4GL_LOAD(glUniform1f);
	PS4GL_LOAD(glUniform2f);
	PS4GL_LOAD(glUniform3f);
	PS4GL_LOAD(glUniform4f);
	PS4GL_LOAD(glUniform1fv);
	PS4GL_LOAD(glUniform2fv);
	PS4GL_LOAD(glUniform3fv);
	PS4GL_LOAD(glUniform4fv);
	PS4GL_LOAD(glUniformMatrix4fv);
	PS4GL_LOAD(glGetProgramiv);
	PS4GL_LOAD(glDrawArrays);
	PS4GL_LOAD(glDrawElements);
	PS4GL_LOAD(glGenBuffers);
	PS4GL_LOAD(glBindBuffer);
	PS4GL_LOAD(glDeleteBuffers);
	PS4GL_LOAD(glIsBuffer);
	PS4GL_LOAD(glBufferData);
	PS4GL_LOAD(glBufferSubData);
	PS4GL_LOAD(glGetBufferParameteriv);
	PS4GL_LOAD(glVertexAttrib2fv);
	PS4GL_LOAD(glVertexAttrib3fv);
	PS4GL_LOAD(glVertexAttrib4fv);
	PS4GL_LOAD(glVertexAttribPointer);
	PS4GL_LOAD(glEnableVertexAttribArray);
	PS4GL_LOAD(glDisableVertexAttribArray);
#undef PS4GL_LOAD
	const char *identity = (const char *)glGetString(GL_RENDERER);
	if (!identity || Common::String(identity) != "OpenGNM Vulkan bridge (experimental)")
		error("PS4GL subset loader used with the wrong renderer");
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, (GLint *)&maxTextureSize);
	if (!maxTextureSize || glGetError() != GL_NO_ERROR) error("PS4GL invalid texture limit");
	glslVersion = 120;
	enginesShadersSupported = true;
	multitextureSupported = true;
	shadersSupported = true;
	NPOTSupported = true;
	textureEdgeClampSupported = true;
	textureLookupPrecision = UINT_MAX;
	debug(5, "PS4GL explicit desktop shader subset loaded; general GL version remains disabled");
	return;
}
