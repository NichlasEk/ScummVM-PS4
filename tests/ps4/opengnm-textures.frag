#version 450
layout(location=0) out vec4 outcol;
layout(binding=0) uniform Tint { vec4 unused_value; vec4 tint; } a;
layout(binding=1) uniform Bias { vec4 bias; } b;
layout(binding=3) uniform sampler2D nearestTex;
layout(binding=7) uniform sampler2D linearTex;
void main() {
    vec2 uv = fract(gl_FragCoord.xy / 96.0) * 1.5 - .25;
    vec4 sampled;
    if (gl_FragCoord.y < 360.0)
        sampled = textureLod(nearestTex, uv, 0.0);
    else
        sampled = textureLod(linearTex, uv, 0.0);
    outcol = vec4((sampled.rgb * a.tint.rgb + b.bias.rgb) * sampled.a, 1.0);
}
