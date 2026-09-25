#version 450
layout(location=0) out vec4 c;
void main(){c=vec4(gl_FragCoord.x<40.0?.125:.625,gl_FragCoord.y<30.0?.25:.5,.375,.5);}
