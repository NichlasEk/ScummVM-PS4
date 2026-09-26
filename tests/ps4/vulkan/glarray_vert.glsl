#version 450
layout(location=0) in vec2 position;
layout(location=1) in vec4 color;
layout(location=2) in vec2 uv;
layout(set=0,binding=0,std140) uniform VertexDefaults { mat4 transform[2]; float gain; };
layout(location=0) out vec4 outcol;
void main() {
    gl_Position=transform[1]*vec4(position.x,-position.y,.5,1);
    outcol=vec4(color.rg,uv.x,color.a)*gain;
}
