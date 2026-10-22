#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Khantipol Kasemsant; the drawing kit (Pen, the splash encoding) is Clipper's tools/gen_logo.py
"""The Forge Five logo: five flames rising from a forge bed inside the 270-degree dial Clipper's and SLOOP's marks
share (the knobs of the UI), coloured from the blue of a cold flame to the orange of the hot one, and a geometric
monoline wordmark "forge five" in the same lettering as Clipper's (round bowls of the x-height, round-ended strokes).

  tools/gen_ff_logo.py OUT.h          firmware boot splash (RLE, 16-colour palette, RGB565; the names stay SLOOP_*)
  tools/gen_ff_logo.py --assets DIR   forge-five-logo.svg/png, forge-five-icon.svg/png, forge-five-splash.png
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_logo as g                                  # the Pen, curve(), the palette and the splash encoding
from gen_logo import Pen, curve, WHITE, BLACK, BLUE, GREEN, YELLOW, ORANGE, HEX, XH, SW, R, GAP
from PIL import Image, ImageDraw

RED = (255, 60, 40)
FLAMES = (BLUE, GREEN, YELLOW, ORANGE, RED)           # the five, cold to hot


def icon(pen, ox, oy):
    cx, cy = ox + 120, oy + 120
    pen.arc(cx, cy, 100, 135, 405)                    # the dial (SLOOP's, Clipper's)
    bed = cy + 56
    pen.line(cx - 64, bed, cx + 64, bed, w=9)         # the forge bed
    pen.line(cx - 40, bed + 22, cx + 40, bed + 22, w=7)   # its foot
    tops = (-26, -58, -82, -58, -26)                  # the flames: the tallest in the middle
    for k, (dx, top, col) in enumerate(zip((-54, -27, 0, 27, 54), tops, FLAMES)):
        lean = 4 + k * 3                              # all lean a little to the right, the hot ones more (the draught)
        foot_l, foot_r, tip = (cx + dx - 13, bed - 4), (cx + dx + 13, bed - 4), (cx + dx + lean, cy + top)
        pen.poly([foot_l] + curve(foot_l, tip, -9)[1:-1] + [tip] + curve(tip, foot_r, -13)[1:-1] + [foot_r], col)


# ---- the wordmark "forge five" -----------------------------------------------------------------
def word_width():
    return word(Pen(), 0, 0)


def _f(pen, x, oy, base, w, mid):                     # f: a stem from the ascender, a hook to the right, a bar
    rh, top = R * 0.62, oy - 40
    pen.line(x + w / 2, top + rh + w / 2, x + w / 2, base - w / 2)
    pen.arc(x + w / 2 + rh, top + rh + w / 2, rh, 180, 275)
    pen.line(x - 4, oy + w / 2, x + w + 14, oy + w / 2)
    return w + 18


def word(pen, ox, oy):
    x, mid, base, w = ox, oy + XH / 2, oy + XH, SW
    x += _f(pen, x, oy, base, w, mid) + GAP            # f
    pen.arc(x + XH / 2, mid, R, 0, 360, caps=False)    # o
    x += XH + GAP
    pen.line(x + w / 2, oy + w / 2, x + w / 2, base - w / 2)    # r
    pen.arc(x + w / 2 + R, mid, R, 180, 290)
    x += w / 2 + R * 1.55 + GAP
    pen.arc(x + XH / 2, mid, R, 0, 360, caps=False)    # g: the bowl, a stem down the right, a hook at the bottom
    pen.line(x + XH - w / 2, mid, x + XH - w / 2, base + 24)
    pen.arc(x + XH / 2, base + 24, R, 0, 150)
    x += XH + GAP
    pen.arc(x + XH / 2, mid, R, 40, 360)               # e
    pen.line(x + w / 2, mid, x + XH - w / 2, mid)
    x += XH + GAP * 2.6                                # the word space
    x += _f(pen, x, oy, base, w, mid) + GAP            # f
    pen.line(x + w / 2, oy + w / 2, x + w / 2, base - w / 2)    # i
    pen.dot(x + w / 2, oy - 22, w * 1.1, WHITE) if pen.d else pen.svg.append(
        f'<circle cx="{x + w / 2:.2f}" cy="{oy - 22:.2f}" r="{w * 0.55:.2f}" fill="{HEX(WHITE)}"/>')
    x += w + GAP
    pen.line(x + w / 2, oy + w / 2, x + XH / 2, base - w / 2)   # v
    pen.line(x + XH / 2, base - w / 2, x + XH - w / 2, oy + w / 2)
    x += XH + GAP
    pen.arc(x + XH / 2, mid, R, 40, 360)               # e
    pen.line(x + w / 2, mid, x + XH - w / 2, mid)
    return x + XH - ox


def render(kind, W, H, k, ss=4):
    im = Image.new("RGB", (W * ss, H * ss), BLACK)
    d = ImageDraw.Draw(im)
    K = k * ss
    if kind == "h":
        icon(Pen(d, K, 10 * K, (H * ss - 240 * K) / 2), 0, 0)
        word(Pen(d, K, 270 * K, H * ss / 2 - 36 * K), 0, 0)
    elif kind == "v":
        icon(Pen(d, K, (W * ss - 240 * K) / 2, 0), 0, 0)
        word(Pen(d, K, (W * ss - word_width() * K) / 2, 270 * K), 0, 0)
    else:
        icon(Pen(d, K, (W * ss - 240 * K) / 2, (H * ss - 240 * K) / 2), 0, 0)
    return im.resize((W, H), Image.LANCZOS)


def svg(kind):
    pen = Pen()
    if kind == "h":
        W, H = 270 + word_width() + 20, 260
        icon(pen, 10, 10)
        sub = Pen()
        word(sub, 270, H / 2 - 36)
        pen.svg += sub.svg
    else:
        W, H = 240, 240
        icon(pen, 0, 0)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W:.0f} {H}" width="{W:.0f}" height="{H}" role="img" '
            f'aria-label="Forge Five">\n<rect width="{W:.0f}" height="{H}" fill="#000"/>\n' + "\n".join(pen.svg) + "\n</svg>\n")


SPLASH_W, SPLASH_H = g.SPLASH_W, g.SPLASH_H
SPLASH_K = min(0.40, (SPLASH_W - 4) / word_width())   # (the wordmark is longer than "clipper": smaller)
assert (270 + 72 + 24 + 6) * SPLASH_K <= SPLASH_H and 40 * SPLASH_K < 270 * SPLASH_K, "the wordmark's g must fit the splash"
assert word_width() * SPLASH_K <= SPLASH_W - 4, "the wordmark must fit the splash"


def splash_header(path):
    im = render("v", SPLASH_W, SPLASH_H, SPLASH_K)
    cols = [BLACK]                                    # black, and each colour at 1/3, 2/3 and full (anti-aliased edges)
    for c in (WHITE, BLUE, GREEN, YELLOW, ORANGE):
        cols += [tuple(round(v * f) for v in c) for f in (1 / 3, 2 / 3, 1.0)]
    # (16 entries: RED, the fifth flame, is quantised onto ORANGE's and WHITE's steps)
    src = im.tobytes()
    px, cache = [], {}
    for i in range(0, len(src), 3):
        p = (src[i], src[i + 1], src[i + 2])
        if p not in cache:
            cache[p] = min(range(16), key=lambda k: sum((p[j] - cols[k][j]) ** 2 for j in range(3)))
        px.append(cache[p])
    rle = bytearray()
    i = 0
    while i < len(px):
        j = i
        while j < len(px) and px[j] == px[i] and j - i < 16:
            j += 1
        rle.append(((j - i - 1) << 4) | px[i])
        i = j
    L = ["/* generated by tools/gen_ff_logo.py: the Forge Five boot splash */", "#pragma once", "#include <stdint.h>",
         f"#define SLOOP_SPLASH_W {SPLASH_W}", f"#define SLOOP_SPLASH_H {SPLASH_H}",
         "static const uint16_t SLOOP_SPLASH_PAL[16] = {" + ", ".join(f"0x{g.rgb565(c):04X}" for c in cols) + "};",
         f"static const uint8_t SLOOP_SPLASH_RLE[{len(rle)}] = {{"]
    for k in range(0, len(rle), 24):
        L.append("    " + ", ".join(str(b) for b in rle[k:k + 24]) + ",")
    L.append("};")
    Path(path).write_text("\n".join(L) + "\n")
    print(f"logo: Forge Five splash {SPLASH_W}x{SPLASH_H}, {len(rle)} B RLE -> {path}")


def assets(out):
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    (out / "forge-five-logo.svg").write_text(svg("h"))
    (out / "forge-five-icon.svg").write_text(svg("i"))
    W = int(270 + word_width() + 20)
    render("h", W * 2, 520, 2.0).save(out / "forge-five-logo.png")
    render("i", 512, 512, 512 / 240).save(out / "forge-five-icon.png")
    render("v", SPLASH_W, SPLASH_H, SPLASH_K).resize((SPLASH_W * 2, SPLASH_H * 2), Image.NEAREST).save(out / "forge-five-splash.png")
    print(f"logo: assets in {out}")


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--assets":
        assets(sys.argv[2])
    elif len(sys.argv) == 2:
        splash_header(sys.argv[1])
    else:
        sys.exit(__doc__)
