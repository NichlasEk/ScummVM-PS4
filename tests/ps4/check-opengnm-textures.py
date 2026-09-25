#!/usr/bin/env python3
"""Independent CPU oracle for the GPU's 2x2 texture, filters and UBO transform."""
import math
import sys
from PIL import Image

im = Image.open(sys.argv[1]).convert('RGBA')
assert im.size == (1280, 720), im.size
texels = [(240, 40, 20, 255), (30, 220, 70, 128),
          (50, 90, 230, 64), (180, 160, 100, 192)]

def texel(x, y):
    return texels[max(0, min(1, y)) * 2 + max(0, min(1, x))]

def sample(u, v, linear):
    if not linear:
        return texel(math.floor(u * 2), math.floor(v * 2))
    x, y = u * 2 - .5, v * 2 - .5
    ix, iy = math.floor(x), math.floor(y)
    fx, fy = x - ix, y - iy
    return [sum(texel(ix + dx, iy + dy)[c] * wx * wy
                for dx, wx in [(0, 1 - fx), (1, fx)]
                for dy, wy in [(0, 1 - fy), (1, fy)]) for c in range(4)]

def srgb(v):
    return round(255 * (12.92 * v if v <= .0031308 else 1.055 * v ** (1 / 2.4) - .055))

for point in [(20, 20), (640, 100), (100, 360), (1180, 360), (640, 600)]:
    assert im.getpixel(point) == (127, 127, 127, 255), point

counts = [0, 0]
clamped = 0
for y in range(210, 490, 13):
    half_width = (y + .5 - 144) * 320 / 360
    for x in range(math.ceil(640 - half_width + 12), int(640 + half_width - 12), 13):
        u, v = [((coord + .5) / 96 % 1) * 1.5 - .25 for coord in (x, y)]
        linear = y >= 360
        raw = sample(u, v, linear)
        color = [raw[c] / 255 * scale + bias
                 for c, (scale, bias) in enumerate(zip((.75, .5, .625, 1), (.025, .125, .05, 0)))]
        expected = tuple(srgb(c * color[3]) for c in color[:3]) + (255,)
        got = im.getpixel((x, y))
        assert all(abs(a - b) <= 2 for a, b in zip(got, expected)), (x, y, got, expected, u, v)
        counts[linear] += 1
        clamped += int(not (0 <= u <= 1 and 0 <= v <= 1))
assert min(counts) > 100 and clamped > 100, (counts, clamped)
print(f'PASS: {counts[0]} nearest + {counts[1]} bilinear RGBA samples; {clamped} outside-UV samples; 5 background checks')
