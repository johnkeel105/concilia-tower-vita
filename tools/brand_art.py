#!/usr/bin/env python3
"""Vita Tower official art -> 8-bit indexed Vita PNGs (v0.11).

usage: python3 tools/brand_art.py LOGO.jpg DOME.jpg KEYART.jpg
  LOGO   (VITA TOWER / THE VERTICAL IS PORTABLE, on sky) -> livearea startup.png 280x158
  DOME   (round tower-top illustration)                  -> icon0.png 128x128
  KEYART (tower + helicopter over city, logo top right)  -> livearea bg.png 840x500,
                                                            pic0.png 960x544,
                                                            manual page banner strip
Source JPGs are NOT committed; only the generated PNGs are.
"""
import sys
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
SYS = ROOT / "vita/sce_sys"
LA = SYS / "livearea/contents"

def indexed(im, path, colors=256):
    """Median-cut palette + Floyd-Steinberg dither so sky gradients don't band."""
    im = im.convert("RGB")
    pal = im.quantize(colors=colors, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    out = im.quantize(palette=pal, dither=Image.Dither.FLOYDSTEINBERG)
    out.save(path, bits=8, optimize=True)
    return path

def cover(im, w, h, cx=0.5, cy=0.5, inset=0.0):
    """Scale to fill w x h, crop around (cx, cy) as fractions of the source.
    inset trims that fraction off every edge first (rounded white corners)."""
    if inset:
        dx, dy = round(im.width * inset), round(im.height * inset)
        im = im.crop((dx, dy, im.width - dx, im.height - dy))
    s = max(w / im.width, h / im.height)
    sw, sh = round(im.width * s), round(im.height * s)
    im = im.resize((sw, sh), Image.LANCZOS)
    x = min(max(round(sw * cx - w / 2), 0), sw - w)
    y = min(max(round(sh * cy - h / 2), 0), sh - h)
    return im.crop((x, y, x + w, y + h))

def dome_icon(src):
    im = src.convert("RGB")
    # Trim 4% so the white outside the painted circle stays in the corners,
    # which the Vita's circular bubble mask hides.
    side = round(min(im.size) * 0.92)
    im = im.crop(((im.width - side) // 2, (im.height - side) // 2,
                  (im.width + side) // 2, (im.height + side) // 2))
    # The Vita masks bubbles into a circle; keep the square art as-is so the
    # mask edge never shows a hard background seam.
    return im.resize((128, 128), Image.LANCZOS)

def main(logo, dome, key):
    logo, dome, key = (Image.open(p) for p in (logo, dome, key))
    print("sources:", logo.size, dome.size, key.size)
    indexed(dome_icon(dome), SYS / "icon0.png")
    # Gate (startup.png): Vita draws its own Start label over the bottom ~40 px.
    indexed(cover(logo, 280, 158, cx=0.52, cy=0.5, inset=0.035), LA / "startup.png")
    # LiveArea card: tower stays on the left; the template's a3 style puts
    # the gate on the right, over the sky.
    indexed(cover(key, 840, 500, cx=0.515), LA / "bg.png")
    indexed(cover(key, 960, 544), SYS / "pic0.png")
    # In-game splash (missing-EXE / boot error screen) uses pic0-sized art.
    for p in (SYS / "icon0.png", LA / "startup.png", LA / "bg.png", SYS / "pic0.png"):
        im = Image.open(p)
        print(p.relative_to(ROOT), im.size, im.mode, p.stat().st_size, "bytes")

if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:])
