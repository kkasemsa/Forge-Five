/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Khantipol Kasemsant; built on Clipper (isod89 / 3dSam), SLOOP and Felucca (C) 2026 Leo Kuroshita,
 * Hügelton Instruments */
/* BRASS (Forge Five Phase 4): pads and brass, after the classic two-oscillator American polysynths of the late 1970s
 * and early 1980s (the state-variable voice module and the programmable five-voice keyboards). Written from their
 * published behaviour; no code or data of any other synth. Ported from the Forge Five 0.2 engine on the Felucca base
 * (archive/felucca-base), with poly-mod and AGE added (spec v0.2 section 3.3, v0.3 section 4).
 *
 *   OSC A, OSC B   SAW, PULSE (width), both, or triangle; RANGE 16' 8' 4'; B's FREQ (+-7 semitones, fine near the
 *                  centre) and DETUNE (cents); SYNC: A hard-synced to B (B the master: the classic sync sweep is A's
 *                  pitch moved by the poly-mod)
 *   MIXER          A, B, NOISE
 *   POLY-MOD       two sources, OSC B (PMOD, EDIT 2) and the filter envelope (PM ENV), into A's frequency (audio-rate
 *                  FM), A's pulse width, and the cutoff (every 8 samples): each destination on or off
 *   FILTER         TYPE 2P: the 12 dB state-variable filter with MODE, low-pass .. notch .. high-pass continuously;
 *                  4P: a second low-pass SVF after the first (24 dB; the old engine's, spec v0.3 section 4: a fraction
 *                  of the cost of dsp.c's 64-bit ladder); BP: the SVF's band-pass. RESO short of oscillation on 2P,
 *                  as the original's; ENV (both ways) from the filter envelope (F ENV page), KBD OFF HALF FULL, VEL>F
 *   AGE            per-voice slop, one knob: each voice's oscillators drift slowly on their own (a random walk, up
 *                  to +-10 cents), sit a little off tune and off cutoff, and its filter envelope runs a little faster
 *                  or slower; the offsets belong to the voice (its number), so the same voice ages the same way
 *   The loudness envelope is the track's ENV page; ATTACK and RELEASE (EDIT 2) are its A and R (one value each)
 *
 *   EDIT 1  CUTOFF  RESO  ENV  DETUNE          EDIT 2  PMOD  TYPE  ATTACK  RELEASE
 *   XP      OSC A (WAVE PW RANGE LEVEL), OSC B (WAVE PW RANGE LEVEL), MIX (B FREQ, SYNC, NOISE, PWM: the LFO on both
 *           widths), F ENV (A D S R), FILTER (MODE, KBD, VEL>F, AGE), POLYMOD (PM ENV, -> FREQ A, -> PW A, -> FILT)
 * Voice state: ph[0] A, ph[1] B; s[0] s[1] the SVF, s[4] s[5] the second (4P); s[2]
 * the filter envelope's stage, s[3] its value (Q24); s[6] the noise; s[7] SYNC: A restarted a sample ago (1 + the step
 * it made, 0 none). In the part's arena (brass_vx_t): the DC blocker, the drift walks, the voice's random state */

static const char *const N_BR_WAVE[] = {"SAW", "PLS", "BOTH", "TRI"};
static const char *const N_BR_RANGE[] = {"16'", "8'", "4'"};
static const char *const N_BR_TYPE[] = {"2P", "4P", "BP"};
static const char *const N_BR_KBD[] = {"OFF", "HALF", "FULL"};
static const char *const N_BR_ONOFF[] = {"OFF", "ON"};

enum { BW_SAW, BW_PLS, BW_BOTH, BW_TRI };
enum { BT_2P, BT_4P, BT_BP };
enum {
    BX_AWAV, BX_APW, BX_ARNG, BX_ALVL,               /* page OSC A */
    BX_BWAV, BX_BPW, BX_BRNG, BX_BLVL,               /* page OSC B */
    BX_BFRQ, BX_SYNC, BX_NOISE, BX_PWM,              /* page MIX */
    BX_FATK, BX_FDEC, BX_FSUS, BX_FREL,              /* page F ENV */
    BX_MODE, BX_KBD, BX_VELF, BX_AGE,                /* page FILTER */
    BX_PMENV, BX_PMFRQ, BX_PMPW, BX_PMFLT,           /* page POLYMOD */
    BX_COUNT
};
#define BXP(t, k) ((t)->xp[k])

static const param_desc_t BRASS_X[BX_COUNT] = {
    {"A WAVE", F_ENUM, 0, 3, BW_SAW, N_BR_WAVE, 0},
    {"A PW", F_PCT, 0, 127, 64, 0, 0},
    {"A RANGE", F_ENUM, 0, 2, 1, N_BR_RANGE, 0},
    {"A LEVEL", F_PCT, 0, 127, 127, 0, 0},
    {"B WAVE", F_ENUM, 0, 3, BW_SAW, N_BR_WAVE, 0},
    {"B PW", F_PCT, 0, 127, 64, 0, 0},
    {"B RANGE", F_ENUM, 0, 2, 1, N_BR_RANGE, 0},
    {"B LEVEL", F_PCT, 0, 127, 110, 0, 0},
    {"B FREQ", F_ST8, -56, 56, 0, 0, "st"},
    {"SYNC", F_ENUM, 0, 1, 0, N_BR_ONOFF, 0},
    {"NOISE", F_PCT, 0, 127, 0, 0, 0},
    {"PWM", F_PCT, 0, 127, 0, 0, 0},
    {"F ATK", F_TIME, 0, 127, 10, 0, 0},
    {"F DEC", F_TIME, 0, 127, 70, 0, 0},
    {"F SUS", F_PCT, 0, 127, 70, 0, 0},
    {"F REL", F_TIME, 0, 127, 50, 0, 0},
    {"MODE", F_PCT, 0, 127, 0, 0, 0},                /* 0 LP .. 64 NOTCH .. 127 HP (TYPE 2P) */
    {"KBD", F_ENUM, 0, 2, 1, N_BR_KBD, 0},
    {"VEL>F", F_PCT, 0, 127, 0, 0, 0},
    {"AGE", F_PCT, 0, 127, 20, 0, 0},
    {"PM ENV", F_BIPCT, -64, 63, 0, 0, 0},
    {"PM FREQ", F_ENUM, 0, 1, 1, N_BR_ONOFF, 0},
    {"PM PW", F_ENUM, 0, 1, 0, N_BR_ONOFF, 0},
    {"PM FILT", F_ENUM, 0, 1, 0, N_BR_ONOFF, 0},
};

/* per voice, in the part's engine arena (engines.c eng_arena_of) */
typedef struct {
    int32_t dc;                      /* the output's DC blocker (Q8) */
    int32_t dr[2], dt[2];            /* AGE: each oscillator's drift and where it walks to (+-8192: +-1) */
    uint32_t rnd;                    /* the voice's random state (0: not started; seeded from the voice's number) */
    uint8_t type;                    /* the filter TYPE the states belong to + 1 (a change: from rest) */
} brass_vx_t;
typedef brass_vx_t brass_part_t[NVOICE];
static brass_vx_t brass_spare;       /* a track outside trk[] (a host test's copy): state of its own */
static brass_vx_t *brass_vx(track_t *t, const voice_t *v)
{
    brass_part_t *a;
    if (t < &trk[0] || t >= &trk[NPART])
        return &brass_spare;
    a = (brass_part_t *)eng_arena_of(t, ENGI_BRASS);
    return a ? &(*a)[(uint32_t)(v - t->v) % NVOICE] : &brass_spare;
}

/* AGE: the voice's own fixed offsets, from its number (a voice card's character): -128 .. 127, three of them */
static inline int32_t brass_card(const track_t *t, const voice_t *v, uint32_t k)
{
    uint32_t h = ((uint32_t)(v - t->v) + 1u) * 2654435761u + (uint32_t)(t - trk) * 40503u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    return (int32_t)((h >> (8u * k)) & 255u) - 128;
}

/* the SVF's coefficients (dsp.c tsvf_coef with the damping given: RESO short of oscillation) */
static inline void brass_svf(tsvf_t *c, int32_t cut, int32_t k)
{
    int32_t i, g, den;
    cut = clamp(cut, 0, 127 << 8);
    i = cut >> 8;
    g = SVF_G[i];
    if (i < 127)
        g += ((SVF_G[i + 1] - g) * (cut & 255)) >> 8;
    den = 4096 + ((g * (g + k)) >> 12);
    c->a1 = (int32_t)((4096u << 13) / (uint32_t)den);   /* (32 bits: the FM-1 has no 64-bit divide) */
    c->a2 = (c->a1 * g) >> 12;
    c->a3 = (c->a2 * g) >> 12;
}

static void brass_note_on(track_t *t, voice_t *v)
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

/* the filter envelope, once per block (ENV_LIN / ENV_EXP as the track's ENV); AGE moves its times a little a voice */
static int32_t brass_fenv(const track_t *t, voice_t *v, int32_t tshift)
{
    int32_t e = v->s[3], sus = (int32_t)BXP(t, BX_FSUS) << 17;
    if (!v->gate && v->s[2] != 3)
        v->s[2] = 3;
    switch (v->s[2]) {
    case 1:
        e += (int32_t)ENV_LIN[clamp(BXP(t, BX_FATK) + tshift, 0, 127)];
        if (e >= (1 << 24)) {
            e = 1 << 24;
            v->s[2] = 2;
        }
        break;
    case 2:
        e += mulq16(sus - e, ENV_EXP[clamp(BXP(t, BX_FDEC) + tshift, 0, 127)]);
        break;
    default:
        e -= mulq16(e, ENV_EXP[clamp(BXP(t, BX_FREL) + tshift, 0, 127)]);
        break;
    }
    v->s[3] = e;
    return e >> 9;                                    /* Q15 */
}

static inline uint32_t brass_inc(int32_t pitch16, int32_t ct, int32_t fine, uint32_t range)
{
    uint32_t inc = cents_inc(pitch16, ct, fine);
    if (range == 0u)
        inc >>= 1;
    else if (range == 2u)
        inc = inc > 0x70000000u ? 0x70000000u : inc << 1;
    return inc;
}

static inline int32_t brass_wave(uint32_t w, uint32_t ph, uint32_t inc, uint32_t pw)
{
    switch (w) {
    case BW_PLS:
        return osc_pulse(ph, inc, pw);
    case BW_BOTH:
        return (osc_saw(ph, inc) + osc_pulse(ph, inc, pw)) >> 1;
    case BW_TRI:
        return osc_tri(ph);
    default:
        return osc_saw(ph, inc);
    }
}

/* AGE, once per block: each oscillator's drift walks toward a target that moves now and then (1 block in 512, ~0.4
 * s), gliding there over ~0.4 s; each voice from its own seed (its number: the same voice wanders the same way) */
static void brass_age(brass_vx_t *x, uint32_t voice)
{
    uint32_t k;
    if (!x->rnd)
        x->rnd = 0x9E3779B9u ^ ((voice + 1u) * 2654435761u);
    for (k = 0; k < 2u; k++) {
        x->rnd = x->rnd * 1664525u + 1013904223u;
        if ((x->rnd >> 22) < 2u)
            x->dt[k] = (int32_t)((x->rnd >> 8) & 0xFFFu) - 2048;   /* -1 .. 1, Q11 */
        x->dr[k] += (x->dt[k] * 4 - x->dr[k]) >> 9;
    }
}
/* the drift in cents: +-10 at full AGE */
static inline int32_t brass_drift_ct(int32_t dr, int32_t age) { return (((dr * age) >> 7) * 10) >> 13; }

static void brass_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t i;
    uint32_t wa = (uint32_t)BXP(t, BX_AWAV) % 4u, wb = (uint32_t)BXP(t, BX_BWAV) % 4u;
    uint32_t ra = (uint32_t)BXP(t, BX_ARNG) % 3u, rb = (uint32_t)BXP(t, BX_BRNG) % 3u;
    int32_t la = BXP(t, BX_ALVL) * 258, lb = BXP(t, BX_BLVL) * 258, ln = BXP(t, BX_NOISE) * 200;   /* Q15 */
    int32_t pwm = (BXP(t, BX_PWM) * (m->shape - (64 << 8))) >> 7;      /* the LFO's shape swing, scaled by PWM */
    int32_t pwa = clamp((BXP(t, BX_APW) << 8) + pwm, 6 << 8, 121 << 8);   /* 5 .. 95 %, Q8 of 127 */
    uint32_t pwb = (uint32_t)clamp((BXP(t, BX_BPW) << 8) + pwm, 6 << 8, 121 << 8) << 16;
    uint32_t type = (uint32_t)clamp(p[P_E5], 0, 2);
    int sync = BXP(t, BX_SYNC) != 0;
    int pm_frq = BXP(t, BX_PMFRQ) != 0, pm_pw = BXP(t, BX_PMPW) != 0, pm_flt = BXP(t, BX_PMFLT) != 0;
    int32_t age = BXP(t, BX_AGE), pmb = p[P_E4] * 258;  /* PMOD: OSC B's share, Q15 */
    int32_t fenv, cut, cut0, mode = clamp(BXP(t, BX_MODE), 0, 127), k, m1, m2, pme, ca, cb, cc;
    brass_vx_t *x = brass_vx(t, v);
    uint32_t inca, incb, pha = v->ph[0], phb = v->ph[1];
    int32_t a1, a2, b1, b2, nst = v->s[6], jl = v->s[7], dc = x->dc, k4;
    tsvf_t f, g;

    if (x->type != type + 1u) {                       /* (TYPE changed: 4P's second filter from rest) */
        v->s[4] = v->s[5] = 0;
        x->type = (uint8_t)(type + 1u);
    }
    a1 = v->s[0], a2 = v->s[1], b1 = v->s[4], b2 = v->s[5];
    /* AGE: the walks, and the voice's fixed offsets (tune +-2 cents, cutoff +-6 steps, envelope times +-4 steps),
     * each scaled by AGE */
    brass_age(x, (uint32_t)(v - t->v));
    ca = brass_card(t, v, 0), cb = brass_card(t, v, 1), cc = brass_card(t, v, 2);
    fenv = brass_fenv(t, v, (cc * age) >> 12);
    inca = brass_inc(m->pitch16, brass_drift_ct(x->dr[0], age) + ((ca * age) >> 13), m->fine, ra);
    incb = brass_inc(m->pitch16, ladder_cents(BXP(t, BX_BFRQ)) + p[P_E3] + brass_drift_ct(x->dr[1], age), m->fine, rb);
    /* the cutoff in parameter steps (Q8): CUTOFF, the track's modulation, KBD, ENV (both ways, ~60 steps at full),
     * VEL>F, AGE's offset (+-6 steps) */
    cut0 = (p[P_E0] << 8) + m->cutoff;
    cut0 += (BXP(t, BX_KBD) * ((m->pitch16 - 60 * 16) * 299 / 16)) >> 1;
    cut0 += (fenv * p[P_E2] * 60) >> 13;
    cut0 += (((int32_t)v->vel - 64) * BXP(t, BX_VELF)) >> 1;
    cut0 += (cb * age * 6) >> 6;
    pme = (fenv * BXP(t, BX_PMENV)) >> 6;            /* PM ENV: the envelope's share, Q15 (both ways) */
    if (pm_flt)
        cut0 += (pme * 40) >> 7;                      /* (+-40 steps, Q8: the envelope's part, the block's; OSC B's
                                                       * below) */
    cut = clamp(cut0, 0, 127 << 8);
    k = 8192 - p[P_E1] * 6600 / 127;                  /* RESO: the damping 2.0 .. 0.39 (short of oscillation) */
    k4 = 8192 - p[P_E1] * 3000 / 127;                 /* (4P's second pair: a gentler peak) */
    brass_svf(&f, cut, k);
    g = f;
    if (type == BT_4P)
        brass_svf(&g, cut, k4);
    m2 = mode * 258;                                  /* MODE: LP (0) .. NOTCH (64) .. HP (127) */
    m1 = 32767 - m2;
    for (i = 0; i < n; i++) {
        int32_t xa, xb, s, nz, v1, v2, v3, y, pmo, pm;
        uint32_t ia = inca, pa = (uint32_t)pwa << 16;
        xb = brass_wave(wb, phb, incb, pwb);
        pmo = mulq15(xb, pmb);                        /* the poly-mod: OSC B's share */
        pm = clamp(pmo + pme, -32768, 32767);         /* .. and the envelope's, Q15 */
        if (pm_frq) {                                 /* A's frequency: x (1 + 2 pm): up to x3, down to x1/4 */
            int32_t d = (int32_t)(((int64_t)inca * pm) >> 14);
            ia = (uint32_t)clamp((int32_t)inca + d, (int32_t)(inca >> 2), 0x70000000);
        }
        if (pm_pw)
            pa = (uint32_t)clamp(pwa + (pm >> 7) * 58, 6 << 8, 121 << 8) << 16;
        if (pm_flt && pmb && !(i & 7u)) {             /* the cutoff, every 8 samples: OSC B's share (the envelope's:
                                                       * in cut0 for the block) */
            int32_t c2 = clamp(cut0 + ((pmo * 40) >> 7), 0, 127 << 8);   /* (+-40 steps, Q8) */
            brass_svf(&f, c2, k);
            if (type == BT_4P)
                brass_svf(&g, c2, k4);
        }
        if (sync && jl) {                             /* A just restarted: the step was jl - 1, not a whole cycle */
            xa = (int32_t)(pha >> 16) - 32768 - ((blep(pha, ia) * ((jl - 1) >> 1)) >> 15);
            if (wa != BW_SAW)
                xa = brass_wave(wa, pha, ia, pa);
            jl = 0;
        } else {
            xa = brass_wave(wa, pha, ia, pa);
        }
        pha += ia;
        phb += incb;
        if (sync && phb < incb) {                     /* B wrapped: A restarts at the same point */
            uint32_t fr = phb / ((incb >> 16) | 1u), at = pha - fr * (ia >> 16);
            jl = (int32_t)(at >> 16) + 1;
            pha = fr * (ia >> 16);
        }
        nz = ln ? mulq15((int32_t)(noise32(&nst) >> 17) - 16384, ln) : 0;
        s = (mulq15(xa, la) + mulq15(xb, lb) + nz) >> 2;   /* the mixer: a full oscillator a quarter scale in */
        s = soft_knee(s, 12000);                      /* two full oscillators and noise saturate softly */
        v3 = s - a2;
        v1 = (f.a1 * a1 + f.a2 * v3) >> 13;
        v2 = a2 + ((f.a2 * a1 + f.a3 * v3) >> 13);
        a1 = clamp(2 * v1 - a1, -150000, 150000);
        a2 = clamp(2 * v2 - a2, -150000, 150000);
        if (type == BT_BP)                            /* the band output, level-matched with the low-pass on a saw */
            y = (k * v1) >> 11;
        else if (type == BT_4P) {                     /* the low-pass into a second one: 24 dB */
            v3 = v2 - b2;
            v1 = (g.a1 * b1 + g.a2 * v3) >> 13;
            v2 = b2 + ((g.a2 * b1 + g.a3 * v3) >> 13);
            b1 = clamp(2 * v1 - b1, -150000, 150000);
            b2 = clamp(2 * v2 - b2, -150000, 150000);
            y = v2;
        } else                                        /* LP .. NOTCH .. HP: notch = in - k v1 = LP + HP */
            y = mulq15(v2, m1) + mulq15(s - ((k * v1) >> 12) - v2, m2);
        dc += (y * 256 - dc) >> 11;                   /* ~3 Hz high-pass: a synced saw's offset */
        y -= dc >> 8;
        y = clamp(soft_knee(y, 10000) << 1, -32000, 32000);
        out[i] += voice_amp(y, m, i) << 1;
    }
    v->ph[0] = pha;
    v->ph[1] = phb;
    v->s[0] = a1, v->s[1] = a2, v->s[4] = b1, v->s[5] = b2;
    v->s[6] = nst;
    v->s[7] = jl;
    x->dc = dc;
}

/* ATTACK and RELEASE (EDIT 2) are the ENV page's A and R: the one that moved is copied to the other */
static int16_t br_last[NTRK][4];
static void brass_block(track_t *t)
{
    int16_t *l = br_last[(uint32_t)(t - trk) % NTRK];
    if (t >= &trk[0] && t < &trk[NPART])                /* the part's arena claimed here, on this core, before the voices
                                                         * (both cores' renders then only read whose it is) */
        (void)eng_arena_of(t, ENGI_BRASS);
    if (t->p[P_E6] != l[0])
        t->p[P_ATK] = t->p[P_E6];
    else if (t->p[P_ATK] != l[1])
        t->p[P_E6] = t->p[P_ATK];
    if (t->p[P_E7] != l[2])
        t->p[P_REL] = t->p[P_E7];
    else if (t->p[P_REL] != l[3])
        t->p[P_E7] = t->p[P_REL];
    l[0] = t->p[P_E6], l[1] = t->p[P_ATK], l[2] = t->p[P_E7], l[3] = t->p[P_REL];
}

/*                    E: CUTOFF RESO ENV DETUNE  PMOD TYPE ATTACK RELEASE (= the ADSR's A and R)
 * The level trims (P_ED_FX, 1/2 dB, in the mix): each note near one note of the DCO pad (tests/brass_test.c levels) */
static const preset_t BRASS_PRESETS[] = {
    {"JUMP BRASS", {50, 22, 34, 8, 0, BT_2P, 4, 44}, {4, 70, 110, 44}, 0, 0, FX(0, 20, 16, 36)},
    {"SYNC LEAD", {76, 30, 20, 0, 0, BT_2P, 0, 46}, {0, 70, 110, 46}, 0, 1, FX(10, 0, 40, 30), XP(P_GLIDE + 1, 20)},
    {"WARM PAD", {46, 18, 14, 12, 0, BT_2P, 72, 92}, {72, 90, 118, 92}, 0, 0, FX(0, 50, 22, 70),
     XP(P_LRATE + 1, 26, P_LD_SHP + 1, 40)},
    {"POLYMOD BELL", {88, 10, 0, 0, 70, BT_2P, 0, 86}, {0, 96, 0, 86}, 0, 0, FX(0, 30, 40, 60)},
    {"STACK BRASS", {44, 26, 40, 10, 0, BT_4P, 8, 50}, {8, 72, 108, 50}, 0, 0, FX(0, 24, 18, 40)},
    {"SOFT STRINGS", {58, 10, 6, 14, 0, BT_2P, 60, 84}, {60, 90, 120, 84}, 0, 0, FX(0, 60, 24, 60),
     XP(P_LRATE + 1, 30, P_LD_SHP + 1, 20)},
    {"NOTCH SWEEP", {60, 50, 30, 10, 0, BT_2P, 40, 90}, {40, 90, 120, 90}, 0, 0, FX(0, 40, 30, 60),
     XP(P_LRATE + 1, 12, P_LD_FLT + 1, 30)},
    {"PWM PAD", {54, 18, 8, 10, 0, BT_2P, 80, 100}, {80, 90, 118, 100}, 0, 0, FX(0, 60, 24, 70),
     XP(P_LRATE + 1, 34, P_LD_SHP + 1, 50)},
    {"BP PLUCK", {64, 90, 40, 8, 0, BT_BP, 0, 40}, {0, 70, 0, 40}, 0, 0, FX(0, 20, 50, 30), XP(P_ED_FX + 1, 12)},
    {"POLY BASS", {40, 34, 36, 6, 0, BT_4P, 0, 34}, {0, 66, 90, 34}, 0, 0, FX(0, 0, 10, 16)},
    {"GROWL", {48, 40, 24, 0, 80, BT_4P, 0, 40}, {0, 70, 100, 40}, 0, 1, FX(10, 0, 20, 20)},
    {"AGED PAD", {50, 20, 12, 6, 0, BT_2P, 76, 96}, {76, 92, 118, 96}, 0, 0, FX(0, 50, 26, 72)},
    {"ARP PLUCK", {62, 40, 40, 8, 0, BT_2P, 0, 30}, {0, 52, 0, 30}, 0, 0, FX(0, 20, 50, 30), XP(P_ED_FX + 1, 16)},
};

/*                 X: AWAV APW ARNG ALVL  BWAV BPW BRNG BLVL  BFRQ SYNC NOISE PWM  FATK FDEC FSUS FREL
 *                    MODE KBD VELF AGE  PMENV PMFRQ PMPW PMFLT */
typedef struct {
    int8_t x[BX_COUNT];
} brass_xp_t;
static const brass_xp_t BRASS_XP[NELEM(BRASS_PRESETS)] = {
    /* JUMP BRASS: two saws a few cents apart, the filter opening on each chord: the 80s stab */
    {{BW_SAW, 64, 1, 127, BW_SAW, 64, 1, 110, 0, 0, 0, 0, 12, 70, 70, 50, 0, 1, 30, 24, 0, 1, 0, 0}},
    /* SYNC LEAD: A synced to B, the filter envelope sweeping A's pitch (poly-mod): the scream */
    {{BW_SAW, 64, 1, 127, BW_SAW, 64, 1, 60, 0, 1, 0, 0, 0, 80, 40, 50, 0, 1, 20, 16, 50, 1, 0, 0}},
    /* WARM PAD: a saw and a pulse, the LFO on the widths, the 12 dB filter low */
    {{BW_SAW, 64, 1, 120, BW_PLS, 44, 1, 110, 0, 0, 0, 50, 60, 90, 100, 90, 0, 1, 0, 40, 0, 1, 0, 0}},
    /* POLYMOD BELL: B a fifth and an octave up into A's frequency: an inharmonic, decaying ring */
    {{BW_TRI, 64, 1, 127, BW_TRI, 64, 2, 0, 56, 0, 0, 0, 0, 90, 0, 80, 0, 2, 0, 16, 20, 1, 0, 0}},
    /* STACK BRASS: the 24 dB filter (4P), two saws, more age */
    {{BW_SAW, 64, 1, 127, BW_SAW, 64, 0, 100, 0, 0, 0, 0, 16, 72, 70, 50, 0, 1, 24, 50, 0, 1, 0, 0}},
    /* SOFT STRINGS: saws at 8' and 4', slow */
    {{BW_SAW, 64, 1, 127, BW_SAW, 64, 2, 70, 0, 0, 0, 0, 50, 80, 110, 80, 0, 1, 0, 30, 0, 1, 0, 0}},
    /* NOTCH SWEEP: MODE at the notch, the LFO on the cutoff, a little noise */
    {{BW_SAW, 64, 1, 127, BW_SAW, 64, 1, 110, 0, 0, 10, 0, 80, 100, 90, 90, 64, 1, 0, 24, 0, 1, 0, 0}},
    /* PWM PAD: two pulses, deep PWM */
    {{BW_PLS, 60, 1, 127, BW_PLS, 50, 1, 120, 0, 0, 0, 90, 70, 90, 110, 95, 0, 1, 0, 30, 0, 1, 0, 0}},
    /* BP PLUCK: the band-pass, resonant */
    {{BW_SAW, 64, 1, 127, BW_SAW, 64, 1, 110, 0, 0, 0, 0, 0, 64, 0, 40, 0, 2, 30, 20, 0, 1, 0, 0}},
    /* POLY BASS: a saw and a square an octave down, the 24 dB filter (4P) */
    {{BW_BOTH, 60, 0, 127, BW_SAW, 64, 0, 90, 0, 0, 0, 0, 0, 56, 50, 36, 0, 1, 20, 16, 0, 1, 0, 0}},
    /* GROWL: OSC B into the cutoff (poly-mod to the filter) and A's pitch: a snarling bass-lead */
    {{BW_SAW, 64, 0, 127, BW_SAW, 64, 0, 40, 3, 0, 0, 0, 0, 60, 80, 40, 0, 1, 20, 20, 0, 1, 0, 1}},
    /* AGED PAD: AGE near the top: each voice its own tune, cutoff and drift */
    {{BW_SAW, 64, 1, 127, BW_PLS, 50, 1, 110, 0, 0, 0, 30, 70, 90, 110, 96, 0, 1, 0, 110, 0, 1, 0, 0}},
    /* ARP PLUCK: short, bright, for the arpeggiator */
    {{BW_SAW, 64, 1, 127, BW_PLS, 40, 1, 100, 0, 0, 0, 0, 0, 48, 0, 30, 0, 1, 30, 20, 0, 1, 0, 0}},
};

static void brass_load(track_t *t, int32_t pi)
{
    uint32_t k;
    if (pi < 0)
        return;
    for (k = 0; k < BX_COUNT; k++)
        t->xp[k] = BRASS_XP[(uint32_t)pi % NELEM(BRASS_XP)].x[k];
}
static const char *const BRASS_PAGES[] = {"OSC A", "OSC B", "MIX", "F ENV", "FILTER", "POLYMOD"};
static const xp_info_t BRASS_XPI = {BRASS_X, BX_COUNT, BRASS_PAGES, brass_load};
_Static_assert(BX_COUNT <= NXP && (BX_COUNT + 3u) / 4u == NELEM(BRASS_PAGES), "BRASS: its pages and values");
_Static_assert(NELEM(BRASS_XP) == NELEM(BRASS_PRESETS), "BRASS: a row of extra values per preset");

static const engine_t ENG_BRASS = {
    .name = "BRASS",
    .page_title = {"VCF", "PMOD ENV"},
    .edit = {
        {"CUTOFF", F_CUTOFF, 0, 127, 60, 0, 0},
        {"RESO", F_PCT, 0, 127, 20, 0, 0},
        {"ENV", F_BIPCT, -64, 63, 24, 0, 0},
        {"DETUNE", F_INT, 0, 127, 8, 0, "ct"},
        {"PMOD", F_PCT, 0, 127, 0, 0, 0},
        {"TYPE", F_ENUM, 0, 2, BT_2P, N_BR_TYPE, 0},
        {"ATTACK", F_TIME, 0, 127, 4, 0, 0},
        {"RELEASE", F_TIME, 0, 127, 50, 0, 0},
    },
    .presets = BRASS_PRESETS,
    .npresets = NELEM(BRASS_PRESETS),
    .fil_page = 0,
    .note_on = brass_note_on,
    .render = brass_render,
    .color = 0xFC60,
    .macro = {P_E0, P_E1, P_E2, P_E4},
    .poly = 0,
    .block = brass_block,
};
