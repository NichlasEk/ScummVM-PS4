#version 450
layout(location=0) in vec2 position;
layout(location=1) in vec4 color;
layout(location=2) in vec2 texcoord;
layout(location=0) out vec4 outcolor;
layout(location=1) out vec2 outuv;
layout(binding=9) uniform Transform { mat4 matrix; } transform;
void main() {
    gl_Position = transform.matrix * vec4(position, 0.0, 1.0);
    outcolor = color;
    outuv = texcoord;
}
