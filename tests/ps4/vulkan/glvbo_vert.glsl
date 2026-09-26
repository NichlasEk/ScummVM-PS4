#version 450
layout(location=0) in vec2 position;
layout(location=1) in vec4 color;
layout(location=2) in vec2 uv;
layout(location=0) out vec4 outcol;
void main() {
    gl_Position=vec4(position.x,-position.y,.5,1);
    outcol=vec4(color.rg,uv.x,color.a);
}
