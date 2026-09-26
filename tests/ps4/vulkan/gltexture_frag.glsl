#version 450
layout(location=0) in vec4 outcol;
layout(set=0,binding=2) uniform sampler2D imageA;
layout(set=0,binding=3) uniform sampler2D imageB;
layout(set=0,binding=1,std140) uniform FragmentDefaults { vec4 coordinates; };
layout(location=0) out vec4 result;
void main() {
    result=vec4(texture(imageA,coordinates.xy).r,outcol.g,texture(imageB,coordinates.zw).b,1);
}
