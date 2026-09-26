#version 450
layout(location=0) in vec4 outcol;
layout(set=0,binding=1,std140) uniform FragmentDefaults { vec4 tint; float gain; };
layout(location=0) out vec4 result;
void main() { result=outcol*tint*gain; }
