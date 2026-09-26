#!/usr/bin/env python3
"""Independent RGB and destination-alpha oracle for GL blend state."""
from PIL import Image
import sys
im=Image.open(sys.argv[1]).convert('RGB')
assert im.size==(1280,720)
def srgb(x):
    x=max(0,min(1,x))
    return round(255*(12.92*x if x<=.0031308 else 1.055*x**(1/2.4)-.055))
s=[.8,.2,.1,.5]; d=[.2,.4,.6,.25]
expected=[s,[(a+b)*.5 for a,b in zip(s,d)],
          [(s[i]+d[i])*.5 for i in range(3)]+[s[3]+d[3]*.5],
          [min(1,a+b) for a,b in zip(s,d)], [max(0,a-b) for a,b in zip(s,d)],
          [max(0,b-a) for a,b in zip(s,d)], [min(a,b) for a,b in zip(s,d)],
          [max(s[i],d[i]) for i in range(3)]+[min(s[3],d[3])]]
checks=0
for tile,rgba in enumerate(expected):
    for y in range(13,710,17):
        if abs(y-360)<5: continue
        want=tuple(srgb(v) for v in (rgba[:3] if y<360 else [rgba[3]]*3))
        for x in range(tile*160+9,(tile+1)*160-5,19):
            got=im.getpixel((x,y))
            assert max(abs(a-b) for a,b in zip(got,want))<=3,(tile,x,y,got,want)
            checks+=1
assert checks>2000
print(f'PASS GL blend: {checks} RGB/alpha checks across eight blend modes')
