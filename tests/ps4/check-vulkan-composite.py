#!/usr/bin/env python3
"""Independent UNORM attachment/blend -> sampled alpha -> sRGB oracle."""
import math
import sys
from PIL import Image

im = Image.open(sys.argv[1]).convert('RGBA')
assert im.size == (1280, 720), im.size

def unorm(v):
    return math.floor(max(0, min(1, v)) * 255 + .5) / 255

def srgb(v):
    return round(255 * (12.92 * v if v <= .0031308 else 1.055 * v ** (1/2.4) - .055))

counts = [0] * 4
for y in range(17, 710, 23):
    for x in range(19, 1260, 29):
        if abs(x - 640) < 20 or abs(y - 360) < 16:
            continue
        base = list(map(unorm, (.125 if x < 640 else .625, .25 if y < 360 else .5, .375, .5)))
        if x < 640:
            color = [unorm(src * .25 + dst * .75) for src, dst in zip((.75,.125,.25), base[:3])]
            alpha = unorm(.25 + base[3] * .75)
        else:
            color, alpha = base[:3], base[3]
            if y < 360:
                color[0] = unorm(.75 * .25 + color[0])  # Red-only additive write.
        expected = tuple(srgb(c * alpha) for c in color) + (255,)
        actual = im.getpixel((x,y))
        assert all(abs(a-b) <= 2 for a,b in zip(actual,expected)), (x,y,actual,expected)
        counts[(y >= 360) * 2 + (x >= 640)] += 1
assert min(counts) > 200, counts
print(f'PASS: {sum(counts)} offscreen/blend/sampled-alpha pixel checks; quadrants {counts}')
