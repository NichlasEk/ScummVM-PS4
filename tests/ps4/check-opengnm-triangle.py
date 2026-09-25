#!/usr/bin/env python3
"""Check real shadPS4 framebuffer pixels, including interpolated VS outputs.

The gray background is a CPU prefill. Only the triangle is GPU rendered.
Rejects the prior all-black frame and the broken compiler's black triangle.
"""
import math
import sys
from PIL import Image

im = Image.open(sys.argv[1]).convert('RGBA')
assert im.size == (1280, 720), im.size

def check(x, y, expected, tolerance=2):
    got = im.getpixel((x, y))
    assert all(abs(a - b) <= tolerance for a, b in zip(got, expected)), (x, y, got, expected)

for point in [(20, 20), (640, 100), (100, 360), (1180, 360), (640, 600)]:
    check(*point, (127, 127, 127, 255), 0)

def srgb(v):
    return round(255 * (12.92 * v if v <= 0.0031308 else 1.055 * v ** (1 / 2.4) - 0.055))

# GLSL vertex colors vary linearly with x, then the sRGB render target encodes.
count = 0
for y in range(230, 480, 25):
    half_width = (y + .5 - 144) * 320 / 360
    for x in range(math.ceil(640 - half_width + 12), int(640 + half_width - 12), 23):
        green = .1 + .8 * (x + .5 - 320) / 640
        check(x, y, (255, srgb(green), 0, 255))
        count += 1
print(f'PASS: {count} GPU-interpolated color samples and 5 background samples')
