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
# System e-manual pages (LiveArea "Manual"): one 960x544 8-bit indexed PNG per section.
bold = ImageFont.truetype(str(ROOT / "web/fonts/LiberationSans-Bold.ttf"), 30)
body = ImageFont.truetype(str(ROOT / "web/fonts/LiberationSans-Regular.ttf"), 22)
dest = ROOT / "vita/sce_sys/manual"; dest.mkdir(parents=True, exist_ok=True)
for old in dest.glob("*.png"): old.unlink()
for i, (title, lines) in enumerate(secs):
    im = Image.new("RGB", (960, 544), (192, 192, 192)); d = ImageDraw.Draw(im)
    d.rectangle((0, 0, 959, 44), fill=(0, 0, 128))
    d.text((20, 7), "Vita Tower - %s (%d/%d)" % (title, i + 1, len(secs)), font=bold, fill=(255, 255, 255))
    y = 54; size = 22 if len(lines) <= 19 else 19
    f = ImageFont.truetype(str(ROOT / "web/fonts/LiberationSans-Regular.ttf"), size)
    for l in lines:
        d.text((24, y), l, font=f, fill=(0, 0, 0)); y += size + 4
    assert y <= 544, title
    im.quantize(colors=256, method=Image.Quantize.MEDIANCUT).save(dest / ("%03d.png" % (i + 1)), bits=8)
print(len(secs), "sections")
