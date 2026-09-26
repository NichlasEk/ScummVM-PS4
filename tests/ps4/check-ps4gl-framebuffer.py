#!/usr/bin/env python3
"""Asymmetric GL framebuffer oracle: orientation, LOAD, updates and old storage."""
from PIL import Image
import sys
im=Image.open(sys.argv[1]).convert('RGB')
assert im.size==(1280,720)
checks=0
for y in range(13,710,17):
    if abs(y-360)<8: continue
    for x in range(11,1270,19):
        if min(abs(x-edge) for edge in (320,480,640))<8: continue
        if x<640:
            want=(255,0,0) if y>360 else (0,255,0) if x>480 else (0,0,255)
        else:
            want=(188,255,188) if y>360 else (255,255,0)
        if x<64 and y<64: want=(255,0,255)
        if abs(x-64)<5 and y<70 or abs(y-64)<5 and x<70: continue
        got=im.getpixel((x,y))
        assert max(abs(a-b) for a,b in zip(got,want))<=3,(x,y,got,want)
        checks+=1
assert checks>2000
print(f'PASS GL framebuffer: {checks} pixel checks for orientation, preservation, blend and retained storage')
