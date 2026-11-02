// SPDX-License-Identifier: GPL-3.0-or-later
// Lightweight PS4 post-processing filter for ScummVM's Libretro pipeline.
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
    float brightness = mix(0.70, 1.0, step(0.25, row));
    gl_FragColor = vec4(pixel.rgb * brightness, pixel.a);
}
#endif
