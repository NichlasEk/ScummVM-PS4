#version 450
layout(set=0,binding=1,std140) uniform FragmentDefaults { vec4 color; };
layout(location=0) out vec4 result;
void main() { result=color; }
