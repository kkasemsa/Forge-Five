/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Khantipol Kasemsant; built on Clipper (isod89 / 3dSam), SLOOP and Felucca (C) 2026 Leo Kuroshita,
 * Hügelton Instruments */
/* WAVE-P (Forge Five Phase 6): the stepped wavetable sound of the early-1980s German wavetable synths (spec v0.2
 * section 3.5). Written from their published behaviour; the tables are made by tools/gen_wavep_tables.py from formulas
 * (forge_wavep.h): no ROM, wave or sample of any instrument.
 *
 *   8 tables of 64 waves, a wave one cycle of 128 points, 8 bits (in flash, 64 KB); waves 0..59 a table's sweep,
 *   60..63 the same four plain shapes in every table (triangle, square, 25 % pulse, saw: the "upper" waves)
 *   two oscillators a voice, one table: OSC 2 its own interval (SEMI), DETUNE, LEVEL and an offset in the table
 *   (OFFS); a SUB square an octave under OSC 1; NOISE
 *   MODE   STEP (the default): the waves are read point by point as they are, and an oscillator moves to the next wave
 *          only at the start of a cycle (no crossfade: the sweep steps, and does not click); SMOOTH: the points joined
 *          and the waves crossfaded (a clean sweep)
 *   BITS   8 / 6 / 5 / 4: the oscillators' resolution (8: the tables' own; 4: crushed). (The spec's 12-bit option
 *          would need tables of 12 bits, 96 KB more flash than these 64: not now.)
 *   WAVE   the position: + the wave envelope (W ENV, ENV>W both ways), the LFO and the ENV page's SHP destinations
 *          (LFO DEST SHP: +-32 waves), W KBD (a wave a semitone at full), VEL>W; modulation stays in the sweep
 *          (0..59) unless UPPER is on or WAVE itself is past it
 *   a 24 dB low-pass (two 12 dB stages, BRASS's 4P), KBD, VEL>F, DRIVE before it; the ENV page's envelope moves the
 *   cutoff through its FLT destination (the presets' ENV -> FILTER)
 *   EDIT 1  WAVE  CUTOFF  ENV>W  RESO          EDIT 2  TABLE  DETUNE  BITS  RELEASE (= the ENV page's R)
 * Voice state: ph[0] OSC 1, ph[1] OSC 2, ph[2] the sub; s[0] s[1] the first low-pass, s[2] s[3] the second; s[4] s[5]
 * the wave each oscillator plays (STEP), s[6] the position at the last block's end (Q8 waves, -1: none yet), s[7] the
 * noise. In the part's arena (wavep_vx_t): the wave envelope. */
#include "forge_wavep.h"

static const char *const N_WP_BITS[] = {"8", "6", "5", "4"};
static const char *const N_WP_MODE[] = {"STEP", "SMOOTH"};
static const char *const N_WP_KBD[] = {"OFF", "HALF", "FULL"};
static const char *const N_WP_ONOFF[] = {"OFF", "ON"};
static const uint8_t WP_BITSH[4] = {0, 10, 11, 12};   /* BITS: the low bits of a Q15 sample dropped (8: none, the
                                                       * tables' own) */

enum { WM_STEP, WM_SMOOTH };
enum {
    WX_SEMI, WX_LEVEL, WX_OFFS, WX_SUB,              /* page OSC 2 */
    WX_WATK, WX_WDEC, WX_WSUS, WX_WREL,              /* page W ENV */
    WX_MODE, WX_WKBD, WX_VELW, WX_UPPER,             /* page WAVE MOD */
    WX_KBD, WX_VELF, WX_DRIVE, WX_NOISE,             /* page FILTER */
    WX_COUNT
};
#define WXP(t, k) ((t)->xp[k])

static const param_desc_t WAVEP_X[WX_COUNT] = {
    {"2 SEMI", F_INT, -24, 24, 0, 0, "st"},
    {"2 LEVEL", F_PCT, 0, 127, 100, 0, 0},
    {"2 OFFS", F_INT, -32, 31, 0, 0, 0},
    {"SUB", F_PCT, 0, 127, 0, 0, 0},
    {"W ATK", F_TIME, 0, 127, 0, 0, 0},
    {"W DEC", F_TIME, 0, 127, 80, 0, 0},
    {"W SUS", F_PCT, 0, 127, 0, 0, 0},
    {"W REL", F_TIME, 0, 127, 60, 0, 0},
    {"MODE", F_ENUM, 0, 1, WM_STEP, N_WP_MODE, 0},
    {"W KBD", F_BIPCT, -64, 63, 0, 0, 0},
    {"VEL>W", F_BIPCT, -64, 63, 0, 0, 0},
    {"UPPER", F_ENUM, 0, 1, 0, N_WP_ONOFF, 0},
    {"KBD", F_ENUM, 0, 2, 1, N_WP_KBD, 0},
    {"VEL>F", F_BIPCT, -64, 63, 0, 0, 0},
    {"DRIVE", F_PCT, 0, 127, 0, 0, 0},
    {"NOISE", F_PCT, 0, 127, 0, 0, 0},
};

/* per voice, in the part's engine arena (engines.c eng_arena_of) */
typedef struct {
    int32_t we;                      /* the wave envelope, Q24 */
    uint8_t ws;                      /* its stage: 1 attack, 2 decay / sustain, 3 release */
} wavep_vx_t;
typedef wavep_vx_t wavep_part_t[NVOICE];
static wavep_vx_t wavep_spare;       /* a track outside trk[] (a host test's copy): state of its own */
static wavep_vx_t *wavep_vx(track_t *t, const voice_t *v)
{
    wavep_part_t *a;
    if (t < &trk[0] || t >= &trk[NPART])
        return &wavep_spare;
    a = (wavep_part_t *)eng_arena_of(t, ENGI_WAVEP);
    return a ? &(*a)[(uint32_t)(v - t->v) % NVOICE] : &wavep_spare;
}

static void wavep_note_on(track_t *t, voice_t *v)
{
    wavep_vx_t *x = wavep_vx(t, v);
    if (!v->env && !v->env_out) {                     /* from silence: the filters clear, the oscillators from the
                                                       * cycle's start, the wave envelope from 0 */
        v->s[0] = v->s[1] = v->s[2] = v->s[3] = 0;
        v->ph[0] = 0, v->ph[1] = 0, v->ph[2] = 0;
        x->we = 0;
        v->s[6] = -1;                                 /* (the position taken afresh, the waves with it; a voice still
                                                       * sounding keeps its waves until each cycle's start, and its
                                                       * position glides from where it was over the block) */
    }
    if (!v->s[7])
        v->s[7] = 0x2545F491 + (int32_t)v->age * 2654435;
    x->ws = 1;
}

/* one point of wave w as it is (STEP), Q15 */
static inline int32_t wp_step(const int8_t *a, uint32_t ph) { return (int32_t)a[ph >> 25] << 8; }
/* SMOOTH: waves a and b crossfaded by wf (Q8) at the two points either side of ph, then joined, Q15 */
static inline int32_t wp_lin2(const int8_t *a, const int8_t *b, int32_t wf, uint32_t ph)
{
    uint32_t k = ph >> 25, k1 = (k + 1u) & (WP_NP - 1u);
    int32_t fr = (int32_t)((ph >> 17) & 255u);
    int32_t c0 = (a[k] << 8) + (b[k] - a[k]) * wf, c1 = (a[k1] << 8) + (b[k1] - a[k1]) * wf;
    return c0 + (((c1 - c0) * fr) >> 8);
}

static void wavep_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    wavep_vx_t *x = wavep_vx(t, v);
    const int8_t (*tab)[WP_NP] = WP_TAB[(uint32_t)p[P_E4] % WP_NT];
    uint32_t i, smooth = WXP(t, WX_MODE) == WM_SMOOTH, sh = WP_BITSH[(uint32_t)p[P_E6] & 3u];
    int32_t base = clamp(p[P_E0], 0, WP_NW - 1), hi = WXP(t, WX_UPPER) || base >= WP_NSWEEP ? WP_NW - 1 : WP_NSWEEP - 1;
    int32_t offs = WXP(t, WX_OFFS), we, pos0, pos1, cut, k, k4, drive = WXP(t, WX_DRIVE);
    int32_t dg = 32768 + drive * 768, dw = drive * 258;
    int32_t l2 = WXP(t, WX_LEVEL) * 258, sub = WXP(t, WX_SUB) * 200, nz = WXP(t, WX_NOISE) * 120;
    int32_t a1 = v->s[0], a2 = v->s[1], b1 = v->s[2], b2 = v->s[3], w1 = v->s[4], w2 = v->s[5], nst = v->s[7];
    uint32_t ph1 = v->ph[0], ph2 = v->ph[1], phs = v->ph[2];
    uint32_t inc1 = cents_inc(m->pitch16, 0, m->fine);
    uint32_t inc2 = cents_inc(m->pitch16, WXP(t, WX_SEMI) * 100 + p[P_E5], m->fine), incs = inc1 >> 1;
    tsvf_t f, g;

    /* the position, once a block (Q8 waves): WAVE, the wave envelope (ENV>W: +-63 waves at full), the SHP
     * destinations (the LFO's, the ENV page's: +-32), W KBD (a wave a semitone at full), VEL>W (+-32 at full) */
    we = rb_env(&x->we, &x->ws, v->gate, WXP(t, WX_WATK), WXP(t, WX_WDEC), WXP(t, WX_WSUS), WXP(t, WX_WREL));
    pos1 = (base << 8) + ((we * p[P_E2]) >> 7) + ((m->shape - (64 << 8)) >> 1);
    pos1 += (((m->pitch16 - 60 * 16) * WXP(t, WX_WKBD)) >> 2);
    pos1 += (((int32_t)v->vel - 64) * WXP(t, WX_VELW)) * 2;
    pos1 = clamp(pos1, 0, hi << 8);
    pos0 = v->s[6] < 0 || v->s[6] > (hi << 8) ? pos1 : v->s[6];   /* (never a wave past the table: a voice
                                                       * another engine's state reached) */
    w1 = clamp(w1, 0, hi), w2 = clamp(w2, 0, hi);
    if (v->s[6] < 0) {                                /* a note's start: the waves its position's */
        w1 = pos1 >> 8;
        w2 = clamp(w1 + offs, 0, hi);
    }
    /* the cutoff in parameter steps (Q8): CUTOFF, the track's modulation (the ENV page's FLT, the LFO), KBD, VEL>F */
    cut = (p[P_E1] << 8) + m->cutoff;
    cut += (WXP(t, WX_KBD) * ((m->pitch16 - 60 * 16) * 299 / 16)) >> 1;
    cut += (((int32_t)v->vel - 64) * WXP(t, WX_VELF)) >> 1;
    k = 8192 - p[P_E3] * 6600 / 127;                  /* RESO: the damping 2.0 .. 0.39 (short of oscillation) */
    k4 = 8192 - p[P_E3] * 3000 / 127;                 /* (the second stage: a gentler peak, as BRASS's 4P) */
    brass_svf(&f, cut, k);
    brass_svf(&g, cut, k4);
    for (i = 0; i < n; i++) {
        int32_t pq = pos0 + (((pos1 - pos0) * (int32_t)i) >> CTL_LOG2), xo, s, v1, v2, v3, y;
        uint32_t o1 = ph1, o2 = ph2;
        ph1 += inc1, ph2 += inc2;
        if (smooth) {                                 /* the points joined, the waves crossfaded */
            int32_t wi = pq >> 8, wf = pq & 255, wj = wi < hi ? wi + 1 : wi, wo = clamp(wi + offs, 0, hi);
            int32_t wp = wo < hi ? wo + 1 : wo;
            xo = wp_lin2(tab[wi], tab[wj], wf, o1);
            if (l2)
                xo += mulq15(wp_lin2(tab[wo], tab[wp], wf, o2), l2);
        } else {                                      /* STEP: the points as they are; a new wave at a cycle's start */
            if (ph1 < o1)
                w1 = pq >> 8;
            if (ph2 < o2)
                w2 = clamp((pq >> 8) + offs, 0, hi);
            xo = wp_step(tab[w1], o1);
            if (l2)
                xo += mulq15(wp_step(tab[w2], o2), l2);
        }
        if (sh)                                       /* BITS */
            xo = (xo >> sh) << sh;
        if (sub)
            xo += mulq15(osc_pulse(phs, incs, 0x80000000u), sub);
        phs += incs;
        if (nz)
            xo += mulq15((int32_t)(noise32(&nst) >> 17) - 16384, nz);
        s = xo >> 2;                                  /* (a full oscillator a quarter scale in, as BRASS's mixer) */
        if (drive)                                    /* DRIVE: ANALOG's (up to ~x4 into a soft clip, mixed in) */
            s += mulq15(softclip(((s >> 2) * (dg >> 2)) >> 11) - s, dw);
        s = soft_knee(s, 12000);
        v3 = s - a2;                                  /* two 12 dB low-passes: 24 dB */
        v1 = (f.a1 * a1 + f.a2 * v3) >> 13;
        v2 = a2 + ((f.a2 * a1 + f.a3 * v3) >> 13);
        a1 = clamp(2 * v1 - a1, -150000, 150000);
        a2 = clamp(2 * v2 - a2, -150000, 150000);
        v3 = v2 - b2;
        v1 = (g.a1 * b1 + g.a2 * v3) >> 13;
        v2 = b2 + ((g.a2 * b1 + g.a3 * v3) >> 13);
        b1 = clamp(2 * v1 - b1, -150000, 150000);
        b2 = clamp(2 * v2 - b2, -150000, 150000);
        y = clamp(soft_knee(v2, 10000) << 1, -32000, 32000);
        out[i] += voice_amp(y, m, i) << 1;
    }
    v->ph[0] = ph1, v->ph[1] = ph2, v->ph[2] = phs;
    v->s[0] = a1, v->s[1] = a2, v->s[2] = b1, v->s[3] = b2;
    v->s[4] = w1, v->s[5] = w2, v->s[6] = pos1, v->s[7] = nst;
}

/* once per block, before the voices (and before any is handed to the second core): the part's arena claimed;
 * RELEASE (EDIT 2) and the ENV page's R kept one value */
static int16_t wp_mir[NTRK][2];
static void wavep_block(track_t *t)
{
    int16_t *l = wp_mir[(uint32_t)(t - trk) % NTRK];
    if (t >= &trk[0] && t < &trk[NPART])
        (void)eng_arena_of(t, ENGI_WAVEP);
    if (t->p[P_E7] != l[0])
        t->p[P_REL] = t->p[P_E7];
    else if (t->p[P_REL] != l[1])
        t->p[P_E7] = t->p[P_REL];
    l[0] = t->p[P_E7], l[1] = t->p[P_REL];
}

enum { WT_BUILD, WT_PULSE, WT_VOCAL, WT_SYNC, WT_GLASS, WT_ORGAN, WT_FRACT, WT_RESO };
_Static_assert(WT_RESO + 1 == WP_NT, "WAVE-P: the tables as tools/gen_wavep_tables.py makes them");

/*                    E: WAVE CUTOFF ENV>W RESO  TABLE DETUNE BITS RELEASE (= the ADSR's R)
 * The level trims (P_ED_FX, 1/2 dB, in the mix): each note near one note of the DCO pad (tests/wavep_test.c) */
static const preset_t WAVEP_PRESETS[] = {
    {"DIGITAL CHOIR", {14, 92, 18, 14, WT_VOCAL, 9, 0, 96}, {70, 100, 120, 96}, 0, 0, FX(0, 40, 20, 80),
     XP(P_ED_FX + 1, 10, P_LRATE + 1, 20, P_LD_SHP + 1, 10)},
    {"GLASS SWEEP", {0, 96, 54, 30, WT_GLASS, 6, 0, 100}, {40, 100, 118, 100}, 0, 0, FX(0, 30, 30, 80), XP(P_ED_FX + 1, 14)},
    {"STEP BELLS", {34, 104, -30, 10, WT_GLASS, 0, 0, 90}, {0, 92, 0, 90}, 0, 0, FX(0, 10, 40, 60), XP(P_ED_FX + 1, 14)},
    {"HOLLOW BASS", {0, 40, 10, 26, WT_PULSE, 4, 0, 30}, {0, 70, 100, 30}, 30, 1, FX(10, 0, 0, 10),
     XP(P_ED_FX + 1, -4, P_TRANS + 1, -12, P_GLIDE + 1, 14)},
    {"FRACTAL PAD", {20, 88, 0, 16, WT_FRACT, 10, 0, 104}, {80, 100, 120, 104}, 0, 0, FX(0, 40, 30, 90),
     XP(P_ED_FX + 1, 4, P_LRATE + 1, 12, P_LD_SHP + 1, 40)},
    {"SWEEP LEAD", {0, 84, 46, 24, WT_SYNC, 7, 0, 50}, {2, 80, 112, 50}, 10, 1, FX(10, 10, 30, 30),
     XP(P_ED_FX + 1, 2, P_GLIDE + 1, 24, P_LRATE + 1, 34, P_LD_PIT + 1, 3)},
    {"RESO STAB", {6, 70, 50, 20, WT_RESO, 0, 0, 40}, {0, 66, 40, 40}, 20, 0, FX(0, 10, 30, 40), XP(P_ED_FX + 1, 11)},
    {"CRUNCH KEYS", {24, 100, -20, 8, WT_BUILD, 0, 2, 60}, {0, 84, 50, 60}, 10, 0, FX(0, 20, 20, 40), XP(P_ED_FX + 1, 2)},
    {"STEP ORGAN", {10, 110, 0, 0, WT_ORGAN, 3, 1, 30}, {0, 30, 127, 30}, 0, 0, FX(0, 40, 0, 40),
     XP(P_LRATE + 1, 40, P_LD_SHP + 1, 6)},
};

/*                 X: SEMI LEVEL OFFS SUB  WATK WDEC WSUS WREL  MODE WKBD VELW UPPER  KBD VELF DRIVE NOISE */
typedef struct {
    int8_t x[WX_COUNT];
} wavep_xp_t;
static const wavep_xp_t WAVEP_XP[NELEM(WAVEP_PRESETS)] = {
    /* DIGITAL CHOIR: the vowels drifting, two voices a few waves apart, smooth */
    {{0, 100, 4, 0, 90, 110, 90, 100, WM_SMOOTH, 0, 0, 0, 1, 0, 0, 6}},
    /* GLASS SWEEP: a long climb through the glass table, stepped */
    {{12, 70, 0, 0, 104, 120, 127, 110, WM_STEP, 0, 10, 0, 1, 0, 0, 0}},
    /* STEP BELLS: struck high in the table, falling back in audible steps; higher notes a darker wave (W KBD -16:
     * a 128-point wave's top kept clear of the fold-back) */
    {{12, 40, -10, 0, 0, 96, 0, 96, WM_STEP, -16, 20, 0, 2, 10, 0, 0}},
    /* HOLLOW BASS: a square (the pulse table's top) and its sub, a short wave flick */
    {{0, 40, 2, 90, 0, 60, 0, 40, WM_STEP, 0, 0, 0, 1, 20, 20, 0}},
    /* FRACTAL PAD: the random table scanned slowly by the LFO */
    {{7, 90, 9, 0, 100, 100, 100, 100, WM_SMOOTH, 0, 0, 0, 1, 0, 0, 0}},
    /* SWEEP LEAD: the sync sweep on each note */
    {{0, 80, 3, 0, 0, 90, 30, 60, WM_STEP, 0, 16, 0, 1, 10, 30, 0}},
    /* RESO STAB: the resonance table swept down fast */
    {{-12, 60, 0, 40, 0, 70, 0, 50, WM_STEP, 0, 0, 0, 1, 20, 10, 0}},
    /* CRUNCH KEYS: the harmonics building, six bits */
    {{0, 50, 6, 0, 0, 80, 40, 60, WM_STEP, 0, 20, 0, 1, 16, 0, 0}},
    /* STEP ORGAN: stops pulled, the LFO stepping through them */
    {{12, 60, 8, 0, 0, 60, 127, 30, WM_STEP, 0, 0, 0, 0, 0, 0, 0}},
};

static void wavep_load(track_t *t, int32_t pi)
{
    uint32_t k;
    if (pi < 0)
        return;
    for (k = 0; k < WX_COUNT; k++)
        t->xp[k] = WAVEP_XP[(uint32_t)pi % NELEM(WAVEP_XP)].x[k];
}
static const char *const WAVEP_PAGES[] = {"OSC 2", "W ENV", "WAVE MOD", "FILTER"};
static const xp_info_t WAVEP_XPI = {WAVEP_X, WX_COUNT, WAVEP_PAGES, wavep_load};
_Static_assert(WX_COUNT <= NXP && (WX_COUNT + 3u) / 4u == NELEM(WAVEP_PAGES), "WAVE-P: its pages and values");
_Static_assert(NELEM(WAVEP_XP) == NELEM(WAVEP_PRESETS), "WAVE-P: a row of extra values per preset");

static const engine_t ENG_WAVEP = {
    .name = "WAVE-P",
    .page_title = {"WAVE", "TABLE"},
    .edit = {
        {"WAVE", F_INT, 0, WP_NW - 1, 0, 0, 0},
        {"CUTOFF", F_CUTOFF, 0, 127, 100, 0, 0},
        {"ENV>W", F_BIPCT, -64, 63, 30, 0, 0},
        {"RESO", F_PCT, 0, 127, 10, 0, 0},
        {"TABLE", F_ENUM, 0, WP_NT - 1, 0, WP_NAMES, 0},
        {"DETUNE", F_INT, 0, 50, 6, 0, "ct"},
        {"BITS", F_ENUM, 0, 3, 0, N_WP_BITS, 0},
        {"RELEASE", F_TIME, 0, 127, 60, 0, 0},
    },
    .presets = WAVEP_PRESETS,
    .npresets = NELEM(WAVEP_PRESETS),
    .fil_page = 0,
    .note_on = wavep_note_on,
    .render = wavep_render,
    .color = 0x07FF,
    .macro = {P_E0, P_E1, P_E2, P_E3},
    .poly = 0,
    .block = wavep_block,
};
