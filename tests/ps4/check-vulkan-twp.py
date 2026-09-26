#!/usr/bin/env python3
"""Independent CPU color oracle for ten original-shader Vulkan tiles."""
import sys
from PIL import Image

image = Image.open(sys.argv[1]).convert('RGB')
assert image.size == (1280, 720), image.size
color = [.8, .6, .4]

def expected(row, effect):
    tex = ([240,40,20] if row == 0 else [180,160,100])
    other = ([20,80,200] if row == 0 else [40,200,220])
    tex = [v/255 for v in tex]
    other = [v/255 for v in other]
    base = [a*b for a,b in zip(tex,color)]
    gray = sum(a*b for a,b in zip(base,[.299,.587,.114]))
    if effect == 0:
        return base
    if effect == 1:
        return [gray]*3
    if effect == 2:
        intensity = (max(tex)+min(tex))*.5
        weights = [(1-intensity)**4, (1-abs(-1+2*intensity))**4, intensity**4]
        adjustment = [[.1,.02,-.03],[-.02,.06,.01],[.04,-.01,.08]]
        fade = .5 if row == 0 else .8
        return [color[c]*min(1,max(0,tex[c]+fade*sum(weights[k]*adjustment[k][c] for k in range(3)))) for c in range(3)]
    if effect == 3:
        flicker = .9 if row == 0 else .7
        # UV (.25,.25)/(.75,.75): radius squared=.125, full vignette.
        return [(.2*base[c]+.8*gray*[.9,.8,.6][c])*flicker for c in range(3)]
    blend = .25 if row == 0 else .75
    second = [a*b for a,b in zip(other,color)]
    if row:
        luminance = sum(a*b for a,b in zip(second,[.299,.587,.114]))
        second = [.2*second[c]+.8*luminance*[.9,.8,.6][c] for c in range(3)]
    return [base[c]*(1-blend)+second[c]*blend for c in range(3)]

def srgb(v):
    return round(255*(12.92*v if v <= .0031308 else 1.055*v**(1/2.4)-.055))

count = 0
for row in range(2):
    for effect in range(5):
        want = tuple(srgb(v) for v in expected(row,effect))
        for y in (40,180,320):
            for x in (24,128,232):
                xy = (effect*256+x,row*360+y)
                actual = image.getpixel(xy)
                assert max(abs(a-b) for a,b in zip(actual,want)) <= 2, (row,effect,xy,actual,want)
                count += 1
print(f'PASS {count} original TWP shader RGB samples across ten tiles (tolerance 2/255)')
