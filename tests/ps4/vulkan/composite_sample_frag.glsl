#version 450
layout(binding=3) uniform sampler2D rendered;
layout(location=0) out vec4 c;
void main(){vec4 t=textureLod(rendered,gl_FragCoord.xy/vec2(1280,720),0.0);c=vec4(t.rgb*t.a,1);}
