/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Khantipol Kasemsant
 * Built on Felucca, Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* SEM: a two-oscillator polysynth voice in the manner of the classic 1970s–80s American state-variable polys (the
 * single-voice module and the eight-voice keyboards built from it). Written from scratch from the published behaviour
 * of those circuits; no code or data of any other synth.
 *
 * Signal path, as on the original's panel:
 *   OSCILLATORS  two VCOs, each SAW, PULSE (width), both, or the module's triangle; RANGE 16' 8' 4'; OSC 2 FREQ
 *                (+-7 semitones, fine near the centre) and DETUNE in cents; SYNC: OSC 2 hard-synced to OSC 1
 *   MIXER        a level per oscillator and NOISE
 *   FILTER       the 12 dB state-variable filter with its MODE knob: LOW-PASS .. NOTCH .. HIGH-PASS, continuous (at
 *                the centre the notch, where the two are equal), or BAND-PASS (its switch); RESONANCE short of
 *                self-oscillation, as the original's; TYPE 2P (the MODE knob), 4P (the later keyboards' switch: a second
 *                low-pass after the first, 24 dB), BP (the module's band-pass switch); the FILTER ENVELOPE (ADSR, its
 *                AMOUNT both ways), KBD tracking OFF HALF FULL; X-MOD: OSC 2 into the cutoff (once a block, 0.7 ms)
 *   PWM          LFO -> WIDTH (the track's LFO, the panel's PWM depth) on both pulses
 *   The loudness envelope is the track's ENV page; the LFO the track's LFO page (its DEST too)
 *
 * Parameters: the engine's eight (EDIT 1 FILTER: CUTOFF RESO MODE TYPE; EDIT 2 F ENV: F ATK F DEC F SUS F REL) and
 * sixteen more as its engine patch (engine_xp.c, SEM_XP): OSC 1, OSC 2, MIX, MOD. */

static const char *const N_SEM_WAVE[] = {"SAW", "PLS", "BOTH", "TRI"};
static const char *const N_SEM_RANGE[] = {"16'", "8'", "4'"};
static const char *const N_SEM_TYPE[] = {"2P", "4P", "BP"};   /* the MODE knob's 2 poles, the 4-pole low-pass, band-pass */
static const char *const N_SEM_KBD[] = {"OFF", "HALF", "FULL"};
static const char *const N_SEM_BP[] = {"OFF", "ON"};

enum { SW_SAW, SW_PLS, SW_BOTH, SW_TRI };
enum {
    SX_O1WAV, SX_O1PW, SX_O1RNG, SX_O1LVL,           /* OSC 1 */
    SX_O2WAV, SX_O2PW, SX_O2RNG, SX_O2LVL,           /* OSC 2 */
    SX_O2FRQ, SX_DTN, SX_SYNC, SX_NOISE,             /* MIX */
    SX_FAMT, SX_KBD, SX_XMOD, SX_PWM,                /* MOD */
    SX_COUNT
};
#define SXP(t, k) ((t)->xp[k])

static const param_desc_t SEM_X[SX_COUNT] = {
    {"O1 WAV", F_ENUM, 0, 3, SW_SAW, N_SEM_WAVE, 0},
    {"O1 PW", F_PCT, 0, 127, 64, 0, 0},
    {"O1 RNG", F_ENUM, 0, 2, 1, N_SEM_RANGE, 0},
    {"OSC 1", F_PCT, 0, 127, 127, 0, 0},
    {"O2 WAV", F_ENUM, 0, 3, SW_SAW, N_SEM_WAVE, 0},
    {"O2 PW", F_PCT, 0, 127, 64, 0, 0},
    {"O2 RNG", F_ENUM, 0, 2, 1, N_SEM_RANGE, 0},
    {"OSC 2", F_PCT, 0, 127, 100, 0, 0},
    {"O2 FRQ", F_ST8, -56, 56, 0, 0, "st"},
    {"DETUNE", F_INT, 0, 127, 8, 0, "ct"},
    {"SYNC", F_ENUM, 0, 1, 0, N_SEM_BP, 0},
    {"NOISE", F_PCT, 0, 127, 0, 0, 0},
    {"F AMT", F_BIPCT, -64, 63, 24, 0, 0},
    {"KBD", F_ENUM, 0, 2, 1, N_SEM_KBD, 0},
    {"X-MOD", F_PCT, 0, 127, 0, 0, 0},
    {"PWM", F_PCT, 0, 127, 0, 0, 0},
};

/* per voice, in the part's engine memory (engines.c ENG_MEM): the output's DC blocker */
typedef int32_t sem_part_t[NVOICE];
ENG_MEM_FITS("SEM", sizeof(sem_part_t));
static int32_t sem_spare;
static int32_t *sem_dc(const track_t *t, const voice_t *v)
{
    if (t < &trk[0] || t >= &trk[NPART])
        return &sem_spare;
    return &(*(sem_part_t *)ENG_MEM(t - trk))[(uint32_t)(v - t->v) % NVOICE];
}

/* voice state: s[0] s[1] the first SVF, s[4] s[5] the second (TYPE 4P), s[2] the filter envelope's stage, s[3] its
 * value (Q24), s[6] the noise generator, s[7] SYNC: a restart a sample ago (1 + the step, 0 none) */
static void sem_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->s[0] = v->s[1] = v->s[4] = v->s[5] = 0;
    v->s[2] = 1;
    v->s[3] = 0;
    v->s[7] = 0;
    if (!v->s[6])
        v->s[6] = 0x3C6EF372 + (int32_t)v->age * 2654435;
    v->ph[1] = v->ph[0] + 0x5A000000u;
}

/* the filter envelope, once per block: ADSR on EDIT 2 (ENV_LIN / ENV_EXP as the track's ENV) */
static int32_t sem_fenv(const track_t *t, voice_t *v)
{
    const int16_t *p = t->p;
    int32_t e = v->s[3], sus = (int32_t)p[P_E6] << 17;
    if (!v->gate && v->s[2] != 3)
        v->s[2] = 3;
    switch (v->s[2]) {
    case 1:
        e += (int32_t)ENV_LIN[p[P_E4] & 127];
        if (e >= (1 << 24)) {
            e = 1 << 24;
            v->s[2] = 2;
        }
        break;
    case 2:
        e += mulq16(sus - e, ENV_EXP[p[P_E5] & 127]);
        break;
    default:
        e -= mulq16(e, ENV_EXP[p[P_E7] & 127]);
        break;
    }
    v->s[3] = e;
    return e >> 9;                                    /* Q15 */
}

static inline uint32_t sem_inc(int32_t pitch16, int32_t ct, int32_t fine, uint32_t range)
{
    uint32_t inc = cents_inc(pitch16, ct, fine);
    if (range == 0u)
        inc >>= 1;
    else if (range == 2u)
        inc = inc > 0x70000000u ? 0x70000000u : inc << 1;
    return inc;
}

/* one sample of a wave: pw the pulse width as a phase offset */
static inline int32_t sem_wave(uint32_t w, uint32_t ph, uint32_t inc, uint32_t pw)
{
    switch (w) {
    case SW_PLS:
        return osc_pulse(ph, inc, pw);
    case SW_BOTH:
        return (osc_saw(ph, inc) + osc_pulse(ph, inc, pw)) >> 1;
    case SW_TRI:
        return osc_tri(ph);
    default:
        return osc_saw(ph, inc);
    }
}

static void sem_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t i;
    uint32_t w1 = (uint32_t)SXP(t, SX_O1WAV) % 4u, w2 = (uint32_t)SXP(t, SX_O2WAV) % 4u;
    uint32_t r1 = (uint32_t)SXP(t, SX_O1RNG) % 3u, r2 = (uint32_t)SXP(t, SX_O2RNG) % 3u;
    int32_t l1 = SXP(t, SX_O1LVL) * 258, l2 = SXP(t, SX_O2LVL) * 258, ln = SXP(t, SX_NOISE) * 200;   /* Q15 */
    int32_t pwm = (SXP(t, SX_PWM) * (m->shape - (64 << 8))) >> 7;      /* the LFO's shape swing, scaled by PWM */
    uint32_t pw1 = (uint32_t)clamp((SXP(t, SX_O1PW) << 8) + pwm, 6 << 8, 121 << 8) << 16;   /* 5 .. 95 % */
    uint32_t pw2 = (uint32_t)clamp((SXP(t, SX_O2PW) << 8) + pwm, 6 << 8, 121 << 8) << 16;
    int sync = SXP(t, SX_SYNC) != 0, four = p[P_E3] == 1, bp = p[P_E3] == 2;
    int32_t xmod = SXP(t, SX_XMOD), fenv, cut, mode = p[P_E2], k, m1, m2;
    uint32_t inc1 = sem_inc(m->pitch16, 0, m->fine, r1);
    uint32_t inc2 = sem_inc(m->pitch16, ladder_cents(SXP(t, SX_O2FRQ)) + SXP(t, SX_DTN), m->fine, r2);
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1];
    int32_t a1 = v->s[0], a2 = v->s[1], b1 = v->s[4], b2 = v->s[5], nst = v->s[6], jl = v->s[7];
    int32_t *dcp = sem_dc(t, v), dc = *dcp;
    tsvf_t f, g;

    fenv = sem_fenv(t, v);
    /* the cutoff in parameter steps (Q8): CUTOFF, the track's modulation, KBD (HALF, FULL), the filter envelope's
     * AMOUNT (both ways, up to ~60 steps: four octaves) */
    cut = (p[P_E0] << 8) + m->cutoff;
    cut += (SXP(t, SX_KBD) * ((m->pitch16 - 60 * 16) * 299 / 16)) >> 1;
    cut += (fenv * SXP(t, SX_FAMT) * 60) >> 13;
    if (xmod)                                         /* X-MOD: OSC 2's saw into the cutoff, +-32 steps at full */
        cut += ((((int32_t)(ph1 >> 16) - 32768) * xmod) >> 9);
    cut = clamp(cut, 0, 127 << 8);
    k = 8192 - p[P_E1] * 6600 / 127;                  /* RESONANCE: the damping 2.0 .. 0.39 (short of oscillation) */
    tsvf_coef_k(&f, cut, k);
    if (four)
        tsvf_coef_k(&g, cut, 8192 - p[P_E1] * 3000 / 127);   /* the second pole pair: a gentler peak */
    /* MODE: LOW-PASS (0) .. NOTCH (64) .. HIGH-PASS (127) as a mix of the two outputs */
    m2 = clamp(mode, 0, 127) * 258;
    m1 = 32767 - m2;
    for (i = 0; i < n; i++) {
        int32_t x1, x2, s, nz, v1, v2, v3, y;
        if (sync && jl) {                             /* OSC 2 just restarted: the step was jl - 1, not a whole cycle */
            x2 = (int32_t)(ph1 >> 16) - 32768 - ((blep(ph1, inc2) * ((jl - 1) >> 1)) >> 15);
            if (w2 != SW_SAW)
                x2 = sem_wave(w2, ph1, inc2, pw2);    /* (the other waves restart without the BLEP's scaling) */
            jl = 0;
        } else {
            x2 = sem_wave(w2, ph1, inc2, pw2);
        }
        x1 = sem_wave(w1, ph0, inc1, pw1);
        ph0 += inc1;
        ph1 += inc2;
        if (sync && ph0 < inc1) {                     /* OSC 1 wrapped: OSC 2 restarts at the same point */
            uint32_t fr = ph0 / ((inc1 >> 16) | 1u), at = ph1 - fr * (inc2 >> 16);
            jl = (int32_t)(at >> 16) + 1;
            ph1 = fr * (inc2 >> 16);
        }
        nz = ln ? mulq15((int32_t)(noise32(&nst) >> 17) - 16384, ln) : 0;
        s = (mulq15(x1, l1) + mulq15(x2, l2) + nz) >> 2;   /* the mixer: a full oscillator a quarter scale into the filter
                                                            * (ANALOG's level at its MIX: tests/sem_test.c levels) */
        s = soft_knee(s, 12000);                      /* the input stage: two full oscillators and noise saturate softly */
        v3 = s - a2;
        v1 = (f.a1 * a1 + f.a2 * v3) >> 13;
        v2 = a2 + ((f.a2 * a1 + f.a3 * v3) >> 13);
        a1 = clamp(2 * v1 - a1, -150000, 150000);
        a2 = clamp(2 * v2 - a2, -150000, 150000);
        if (bp)                                       /* BAND-PASS (the switch): twice the SVF's band output (it peaks at
                                                       * 1/k: level-matched with the low-pass on a saw) */
            y = (k * v1) >> 11;
        else                                          /* LP .. NOTCH .. HP: notch = in - k v1 = LP + HP */
            y = mulq15(v2, m1) + mulq15(s - ((k * v1) >> 12) - v2, m2);
        if (four) {                                   /* TYPE 4P: a second low-pass after the first */
            v3 = y - b2;
            v1 = (g.a1 * b1 + g.a2 * v3) >> 13;
            v2 = b2 + ((g.a2 * b1 + g.a3 * v3) >> 13);
            b1 = clamp(2 * v1 - b1, -150000, 150000);
            b2 = clamp(2 * v2 - b2, -150000, 150000);
            y = v2;
        }
        dc += (y * 256 - dc) >> 11;                   /* ~3 Hz high-pass: a synced saw's offset, as the original's AC
                                                       * coupled output */
        y -= dc >> 8;
        y = clamp(soft_knee(y, 10000) << 1, -32000, 32000);   /* (a voice stays under 2 x VOICE_FS: HP at full drive) */
        out[i] += voice_amp(y, m, i) << 1;
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->s[0] = a1;
    v->s[1] = a2;
    v->s[4] = b1;
    v->s[5] = b2;
    v->s[6] = nst;
    v->s[7] = jl;
    *dcp = dc;
}

typedef struct {
    int8_t x[SX_COUNT];
} sem_xp_t;

/*                     E: CUT RES MODE TYPE  F.ATK F.DEC F.SUS F.REL
 *                     X: O1WAV O1PW O1RNG O1LVL  O2WAV O2PW O2RNG O2LVL  O2FRQ DTN SYNC NOISE  FAMT KBD XMOD PWM */
static const preset_t SEM_PRESETS[] = {
    {"OB BRASS", {52, 30, 0, 1, 20, 70, 70, 50}, {12, 70, 110, 50}, 0, 0, FX(0, 20, 16, 36)},
    {"JUMP STAB", {70, 20, 0, 1, 0, 64, 40, 40}, {0, 60, 100, 40}, 0, 0, FX(0, 30, 20, 40)},
    {"OBX PAD", {46, 24, 0, 0, 60, 90, 100, 90}, {70, 90, 115, 95}, 0, 0, FX(0, 50, 20, 70)},
    {"STRINGS 8", {58, 10, 20, 0, 50, 80, 110, 80}, {60, 90, 120, 85}, 0, 0, FX(0, 60, 24, 60)},
    {"SEM BASS", {38, 40, 0, 1, 0, 60, 30, 30}, {0, 70, 100, 30}, 0, 1, FX(0, 0, 8, 10)},
    {"NOTCH SWEEP", {60, 50, 64, 0, 80, 100, 90, 90}, {40, 90, 120, 90}, 0, 0, FX(0, 40, 30, 60)},
    {"SYNC LEAD", {76, 36, 0, 0, 10, 72, 60, 50}, {0, 70, 110, 45}, 0, 1, FX(10, 0, 40, 30)},
    {"PWM PAD", {54, 18, 10, 0, 70, 90, 110, 95}, {80, 90, 118, 100}, 0, 0, FX(0, 60, 24, 70)},
    {"UNISON LEAD", {66, 30, 0, 0, 4, 70, 80, 50}, {2, 70, 110, 50}, 0, 1, FX(0, 20, 40, 30)},
    {"BP PLUCK", {64, 90, 0, 2, 0, 64, 0, 40}, {0, 70, 0, 40}, 0, 0, FX(0, 20, 50, 30)},
    {"FILM PAD", {40, 30, 0, 1, 90, 110, 100, 110}, {100, 100, 120, 115}, 0, 0, FX(0, 40, 30, 90)},
    {"POLY BASS", {44, 34, 0, 1, 0, 56, 50, 36}, {0, 66, 90, 34}, 0, 0, FX(0, 0, 10, 16)},
    {"SYNC BELL", {84, 20, 0, 0, 0, 90, 0, 90}, {0, 100, 0, 90}, 0, 0, FX(0, 30, 40, 60)},
    {"TRI ORGAN", {96, 6, 0, 0, 0, 40, 127, 20}, {0, 30, 127, 20}, 0, 0, FX(0, 20, 0, 30)},
    {"DRONE", {36, 56, 40, 0, 110, 110, 90, 110}, {110, 110, 127, 112}, 0, 0, FX(0, 40, 40, 90)},
    {"ARP PLUCK", {62, 40, 0, 0, 0, 48, 0, 28}, {0, 52, 0, 30}, 0, 0, FX(0, 20, 50, 30)},
};
static const sem_xp_t SEM_XPV[NELEM(SEM_PRESETS)] = {
    {{SW_SAW, 64, 1, 127, SW_SAW, 64, 1, 110, 0, 6, 0, 0, 36, 1, 0, 0}},            /* OB BRASS */
    {{SW_BOTH, 50, 1, 127, SW_SAW, 64, 1, 110, 0, 10, 0, 0, 30, 1, 0, 0}},          /* JUMP STAB */
    {{SW_SAW, 64, 1, 120, SW_PLS, 40, 1, 110, 0, 12, 0, 0, 10, 1, 0, 50}},          /* OBX PAD */
    {{SW_SAW, 64, 1, 127, SW_SAW, 64, 2, 70, 0, 14, 0, 0, 0, 1, 0, 0}},             /* STRINGS 8 */
    {{SW_SAW, 64, 0, 127, SW_PLS, 64, 0, 100, 0, 4, 0, 0, 40, 1, 0, 0}},            /* SEM BASS */
    {{SW_SAW, 64, 1, 127, SW_SAW, 64, 1, 110, 0, 10, 0, 10, 10, 1, 0, 0}},          /* NOTCH SWEEP */
    {{SW_SAW, 64, 1, 100, SW_SAW, 64, 1, 127, 30, 0, 1, 0, 40, 1, 0, 0}},           /* SYNC LEAD */
    {{SW_PLS, 60, 1, 127, SW_PLS, 50, 1, 120, 0, 10, 0, 0, 0, 1, 0, 90}},           /* PWM PAD */
    {{SW_SAW, 64, 1, 127, SW_SAW, 64, 1, 120, 0, 12, 0, 0, 20, 1, 0, 0}},           /* UNISON LEAD */
    {{SW_SAW, 64, 1, 127, SW_SAW, 64, 1, 110, 0, 8, 0, 0, 50, 2, 0, 0}},            /* BP PLUCK */
    {{SW_SAW, 64, 0, 110, SW_TRI, 64, 1, 110, 0, 8, 0, 6, 20, 1, 0, 30}},           /* FILM PAD */
    {{SW_BOTH, 60, 0, 127, SW_SAW, 64, 0, 90, 0, 6, 0, 0, 30, 1, 0, 0}},            /* POLY BASS */
    {{SW_SAW, 64, 1, 90, SW_SAW, 64, 1, 127, 44, 0, 1, 0, 50, 2, 0, 0}},            /* SYNC BELL */
    {{SW_TRI, 64, 1, 127, SW_TRI, 64, 2, 100, 0, 2, 0, 0, 0, 2, 0, 0}},             /* TRI ORGAN */
    {{SW_SAW, 64, 0, 127, SW_SAW, 64, 0, 110, 56, 20, 0, 20, 6, 1, 0, 0}},          /* DRONE: a fifth */
    {{SW_SAW, 64, 1, 127, SW_PLS, 40, 1, 100, 0, 8, 0, 0, 44, 1, 0, 0}},            /* ARP PLUCK */
};

static void sem_load(track_t *t, int32_t pi)
{
    uint32_t k;
    if (pi < 0)
        return;
    for (k = 0; k < SX_COUNT; k++)
        t->xp[k] = SEM_XPV[(uint32_t)pi % NELEM(SEM_XPV)].x[k];
}
static const char *const SEM_PAGES[] = {"OSC 1", "OSC 2", "MIX", "MOD"};
static const xp_info_t SEM_XPI = {SEM_X, SX_COUNT, SEM_PAGES, sem_load};
_Static_assert(SX_COUNT <= NXP && (SX_COUNT + 3u) / 4u == NELEM(SEM_PAGES), "SEM: its pages and values");
_Static_assert(NELEM(SEM_XPV) == NELEM(SEM_PRESETS), "SEM: a row of extra values per preset");

static const engine_t ENG_SEM = {
    .name = "SEM",
    .page_title = {"FILTER", "F ENV"},
    .edit = {
        {"CUTOFF", F_CUTOFF, 0, 127, 60, 0, 0},
        {"RESO", F_PCT, 0, 127, 20, 0, 0},
        {"MODE", F_PCT, 0, 127, 0, 0, 0},             /* 0 LP .. 64 NOTCH .. 127 HP */
        {"TYPE", F_ENUM, 0, 2, 0, N_SEM_TYPE, 0},
        {"F ATK", F_TIME, 0, 127, 10, 0, 0},
        {"F DEC", F_TIME, 0, 127, 70, 0, 0},
        {"F SUS", F_PCT, 0, 127, 70, 0, 0},
        {"F REL", F_TIME, 0, 127, 50, 0, 0},
    },
    .presets = SEM_PRESETS,
    .npresets = NELEM(SEM_PRESETS),
    .note_on = sem_note_on,
    .render = sem_render,
    .knob = {P_E0, P_E1, P_E2, P_GLIDE},
    .keep = 0x3B,                /* the two filters (s0 s1 s4 s5), the filter envelope (s3) */
    .mem = sizeof(sem_part_t),
};
