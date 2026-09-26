#version 450
layout(location=0) in vec4 outcol;
// Match TWP's five float slots: offset 16, stride 16, next scalar at 96.
layout(set=0,binding=1,std140) uniform FragmentDefaults {
    vec4 tint;
    float RandomValue[5];
    float TimeLapse;
    float gain;
};
layout(location=0) out vec4 result;
void main() {
    float sum=0;
    for(int i=0;i<4;++i) sum+=RandomValue[i];
    vec4 multiplier=mix(vec4(1),tint,RandomValue[4]);
    result=outcol*multiplier*gain*(sum+TimeLapse);
}
