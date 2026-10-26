/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Khantipol Kasemsant; built on Clipper (isod89 / 3dSam), SLOOP and Felucca */
/* WAVE-P engine test (firmware/src/eng_wavep.c) on the host, through hostsim.c.
 *   build/host/wavep_test [DEMODIR]           (DEMODIR: a WAV per preset and a tour of them all)
 * 1. golden (first, from the power-on state): every preset's first half second of a note, a STEP and a SMOOTH sweep,
 *    hash as tests/wavep_golden.txt says (GOLDEN_UPDATE=1 rewrites it).
 * 2. the tables: 8 x 64 x 128 points, 8 bits; BUILD's wave 0 a sine, wave 39 a saw; the upper waves the same four in
 *    every table (61: a square, no even harmonics).
 * 3. pitch: OSC 1 at A4 440 Hz (+-0.3 %); OSC 2 at SEMI +12 an octave up; DETUNE 50 its cents; SUB an octave under.
 * 4. STEP: an oscillator moves to another wave only where its cycle starts; SMOOTH: fewer images than STEP high up.
 * 5. the position: ENV>W both ways, the LFO through SHP, W KBD, VEL>W; held in the sweep (0..59) unless UPPER.
 * 6. BITS: the tables' 8 bits keep the floor between a sine's harmonics down; 6, 5, 4 raise it a step each.
 * 7. the filter: 24 dB an octave (a saw's harmonics two octaves over the cutoff); RESO a peak; DRIVE more harmonics.
 * 8. RELEASE (EDIT 2) and the ENV page's R are one value.
 * 9. the engine patch: 16 values on 4 pages, a preset's round-trip.
 * 10. levels: every preset's note within 4 dB of the DCO pad's in the mix; corners: every preset at five notes with
 *    everything up (BITS 4, DRIVE, RESO, NOISE, SUB, UPPER, ENV>W at both ends): within 2 x VOICE_FS, silent once
 *    released.
 * 11. cost: a voice against BRASS's on the host, STEP, SMOOTH, and with SUB, NOISE, DRIVE on (the weight, 1.0, is the
 *    FM-1's measure: 4.5 % a voice).
 * 12. demos (DEMODIR). */
#include <ctype.h>
#define main hostsim_main
#include "hostsim.c"
#undef main

#define NFFT 16384
#define WAVEP_COST_MAX 5.0              /* x Clipper's ANALOG on the host: a sanity bound */

static int bad;
static void check(const char *what, int ok)
{
    printf("wavep: %-93s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

/* track 0 WAVE-P preset pi; pi < 0: a plain patch (BUILD's wave 0, a sine, OSC 1 alone, the filter open, flat
 * envelopes, STEP, BITS 8) */
static void setup(int32_t pi)
{
    uint32_t i;
    memset(trk, 0, sizeof trk);
    memset(eng_arena, 0, sizeof eng_arena);
    memset(eng_arena_own, 0, sizeof eng_arena_own);
    memset(wp_mir, 0, sizeof wp_mir);
    for (i = 0; i < NPART; i++)
        cm_reset(i);
    host_tracks_init();
    host_preset(&trk[0], ENGI_WAVEP, pi < 0 ? 0u : (uint32_t)pi);
    if (pi < 0) {
        uint32_t k;
        xp_defaults(&trk[0]);
        for (k = 0; k < 8u; k++)
            trk[0].p[P_E0 + k] = ENG_WAVEP.edit[k].def;
        trk[0].p[P_E0] = 0;             /* WAVE */
        trk[0].p[P_E1] = 127;           /* CUTOFF */
        trk[0].p[P_E2] = 0;             /* ENV>W */
        trk[0].p[P_E3] = 0;             /* RESO */
        trk[0].p[P_E4] = WT_BUILD;      /* TABLE */
        trk[0].p[P_E5] = 0;             /* DETUNE */
        trk[0].p[P_E6] = 0;             /* BITS 8 */
        trk[0].p[P_E7] = trk[0].p[P_REL] = 10;
        trk[0].xp[WX_LEVEL] = 0;
        trk[0].xp[WX_KBD] = 0;
        trk[0].p[P_ATK] = 0, trk[0].p[P_SUS] = 127;
        trk[0].p[P_GLIDE] = 0;
        trk[0].p[P_VINT] = 0;
        trk[0].p[P_LD_PIT] = trk[0].p[P_LD_FLT] = trk[0].p[P_LD_SHP] = 0;
        trk[0].p[P_ED_FLT] = trk[0].p[P_ED_PIT] = trk[0].p[P_ED_SHP] = 0;
        trk[0].p[P_VOICE] = V_POLY;
    }
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = trk[0].p[P_DIST] = 0;
}

static void blocks_of(double *y, uint32_t n)          /* the part's next n samples into y (0: rendered, dropped) */
{
    static int32_t b[CTL];
    uint32_t i, k;
    for (i = 0; i < n; i += CTL) {
        memset(b, 0, sizeof b);
        track_render(&trk[0], b, CTL);
        if (y)
            for (k = 0; k < CTL && i + k < n; k++)
                y[i + k] = b[k] / 32768.0;
    }
}
static void voice_render(uint32_t note, double skip_s, double *y, uint32_t n)
{
    trk_note_on(&trk[0], note, 100);
    blocks_of(0, (uint32_t)(skip_s * FS));
    blocks_of(y, n);
}

#include "spectra.h"

static double centroid(const double *ps, double f0, double f1)
{
    uint32_t a = (uint32_t)(f0 * NFFT / FS), b = (uint32_t)(f1 * NFFT / FS), i;
    double s = 0, w = 0;
    for (i = a; i <= b; i++)
        s += ps[i] * i, w += ps[i];
    return w > 0 ? s / w * FS / NFFT : 0;
}
/* the power off the harmonics of f0 (more than 12 Hz from each) against the power on them, dB */
static double inharm_db(const double *ps, double f0)
{
    double on = 0, off = 0;
    uint32_t i;
    for (i = (uint32_t)(40.0 * NFFT / FS); i < NFFT / 2; i++) {
        double f = (double)i * FS / NFFT, h = f / f0, d = fabs(h - floor(h + 0.5)) * f0;
        if (d < 12)
            on += ps[i];
        else
            off += ps[i];
    }
    return 10 * log10((off + 1e-30) / (on + 1e-30));
}
static double harm_db(const double *ps, double f0, uint32_t h) { return band_db(ps, h * f0 - 6, h * f0 + 6); }

/* ---------------------------------------------------------------- golden --- */
#define NGOLD (32u + 2u)
static const char *const GX[2] = {"STEP SWEEP", "SMOOTH SWEEP"};
static void t_golden(void)
{
    static int32_t b[CTL];
    static uint64_t h[NGOLD];
    static const char *name[NGOLD];
    char line[160];
    const char *path = "tests/wavep_golden.txt";
    uint32_t pi, i, k, n = 0, changed = 0, missing = 0;
    FILE *f;
    for (pi = 0; pi < ENG_WAVEP.npresets + 2u && n < NGOLD; pi++, n++) {
        uint64_t x = 1469598103934665603ull;
        uint32_t extra = pi >= ENG_WAVEP.npresets ? pi - ENG_WAVEP.npresets + 1u : 0u;
        setup(extra ? 1 : (int32_t)pi);                 /* (the extras: GLASS SWEEP, a fast climb, STEP and SMOOTH) */
        if (extra) {
            trk[0].xp[WX_MODE] = (int16_t)(extra - 1u);
            trk[0].xp[WX_WATK] = 40;
            trk[0].p[P_E2] = 63;
        }
        trk_note_on(&trk[0], extra ? 57u : 48 + (pi % 24u), 100);
        for (i = 0; i < FS / 2 / CTL; i++) {
            memset(b, 0, sizeof b);
            track_render(&trk[0], b, CTL);
            for (k = 0; k < CTL; k++)
                x = (x ^ (uint32_t)b[k]) * 1099511628211ull;
        }
        h[n] = x, name[n] = extra ? GX[extra - 1u] : ENG_WAVEP.presets[pi].name;
    }
    if (getenv("GOLDEN_UPDATE")) {
        f = fopen(path, "w");
        if (f) {
            fprintf(f, "# WAVE-P: FNV-1a of half a second of a note: each preset, a STEP and a SMOOTH sweep (tests/wavep_test.c)\n");
            for (i = 0; i < n; i++)
                fprintf(f, "%s|%016llx\n", name[i], (unsigned long long)h[i]);
            fclose(f);
        }
        check("golden: tests/wavep_golden.txt rewritten", f != 0);
        return;
    }
    f = fopen(path, "r");
    if (!f) {
        check("golden: tests/wavep_golden.txt present (GOLDEN_UPDATE=1 writes it)", 0);
        return;
    }
    for (i = 0; i < n; i++) {
        unsigned long long want = 0;
        int found = 0;
        size_t ln = strlen(name[i]);
        rewind(f);
        while (fgets(line, sizeof line, f))
            if (!strncmp(line, name[i], ln) && line[ln] == '|' && sscanf(line + ln + 1, "%llx", &want) == 1) {
                found = 1;
                break;
            }
        if (!found) {
            printf("wavep: golden: %s has no entry\n", name[i]);
            missing++;
        } else if (want != h[i]) {
            printf("wavep: golden: %s changed (%016llx, was %016llx)\n", name[i], (unsigned long long)h[i], want);
            changed++;
        }
    }
    fclose(f);
    snprintf(line, sizeof line, "golden: %u presets and two sweeps render as tests/wavep_golden.txt says "
             "(%u changed, %u missing)", ENG_WAVEP.npresets, changed, missing);
    check(line, !changed && !missing);
}

/* ---------------------------------------------------------------- tables --- */
static void t_tables(void)
{
    static double ps[NFFT / 2];
    char m[200];
    double h1, h2, s1, s2, q2, q3;
    uint32_t t, w, k, same = 1;
    check("tables: 8 tables x 64 waves x 128 points, 8 bits (64 KB)",
          WP_NT == 8 && WP_NW == 64 && WP_NP == 128 && sizeof WP_TAB == 65536u);
    for (t = 1; t < WP_NT; t++)
        for (w = WP_NSWEEP; w < WP_NW; w++)
            for (k = 0; k < WP_NP; k++)
                same &= WP_TAB[t][w][k] == WP_TAB[0][w][k];
    check("tables: the upper waves (60..63) the same four in every table", same);
    setup(-1);
    voice_render(57, 0.1, buf, 2 * NFFT);
    spectrum(buf, 2, ps);
    h1 = harm_db(ps, 220, 1), h2 = harm_db(ps, 220, 2);
    setup(-1);
    trk[0].p[P_E0] = 39;
    voice_render(57, 0.1, buf, 2 * NFFT);
    spectrum(buf, 2, ps);
    s1 = harm_db(ps, 220, 1), s2 = harm_db(ps, 220, 2);
    snprintf(m, sizeof m, "tables: BUILD's wave 0 a sine (2nd harmonic %.1f dB under), wave 39 a saw's (2nd %.1f dB under)",
             h1 - h2, s1 - s2);
    check(m, h1 - h2 > 40 && fabs(s1 - s2 - 6.0) < 1.5);
    setup(-1);
    trk[0].p[P_E4] = WT_VOCAL;
    trk[0].p[P_E0] = 61;
    voice_render(57, 0.1, buf, 2 * NFFT);
    spectrum(buf, 2, ps);
    q2 = harm_db(ps, 220, 2), q3 = harm_db(ps, 220, 3);
    snprintf(m, sizeof m, "tables: VOCAL's wave 61 a square (2nd harmonic %.1f dB under the 3rd)", q3 - q2);
    check(m, q3 - q2 > 30);
}

/* ----------------------------------------------------------------- pitch --- */
static void t_pitch(void)
{
    static double ps[NFFT / 2];
    char m[160];
    double f, want, a, b;
    setup(-1);
    voice_render(69, 0.1, buf, 2 * NFFT);
    f = peak_hz(buf, 30);
    snprintf(m, sizeof m, "pitch: OSC 1 at A4: %.2f Hz (440)", f);
    check(m, fabs(f / 440 - 1) < 0.003);
    setup(-1);
    trk[0].xp[WX_LEVEL] = 127;
    trk[0].xp[WX_SEMI] = 12;
    trk[0].p[P_E5] = 50;
    voice_render(57, 0.1, buf, 2 * NFFT);
    f = peak_hz(buf, 300), want = 440 * pow(2, 50 / 1200.0);
    snprintf(m, sizeof m, "pitch: OSC 2 at SEMI +12, DETUNE 50 over A3: %.2f Hz (%.2f)", f, want);
    check(m, fabs(f / want - 1) < 0.003);
    setup(-1);
    trk[0].xp[WX_SUB] = 127;
    voice_render(69, 0.1, buf, 2 * NFFT);
    spectrum(buf, 2, ps);
    a = band_db(ps, 215, 225), b = band_db(ps, 300, 400);
    snprintf(m, sizeof m, "pitch: SUB a square an octave under A4 (220 Hz %.1f dB over the floor)", a - b);
    check(m, a - b > 30);
}

/* --------------------------------------------------------- STEP, SMOOTH --- */
static void t_step(void)
{
    static double ps[NFFT / 2];
    char m[220];
    uint32_t i, moves = 0, at_start = 0;
    double st, sm;
    /* STEP: a climb (W ATK 96, ~1 s; ENV>W +63) through BUILD at A3; every block the wave OSC 1 plays: when it
     * moved, its phase wrapped in that block (a new cycle began) */
    setup(-1);
    trk[0].p[P_E2] = 63;
    trk[0].xp[WX_WATK] = 96;
    trk[0].xp[WX_WSUS] = 127;
    trk_note_on(&trk[0], 57, 100);
    blocks_of(0, CTL);
    for (i = 0; i < FS / CTL; i++) {
        voice_t *v = &trk[0].v[0];
        int32_t w0 = v->s[4];
        uint32_t p0 = v->ph[0];
        blocks_of(0, CTL);
        if (v->s[4] != w0) {
            moves++;
            at_start += v->ph[0] < p0;                  /* (a block is far shorter than a cycle: a wrap is a smaller phase) */
        }
    }
    snprintf(m, sizeof m, "STEP: the climb moves OSC 1 to another wave %u times, each in a block where a cycle began (%u)",
             moves, at_start);
    check(m, moves > 20 && at_start == moves);
    {   /* a key struck again while its voice still sounds, with WAVE moved: rendered one sample at a time from there,
         * the voice keeps its wave until the very sample its phase wraps (a cycle starts), then takes the new one */
        uint32_t k, n, kept = 0, at_wrap = 0, vi = NVOICE;
        static int32_t one[1];
        setup(-1);
        trk[0].p[P_E0] = 10;
        trk_note_on(&trk[0], 45, 100);
        blocks_of(0, FS / 4);
        for (k = 0; k < NVOICE; k++)
            if (trk[0].v[k].active)
                vi = k;
        for (k = 0; k < 40u && vi < NVOICE; k++) {        /* struck again 40 times, at different points of the cycle */
            voice_t *v = &trk[0].v[vi];
            int32_t w0, ok_until = 1;
            blocks_of(0, CTL * (20u + k % 5u));           /* (a cycle or more: the wave settled) */
            w0 = v->s[4];
            trk[0].p[P_E0] = (int16_t)(k & 1u ? 10 : 40); /* WAVE moved and the key struck at once: a wave to go to */
            trk_note_on(&trk[0], 45, 100);
            for (n = 0; n < 2u * FS / 110u && v->s[4] == w0; n++) {   /* (two cycles of A2 at most) */
                uint32_t p0 = v->ph[0];
                one[0] = 0;
                track_render(&trk[0], one, 1);
                if (v->s[4] != w0)
                    ok_until = v->ph[0] < p0;             /* the sample it changed in: the one its phase wrapped in */
            }
            kept += v->s[4] != w0;
            at_wrap += v->s[4] != w0 && ok_until;
        }
        snprintf(m, sizeof m, "STEP: a key struck again under its own note, WAVE moved: the new wave in %u of 40, each at the "
                 "sample its cycle began (%u)", kept, at_wrap);
        check(m, vi < NVOICE && kept == 40u && at_wrap == 40u);
    }
    /* SMOOTH against STEP high up: the images of a 128-point cycle read point by point */
    setup(-1);
    trk[0].p[P_E0] = 39;
    voice_render(81, 0.1, buf, 2 * NFFT);
    spectrum(buf, 2, ps);
    st = inharm_db(ps, 880);
    setup(-1);
    trk[0].p[P_E0] = 39;
    trk[0].xp[WX_MODE] = WM_SMOOTH;
    voice_render(81, 0.1, buf, 2 * NFFT);
    spectrum(buf, 2, ps);
    sm = inharm_db(ps, 880);
    snprintf(m, sizeof m, "SMOOTH: a saw wave at A5 off its harmonics %.1f dB (STEP %.1f dB: the stepped grit)", sm, st);
    check(m, sm < st - 6 && sm < -30);
}

/* -------------------------------------------------------------- position --- */
static int32_t pos_after(double s)                       /* OSC 1's position after s seconds of A3 (Q8 waves) */
{
    trk_note_on(&trk[0], 57, 100);
    blocks_of(0, (uint32_t)(s * FS));
    return trk[0].v[0].s[6];
}
static void t_position(void)
{
    char m[260];
    int32_t a, b, c, d, e, f, g, h;
    setup(-1);
    trk[0].p[P_E0] = 30;
    trk[0].p[P_E2] = 63;
    trk[0].xp[WX_WSUS] = 127;
    a = pos_after(0.3);
    setup(-1);
    trk[0].p[P_E0] = 30;
    trk[0].p[P_E2] = -32;
    trk[0].xp[WX_WSUS] = 127;
    b = pos_after(0.3);
    setup(-1);
    trk[0].p[P_E0] = 30;
    trk[0].p[P_LD_SHP] = 63;
    trk[0].p[P_LRATE] = 40;
    {
        int32_t lo = 1 << 30, hi = -1;
        uint32_t i;
        trk_note_on(&trk[0], 57, 100);
        for (i = 0; i < 3u * FS / CTL; i++) {
            blocks_of(0, CTL);
            lo = trk[0].v[0].s[6] < lo ? trk[0].v[0].s[6] : lo;
            hi = trk[0].v[0].s[6] > hi ? trk[0].v[0].s[6] : hi;
        }
        c = lo, d = hi;
    }
    setup(-1);
    trk[0].p[P_E0] = 30;
    trk[0].xp[WX_WKBD] = 63;
    trk_note_on(&trk[0], 72, 100);
    blocks_of(0, CTL * 2);
    e = trk[0].v[0].s[6];
    setup(-1);
    trk[0].p[P_E0] = 30;
    trk[0].xp[WX_VELW] = 63;
    trk_note_on(&trk[0], 57, 127);
    blocks_of(0, CTL * 2);
    f = trk[0].v[0].s[6];
    setup(-1);
    trk[0].p[P_E0] = 50;
    trk[0].p[P_E2] = 63;
    trk[0].xp[WX_WSUS] = 127;
    g = pos_after(0.3);
    setup(-1);
    trk[0].p[P_E0] = 50;
    trk[0].p[P_E2] = 63;
    trk[0].xp[WX_WSUS] = 127;
    trk[0].xp[WX_UPPER] = 1;
    h = pos_after(0.3);
    snprintf(m, sizeof m, "position from WAVE 30: ENV>W +63 %.1f, -32 %.1f; LFO %.1f..%.1f; W KBD C5 %.1f; VEL>W %.1f; "
             "WAVE 50 +63: %.1f, UPPER %.1f", a / 256.0, b / 256.0, c / 256.0, d / 256.0, e / 256.0, f / 256.0,
             g / 256.0, h / 256.0);
    check(m, a == 59 << 8 && b < 16 << 8 && c < 10 << 8 && d > 50 << 8 && e > 40 << 8 && f > 55 << 8 &&
             g == 59 << 8 && h == 63 << 8);
}

/* ------------------------------------------------------------------ BITS --- */
static void t_bits(void)
{
    static double ps[NFFT / 2];
    char m[200];
    double q[4];
    uint32_t k;
    for (k = 0; k < 4u; k++) {
        setup(-1);
        trk[0].xp[WX_MODE] = WM_SMOOTH;
        trk[0].p[P_E6] = (int16_t)k;
        voice_render(57, 0.1, buf, 2 * NFFT);
        spectrum(buf, 2, ps);
        q[k] = inharm_db(ps, 220);
    }
    snprintf(m, sizeof m, "BITS: a sine's floor off its harmonics (SMOOTH): 8 %.1f dB, 6 %.1f, 5 %.1f, 4 %.1f", q[0], q[1], q[2], q[3]);
    check(m, q[0] < -45 && q[1] > q[0] + 3 && q[2] > q[1] && q[3] > q[2] && q[3] > q[0] + 10);
}

/* ---------------------------------------------------------------- filter --- */
static void t_filter(void)
{
    static double ps[NFFT / 2];
    char m[240];
    double s5, s10, s20, o10, o20, r0, r1, d0, d1;
    uint32_t k;
    setup(-1);
    trk[0].p[P_E0] = 39;
    voice_render(45, 0.1, buf, 2 * NFFT);
    spectrum(buf, 2, ps);
    o10 = harm_db(ps, 110, 10), o20 = harm_db(ps, 110, 20);
    setup(-1);
    trk[0].p[P_E0] = 39;
    trk[0].p[P_E1] = 60;
    voice_render(45, 0.1, buf, 2 * NFFT);
    spectrum(buf, 2, ps);
    s5 = harm_db(ps, 110, 5), s10 = harm_db(ps, 110, 10), s20 = harm_db(ps, 110, 20);
    snprintf(m, sizeof m, "filter: CUTOFF 60 on a saw at A2: harmonic 10 -> 20 %.1f dB down (open %.1f): 24 dB an octave",
             s10 - s20, o10 - o20);
    check(m, (s10 - s20) - (o10 - o20) > 18);
    for (k = 0; k < 2u; k++) {
        setup(-1);
        trk[0].p[P_E0] = 39;
        trk[0].p[P_E1] = 60;
        trk[0].p[P_E3] = k ? 120 : 0;
        voice_render(45, 0.1, buf, 2 * NFFT);
        spectrum(buf, 2, ps);
        if (k)
            r1 = centroid(ps, 50, 8000);
        else
            r0 = centroid(ps, 50, 8000);
    }
    for (k = 0; k < 2u; k++) {
        setup(-1);
        trk[0].xp[WX_DRIVE] = k ? 127 : 0;
        trk[0].p[P_E0] = 0;
        voice_render(45, 0.1, buf, 2 * NFFT);
        spectrum(buf, 2, ps);
        if (k)
            d1 = harm_db(ps, 110, 3) - harm_db(ps, 110, 1);
        else
            d0 = harm_db(ps, 110, 3) - harm_db(ps, 110, 1);
    }
    (void)s5;
    snprintf(m, sizeof m, "filter: RESO 120 a peak (centre %.0f -> %.0f Hz); DRIVE 127 a sine's 3rd harmonic %.1f -> %.1f dB",
             r0, r1, d0, d1);
    check(m, r1 > r0 * 1.15 && d1 > d0 + 20);
}

/* -------------------------------------------------------------- RELEASE --- */
static void t_mirror(void)
{
    static int32_t o[CTL * 2];
    setup(0);
    mix_block(o, CTL);
    check("RELEASE: a preset sets EDIT 2's and the ENV page's alike", trk[0].p[P_E7] == trk[0].p[P_REL]);
    trk[0].p[P_E7] = 90;
    mix_block(o, CTL);
    check("RELEASE: EDIT 2 turned: R follows", trk[0].p[P_REL] == 90);
    trk[0].p[P_REL] = 20;
    mix_block(o, CTL);
    check("RELEASE: R turned: EDIT 2 follows", trk[0].p[P_E7] == 20);
}

/* --------------------------------------------------------- engine patch --- */
static void t_patch(void)
{
    static uint8_t b[XP_BYTES];
    int16_t keep[NXP];
    uint32_t k, same = 1;
    setup(6);
    memcpy(keep, trk[0].xp, sizeof keep);
    check("engine patch: WAVE-P has 16 values on 4 pages", WAVEP_XPI.n == 16u && xp_title(&trk[0], 3) && !xp_title(&trk[0], 4));
    xp_pack(&trk[0], b);
    xp_defaults(&trk[0]);
    xp_unpack(&trk[0], b);
    for (k = 0; k < NXP; k++)
        same &= trk[0].xp[k] == keep[k];
    check("engine patch: a preset's values round-trip (SEMI and OFFS signed)", same);
}

/* ------------------------------------------------------- levels, corners --- */
static double part_loudest(void)
{
    static double y[FS / 20];
    double best = 0, r;
    uint32_t w;
    for (w = 0; w < 30u; w++) {
        blocks_of(y, FS / 20);
        r = rms(y, FS / 20);
        best = r > best ? r : best;
    }
    return best;
}
static void t_levels(void)
{
    double ref, r;
    uint32_t pi, fails = 0;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    host_preset(&trk[0], ENGI_JUNO, 0);                 /* the DCO engine's pad, one note */
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    trk_note_on(&trk[0], 60, 100);
    ref = part_loudest();
    for (pi = 0; pi < ENG_WAVEP.npresets; pi++) {
        double db;
        uint32_t key;
        setup((int32_t)pi);
        key = trk[0].p[P_VOICE] == V_POLY ? 60u : 64u;
        trk_note_on(&trk[0], key, 100);
        r = part_loudest();
        db = 20 * log10(r / ref) + trk[0].p[P_ED_FX] * 0.5;
        printf("wavep:   %-14s %+.1f dB in the mix against one pad note (trim %+.1f dB)\n", ENG_WAVEP.presets[pi].name, db,
               trk[0].p[P_ED_FX] * 0.5);
        fails += db < -4 || db > 4;
    }
    check("levels: every preset's note, its loudest 50 ms in the mix, within 4 dB of a pad note", !fails);
}
static void t_corners(void)
{
    static const uint8_t NOTES[] = {24, 48, 72, 96, 115};
    double worst = 0;
    uint32_t pi, nt, r, k, fails = 0;
    char m[200];
    for (pi = 0; pi < ENG_WAVEP.npresets; pi++)
        for (nt = 0; nt < NELEM(NOTES); nt++)
            for (r = 0; r < 2u; r++) {
                static int32_t b[CTL];
                double pk = 0;
                uint32_t i, act = 1;
                setup((int32_t)pi);
                trk[0].p[P_E1] = 127, trk[0].p[P_E3] = 127, trk[0].p[P_E2] = r ? 63 : -64, trk[0].p[P_E6] = 3;
                trk[0].p[P_E0] = r ? 63 : 0;
                trk[0].xp[WX_SUB] = trk[0].xp[WX_NOISE] = trk[0].xp[WX_DRIVE] = trk[0].xp[WX_LEVEL] = 127;
                trk[0].xp[WX_UPPER] = 1, trk[0].xp[WX_OFFS] = r ? 31 : -32, trk[0].xp[WX_VELF] = 63;
                trk[0].xp[WX_WKBD] = r ? 63 : -64, trk[0].xp[WX_MODE] = (int16_t)r;
                trk_note_on(&trk[0], NOTES[nt], 127);
                for (i = 0; i < FS / 4 / CTL; i++) {
                    memset(b, 0, sizeof b);
                    track_render(&trk[0], b, CTL);
                    for (k = 0; k < CTL; k++) {
                        double x = fabs(b[k] / 32768.0);
                        pk = x > pk ? x : pk;
                        fails += x != x;
                    }
                }
                worst = pk > worst ? pk : worst;
                fails += !(pk < 2.0 * VOICE_FS / 32768);
                trk_note_off(&trk[0], NOTES[nt]);
                for (i = 0; i < 20u * FS / CTL && act; i++) {
                    memset(b, 0, sizeof b);
                    track_render(&trk[0], b, CTL);
                    for (act = 0, k = 0; k < NVOICE; k++)
                        act |= trk[0].v[k].active;
                }
                memset(b, 0, sizeof b);
                track_render(&trk[0], b, CTL);
                for (k = 0; k < CTL; k++)
                    if (fabs(b[k] / 32768.0) > 0.0005) {
                        fails++;
                        break;
                    }
            }
    snprintf(m, sizeof m, "corners: every preset x 5 notes x both ends, all up: within 2 x VOICE_FS (worst %.2f), silent once released", worst);
    check(m, !fails);
}

/* ------------------------------------------------------------------ cost --- */
static double cost_ns(uint32_t ei, uint32_t pi, int smooth, int extras)
{
    static int32_t ob[CTL];
    uint32_t i, k, nb = 20000, nv = 0;
    uint64_t t0;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    host_preset(&trk[0], ei, pi);
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_REL] = 127;
    if (ei == ENGI_WAVEP) {
        trk[0].p[P_E7] = 127;
        trk[0].xp[WX_MODE] = (int16_t)smooth;
        trk[0].xp[WX_LEVEL] = 100;
        if (extras)
            trk[0].xp[WX_SUB] = 60, trk[0].xp[WX_NOISE] = 20, trk[0].xp[WX_DRIVE] = 30;
        else
            trk[0].xp[WX_SUB] = trk[0].xp[WX_NOISE] = trk[0].xp[WX_DRIVE] = 0;
    }
    for (k = 0; k < NVOICE; k++)
        trk_note_on(&trk[0], 40 + 3 * k, 100);
    for (i = 0; i < 200; i++)
        track_render(&trk[0], ob, CTL);
    for (k = 0; k < NVOICE; k++)
        nv += trk[0].v[k].active;
    t0 = now_ns();
    for (i = 0; i < nb; i++)
        track_render(&trk[0], ob, CTL);
    return (double)(now_ns() - t0) / (nb * CTL) / (nv ? nv : 1u);
}
static void t_cost(void)
{
    double a = 0, st = 0, sm = 0, hv = 0, br = 0;
    uint32_t r;
    char m[240];
    for (r = 0; r < 3u; r++) {
        double x = cost_ns(0, 0, 0, 0), y = cost_ns(ENGI_WAVEP, 0, 0, 0), z = cost_ns(ENGI_WAVEP, 0, 1, 0);
        double u = cost_ns(ENGI_WAVEP, 0, 1, 1), w = cost_ns(ENGI_BRASS, 4, 0, 0);
        a = r && a < x ? a : x;
        st = r && st < y ? st : y;
        sm = r && sm < z ? sm : z;
        hv = r && hv < u ? hv : u;
        br = r && br < w ? br : w;
    }
    snprintf(m, sizeof m, "cost per voice: WAVE-P STEP x%.2f BRASS's (STACK BRASS), SMOOTH x%.2f, SMOOTH + SUB NOISE DRIVE x%.2f (ANALOG x%.2f)",
             st / br, sm / br, hv / br, a / br);
    check(m, hv / a < WAVEP_COST_MAX);
    printf("wavep:   (BRASS measured on the FM-1 ~5.3 %% a voice: WAVE-P STEP ~%.1f %%, SMOOTH ~%.1f %%, all on ~%.1f %%?)\n",
           5.3 * st / br, 5.3 * sm / br, 5.3 * hv / br);
}

/* ---------------------------------------------------------------- demos --- */
static void demo(const char *dir, uint32_t pi, FILE *tour)
{
    static int32_t o[2 * CTL];
    static const uint8_t CH[4][3] = {{48, 55, 63}, {44, 51, 60}, {46, 53, 62}, {43, 50, 58}};
    static const uint8_t LEAD[8] = {60, 67, 70, 72, 70, 67, 65, 67};
    char path[512], nm[40];
    uint32_t i, j, frames = 6u * FS, step = FS / 2, held[3] = {0}, nh = 0, mono;
    FILE *w;
    for (i = 0; ENG_WAVEP.presets[pi].name[i] && i < 39u; i++)
        nm[i] = isalnum((unsigned char)ENG_WAVEP.presets[pi].name[i]) ? (char)tolower(ENG_WAVEP.presets[pi].name[i]) : '_';
    nm[i] = 0;
    snprintf(path, sizeof path, "%s/%02u_%s.wav", dir, pi + 1u, nm);
    w = fopen(path, "wb");
    if (!w)
        return;
    wav_hdr(w, frames);
    memset(trk, 0, sizeof trk);
    memset(eng_arena_own, 0, sizeof eng_arena_own);
    host_tracks_init();
    host_preset(&trk[0], ENGI_WAVEP, pi);
    mono = trk[0].p[P_VOICE] != V_POLY;
    for (i = 0; i < frames; i += CTL) {
        uint32_t s = i / step;
        if (i % step < CTL) {
            for (j = 0; j < nh; j++)
                trk_note_off(&trk[0], held[j]);
            nh = 0;
            if (i < FS * 5) {
                if (mono) {
                    held[0] = LEAD[s % 8u];
                    trk_note_on(&trk[0], held[0], 100);
                    nh = 1;
                } else if (s % 4u == 0u || ENG_WAVEP.presets[pi].env[0] < 40u) {
                    for (j = 0; j < 3u; j++) {
                        held[j] = CH[(s / 4u) % 4u][j] + 12u;
                        trk_note_on(&trk[0], held[j], 100);
                    }
                    nh = 3;
                }
            }
        }
        mix_block(o, CTL);
        for (j = 0; j < CTL; j++) {
            wav_put(w, o[2 * j], o[2 * j + 1]);
            if (tour)
                wav_put(tour, o[2 * j], o[2 * j + 1]);
        }
    }
    fclose(w);
}

int main(int argc, char **argv)
{
    song.master_q12 = 4096;
    t_golden();
    t_tables();
    t_pitch();
    t_step();
    t_position();
    t_bits();
    t_filter();
    t_mirror();
    t_patch();
    t_levels();
    t_corners();
    t_cost();
    if (argc > 1) {
        char path[512];
        uint32_t pi;
        FILE *tour;
        snprintf(path, sizeof path, "%s/00_tour.wav", argv[1]);
        tour = fopen(path, "wb");
        if (tour)
            wav_hdr(tour, ENG_WAVEP.npresets * 6u * FS);
        for (pi = 0; pi < ENG_WAVEP.npresets; pi++)
            demo(argv[1], pi, tour);
        if (tour)
            fclose(tour);
        printf("wavep: demos in %s\n", argv[1]);
    }
    printf("wavep: %s (%d failed)\n", bad ? "FAIL" : "all ok", bad);
    return bad ? 1 : 0;
}
