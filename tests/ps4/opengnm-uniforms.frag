#version 450
layout(location=0) in vec4 incol;
layout(location=0) out vec4 outcol;
layout(binding=0) uniform Tint { vec4 unused_value; vec4 tint; } a;
layout(binding=1) uniform Bias { vec4 bias; } b;
void main() { outcol = incol * a.tint + b.bias; }
