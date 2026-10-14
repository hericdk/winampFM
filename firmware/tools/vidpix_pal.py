#!/usr/bin/env python3
"""winampFM: one palette for several clips. python3 tools/vidpix_pal.py OUT.png K IMG... -> a K-colour palette PNG made from all of them."""
import sys
from PIL import Image
out, k, imgs = sys.argv[1], int(sys.argv[2]), [Image.open(p).convert('RGB') for p in sys.argv[3:]]
w = max(i.width for i in imgs); sheet = Image.new('RGB', (w, sum(i.height for i in imgs))); y = 0
for i in imgs: sheet.paste(i, (0, y)); y += i.height
sheet.quantize(colors=k, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).save(out)
print(out, k)
