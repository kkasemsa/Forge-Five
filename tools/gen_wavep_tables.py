#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Khantipol Kasemsant
"""WAVE-P's wavetables (Forge Five Phase 6, eng_wavep.c): 8 tables of 64 waves, each wave one cycle of 128 points,
8 bits a point, kept in flash (64 KB). Every wave is made here from a formula: harmonic sums, pulse widths, sync
sweeps, formants, seeded random spectra. Nothing is taken from any instrument's ROM.

Each table's waves 0..59 are its sweep; waves 60..63 are the same four plain shapes in every table (triangle, square,
a 25 % pulse, saw), the "upper" waves the engine can reach from any table.

A wave holds harmonics 1..40 at most, the top ones faded (from 24 to 40) so a high note's fold-back stays soft (the
engine reads the points as they are in STEP, the stepped sound; a 128-point cycle above ~550 Hz folds its top
harmonics back: the grit is meant, the fade keeps it in hand). The engine moves from one wave to the next at the
cycle's start, so the waves of a sweep start where their neighbours do (most at zero: sine phase); each is scaled to a
peak of 0.92.

Pure Python, cached on this script's hash.
  tools/gen_wavep_tables.py build/gen/forge_wavep.h"""
import hashlib
import math
import random
import sys
from pathlib import Path

NP = 128            # points a wave
NW = 64             # waves a table (0..59 the sweep, 60..63 the upper four)
NSWEEP = 60
HMAX = 40           # harmonics at most
HFADE = 24          # full up to here, then a raised cosine to 0 past HMAX
TWO_PI = 2 * math.pi
SIN = [[math.sin(TWO_PI * h * n / NP) for n in range(NP)] for h in range(HMAX + 1)]
COS = [[math.cos(TWO_PI * h * n / NP) for n in range(NP)] for h in range(HMAX + 1)]


def fade(h):
    if h <= HFADE:
        return 1.0
    if h > HMAX:
        return 0.0
    return 0.5 + 0.5 * math.cos(math.pi * (h - HFADE) / (HMAX + 1 - HFADE))


def from_amps(amps, cos_terms=None):
    """amps[h] (h = 1..): sine terms; cos_terms[h]: cosine terms (optional) -> NP points"""
    w = [0.0] * NP
    for h in range(1, HMAX + 1):
        a = amps[h] if h < len(amps) else 0.0
        c = cos_terms[h] if cos_terms and h < len(cos_terms) else 0.0
        g = fade(h)
        if a:
            s = SIN[h]
            for n in range(NP):
                w[n] += a * g * s[n]
        if c:
            k = COS[h]
            for n in range(NP):
                w[n] += c * g * k[n]
    return w


def from_wave(f, ov=1024):
    """a waveform f(x), x in [0, 1) one cycle, measured with a DFT (ov points) -> its harmonics 1..HMAX"""
    pts = [f(i / ov) for i in range(ov)]
    sa, ca = [0.0] * (HMAX + 1), [0.0] * (HMAX + 1)
    for h in range(1, HMAX + 1):
        s = c = 0.0
        for i, x in enumerate(pts):
            ph = TWO_PI * h * i / ov
            s += x * math.sin(ph)
            c += x * math.cos(ph)
        sa[h], ca[h] = s * 2 / ov, c * 2 / ov
    return from_amps(sa, ca)


def saw_amps(n=HMAX):
    return [0.0] + [1.0 / h if h <= n else 0.0 for h in range(1, HMAX + 1)]


def upper():
    """the four plain shapes every table ends with: triangle, square, 25 % pulse, saw"""
    tri = [0.0] * (HMAX + 1)
    sq = [0.0] * (HMAX + 1)
    for h in range(1, HMAX + 1, 2):
        tri[h] = (1 if (h // 2) % 2 == 0 else -1) / (h * h)
        sq[h] = 1.0 / h
    return [from_amps(tri), from_amps(sq), pulse(0.25), from_amps(saw_amps())]


def pulse(d):
    """a pulse of width d (its rising edge at the cycle's start, less its mean)"""
    sa, ca = [0.0] * (HMAX + 1), [0.0] * (HMAX + 1)
    for h in range(1, HMAX + 1):
        # x(t) = 1 on [0, d): b_h = (1 - cos 2 pi h d) / (pi h), a_h = sin(2 pi h d) / (pi h)
        sa[h] = (1 - math.cos(TWO_PI * h * d)) / (math.pi * h)
        ca[h] = math.sin(TWO_PI * h * d) / (math.pi * h)
    return from_amps(sa, ca)


# ---- the tables ----------------------------------------------------------------------------------------------------
def t_build():
    """BUILD: harmonics added one at a time, a saw's: from a sine to the full saw, then odd ones thinned"""
    out = []
    for k in range(NSWEEP):
        if k < 40:
            out.append(from_amps(saw_amps(k + 1)))
        else:                                   # 40..59: the even harmonics fade out (saw to square-ish)
            e = 1 - (k - 39) / 20
            out.append(from_amps([0.0] + [(1.0 if h % 2 else e) / h for h in range(1, HMAX + 1)]))
    return out


def t_pulse():
    """PULSE: the width from 50 % down to 3 %, then the narrow pulse's harmonics combed (two pulses a cycle)"""
    out = []
    for k in range(NSWEEP):
        if k < 44:
            out.append(pulse(0.5 - 0.47 * k / 43))
        else:
            d = 0.04 + 0.2 * (k - 44) / 15
            out.append(from_wave(lambda x, d=d: (1.0 if x % 0.5 < d else 0.0)))
    return out


def formant(f1, f2, bw=1.6, tilt=0.6):
    amps = [0.0] * (HMAX + 1)
    for h in range(1, HMAX + 1):
        g = math.exp(-((h - f1) / bw) ** 2) + 0.7 * math.exp(-((h - f2) / (bw * 1.4)) ** 2)
        amps[h] = (0.25 / h ** tilt + g)
    return amps


def t_vocal():
    """VOCAL: two formant peaks moving through five vowel-like places (harmonic numbers, an A2-ish voice)"""
    places = [(3.0, 9.0), (2.0, 13.0), (4.0, 7.0), (2.5, 18.0), (5.0, 11.0), (3.0, 9.0)]
    out = []
    for k in range(NSWEEP):
        x = k / (NSWEEP - 1) * (len(places) - 1)
        i = min(int(x), len(places) - 2)
        f = x - i
        a, b = places[i], places[i + 1]
        out.append(from_amps(formant(a[0] + (b[0] - a[0]) * f, a[1] + (b[1] - a[1]) * f)))
    return out


def t_sync():
    """SYNC: a saw restarted each cycle (from its middle: the wave starts at zero), its own rate from 1 to 7 times
    the cycle's (the hard-sync sweep)"""
    return [from_wave(lambda x, r=1 + 6 * k / (NSWEEP - 1): 2 * ((x * r + 0.5) % 1.0) - 1) for k in range(NSWEEP)]


def t_glass():
    """GLASS: a soft fundamental and a bright cluster that climbs (harmonics 3 .. 30, its partner 1.5 x up)"""
    out = []
    for k in range(NSWEEP):
        c = 3 + 27 * k / (NSWEEP - 1)
        amps = [0.0] * (HMAX + 1)
        amps[1] = 0.6
        for h in range(2, HMAX + 1):
            amps[h] = math.exp(-((h - c) / 1.1) ** 2) + 0.6 * math.exp(-((h - 1.5 * c) / 1.4) ** 2) + 0.03 / h
        out.append(from_amps(amps))
    return out


def t_organ():
    """ORGAN: registrations of the footages (harmonics 1, 2, 3, 4, 6, 8, 10, 12, 16) from mellow to full, in steps"""
    feet = [1, 2, 3, 4, 6, 8, 10, 12, 16]
    rng = random.Random(1906)
    regs = []
    for k in range(NSWEEP):
        n = 1 + k * len(feet) // NSWEEP           # more stops pulled along the table
        lv = [0.0] * len(feet)
        lv[0] = 1.0
        for j in range(1, len(feet)):
            if j < n + 1:
                lv[j] = rng.choice([0.25, 0.5, 0.75, 1.0]) if j > 1 else 0.8
        regs.append(lv)
    out = []
    for lv in regs:
        amps = [0.0] * (HMAX + 1)
        for f, l in zip(feet, lv):
            amps[f] += l / (1 + 0.15 * f)
        out.append(from_amps(amps))
    return out


def t_fract():
    """FRACT: seeded random spectra (a tilt and a self-similar ripple), one every 12 waves, the waves between
    them in between: a sweep that keeps changing its mind"""
    rng = random.Random(27)
    keys = []
    for _ in range(6):
        amps = [0.0] * (HMAX + 1)
        cos_ = [0.0] * (HMAX + 1)
        r = rng.uniform(0.4, 1.4)
        for h in range(1, HMAX + 1):
            ripple = 0.5 + 0.5 * math.sin(r * math.log2(h) * 6.0 + rng.uniform(0, 1))
            m = rng.uniform(0.2, 1.0) * ripple / h ** rng.uniform(0.5, 1.1)
            p = rng.uniform(0, TWO_PI)
            amps[h], cos_[h] = m * math.cos(p), m * math.sin(p)
        keys.append((amps, cos_))
    out = []
    for k in range(NSWEEP):
        x = k / (NSWEEP - 1) * (len(keys) - 1)
        i = min(int(x), len(keys) - 2)
        f = x - i
        f = f * f * (3 - 2 * f)
        (a0, c0), (a1, c1) = keys[i], keys[i + 1]
        out.append(from_amps([a + (b - a) * f for a, b in zip(a0, a1)], [a + (b - a) * f for a, b in zip(c0, c1)]))
    return out


def t_reso():
    """RESO: a saw through a resonance that climbs from the 2nd harmonic to the 36th (a filter sweep frozen in waves)"""
    out = []
    for k in range(NSWEEP):
        c = 2 + 34 * (k / (NSWEEP - 1)) ** 1.3
        amps = [0.0] * (HMAX + 1)
        for h in range(1, HMAX + 1):
            lp = 1 / math.sqrt(1 + (h / c) ** 8)
            pk = 2.5 * math.exp(-((h - c) / 1.2) ** 2)
            amps[h] = (lp + pk) / h
        out.append(from_amps(amps))
    return out


TABLES = [("BUILD", t_build), ("PULSE", t_pulse), ("VOCAL", t_vocal), ("SYNC", t_sync),
          ("GLASS", t_glass), ("ORGAN", t_organ), ("FRACT", t_fract), ("RESO", t_reso)]


def q8(w):
    p = max(abs(x) for x in w) or 1.0
    g = 0.92 / p
    return [max(-127, min(127, round(x * g * 127))) for x in w]


def build():
    up = [q8(w) for w in upper()]
    lines = ["/* generated by tools/gen_wavep_tables.py: WAVE-P's wavetables (eng_wavep.c); made from formulas, no",
             " * instrument's ROM. Do not edit. */",
             "#pragma once", "#include <stdint.h>", "",
             f"#define WP_NT {len(TABLES)}", f"#define WP_NW {NW}", f"#define WP_NP {NP}", f"#define WP_NSWEEP {NSWEEP}",
             "/* [table][wave][point], 8 bits (the peak 0.92 of 127) */",
             "static const int8_t WP_TAB[WP_NT][WP_NW][WP_NP] = {"]
    for name, fn in TABLES:
        waves = [q8(w) for w in fn()]
        assert len(waves) == NSWEEP, name
        lines.append(f"    {{   /* {name} */")
        for w in waves + up:
            lines.append("        {" + ",".join(str(v) for v in w) + "},")
        lines.append("    },")
    lines.append("};")
    lines.append("static const char *const WP_NAMES[WP_NT] = {" + ", ".join(f'"{n}"' for n, _ in TABLES) + "};")
    return "\n".join(lines) + "\n"


def main(out):
    out = Path(out)
    key = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()[:16]
    cache = out.with_suffix(".key")
    if out.exists() and cache.exists() and cache.read_text().strip() == key:
        print(f"wavep: {out} (cached)")
        return
    text = build()
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(text)
    cache.write_text(key + "\n")
    print(f"wavep: {len(TABLES)} tables x {NW} waves x {NP} points -> {out}")


if __name__ == "__main__":
    main(sys.argv[1])
