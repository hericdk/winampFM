#!/usr/bin/env python3
"""winampFM: a video (or a picture) turned into chunky pixel art for the whole-screen visualizer (local tests).
    python3 tools/vidpix_import.py NAME VIDEO --t0 S --dur S --frames N [--crop X,Y,W,H] [--colors K] [--key-green] [--size 60]
-> assets/vidpix/NAME/00.png ... (palette PNGs, SIZE x SIZE, one shared palette of K colours, no dithering: long runs pack small).
gen_vidpix.py packs them into build/gen/wfm_vidpix.h."""
import argparse, os, subprocess, sys, tempfile
from PIL import Image

ap = argparse.ArgumentParser()
ap.add_argument('name'); ap.add_argument('video')
ap.add_argument('--t0', type=float, default=0.0); ap.add_argument('--dur', type=float, default=4.0); ap.add_argument('--frames', type=int, default=16)
ap.add_argument('--crop', default=''); ap.add_argument('--colors', type=int, default=6); ap.add_argument('--key-green', action='store_true')
ap.add_argument('--size', type=int, default=60)
ap.add_argument('--circle', action='store_true')
ap.add_argument('--pal', default='')
ap.add_argument('--times', default='')
ap.add_argument('--eq', default='')          # an ffmpeg eq= before the scale (contrast / brightness / gamma)
ap.add_argument('--first', default='')        # a still image put before the video's frames (frame 0)
ap.add_argument('--first-crop', default='')   # exact frame times, comma separated (overrides t0 / dur / frames)   # a palette PNG to share (tools/vidpix_pal.py)   # outside the round frame: black
a = ap.parse_args()
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
out = os.path.join(root, 'assets', 'vidpix', a.name)
os.makedirs(out, exist_ok=True)
for f in os.listdir(out):
    if f.endswith('.png'): os.remove(os.path.join(out, f))
tmp = tempfile.mkdtemp()
frames = []
TS = [float(v) for v in a.times.split(',')] if a.times else [a.t0 + a.dur * i / a.frames for i in range(a.frames)]
for i, t in enumerate(TS):
    p = os.path.join(tmp, f'{i:02d}.png')
    cx, cy, cw, ch = (a.crop.split(',') + ['0'] * 4)[:4] if a.crop else ('', '', '', '')
    vf = (f'crop={cw}:{ch}:{cx}:{cy},' if a.crop else '') + (f'eq={a.eq},' if a.eq else '') + f'scale={a.size}:{a.size}:flags=area'
    subprocess.run(['ffmpeg', '-loglevel', 'error', '-y'] + ([] if a.video.endswith(('.png', '.webp', '.jpg')) else ['-ss', f'{t:.3f}']) + ['-i', a.video, '-frames:v', '1', '-vf', vf, p], check=True)
    im = Image.open(p).convert('RGB')
    if a.key_green:                                   # the green screen goes black
        px = im.load()
        for y in range(im.height):
            for x in range(im.width):
                r, g, b = px[x, y]
                if g > 90 and g > r * 1.3 and g > b * 1.3: px[x, y] = (0, 0, 0)
    if a.circle:                                       # only the round picture: the rest black
        px = im.load(); c = (a.size - 1) / 2
        for y in range(im.height):
            for x in range(im.width):
                if (x - c) ** 2 + (y - c) ** 2 > (c - 1) ** 2: px[x, y] = (0, 0, 0)
    frames.append(im)
if a.first:
    p0 = os.path.join(tmp, 'first.png')
    fx, fy, fw, fh = a.first_crop.split(',') if a.first_crop else ('0', '0', 'iw', 'ih')
    subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', '-i', a.first, '-vf', f'crop={fw}:{fh}:{fx}:{fy},scale={a.size}:{a.size}:flags=area', p0], check=True)
    im = Image.open(p0).convert('RGB')
    if a.circle:
        px = im.load(); c = (a.size - 1) / 2
        for y in range(im.height):
            for x in range(im.width):
                if (x - c) ** 2 + (y - c) ** 2 > (c - 1) ** 2: px[x, y] = (0, 0, 0)
    frames.insert(0, im)
sheet = Image.new('RGB', (a.size, a.size * len(frames)))
for i, im in enumerate(frames): sheet.paste(im, (0, a.size * i))
pal = Image.open(a.pal) if a.pal else sheet.quantize(colors=a.colors, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
for i, im in enumerate(frames):
    im.quantize(palette=pal, dither=Image.Dither.NONE).save(os.path.join(out, f'{i:02d}.png'))
print(a.name, len(frames), 'frames', a.colors, 'colours ->', out)
