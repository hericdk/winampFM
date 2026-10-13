#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The SAMPLE engine's Brazilian pitched sets, synthesised here (no recordings, nothing to credit):
writes assets/samples-cc0/<SET>/NN_<note>_m<MIDI root>.wav (22050 Hz mono) that gen_samples.py builds in.

    python3 tools/gen_builtin_brasil.py      (standard library only)

  CAVAQ  cavaquinho: a plucked steel string (Karplus-Strong), two zones
  AGOGO  agogo bells as a pitched instrument, two zones
  SANFO  sanfona (accordion): three detuned reeds with a slow bellows, one zone, looped
"""
import math
import random
import struct
import wave
from pathlib import Path

RATE = 22050
OUT = Path(__file__).resolve().parents[1] / "assets" / "samples-cc0"
NOTE = ["C", "Cs", "D", "Ds", "E", "F", "Fs", "G", "Gs", "A", "As", "B"]


def hz(m):
    return 440.0 * 2 ** ((m - 69) / 12)


def write(path, x):
    x = x[:len(x) & ~1]                                   # an even number of samples (ADPCM packs two to a byte)
    pk = max(abs(v) for v in x) or 1.0
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1), w.setsampwidth(2), w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(v / pk * 0.92 * 32767)) for v in x))


def fade(x, ms=60):
    n = max(1, int(RATE * ms / 1000))
    for i in range(n):
        x[len(x) - n + i] *= 0.5 * (1 + math.cos(math.pi * i / n))
    return x


def pluck(root, sec, seed):
    """Karplus-Strong, fractional delay, a little bright (steel strings), the pick as a noise burst"""
    rnd = random.Random(seed)
    n, period = int(sec * RATE), RATE / hz(root)
    N = int(period) + 2
    buf = [0.0] * n
    prev = 0.0
    for i in range(N):                                  # the burst: noise through a one-pole low pass
        prev = prev * 0.45 + rnd.uniform(-1, 1) * 0.55
        buf[i] = prev
    d, y1 = period, 0.0
    for i in range(N, n):
        p = i - d
        a = int(p)
        f = p - a
        s = buf[a] * (1 - f) + buf[a + 1] * f if a + 1 < i else buf[a]
        s2 = buf[a - 1] * (1 - f) + buf[a] * f
        buf[i] = 0.5 * (s + s2) * 0.9965
        y1 = buf[i]
    return fade(buf, 80)


def bell(root, sec):
    f0, out = hz(root), [0.0] * int(sec * RATE)
    for ratio, amp, dec in ((1.0, 1.0, 9.0), (2.76, 0.55, 14.0), (5.40, 0.30, 22.0), (8.93, 0.14, 30.0), (1.504, 0.35, 11.0)):
        w = 2 * math.pi * f0 * ratio / RATE
        for i in range(len(out)):
            out[i] += amp * math.sin(w * i) * math.exp(-dec * i / RATE)
    for i in range(int(0.003 * RATE)):                 # the stick
        out[i] *= i / (0.003 * RATE)
    return fade(out, 60)


def accordion(root, sec):
    n, out = int(sec * RATE), []
    f0 = hz(root)
    for i in range(n):
        t = i / RATE
        bell = 0.82 + 0.18 * math.sin(2 * math.pi * 4.2 * t)      # the bellows
        s = 0.0
        for det in (-0.0035, 0.0, 0.0042):                       # three reeds, a musette beat
            ph = (f0 * (1 + det) * t) % 1.0
            saw = 2 * ph - 1
            s += saw * 0.6 + (0.3 if ph < 0.35 else -0.7) * 0.4 * 1.0        # reed: saw + a narrow pulse
        out.append(s / 3 * bell * min(1.0, t * 40))
    mean = sum(out) / n                                         # (no DC: the pulse is not symmetric)
    out = [v - mean for v in out]
    # a gentle low pass (the box)
    y = 0.0
    for i in range(n):
        y += (out[i] - y) * 0.32
        out[i] = y
    return out


SETS = {
    "CAVAQ": [(62, 0.34, lambda r, s, k: pluck(r, s, 11 + k)), (74, 0.30, lambda r, s, k: pluck(r, s, 21 + k))],
    "AGOGO": [(67, 0.26, lambda r, s, k: bell(r, s)), (79, 0.24, lambda r, s, k: bell(r, s))],
    "SANFO": [(60, 0.52, lambda r, s, k: accordion(r, s))],
}


def main():
    for name, zones in SETS.items():
        for k, (root, sec, make) in enumerate(zones):
            write(OUT / name / f"{k:02d}_{NOTE[root % 12]}{root // 12 - 1}_m{root}.wav", make(root, sec, k))
        print(name, len(zones), "zones")


if __name__ == "__main__":
    main()
