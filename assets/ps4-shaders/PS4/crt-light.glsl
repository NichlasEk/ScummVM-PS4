// SPDX-License-Identifier: GPL-3.0-or-later
// Scanlines and a subtle RGB phosphor mask, without curvature or blur.
#version 120
#if defined(VERTEX)
attribute vec4 VertexCoord;
attribute vec4 TexCoord;
uniform mat4 MVPMatrix;
varying vec2 ps4TexCoord;
void main() {
    gl_Position = MVPMatrix * VertexCoord;
    ps4TexCoord = TexCoord.xy;
}
#elif defined(FRAGMENT)
uniform sampler2D Texture;
uniform vec2 TextureSize;
varying vec2 ps4TexCoord;
void main() {
    vec4 pixel = texture2D(Texture, ps4TexCoord);
    float row = fract(ps4TexCoord.y * TextureSize.y);
    float brightness = mix(0.65, 1.0, step(0.25, row));
    float column = mod(floor(gl_FragCoord.x), 3.0);
    vec3 mask = vec3(0.85);
    if (column < 1.0) mask.r = 1.0;
    else if (column < 2.0) mask.g = 1.0;
    else mask.b = 1.0;
    gl_FragColor = vec4(pixel.rgb * brightness * mask, pixel.a);
}
#endif
