#!/usr/bin/env python3
"""Make the Android TV banner from your own picture (box art, a screenshot).

    py -3 tools/make_banner.py boxart.jpg [--top Y] [--left X] [--width W]
    py -3 tools/make_banner.py --demo demoart.jpg ...   # the demo app's banner

Crops a 16:9 piece of the picture (the full width from the top by default,
where a box front has its title, above the ratings and publisher logos at
the bottom) and writes it at the launcher's size, 640x360, to
android/banner/res/drawable-xhdpi/banner.png (--demo: android/banner-demo/,
for the demo app; without one the demo app uses the full game's). The APK
build uses it in place of the plain built-in banner when it is there.

android/banner/ and banner-demo/ are gitignored: box art is the publisher's, and this repo
ships nothing of the game's (README, "What is not in this repo").
Needs Pillow (py -3 -m pip install pillow).
"""
import argparse
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
def out_for(demo):
    return os.path.join(HERE, '..', 'android', 'banner-demo' if demo else 'banner',
                        'res', 'drawable-xhdpi', 'banner.png')

def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('image')
    ap.add_argument('--top', type=int, default=0, help='first row of the crop, in source pixels')
    ap.add_argument('--left', type=int, default=0, help='first column of the crop')
    ap.add_argument('--width', type=int, default=0, help='crop width (default: the rest of the picture)')
    ap.add_argument('--demo', action='store_true', help="the demo app's banner")
    ap.add_argument('--out')
    a = ap.parse_args()
    a.out = a.out or out_for(a.demo)

    im = Image.open(a.image).convert('RGB')
    w = a.width or im.width - a.left
    h = round(w * 9 / 16)
    if a.left + w > im.width or a.top + h > im.height:
        sys.exit('a 16:9 crop of width %d from (%d,%d) does not fit in %dx%d'
                 % (w, a.left, a.top, im.width, im.height))
    banner = im.crop((a.left, a.top, a.left + w, a.top + h)).resize((640, 360), Image.LANCZOS)
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    banner.save(a.out, optimize=True)
    print('banner: %dx%d from (%d,%d) -> %s' % (w, h, a.left, a.top, os.path.normpath(a.out)))

if __name__ == '__main__':
    main()
