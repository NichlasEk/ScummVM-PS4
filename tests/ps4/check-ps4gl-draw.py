#!/usr/bin/env python3
"""Independent full-frame sample oracle for GL program/viewport/scissor draws."""
import sys
from PIL import Image
im=Image.open(sys.argv[1]).convert('RGB')
assert im.size==(1280,720)
inside=outside=0
for y in range(11,710,13):
    half=(y+.5-198)*240/270
    for x in range(9,1270,17):
        if abs(abs(x+.5-640)-half)<2 or abs(y+.5-468)<2:
            continue
        triangle=198<=y+.5<468 and abs(x+.5-640)<half
        clipped=500<=x+.5<800 and 280<=y+.5<540
        if triangle and clipped:
            green=.1+.8*(x+.5-400)/480
            want=(255,round(255*(1.055*green**(1/2.4)-.055)),0)
            inside+=1
        else:
            want=(127,127,127)
            outside+=1
        got=im.getpixel((x,y))
        assert max(abs(a-b) for a,b in zip(got,want))<=2,(x,y,got,want)
assert inside>100 and outside>1000
print(f'PASS GL draws: {inside} triangle and {outside} background/clipped samples')
