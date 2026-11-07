#!/usr/bin/env python3
"""Make the TV banner and the Xbox tiles from your own picture (box art).

    py -3 tools/make_banner.py boxart.jpg [--top Y] [--left X] [--width W]
    py -3 tools/make_banner.py --demo demoart.jpg ...   # the demo app's

From one picture it writes:

  Android TV  the 640x360 launcher banner: a 16:9 crop, the full width from
              --top (on a box front, the title, above the ratings and the
              publisher's logos at the bottom), to
              android/banner/res/drawable-xhdpi/banner.png
  Xbox        the package's tiles: the square ones (150x150, 44x44, the 50x50
              store logo) from the largest square of the picture (all of a
              square picture), the wide tile (310x150) and the splash screen
              (620x300) from a crop of that shape at --top, to xbox/Assets-user/

With --demo they go to android/banner-demo/ and xbox/Assets-user-demo/ for the
demo app. Without demo art of its own, the demo app uses yours; without any,
both use the plain built-in ones. The APK build and xbox/package.ps1 pick
them up when they are there.

All of these folders are gitignored: box art is the publisher's, and this
repo ships nothing of the game's (README, "What is not in this repo").
Needs Pillow (py -3 -m pip install pillow).
"""
import argparse
import os
import sys

from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')


def banner_out(demo):
    return os.path.join(ROOT, 'android', 'banner-demo' if demo else 'banner',
                        'res', 'drawable-xhdpi', 'banner.png')


def tiles_out(demo):
    return os.path.join(ROOT, 'xbox', 'Assets-user-demo' if demo else 'Assets-user')


def band(im, left, top, width, aspect):
    """A crop of width x width/aspect from (left, top), or exit if it doesn't fit."""
    w = width or im.width - left
    h = round(w / aspect)
    if left + w > im.width or top + h > im.height:
        sys.exit('a %.2f:1 crop of width %d from (%d,%d) does not fit in %dx%d'
                 % (aspect, w, left, top, im.width, im.height))
    return im.crop((left, top, left + w, top + h))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('image')
    ap.add_argument('--top', type=int, default=0, help='first row of the wide crops, in source pixels')
    ap.add_argument('--left', type=int, default=0, help='first column of the wide crops')
    ap.add_argument('--width', type=int, default=0, help='width of the wide crops (default: the rest of the picture)')
    ap.add_argument('--demo', action='store_true', help="the demo app's")
    a = ap.parse_args()
    im = Image.open(a.image).convert('RGB')

    def save(img, path):
        os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
        img.save(path, optimize=True)
        print('%-26s %4dx%-4d -> %s' % (os.path.basename(path), img.width, img.height, os.path.normpath(path)))

    # Android TV: the 16:9 banner.
    save(band(im, a.left, a.top, a.width, 16 / 9).resize((640, 360), Image.LANCZOS), banner_out(a.demo))

    # Xbox: the square tiles from the largest square (from the top of a tall
    # picture, where a box front has its title).
    side = min(im.width, im.height)
    sq_left = (im.width - side) // 2
    sq_top = 0 if im.height == side else min(a.top, im.height - side)
    square = im.crop((sq_left, sq_top, sq_left + side, sq_top + side))
    tiles = tiles_out(a.demo)
    for name, size in (('Square150x150Logo.png', 150), ('Square44x44Logo.png', 44), ('StoreLogo.png', 50)):
        save(square.resize((size, size), Image.LANCZOS), os.path.join(tiles, name))
    # ... and the wide ones from a crop of their shape at --top.
    wide = band(im, a.left, a.top, a.width, 310 / 150)
    save(wide.resize((310, 150), Image.LANCZOS), os.path.join(tiles, 'Wide310x150Logo.png'))
    save(wide.resize((620, 300), Image.LANCZOS), os.path.join(tiles, 'SplashScreen.png'))


if __name__ == '__main__':
    main()
