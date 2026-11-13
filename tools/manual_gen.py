#!/usr/bin/env python3
"""Generate src/manual_text.h and LiveArea e-manual pages from vita/manual/manual.txt."""
import json
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
ROOT = Path(__file__).resolve().parents[1]
secs, cur = [], None
for line in (ROOT / "vita/manual/manual.txt").read_text().splitlines():
    if line.startswith("## "):
        cur = [line[3:], []]; secs.append(cur)
    elif cur is not None:
        cur[1].append(line)
for s in secs:
    while s[1] and not s[1][-1].strip(): s[1].pop()
    for l in s[1]: assert len(l) <= 80, l
c = json.dumps
out = ["/* Generated from vita/manual/manual.txt by tools/manual_gen.py. */",
       "typedef struct { const char *title; const char *const *lines; int count; } CT_ManualSection;"]
for i, s in enumerate(secs):
    out.append("static const char *const ct_manual_lines_%d[]={%s};" % (i, ",".join(c(l) for l in s[1])))
out.append("static const CT_ManualSection ct_manual[]={%s};" % ",".join(
    "{%s,ct_manual_lines_%d,%d}" % (c(s[0]), i, len(s[1])) for i, s in enumerate(secs)))
out.append("#define CT_MANUAL_SECTIONS %d" % len(secs))
(ROOT / "src/manual_text.h").write_text("\n".join(out) + "\n")
# System e-manual pages (LiveArea "Manual"): one 960x544 8-bit indexed PNG per
# section (960x544 = native fullscreen manual page; files 001.png, 002.png...).
# Background: Tyler's navy silhouette key art, kept in-repo as an indexed
# 960x544 PNG (vita/manual/manual_bg.png). Recreate it from the source JPG with
#   python3 tools/manual_gen.py --bg /path/to/readme_silhouette.jpg
import sys
BG = ROOT / "vita/manual/manual_bg.png"

def dither(im):
    im = im.convert("RGB")
    pal = im.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    return im.quantize(palette=pal, dither=Image.Dither.FLOYDSTEINBERG)

if "--bg" in sys.argv:
    src = Image.open(sys.argv[sys.argv.index("--bg") + 1]).convert("RGB")
    sc = max(960 / src.width, 544 / src.height)
    src = src.resize((round(src.width * sc), round(src.height * sc)), Image.LANCZOS)
    x, y = (src.width - 960) // 2, (src.height - 544) // 2
    dither(src.crop((x, y, x + 960, y + 544))).save(BG, bits=8, optimize=True)
background = Image.open(BG).convert("RGB") if BG.exists() else Image.new("RGB", (960, 544), (20, 40, 70))
REG, BOLD = str(ROOT / "web/fonts/LiberationSans-Regular.ttf"), str(ROOT / "web/fonts/LiberationSans-Bold.ttf")
bold = ImageFont.truetype(BOLD, 30)
dest = ROOT / "vita/sce_sys/manual"; dest.mkdir(parents=True, exist_ok=True)
for old in dest.glob("*.png"): old.unlink()
WHITE, BODY, SHADOW = (255, 255, 255), (232, 238, 246), (0, 8, 24)
for i, (title, lines) in enumerate(secs):
    im = background.copy().convert("RGBA")
    ov = Image.new("RGBA", im.size, (0, 0, 0, 0)); o = ImageDraw.Draw(ov)
    o.rectangle((0, 0, 959, 46), fill=(4, 14, 40, 205))                     # header bar
    size = 22 if len(lines) <= 19 else 19
    bottom = 56 + len(lines) * (size + 4) + 8
    fw = ImageFont.truetype(REG, size)
    right = max([400] + [26 + round(fw.getlength(l)) for l in lines]) + 24   # panel hugs the text
    o.rounded_rectangle((12, 52, min(right, 947), min(bottom, 538)), 10, fill=(4, 12, 34, 150))  # text panel
    im = Image.alpha_composite(im, ov); d = ImageDraw.Draw(im)
    head = "Vita Tower - %s (%d/%d)" % (title, i + 1, len(secs))
    d.text((21, 9), head, font=bold, fill=SHADOW); d.text((20, 7), head, font=bold, fill=WHITE)
    f = ImageFont.truetype(REG, size); y = 58
    for l in lines:
        d.text((27, y + 1), l, font=f, fill=SHADOW); d.text((26, y), l, font=f, fill=BODY); y += size + 4
    assert y <= 544, title
    dither(im).save(dest / ("%03d.png" % (i + 1)), bits=8, optimize=True)
print(len(secs), "sections")
