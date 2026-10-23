/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Khantipol Kasemsant; built on Clipper (isod89 / 3dSam), SLOOP and Felucca */
/* RIBBON engine test (firmware/src/eng_ribbon.c) on the host, through hostsim.c.
 *   build/host/ribbon_test [DEMODIR]          (DEMODIR: a WAV per preset and a tour of them all)
 * 1. golden (first, from the power-on state): every preset's first half second of a note, and the ribbon bent, hash
 *    as tests/ribbon_golden.txt says (GOLDEN_UPDATE=1 rewrites it).
 * 2. pitch: layer I at A4 440 Hz at 8' (+-0.3 %), 16' and 4' its octaves; layer II DETUNE 100 a semitone up.
 * 3. BAL: -64 layer I alone, +63 layer II alone (layer II at 4': its octave 30 dB under / over).
 * 4. the ribbon: RIBBON +32 at RANGE 12 bends a note 6 semitones (+-0.3 %); TARGET LAST the newest note held only,
 *    ALL every one.
 * 5. the spring: K4 at rest, RIBBON stays 300 ms, then is back in the middle within ~175 ms more; SPRING OFF it stays.
 * 6. TOUCH: brighter the longer the key is held (to HOLD: 25 % up) and the harder it is struck; TOUCH 0: neither.
 * 7. RING: I x II (440 and 880 Hz: 1320 Hz 20 dB up); the high-pass: a low saw's bass 10 dB down.
 * 8. the envelopes: layer I's filter envelope (bright, then darker); layer II's own loudness (a slow attack).
 * 9. ATTACK / RELEASE (EDIT 2) and the ENV page's A / R are one value each.
 * 10. the engine patch: 28 values on 7 pages, a preset's round-trip.
 * 11. levels: every preset's note within 4 dB of the DCO pad's in the mix; corners: every preset at five notes with
 *    BRILL, RESO, TOUCH, SUB, RING, NOISE and RIBBON up: within 2 x VOICE_FS, silent once released.
 * 12. cost: ns per sample and voice against ANALOG's (the budget's weight, 2.0, waits for the unit).
 * 13. demos (DEMODIR). */
#include <ctype.h>
#define main hostsim_main
#include "hostsim.c"
#undef main

#define NFFT 16384
#define RIBBON_COST_MAX 5.0             /* x Clipper's ANALOG on the host: a sanity bound */

static int bad;
static void check(const char *what, int ok)
{
    printf("ribbon: %-92s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

/* track 0 RIBBON preset pi; pi < 0: a plain patch (layer I's sine at 8' alone, the filters open, flat envelopes) */
static void setup(int32_t pi)
{
    uint32_t i;
    memset(trk, 0, sizeof trk);
    memset(eng_arena, 0, sizeof eng_arena);
    memset(eng_arena_own, 0, sizeof eng_arena_own);
    memset(rb_mir, 0, sizeof rb_mir);
    memset(rb_last, 0, sizeof rb_last);
    memset(rb_idle, 0, sizeof rb_idle);
    memset(rb_new, 0, sizeof rb_new);
    for (i = 0; i < NPART; i++)
        cm_reset(i);
    host_tracks_init();
    host_preset(&trk[0], ENGI_RIBBON, pi < 0 ? 0u : (uint32_t)pi);
    if (pi < 0) {
        uint32_t k;
        xp_defaults(&trk[0]);
        for (k = 0; k < 8u; k++)
            trk[0].p[P_E0 + k] = ENG_RIBBON.edit[k].def;
        trk[0].p[P_E0] = 127;           /* BRILL */
        trk[0].p[P_E1] = 0;             /* RESO */
        trk[0].p[P_E2] = -64;           /* BAL: layer I */
        trk[0].p[P_E3] = 0;             /* RIBBON */
        trk[0].p[P_E4] = 0;             /* DETUNE */
        trk[0].p[P_E7] = 0;             /* TOUCH */
        trk[0].p[P_E5] = trk[0].p[P_ATK] = 0;
        trk[0].p[P_E6] = trk[0].p[P_REL] = 10;
        trk[0].xp[RX_IWAV] = trk[0].xp[RX_JWAV] = RW_SINE;
        trk[0].xp[RX_IFAMT] = trk[0].xp[RX_JFAMT] = 0;
        trk[0].xp[RX_KBD] = 0;
        trk[0].xp[RX_JATK] = 0, trk[0].xp[RX_JSUS] = 127;
        trk[0].p[P_SUS] = 127;
        trk[0].p[P_GLIDE] = 0;
        trk[0].p[P_VINT] = 0;
        trk[0].p[P_LD_PIT] = trk[0].p[P_LD_FLT] = 0;
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

/* brightness: the RMS of the slope over the RMS (x 1000): more highs, higher */
static double brightness(const double *y, uint32_t n)
{
    double s = 0, d = 0;
    uint32_t i;
    for (i = 1; i < n; i++)
        s += y[i] * y[i], d += (y[i] - y[i - 1]) * (y[i] - y[i - 1]);
    return s > 0 ? 1000 * sqrt(d / s) : 0;
}
static double centroid(const double *ps, double f0, double f1)
{
    uint32_t a = (uint32_t)(f0 * NFFT / FS), b = (uint32_t)(f1 * NFFT / FS), i;
    double s = 0, w = 0;
    for (i = a; i <= b; i++)
        s += ps[i] * i, w += ps[i];
    return w > 0 ? s / w * FS / NFFT : 0;
}

/* ---------------------------------------------------------------- golden --- */
#define NGOLD (32u + 2u)
static const char *const GX[2] = {"RIBBON +32 LAST", "RIBBON -48 ALL"};
static void t_golden(void)
{
    static int32_t b[CTL];
    static uint64_t h[NGOLD];
    static const char *name[NGOLD];
    char line[160];
    const char *path = "tests/ribbon_golden.txt";
    uint32_t pi, i, k, n = 0, changed = 0, missing = 0;
    FILE *f;
    for (pi = 0; pi < ENG_RIBBON.npresets + 2u && n < NGOLD; pi++, n++) {
        uint64_t x = 1469598103934665603ull;
        uint32_t extra = pi >= ENG_RIBBON.npresets ? pi - ENG_RIBBON.npresets + 1u : 0u;
        setup(extra ? 1 : (int32_t)pi);                 /* (the extras: VICTORY LEAD, POLY: two notes) */
        if (extra) {
            trk[0].p[P_VOICE] = V_POLY;
            trk[0].p[P_E3] = extra == 1u ? 32 : -48;
            trk[0].xp[RX_TGT] = (int16_t)(extra - 1u);
            trk[0].xp[RX_SPRING] = 0;
            trk_note_on(&trk[0], 45, 100);
        }
        trk_note_on(&trk[0], extra ? 57u : 48 + (pi % 24u), 100);
        for (i = 0; i < FS / 2 / CTL; i++) {
            memset(b, 0, sizeof b);
            track_render(&trk[0], b, CTL);
            for (k = 0; k < CTL; k++)
                x = (x ^ (uint32_t)b[k]) * 1099511628211ull;
        }
        h[n] = x, name[n] = extra ? GX[extra - 1u] : ENG_RIBBON.presets[pi].name;
    }
    if (getenv("GOLDEN_UPDATE")) {
        f = fopen(path, "w");
        if (f) {
            fprintf(f, "# RIBBON: FNV-1a of half a second of a note: each preset, the ribbon bent (tests/ribbon_test.c)\n");
            for (i = 0; i < n; i++)
                fprintf(f, "%s|%016llx\n", name[i], (unsigned long long)h[i]);
            fclose(f);
        }
        check("golden: tests/ribbon_golden.txt rewritten", f != 0);
        return;
    }
    f = fopen(path, "r");
    if (!f) {
        check("golden: tests/ribbon_golden.txt present (GOLDEN_UPDATE=1 writes it)", 0);
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
            printf("ribbon: golden: %s has no entry\n", name[i]);
            missing++;
        } else if (want != h[i]) {
            printf("ribbon: golden: %s changed (%016llx, was %016llx)\n", name[i], (unsigned long long)h[i], want);
            changed++;
        }
    }
    fclose(f);
    snprintf(line, sizeof line, "golden: %u presets and the ribbon bent render as tests/ribbon_golden.txt says "
             "(%u changed, %u missing)", ENG_RIBBON.npresets, changed, missing);
    check(line, !changed && !missing);
}

/* ----------------------------------------------------------------- pitch --- */
static void t_pitch(void)
{
    static const double MUL[3] = {0.5, 1, 2};
    char m[160];
    uint32_t k;
    double f, want;
    for (k = 0; k < 3u; k++) {
        setup(-1);
        trk[0].xp[RX_IFEET] = (int16_t)k;
        voice_render(69, 0.1, buf, 2 * NFFT);
        f = peak_hz(buf, 30);
        snprintf(m, sizeof m, "pitch: layer I %s at A4: %.2f Hz (%.2f)", N_RB_FEET[k], f, 440 * MUL[k]);
        check(m, fabs(f / (440 * MUL[k]) - 1) < 0.003);
    }
    setup(-1);
    trk[0].p[P_E2] = 63;
    trk[0].p[P_E4] = 100;
    voice_render(69, 0.1, buf, 2 * NFFT);
    f = peak_hz(buf, 30), want = 440 * pow(2, 1 / 12.0);
    snprintf(m, sizeof m, "pitch: layer II DETUNE 100: %.2f Hz (%.2f)", f, want);
    check(m, fabs(f / want - 1) < 0.003);
}

/* ------------------------------------------------------------------- BAL --- */
static void t_bal(void)
{
    static double ps[NFFT / 2];
    char m[160];
    double i0, j0, i1, j1;
    setup(-1);
    trk[0].xp[RX_JFEET] = 2;
    voice_render(69, 0.1, buf, 2 * NFFT);
    spectrum(buf, 2, ps);
    i0 = band_db(ps, 430, 450), j0 = band_db(ps, 860, 900);
    setup(-1);
    trk[0].xp[RX_JFEET] = 2;
    trk[0].p[P_E2] = 63;
    voice_render(69, 0.1, buf, 2 * NFFT);
    spectrum(buf, 2, ps);
    i1 = band_db(ps, 430, 450), j1 = band_db(ps, 860, 900);
    snprintf(m, sizeof m, "BAL: -64 layer I alone (II %.1f dB under), +63 layer II alone (I %.1f dB under)", i0 - j0, j1 - i1);
    check(m, i0 - j0 > 30 && j1 - i1 > 30);
}

/* ---------------------------------------------------------------- ribbon --- */
static void t_ribbon(void)
{
    static double ps[NFFT / 2];
    char m[200];
    double f, want = 440 * pow(2, 0.5), a, b, c;
    setup(-1);
    trk[0].p[P_E3] = 32;
    trk[0].xp[RX_SPRING] = 0;
    voice_render(69, 0.1, buf, 2 * NFFT);
    f = peak_hz(buf, 30);
    snprintf(m, sizeof m, "ribbon: RIBBON +32 at RANGE 12: A4 bent to %.2f Hz (%.2f: 6 semitones)", f, want);
    check(m, fabs(f / want - 1) < 0.003);
    /* TARGET LAST: A3 held, then A4: A4 bent, A3 not */
    setup(-1);
    trk[0].xp[RX_SPRING] = 0;
    trk_note_on(&trk[0], 57, 100);
    blocks_of(0, CTL * 4);
    trk_note_on(&trk[0], 69, 100);
    blocks_of(0, CTL * 4);
    trk[0].p[P_E3] = 32;
    blocks_of(0, FS / 10);
    blocks_of(buf, 2 * NFFT);
    spectrum(buf, 2, ps);
    a = band_db(ps, 215, 225), b = band_db(ps, 305, 317), c = band_db(ps, 615, 630);
    snprintf(m, sizeof m, "ribbon: TARGET LAST: the newest (A4) bent (622 Hz), A3 not (220 Hz %.1f dB over 311)", a - b);
    check(m, a - b > 20 && c > b + 20);
    setup(-1);
    trk[0].xp[RX_SPRING] = 0;
    trk[0].xp[RX_TGT] = 1;
    trk_note_on(&trk[0], 57, 100);
    blocks_of(0, CTL * 4);
    trk_note_on(&trk[0], 69, 100);
    trk[0].p[P_E3] = 32;
    blocks_of(0, FS / 10);
    blocks_of(buf, 2 * NFFT);
    spectrum(buf, 2, ps);
    a = band_db(ps, 215, 225), b = band_db(ps, 305, 317);
    snprintf(m, sizeof m, "ribbon: TARGET ALL: both bent (311 Hz %.1f dB over 220)", b - a);
    check(m, b - a > 20);
}

/* ---------------------------------------------------------------- spring --- */
static void t_spring(void)
{
    char m[200];
    uint32_t i, still = 1, t_back = 0;
    setup(-1);
    trk_note_on(&trk[0], 69, 100);
    blocks_of(0, CTL * 4);
    trk[0].p[P_E3] = 40;                                /* K4 turned to +40, then let rest */
    for (i = 0; i < FS * 2 / CTL; i++) {
        blocks_of(0, CTL);
        if (i * CTL < FS * 29 / 100)                    /* (before 290 ms: where it was put) */
            still &= trk[0].p[P_E3] == 40;
        if (!t_back && !trk[0].p[P_E3])
            t_back = i;
    }
    snprintf(m, sizeof m, "spring: RIBBON +40 stays 300 ms, then back in the middle at %.0f ms", t_back * CTL * 1000.0 / FS);
    check(m, still && t_back * CTL > FS * 3 / 10 && t_back * CTL < FS * 52 / 100);
    setup(-1);
    trk[0].xp[RX_SPRING] = 0;
    trk_note_on(&trk[0], 69, 100);
    blocks_of(0, CTL * 4);
    trk[0].p[P_E3] = 40;
    blocks_of(0, FS);
    check("spring: SPRING OFF: RIBBON +40 stays where it was put (1 s)", trk[0].p[P_E3] == 40);
}

/* ----------------------------------------------------------------- touch --- */
static void t_touch(void)
{
    static double ps[NFFT / 2];
    char m[200];
    double e0, e1, n0, n1, v0, v1;
    uint32_t k;
    for (k = 0; k < 2u; k++) {                          /* held: TOUCH 127, then 0; brightness each 0.25 s */
        double br[8];
        uint32_t w;
        setup(-1);
        trk[0].xp[RX_IWAV] = RW_SAW;
        trk[0].p[P_E0] = 30;
        trk[0].p[P_E7] = k ? 0 : 127;
        trk[0].xp[RX_HOLD] = 96;                        /* (~1.06 s to the full touch) */
        trk_note_on(&trk[0], 45, 100);
        for (w = 0; w < 8u; w++) {
            blocks_of(buf, FS / 4);
            br[w] = brightness(buf, FS / 4);
        }
        if (k)
            n0 = br[0], n1 = br[7];
        else
            e0 = br[0], e1 = br[7];
    }
    snprintf(m, sizeof m, "TOUCH: held 2 s brighter (brightness %.1f -> %.1f); TOUCH 0 not (%.1f -> %.1f)", e0, e1, n0, n1);
    check(m, e1 > 1.25 * e0 && fabs(n1 / n0 - 1) < 0.02);
    for (k = 0; k < 2u; k++) {                          /* struck soft, then hard */
        setup(-1);
        trk[0].xp[RX_IWAV] = RW_SAW;
        trk[0].p[P_E0] = 30;
        trk[0].p[P_E7] = 127;
        trk_note_on(&trk[0], 45, k ? 127 : 20);
        blocks_of(0, CTL * 4);
        blocks_of(buf, NFFT);
        spectrum(buf, 1, ps);
        if (k)
            v1 = centroid(ps, 50, 12000);
        else
            v0 = centroid(ps, 50, 12000);
    }
    snprintf(m, sizeof m, "TOUCH: struck hard brighter (centre %.0f Hz soft, %.0f hard)", v0, v1);
    check(m, v1 > 1.2 * v0);
}

/* ------------------------------------------------------------ RING, HPF --- */
static void t_ring_hpf(void)
{
    static double ps[NFFT / 2];
    char m[200];
    double r0, r1, l0, l1;
    uint32_t k;
    for (k = 0; k < 2u; k++) {
        setup(-1);
        trk[0].xp[RX_JFEET] = 2;
        trk[0].xp[RX_RING] = k ? 127 : 0;
        voice_render(69, 0.1, buf, 2 * NFFT);
        spectrum(buf, 2, ps);
        if (k)
            r1 = band_db(ps, 1300, 1340);
        else
            r0 = band_db(ps, 1300, 1340);
    }
    snprintf(m, sizeof m, "RING: layer I (440) x II (880): 1320 Hz %.1f dB up", r1 - r0);
    check(m, r1 - r0 > 20);
    for (k = 0; k < 2u; k++) {
        setup(-1);
        trk[0].xp[RX_IWAV] = RW_SAW;
        trk[0].xp[RX_IHPF] = k ? 70 : 0;
        voice_render(33, 0.1, buf, 2 * NFFT);
        spectrum(buf, 2, ps);
        if (k)
            l1 = band_db(ps, 50, 120);
        else
            l0 = band_db(ps, 50, 120);
    }
    snprintf(m, sizeof m, "I HPF: a low saw's bass (50 .. 120 Hz) %.1f dB down", l0 - l1);
    check(m, l0 - l1 > 10);
}

/* ------------------------------------------------------------- envelopes --- */
static void t_envs(void)
{
    static double ps[NFFT / 2];
    char m[200];
    double e0, e1, a0, a1;
    setup(-1);
    trk[0].xp[RX_IWAV] = RW_SAW;
    trk[0].p[P_E0] = 30;
    trk[0].xp[RX_IFAMT] = 63, trk[0].xp[RX_IFATK] = 0, trk[0].xp[RX_IFDEC] = 100, trk[0].xp[RX_IFSUS] = 0;
    voice_render(45, 0.0, buf, 4096);
    memset(buf + 4096, 0, (NFFT - 4096) * sizeof(double));   /* (the first 93 ms: the envelope near its top) */
    spectrum(buf, 1, ps);
    e0 = centroid(ps, 50, 12000);
    blocks_of(0, FS * 2);
    blocks_of(buf, NFFT);
    spectrum(buf, 1, ps);
    e1 = centroid(ps, 50, 12000);
    snprintf(m, sizeof m, "layer I's filter envelope: bright, then darker (centre %.0f -> %.0f Hz)", e0, e1);
    check(m, e0 > 1.3 * e1);
    setup(-1);
    trk[0].p[P_E2] = 63;
    trk[0].xp[RX_JATK] = 100;
    voice_render(69, 0.0, buf, FS / 50);
    a0 = rms(buf, FS / 50);
    blocks_of(0, FS * 2);
    blocks_of(buf, FS / 50);
    a1 = rms(buf, FS / 50);
    snprintf(m, sizeof m, "layer II's own loudness: a slow attack (%.4f at once, %.4f at 2 s)", a0, a1);
    check(m, a1 > 4 * a0);
}

/* ------------------------------------------------------- ATTACK, RELEASE --- */
static void t_mirror(void)
{
    static int32_t o[CTL * 2];
    setup(0);
    mix_block(o, CTL);
    check("ATTACK / RELEASE: a preset sets EDIT 2's and the ENV page's alike",
          trk[0].p[P_E5] == trk[0].p[P_ATK] && trk[0].p[P_E6] == trk[0].p[P_REL]);
    trk[0].p[P_E5] = 70, trk[0].p[P_E6] = 90;
    mix_block(o, CTL);
    check("ATTACK / RELEASE: EDIT 2 turned: A and R follow", trk[0].p[P_ATK] == 70 && trk[0].p[P_REL] == 90);
    trk[0].p[P_ATK] = 5, trk[0].p[P_REL] = 20;
    mix_block(o, CTL);
    check("ATTACK / RELEASE: A and R turned: EDIT 2 follows", trk[0].p[P_E5] == 5 && trk[0].p[P_E6] == 20);
}

/* --------------------------------------------------------- engine patch --- */
static void t_patch(void)
{
    static uint8_t b[XP_BYTES];
    int16_t keep[NXP];
    uint32_t k, same = 1;
    setup(0);
    memcpy(keep, trk[0].xp, sizeof keep);
    check("engine patch: RIBBON has 28 values on 7 pages", RIBBON_XPI.n == 28u && xp_title(&trk[0], 6) && !xp_title(&trk[0], 7));
    xp_pack(&trk[0], b);
    xp_defaults(&trk[0]);
    xp_unpack(&trk[0], b);
    for (k = 0; k < NXP; k++)
        same &= trk[0].xp[k] == keep[k];
    check("engine patch: a preset's values round-trip (RANGE from 1, the filter depths signed)", same);
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
    for (pi = 0; pi < ENG_RIBBON.npresets; pi++) {
        double db;
        uint32_t key;
        setup((int32_t)pi);
        key = trk[0].p[P_VOICE] == V_POLY ? 60u : 64u;
        trk_note_on(&trk[0], key, 100);
        r = part_loudest();
        db = 20 * log10(r / ref) + trk[0].p[P_ED_FX] * 0.5;
        printf("ribbon:   %-14s %+.1f dB in the mix against one pad note (trim %+.1f dB)\n", ENG_RIBBON.presets[pi].name, db,
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
    for (pi = 0; pi < ENG_RIBBON.npresets; pi++)
        for (nt = 0; nt < NELEM(NOTES); nt++)
            for (r = 0; r < 2u; r++) {
                static int32_t b[CTL];
                double pk = 0;
                uint32_t i, act = 1;
                setup((int32_t)pi);
                trk[0].p[P_E0] = 127, trk[0].p[P_E1] = 127, trk[0].p[P_E7] = 127, trk[0].p[P_E3] = r ? 63 : -64;
                trk[0].xp[RX_SUB] = trk[0].xp[RX_RING] = trk[0].xp[RX_NOISE] = 127;
                trk[0].xp[RX_IFAMT] = trk[0].xp[RX_JFAMT] = 63;
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
    snprintf(m, sizeof m, "corners: every preset x 5 notes x RIBBON at both ends, all up: within 2 x VOICE_FS (worst %.2f), silent once released", worst);
    check(m, !fails);
}

/* ------------------------------------------------------------------ cost --- */
static double cost_ns(uint32_t ei, uint32_t pi, int heavy)
{
    static int32_t ob[CTL];
    uint32_t i, k, nb = 20000, nv = 0;
    uint64_t t0;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    host_preset(&trk[0], ei, pi);
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_REL] = 127;
    if (ei == ENGI_RIBBON)
        trk[0].p[P_E6] = 127;
    if (heavy)
        trk[0].xp[RX_SUB] = trk[0].xp[RX_RING] = trk[0].xp[RX_NOISE] = 80, trk[0].xp[RX_IHPF] = trk[0].xp[RX_JHPF] = 30;
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
    double a = 0, l = 0, h = 0, br = 0;
    uint32_t r;
    char m[220];
    for (r = 0; r < 3u; r++) {
        double x = cost_ns(0, 0, 0), y = cost_ns(ENGI_RIBBON, 0, 0), z = cost_ns(ENGI_RIBBON, 0, 1), w = cost_ns(ENGI_BRASS, 4, 0);
        a = r && a < x ? a : x;
        l = r && l < y ? l : y;
        h = r && h < z ? h : z;
        br = r && br < w ? br : w;
    }
    snprintf(m, sizeof m, "cost per voice: ANALOG %.2f ns, RIBBON %.2f (x%.2f), heaviest %.2f (x%.2f); BRASS STACK BRASS %.2f (x%.2f)",
             a, l, l / a, h, h / a, br, br / a);
    check(m, h / a < RIBBON_COST_MAX);
    printf("ribbon:   RIBBON / BRASS on the host x%.2f (BRASS measured on the FM-1 ~5.3 %% a voice: RIBBON ~%.1f %%?)\n",
           l / br, 5.3 * l / br);
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
    for (i = 0; ENG_RIBBON.presets[pi].name[i] && i < 39u; i++)
        nm[i] = isalnum((unsigned char)ENG_RIBBON.presets[pi].name[i]) ? (char)tolower(ENG_RIBBON.presets[pi].name[i]) : '_';
    nm[i] = 0;
    snprintf(path, sizeof path, "%s/%02u_%s.wav", dir, pi + 1u, nm);
    w = fopen(path, "wb");
    if (!w)
        return;
    wav_hdr(w, frames);
    memset(trk, 0, sizeof trk);
    memset(eng_arena_own, 0, sizeof eng_arena_own);
    host_tracks_init();
    host_preset(&trk[0], ENGI_RIBBON, pi);
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
                } else if (s % 4u == 0u || ENG_RIBBON.presets[pi].env[0] < 40u) {
                    for (j = 0; j < 3u; j++) {
                        held[j] = CH[(s / 4u) % 4u][j] + 12u;
                        trk_note_on(&trk[0], held[j], 100);
                    }
                    nh = 3;
                }
            }
        }
        if (mono && i % step == step / 2 - (step / 2) % CTL && s % 2u)   /* the ribbon: up a fourth, let go */
            trk[0].p[P_E3] = 21;
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
    t_pitch();
    t_bal();
    t_ribbon();
    t_spring();
    t_touch();
    t_ring_hpf();
    t_envs();
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
            wav_hdr(tour, ENG_RIBBON.npresets * 6u * FS);
        for (pi = 0; pi < ENG_RIBBON.npresets; pi++)
            demo(argv[1], pi, tour);
        if (tour)
            fclose(tour);
        printf("ribbon: demos in %s\n", argv[1]);
    }
    printf("ribbon: %s (%d failed)\n", bad ? "FAIL" : "all ok", bad);
    return bad ? 1 : 0;
}
