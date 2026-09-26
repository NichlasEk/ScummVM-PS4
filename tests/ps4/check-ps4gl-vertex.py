#!/usr/bin/env python3
"""Independent image oracle: distinct immutable vertex payloads within one frame."""
from PIL import Image
import sys
im=Image.open(sys.argv[1]).convert('RGB')
assert im.size==(1280,720)
def srgb(v):
    return round(255*(12.92*v if v<=.0031308 else 1.055*v**(1/2.4)-.055))
counts=[0,0,0]
for y in range(11,710,13):
    half=(y+.5-144)*160/360
    for x in range(9,1270,17):
        panel=x//640
        center=320+panel*640
        if abs(abs(x+.5-center)-half)<2 or abs(y+.5-504)<2: continue
        if 144<=y+.5<504 and abs(x+.5-center)<half:
            green=.1+.8*(x+.5-(160+panel*640))/320
            want=(srgb(1 if panel==0 else .25),srgb(green),srgb(.25 if panel==0 else .75))
            counts[panel]+=1
        else:
            want=(127,127,127); counts[2]+=1
        got=im.getpixel((x,y))
        assert max(abs(a-b) for a,b in zip(got,want))<=2,(x,y,got,want)
assert min(counts)>200,counts
print(f'PASS GL vertex snapshots: left/right/background samples {counts}')
