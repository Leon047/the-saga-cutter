"""Compose a contact sheet: python contact_sheet.py <key> <out.png> [cap dir]. Rows = clips, 3 frames each, from cap/<key>_<Clip>_<i>."""
import os
import sys
from PIL import Image, ImageDraw

key, out = sys.argv[1], sys.argv[2]
CAP = sys.argv[3] if len(sys.argv) > 3 else os.path.join(__import__("tempfile").gettempdir(), "tripo_character", "cap")
CLIPS = ["Idle", "Walk", "Run", "PickUp", "SwordSlash", "Shoot_OneHanded", "Punch", "Death"]
N, T = 3, 320
cols = 2 * N  # two clips per row
rows = (len(CLIPS) + 1) // 2
sheet = Image.new("RGB", (cols * T, rows * T), (30, 30, 30))
d = ImageDraw.Draw(sheet)
for ci, clip in enumerate(CLIPS):
    r, c0 = ci // 2, (ci % 2) * N
    for i in range(N):
        im = Image.open(os.path.join(CAP, "%s_%s_%d" % (key, clip, i))).convert("RGB").resize((T, T))
        sheet.paste(im, ((c0 + i) * T, r * T))
        d.text(((c0 + i) * T + 6, r * T + 6), "%s %s %d" % (key, clip, i), fill=(255, 255, 0))
sheet.save(out)
print(out, sheet.size)
