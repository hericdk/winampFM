#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Pack the generated bitmap fonts and the Quest sprites with LZ4 (lossless; build/gen headers rewritten in place).

Fonts (FONT_S, FONT_Q, FONT_QT) and sprites (QSPR) are cut into chunks of whole glyphs / sprites of at most 1 KiB, each chunk is an
LZ4 block (tools/lz4blk.py); the firmware (gfx.c pk_*) unpacks the one chunk a glyph / sprite lives in into a 1 KiB buffer.
  fonts:   FONT_x_DATA = the compressed chunks, FONT_x_OFF[g] = offset inside its chunk, FONT_x_GC[g] = chunk, FONT_x_ZO[c] = chunk start
  sprites: QSPR_DATA likewise, QSPR[i].off inside its chunk, QSPR_CK[i] = chunk, QSPR_ZO[c]
The logo (felucca_qtitle.h) and the title picture are NOT packed. FONT_L (not linked) stays as it is (gc = 0)."""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lz4blk  # noqa: E402

CHUNK = 1024


def ints(s):
    return [int(x, 0) for x in re.findall(r"-?0x[0-9A-Fa-f]+|-?\d+", s)]


def arr_re(name):
    return re.compile(r"static const (uint8_t|uint16_t|int8_t) " + name + r"\[[^\]]*\]\s*=\s*\{(.*?)\};\n", re.S)


def take(text, name):
    m = arr_re(name).search(text)
    if not m:
        raise SystemExit(f"pack_gfx: {name} not found")
    return m, ints(m.group(2))


def pack_items(raw_items):
    """raw_items: list of bytes. -> (compressed blob, [chunk of item], [offset inside chunk], [chunk start in blob])"""
    chunks, ck, off, cur, curlen = [], [], [], bytearray(), 0
    for it in raw_items:
        if cur and len(cur) + len(it) > CHUNK:
            chunks.append(bytes(cur))
            cur = bytearray()
        ck.append(len(chunks))
        off.append(len(cur))
        cur += it
    if cur:
        chunks.append(bytes(cur))
    blob, zo = bytearray(), []
    for c in chunks:
        z = lz4blk.compress(c)
        assert lz4blk.decompress(z) == c, "lz4 round trip"
        zo.append(len(blob))
        blob += z
    zo.append(len(blob))
    assert max(len(c) for c in chunks) <= 1152, "a chunk is bigger than the firmware's unpack buffer"
    return bytes(blob), ck, off, zo


def carr(ctype, name, vals):
    body = ",".join(str(v) for v in vals)
    return f"static const {ctype} {name}[{len(vals)}] = {{{body}}};\n"


def pack_font(text, name):
    if re.search(name + r"_GC\b", text):
        return text                                              # already packed
    md, data = take(text, name + "_DATA")
    mo, off = take(text, name + "_OFF")
    mb, bw = take(text, name + "_BW")
    ms = re.search(r"static const felucca_font_t " + name + r" = \{([^}]*)\};\n", text)
    f = [x.strip() for x in ms.group(1).split(",")]
    h = int(f[0])
    items = []
    for g in range(len(off)):
        n = h * ((bw[g] + 1) // 2)
        items.append(bytes(data[off[g]:off[g] + n]))
    blob, ck, noff, zo = pack_items(items)
    text = text.replace(md.group(0), carr("uint8_t", name + "_DATA", list(blob)))
    text = text.replace(mo.group(0), carr("uint16_t", name + "_OFF", noff) + carr("uint8_t", name + "_GC", ck) + carr("uint16_t", name + "_ZO", zo))
    f += [name + "_GC", name + "_ZO"]
    text = text.replace(ms.group(0), "static const felucca_font_t " + name + " = { " + ", ".join(f) + " };\n")
    print(f"pack_gfx: {name:8s} {len(data):6d} -> {len(blob):6d} B in {len(zo) - 1} chunks")
    return text


def pack_sprites(text):
    if "QSPR_CK" in text:
        return text
    md, data = take(text, "QSPR_DATA")
    ms = re.search(r"static const qspr_t QSPR\[QS_COUNT\] = \{(.*?)\};\n", text, re.S)
    rows = [ints(r) for r in re.findall(r"\{([^}]*)\}", ms.group(1))]
    items = []
    for ox, oy, w, h, off in rows:
        n = w * h
        items.append(bytes(data[off:off + (n + 1) // 2 + (n + 7) // 8]))
    blob, ck, noff, zo = pack_items(items)
    text = text.replace(md.group(0), carr("uint8_t", "QSPR_DATA", list(blob)))
    new = "".join("    {%d, %d, %d, %d, %d},\n" % (r[0], r[1], r[2], r[3], o) for r, o in zip(rows, noff))
    text = text.replace(ms.group(0), "static const qspr_t QSPR[QS_COUNT] = {\n" + new + "};\n" + carr("uint8_t", "QSPR_CK", ck) + carr("uint16_t", "QSPR_ZO", zo))
    print(f"pack_gfx: sprites  {len(data):6d} -> {len(blob):6d} B in {len(zo) - 1} chunks")
    return text


def main(gen):
    gen = Path(gen)
    p = gen / "felucca_qfont.h"
    t = p.read_text()
    for n in ("FONT_QT", "FONT_P"):
        t = pack_font(t, n)
    p.write_text(t)
    p = gen / "felucca_qsprites.h"
    if p.exists():                                                   # (winampFM: no Quest sprites)
        p.write_text(pack_sprites(p.read_text()))


if __name__ == "__main__":
    main(sys.argv[1])
