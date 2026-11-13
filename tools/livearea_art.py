#!/usr/bin/env python3
"""Art-only LiveArea backdrop and a readable launch gate (Pillow)."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / "vita/sce_sys/livearea/contents"
FONT = ROOT / "web/fonts/LiberationSans-Bold.ttf"
CREAM, GOLD = (238, 234, 213), (244, 201, 42)

def font(size):
    return ImageFont.truetype(str(FONT), size)

def indexed(image, path):
    image.quantize(colors=256, method=Image.Quantize.MEDIANCUT).save(path, bits=8)

# Retain the existing skyline; replace only the empty sky/text area. Each
# scanline is a flat gradient sampled outside the old title. The backdrop
# has no title/subtitle overlay: only the launch gate carries text.
bg = Image.open(ART / "bg.png").convert("RGB")
draw = ImageDraw.Draw(bg)
for y in range(212):
    draw.line((0, y, 400, y), fill=bg.getpixel((bg.width - 1, y)))
indexed(bg, ART / "bg.png")

# Launch gate: one-line title, clearly legible platform label. Bottom 40 px
# contain only flat background so Vita's own Start label/button stays clear.
startup = Image.new("RGB", (280, 158), (17, 21, 43))
draw = ImageDraw.Draw(startup)
for y in range(118):
    t = y / 117
    color = tuple(round(a + (b - a) * t) for a, b in zip((13, 18, 40), (43, 37, 70)))
    draw.line((0, y, 279, y), fill=color)
draw.text((140, 17), "Vita Tower", font=font(30), fill=CREAM, anchor="mt")
draw.text((140, 65), "PS Vita", font=font(22), fill=GOLD, anchor="mt")
# Small lit towers frame the platform label without intruding on either label.
for x in (12, 242):
    draw.rectangle((x, 61, x + 25, 110), fill=(17, 23, 41))
    for wx in range(x + 4, x + 23, 6):
        for wy in range(67, 106, 7):
            draw.rectangle((wx, wy, wx + 2, wy + 3), fill=GOLD)
indexed(startup, ART / "startup.png")
