/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Khantipol Kasemsant; built on Clipper (isod89 / 3dSam): its JUNO engine and BBD chorus */
/* CHORD-D (Forge Five Phase 3): the chord machine of dub techno, after the classic 1980s one-DCO polysynth: one DCO
 * (saw, a pulse whose width moves, both), a square sub an octave down, noise; a high-pass in four positions; a 24 dB
 * resonant low-pass; one envelope for the filter and the amplifier (or the amplifier gated); the stereo bucket-
 * brigade chorus I / II / I+II (fx.c juno_chorus: its tone, hiss and width from here); and the chord memory: a chord
 * learnt from the keys plays whole from one key (voice.c trk_note_on1). Clipper's JUNO is its base (eng_juno.c: the
 * same DCO, filters and chorus); the panel is the spec's (v0.2 section 3.2):
 *   EDIT 1  CUTOFF  RESO  PWM  CHORUS        EDIT 2  SUB  HPF  ENV  RELEASE
 *   CUTOFF RESO  the 24 dB ladder (dsp.c), keyboard tracking KBD (half by default)
 *   PWM     the pulse's width, 50 % .. ~5 %; the LFO's SHAPE (LFO DEST SHP), MW / AT SHP and the matrix move it
 *   CHORUS  OFF, I (0.5 Hz, wide), II (0.8 Hz, deeper), I+II (9.75 Hz, shallow)
 *   SUB     the square an octave down        HPF  0 a little bass boost, 1 flat, 2 / 3 a high-pass at ~240 / ~720 Hz
 *   ENV     the envelope's depth on the cutoff (POL: up or down)
 *   RELEASE the envelope's release: the same value as the ENV page's R (each follows the other: chordd_block)
 * The engine patch (engine_xp.c, four XP pages):
 *   DCO      WAVE (SAW, PLS, S+P), NOISE, KBD (the filter's key tracking 0 .. full), POL (+ / -: the ENV's sign)
 *   VCA BBD  VCA (ENV: the envelope; GATE: the amplifier open while the key is held, the envelope on the filter
 *            only; its release still RELEASE), HISS (the BBD's noise), TONE (its low-pass), WIDTH (the stereo)
 *   CHORD    CHORD (OFF; MEM: every key plays the chord; LEARN: hold a chord, let go: it is learnt, MEM),
 *            N2 .. N4: the chord's notes above the key (- none, m2 .. 2OCT)
 *   CHORD 2  N5 .. N7, VEL>F (the velocity on the cutoff)
 * Voice state: ph[0] the DCO, ph[1] the sub, ph[2] the noise; s[0..3] the ladder, s[4] the high-pass's low-pass, s[5]
 * the bass boost's, s[6] GATE: the envelope where the release began, s[7] the envelope (the filter's, in both VCA
 * modes) */
enum { CW_SAW, CW_PLS, CW_SP };
static const char *const N_CDWAVE[] = {"SAW", "PLS", "S+P"};
static const char *const N_CDHPF[] = {"0", "1", "2", "3"};
static const char *const N_CDCHOR[] = {"OFF", "I", "II", "I+II"};
static const char *const N_CDPOL[] = {"+", "-"};
static const char *const N_CDVCA[] = {"ENV", "GATE"};
enum { CM_OFF, CM_MEM, CM_LEARN };
static const char *const N_CDCHORD[] = {"OFF", "MEM", "LEARN"};
#define CD_IVL_MAX 24
static const char *const N_CDIVL[CD_IVL_MAX + 1] = {"-",   "m2",  "M2",   "m3", "M3",  "4",   "TT",  "5",  "m6",
                                                    "M6",  "m7",  "M7",   "OCT", "m9", "9",   "m10", "M10", "11",
                                                    "#11", "12",  "m13",  "13", "m14", "M14", "2OCT"};

enum {
    CX_WAVE, CX_NOISE, CX_KBD, CX_POL,                 /* page DCO */
    CX_VCA, CX_HISS, CX_TONE, CX_WIDTH,                /* page VCA BBD */
    CX_CHORD, CX_N2, CX_N3, CX_N4,                     /* page CHORD */
    CX_N5, CX_N6, CX_N7, CX_VELF,                      /* page CHORD 2 */
    CX_COUNT
};
#define CD_NOTES 6u                                    /* N2 .. N7: the chord's notes above the key */
#define CXP(t, k) ((t)->xp[k])

static const param_desc_t CHORDD_X[CX_COUNT] = {
    {"WAVE", F_ENUM, 0, 2, CW_SAW, N_CDWAVE, 0},
    {"NOISE", F_PCT, 0, 127, 0, 0, 0},
    {"KBD", F_PCT, 0, 127, 64, 0, 0},
    {"POL", F_ENUM, 0, 1, 0, N_CDPOL, 0},
    {"VCA", F_ENUM, 0, 1, 0, N_CDVCA, 0},
    {"HISS", F_PCT, 0, 127, 40, 0, 0},
    {"TONE", F_PCT, 0, 127, 100, 0, 0},
    {"WIDTH", F_PCT, 0, 127, 127, 0, 0},
    {"CHORD", F_ENUM, 0, 2, CM_OFF, N_CDCHORD, 0},
    {"N2", F_ENUM, 0, CD_IVL_MAX, 0, N_CDIVL, 0},
    {"N3", F_ENUM, 0, CD_IVL_MAX, 0, N_CDIVL, 0},
    {"N4", F_ENUM, 0, CD_IVL_MAX, 0, N_CDIVL, 0},
    {"N5", F_ENUM, 0, CD_IVL_MAX, 0, N_CDIVL, 0},
    {"N6", F_ENUM, 0, CD_IVL_MAX, 0, N_CDIVL, 0},
    {"N7", F_ENUM, 0, CD_IVL_MAX, 0, N_CDIVL, 0},
    {"VEL>F", F_PCT, 0, 127, 0, 0, 0},
};

/* the chord the part plays from one key: a mask of the intervals above it (bit k: k semitones up), 0 = the key alone */
static uint32_t chordd_mask(const track_t *t)
{
    uint32_t k, m = 0;
    if (CXP(t, CX_CHORD) != CM_MEM)
        return 0;
    for (k = 0; k < CD_NOTES; k++) {
        int32_t i = CXP(t, CX_N2 + k);
        if (i > 0 && i <= CD_IVL_MAX)
            m |= 1u << i;
    }
    return m;
}

/* LEARN: the notes of a chord held (lowest first) become the memory: N2 .. from the lowest, the rest cleared; MEM */
static void chordd_learn(track_t *t, const uint8_t *notes, uint32_t n)
{
    uint32_t i, j, k = 0, lo = 127;
    for (i = 0; i < n; i++)
        if (notes[i] < lo)
            lo = notes[i];
    for (i = 0; i < CD_NOTES; i++)
        CXP(t, CX_N2 + i) = 0;
    for (i = 1; i <= CD_IVL_MAX && k < CD_NOTES; i++)  /* (ascending: N2 the lowest interval) */
        for (j = 0; j < n; j++)
            if (notes[j] == lo + i) {
                CXP(t, CX_N2 + k) = (int16_t)i;
                k++;
                break;
            }
    if (k)
        CXP(t, CX_CHORD) = CM_MEM;
}

static void chordd_note_on(track_t *t, voice_t *v)
{
    (void)t;
    if (!v->env && !v->env_out) {                    /* from silence: the filters clear (a DCO: its phase runs on) */
        v->s[0] = v->s[1] = v->s[2] = v->s[3] = v->s[4] = v->s[5] = 0;
        if (!v->ph[2])
            v->ph[2] = 0x2545F491u + v->note * 2654435761u;
    }
    v->s[6] = 0;
}

/* the amplifier: ENV the envelope; GATE open while the key is held, the release the envelope's from where it was.
 * The envelope itself kept in s[7] for the filter */
static int32_t chordd_amp(track_t *t, voice_t *v, int32_t adsr)
{
    v->s[7] = adsr;
    if (!CXP(t, CX_VCA))
        return adsr;
    if (v->stage == 1u || v->stage == 2u) {
        v->s[6] = 0;
        return 32767;
    }
    if (!v->s[6])
        v->s[6] = adsr > 0 ? adsr : 1;
    return (int32_t)(((int64_t)adsr * 32767) / v->s[6]);
}

/* RELEASE (EDIT 2) and the ENV page's R are one value: the one that moved is copied to the other (a sound load sets
 * both) */
static int16_t cd_last_e7[NTRK], cd_last_rel[NTRK];
static void chordd_block(track_t *t)
{
    uint32_t i = (uint32_t)(t - trk) % NTRK;
    if (t->p[P_E7] != cd_last_e7[i])
        t->p[P_REL] = t->p[P_E7];
    else if (t->p[P_REL] != cd_last_rel[i])
        t->p[P_E7] = t->p[P_REL];
    cd_last_e7[i] = t->p[P_E7];
    cd_last_rel[i] = t->p[P_REL];
}

static void chordd_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t i, ph = v->ph[0], phs = v->ph[1], nz = v->ph[2], inc = m->inc, incs = inc >> 1;
    uint32_t wave = (uint32_t)clamp(CXP(t, CX_WAVE), 0, 2), hpf = (uint32_t)clamp(p[P_E5], 0, 3);
    int32_t w = clamp(p[P_E2] * 256 + (m->shape - (64 << 8)) * 2, 0, 127 * 256);   /* 0 square .. 127 * 256 narrow */
    uint32_t pw = 0x80000000u - (uint32_t)w * 59000u;                               /* 50 % .. ~5 % */
    int32_t sub = p[P_E4] * 200, nois = CXP(t, CX_NOISE) * 160, hp = v->s[4], bb = v->s[5];
    int32_t env = (int32_t)(((int64_t)v->s[7] * p[P_E6]) >> 7), cut;
    int32_t ls[4] = {v->s[0], v->s[1], v->s[2], v->s[3]};
    ladder_t lad;
    if (CXP(t, CX_POL))
        env = -env;
    cut = (p[P_E0] << 8) + m->cutoff + env + (((v->pitch16 - 60 * 16) * CXP(t, CX_KBD)) >> 3) +
          ((((int32_t)v->vel - 64) * CXP(t, CX_VELF)) >> 1);
    ladder_coef(&lad, cut, p[P_E1]);
    for (i = 0; i < n; i++) {
        int32_t s, y, a;
        if (wave == CW_SAW)
            s = osc_saw(ph, inc);
        else if (wave == CW_PLS)
            s = osc_pulse(ph, inc, pw);
        else
            s = (osc_saw(ph, inc) + osc_pulse(ph, inc, pw)) * 5 >> 3;
        if (sub)
            s += mulq15(osc_pulse(phs, incs, 0x80000000u), sub);
        if (nois) {
            nz = nz * 1664525u + 1013904223u;
            s += mulq15((int32_t)(nz >> 16) - 32768, nois);
        }
        ph += inc;
        phs += incs;
        s = s * 5 >> 3;                              /* (four sources: headroom) */
        if (hpf != 1u) {                             /* 0: bass boost (a low shelf ~120 Hz); 2, 3: a one-pole high-pass */
            if (!hpf) {
                bb += ((s - bb) * 560) >> 15;
                s += bb >> 1;
            } else {
                hp += ((s - hp) * (hpf == 2u ? 1120 : 3360)) >> 15;
                s -= hp;
            }
        }
        y = ladder_tick(&lad, s, ls) >> 1;
        a = y < 0 ? -y : y;
        if (a > 16000) {
            a = 16000 + (softclip((a - 16000) * 2) >> 1);
            y = y < 0 ? -a : a;
        }
        out[i] += mulq15(mulq15(y << 1, amp_at(m, i)), VOICE_FS) << 1;
    }
    v->ph[0] = ph;
    v->ph[1] = phs;
    v->ph[2] = nz;
    v->s[0] = ls[0], v->s[1] = ls[1], v->s[2] = ls[2], v->s[3] = ls[3];
    v->s[4] = hp;
    v->s[5] = bb;
}

/*                 E: CUTOFF RESO PWM CHORUS  SUB HPF ENV RELEASE (= the ADSR's R); the filter envelope is ENV (fenv 0)
 * Every preset's level trim (P_ED_FX, 1/2 dB, in the mix): each key, chord or not, near one note of JUNO's pad
 * (tests/chordd_test.c levels) */
static const preset_t CHORDD_PRESETS[] = {
    {"DUB STAB", {52, 34, 0, 1, 0, 1, 78, 46}, {0, 58, 0, 46}, 0, 0, FX(0, 0, 62, 34)},
    {"ECHO CHORD", {56, 24, 44, 2, 20, 2, 52, 64}, {0, 76, 10, 64}, 0, 0, FX(0, 0, 74, 52),
     XP(P_LRATE + 1, 30, P_LD_SHP + 1, 24)},
    {"DEEP PAD", {64, 16, 36, 1, 30, 1, 22, 100}, {84, 100, 112, 100}, 0, 0, FX(0, 0, 30, 70),
     XP(P_ED_FX + 1, -12, P_LRATE + 1, 22, P_LD_SHP + 1, 36, P_VINT + 1, 24)},
    {"SUB PULSE", {40, 30, 60, 0, 100, 0, 56, 30}, {0, 62, 50, 30}, 0, 1, FX(0, 0, 10, 8),
     XP(P_ED_FX + 1, -4, P_GLIDE + 1, 8)},
    {"CHOR STRINGS", {92, 10, 0, 2, 0, 2, 8, 96}, {80, 100, 120, 96}, 0, 0, FX(0, 0, 14, 56),
     XP(P_ED_FX + 1, 4, P_LRATE + 1, 34, P_VINT + 1, 20)},
};

/*                      X: WAVE NOISE KBD POL  VCA HISS TONE WIDTH  CHORD N2 N3 N4  N5 N6 N7 VEL>F */
typedef struct {
    int8_t x[CX_COUNT];
} chordd_xp_t;
static const chordd_xp_t CHORDD_XP[NELEM(CHORDD_PRESETS)] = {
    /* DUB STAB: a minor seventh from one key, short and dark, into the echo: the dub techno chord */
    {{CW_SAW, 0, 64, 0, 0, 40, 96, 127, CM_MEM, 3, 7, 10, 0, 0, 0, 30}},
    /* ECHO CHORD: a minor ninth on the pulse, chorus II, a longer tail for the delay to carry */
    {{CW_PLS, 0, 64, 0, 0, 48, 90, 127, CM_MEM, 3, 7, 10, 14, 0, 0, 20}},
    /* DEEP PAD: an open minor ninth (fifth, seventh, ninth, tenth), slow, the pulse width breathing */
    {{CW_SP, 0, 48, 0, 0, 40, 100, 127, CM_MEM, 7, 10, 14, 15, 0, 0, 0}},
    /* SUB PULSE: a mono bass, the pulse and the sub, the amplifier gated: the filter envelope alone shapes it */
    {{CW_PLS, 0, 80, 0, 1, 0, 100, 127, CM_OFF, 0, 0, 0, 0, 0, 0, 24}},
    /* CHOR STRINGS: saw strings, high-passed, chorus II wide: play the chords by hand */
    {{CW_SAW, 0, 64, 0, 0, 56, 100, 127, CM_OFF, 0, 0, 0, 0, 0, 0, 0}},
};

static void chordd_load(track_t *t, int32_t pi)
{
    uint32_t k;
    if (pi < 0)
        return;
    for (k = 0; k < CX_COUNT; k++)
        t->xp[k] = CHORDD_XP[(uint32_t)pi % NELEM(CHORDD_XP)].x[k];
}
static const char *const CHORDD_PAGES[] = {"DCO", "VCA BBD", "CHORD", "CHORD 2"};
static const xp_info_t CHORDD_XPI = {CHORDD_X, CX_COUNT, CHORDD_PAGES, chordd_load};
_Static_assert(CX_COUNT <= NXP && (CX_COUNT + 3u) / 4u == NELEM(CHORDD_PAGES), "CHORD-D: its pages and values");

static const engine_t ENG_CHORDD = {
    .name = "CHORD-D",
    .page_title = {"VCF", "SUB ENV"},
    .edit = {
        {"CUTOFF", F_CUTOFF, 0, 127, 70, 0, 0},
        {"RESO", F_PCT, 0, 127, 20, 0, 0},
        {"PWM", F_PCT, 0, 127, 30, 0, 0},
        {"CHORUS", F_ENUM, 0, 3, 1, N_CDCHOR, 0},
        {"SUB", F_PCT, 0, 127, 0, 0, 0},
        {"HPF", F_ENUM, 0, 3, 1, N_CDHPF, 0},
        {"ENV", F_PCT, 0, 127, 40, 0, 0},
        {"RELEASE", F_TIME, 0, 127, 50, 0, 0},
    },
    .presets = CHORDD_PRESETS,
    .npresets = NELEM(CHORDD_PRESETS),
    .fil_page = 0,
    .note_on = chordd_note_on,
    .render = chordd_render,
    .color = 0x45FD,
    .macro = {P_E0, P_E1, P_E6, P_E3},
    .poly = 0,
    .amp = chordd_amp,
    .block = chordd_block,
};
