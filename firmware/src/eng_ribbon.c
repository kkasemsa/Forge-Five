/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Khantipol Kasemsant; built on Clipper (isod89 / 3dSam), SLOOP and Felucca (C) 2026 Leo Kuroshita,
 * Hügelton Instruments */
/* RIBBON (Forge Five Phase 5): the big film synth sound, after the classic late-1970s two-layer polysynth with a
 * ribbon above its keys (spec v0.2 section 3.4). Written from its published behaviour; no code or data of any other
 * synth.
 *
 *   two layers a voice, I and II, each: an oscillator (SAW, PULSE with its width, SINE; 16' 8' 4'), a high-pass into a
 *   12 dB low-pass (BRILL and RESO for both, each layer its own filter envelope and its depth), layer II its own
 *   loudness envelope inside the voice's (the ENV page's ADSR: layer I's, and the voice's life); BAL between them;
 *   DETUNE layer II against I; SUB (a square an octave under layer I), RING (I x II), NOISE (a breath into both)
 *   RIBBON  K4 (EDIT 1's last, the HOME page's fourth): a pitch ribbon, relative: turned from the middle it bends the
 *           newest note held (TARGET LAST; ALL: every voice) up to RANGE semitones each way; with SPRING (on by
 *           default) it glides back to the middle once K4 has rested for 300 ms, in ~175 ms (ribbon_block)
 *   TOUCH   brilliance from the touch: half from the velocity, half from how long the key has been held (full at
 *           HOLD); TOUCH sets how far (up to +40 cutoff steps). The encoder turned while a key is held and the
 *           8-slot matrix of the spec (v0.2 section 4) are Phase 7's
 *   EDIT 1  BRILL  RESO  BAL  RIBBON          EDIT 2  DETUNE  ATTACK  RELEASE  TOUCH
 *   (ATTACK and RELEASE are the ENV page's A and R: each follows the other)
 * Voice state: ph[0] layer I, ph[1] layer II, ph[2] the sub; s[0] s[1] layer I's low-pass, s[2] s[3] layer II's, s[4]
 * s[5] their high-passes, s[6] the noise. In the part's arena (ribbon_vx_t): the filter envelopes, layer II's
 * loudness envelope, the time held, the DC blocker */

static const char *const N_RB_WAVE[] = {"SAW", "PLS", "SINE"};
static const char *const N_RB_FEET[] = {"16'", "8'", "4'"};
static const char *const N_RB_KBD[] = {"OFF", "HALF", "FULL"};
static const char *const N_RB_TGT[] = {"LAST", "ALL"};
static const char *const N_RB_ONOFF[] = {"OFF", "ON"};

enum { RW_SAW, RW_PLS, RW_SINE };
enum {
    RX_IWAV, RX_IPW, RX_IHPF, RX_IFEET,              /* page LAYER I */
    RX_JWAV, RX_JPW, RX_JHPF, RX_JFEET,              /* page LAYER II */
    RX_IFATK, RX_IFDEC, RX_IFSUS, RX_IFAMT,          /* page I FILTER */
    RX_JFATK, RX_JFDEC, RX_JFSUS, RX_JFAMT,          /* page II FILTER */
    RX_JATK, RX_JDEC, RX_JSUS, RX_JREL,              /* page II AMP */
    RX_SUB, RX_RING, RX_NOISE, RX_KBD,               /* page SUB RING */
    RX_RANGE, RX_TGT, RX_SPRING, RX_HOLD,            /* page RIBBON */
    RX_COUNT
};
#define RXP(t, k) ((t)->xp[k])

static const param_desc_t RIBBON_X[RX_COUNT] = {
    {"I WAVE", F_ENUM, 0, 2, RW_SAW, N_RB_WAVE, 0},
    {"I PW", F_PCT, 0, 127, 64, 0, 0},
    {"I HPF", F_CUTOFF, 0, 127, 0, 0, 0},
    {"I FEET", F_ENUM, 0, 2, 1, N_RB_FEET, 0},
    {"II WAVE", F_ENUM, 0, 2, RW_SAW, N_RB_WAVE, 0},
    {"II PW", F_PCT, 0, 127, 64, 0, 0},
    {"II HPF", F_CUTOFF, 0, 127, 0, 0, 0},
    {"II FEET", F_ENUM, 0, 2, 1, N_RB_FEET, 0},
    {"I F ATK", F_TIME, 0, 127, 20, 0, 0},
    {"I F DEC", F_TIME, 0, 127, 70, 0, 0},
    {"I F SUS", F_PCT, 0, 127, 80, 0, 0},
    {"I F AMT", F_BIPCT, -64, 63, 20, 0, 0},
    {"II F ATK", F_TIME, 0, 127, 20, 0, 0},
    {"II F DEC", F_TIME, 0, 127, 70, 0, 0},
    {"II F SUS", F_PCT, 0, 127, 80, 0, 0},
    {"II F AMT", F_BIPCT, -64, 63, 20, 0, 0},
    {"II ATK", F_TIME, 0, 127, 0, 0, 0},
    {"II DEC", F_TIME, 0, 127, 60, 0, 0},
    {"II SUS", F_PCT, 0, 127, 127, 0, 0},
    {"II REL", F_TIME, 0, 127, 127, 0, 0},
    {"SUB", F_PCT, 0, 127, 0, 0, 0},
    {"RING", F_PCT, 0, 127, 0, 0, 0},
    {"NOISE", F_PCT, 0, 127, 0, 0, 0},
    {"KBD", F_ENUM, 0, 2, 1, N_RB_KBD, 0},
    {"RANGE", F_INT, 1, 24, 12, 0, "st"},
    {"TARGET", F_ENUM, 0, 1, 0, N_RB_TGT, 0},
    {"SPRING", F_ENUM, 0, 1, 1, N_RB_ONOFF, 0},
    {"HOLD", F_TIME, 0, 127, 100, 0, 0},           /* (100: ~1.5 s to the full touch) */
};

/* per voice, in the part's engine arena (engines.c eng_arena_of) */
typedef struct {
    int32_t fe[2];                   /* the layers' filter envelopes, Q24 */
    int32_t ae;                      /* layer II's loudness envelope, Q24 */
    uint32_t held;                   /* blocks the key has been held (TOUCH) */
    int32_t dc;                      /* the output's DC blocker (Q8) */
    uint8_t fs[2], as;               /* their stages: 1 attack, 2 decay / sustain, 3 release */
} ribbon_vx_t;
typedef ribbon_vx_t ribbon_part_t[NVOICE];
static ribbon_vx_t ribbon_spare;     /* a track outside trk[] (a host test's copy): state of its own */
static ribbon_vx_t *ribbon_vx(track_t *t, const voice_t *v)
{
    ribbon_part_t *a;
    if (t < &trk[0] || t >= &trk[NPART])
        return &ribbon_spare;
    a = (ribbon_part_t *)eng_arena_of(t, ENGI_RIBBON);
    return a ? &(*a)[(uint32_t)(v - t->v) % NVOICE] : &ribbon_spare;
}

/* the ribbon, per part: the newest voice held (TARGET LAST), and the spring's state (ribbon_block) */
#define RB_IDLE_BLOCKS (300u * FS / 1000u / CTL)      /* K4 at rest this long: the spring starts (300 ms) */
#define RB_BACK_BLOCKS (175u * FS / 1000u / CTL)      /* .. and brings the whole range back in this long (175 ms) */
static uint8_t rb_new[NTRK];                          /* the newest held voice + 1, 0 = none yet */
static int16_t rb_last[NTRK];                         /* RIBBON as last seen (a change: K4 turned) */
static uint16_t rb_idle[NTRK];                        /* blocks since */
static int32_t rb_pos[NTRK];                          /* RIBBON on its way back, Q8 */

static void ribbon_note_on(track_t *t, voice_t *v)
{
    ribbon_vx_t *x = ribbon_vx(t, v);
    if (!v->env && !v->env_out) {                     /* from silence: the filters clear */
        v->s[0] = v->s[1] = v->s[2] = v->s[3] = v->s[4] = v->s[5] = 0;
        x->dc = 0;
    }
    if (!v->s[6])
        v->s[6] = 0x6C078965 + (int32_t)v->age * 2654435;
    x->fs[0] = x->fs[1] = 1;
    x->as = 1;
    x->held = 0;
    v->ph[2] = v->ph[0] >> 1;
}

/* one ADSR, once per block (ENV_LIN / ENV_EXP as the track's ENV); st 1 attack, 2 decay / sustain, 3 release */
static int32_t rb_env(int32_t *e, uint8_t *st, int gate, int32_t a, int32_t d, int32_t s, int32_t r)
{
    int32_t sus = s << 17;
    if (!gate && *st != 3u)
        *st = 3;
    if (*st == 1u) {
        *e += (int32_t)ENV_LIN[a & 127];
        if (*e >= (1 << 24)) {
            *e = 1 << 24;
            *st = 2;
        }
    } else if (*st == 2u) {
        *e += mulq16(sus - *e, ENV_EXP[d & 127]);
    } else {
        *e -= mulq16(*e, ENV_EXP[r & 127]);
    }
    return *e >> 9;                                   /* Q15 */
}

static inline uint32_t rb_inc(int32_t pitch16, int32_t ct, int32_t fine, uint32_t feet)
{
    uint32_t inc = cents_inc(pitch16, ct, fine);
    if (feet == 0u)
        inc >>= 1;
    else if (feet == 2u)
        inc = inc > 0x70000000u ? 0x70000000u : inc << 1;
    return inc;
}
static inline int32_t rb_wave(uint32_t w, uint32_t ph, uint32_t inc, uint32_t pw)
{
    return w == RW_PLS ? osc_pulse(ph, inc, pw) : w == RW_SINE ? osc_sine(ph) : osc_saw(ph, inc);
}
/* a one-pole high-pass's coefficient at CUTOFF step c (0: off), Q15 */
static inline int32_t rb_hpk(int32_t c)
{
    int32_t g;
    if (c <= 0)
        return 0;
    g = SVF_G[c & 127];
    return (int32_t)(((uint32_t)g << 15) / (uint32_t)(4096 + g));   /* g / (1 + g) (32 bits) */
}
/* the low-pass's coefficients (dsp.c tsvf_coef with the damping given) */
static inline void rb_svf(tsvf_t *c, int32_t cut, int32_t k)
{
    int32_t i, g, den;
    cut = clamp(cut, 0, 127 << 8);
    i = cut >> 8;
    g = SVF_G[i];
    if (i < 127)
        g += ((SVF_G[i + 1] - g) * (cut & 255)) >> 8;
    den = 4096 + ((g * (g + k)) >> 12);
    c->a1 = (int32_t)((4096u << 13) / (uint32_t)den);
    c->a2 = (c->a1 * g) >> 12;
    c->a3 = (c->a2 * g) >> 12;
}
static inline int32_t rb_lp(const tsvf_t *f, int32_t x, int32_t *a1, int32_t *a2)
{
    int32_t v3 = x - *a2, v1 = (f->a1 * *a1 + f->a2 * v3) >> 13, v2 = *a2 + ((f->a2 * *a1 + f->a3 * v3) >> 13);
    *a1 = clamp(2 * v1 - *a1, -150000, 150000);
    *a2 = clamp(2 * v2 - *a2, -150000, 150000);
    return v2;
}

static void ribbon_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    ribbon_vx_t *x = ribbon_vx(t, v);
    uint32_t i, pi = (uint32_t)(t - trk) % NTRK, vi = (uint32_t)(v - t->v);
    uint32_t wi = (uint32_t)RXP(t, RX_IWAV) % 3u, wj = (uint32_t)RXP(t, RX_JWAV) % 3u;
    uint32_t pwi = (uint32_t)clamp(RXP(t, RX_IPW) << 8, 6 << 8, 121 << 8) << 16;
    uint32_t pwj = (uint32_t)clamp(RXP(t, RX_JPW) << 8, 6 << 8, 121 << 8) << 16;
    int32_t bend = 0, fi, fj, aj, brill, ci, cj, k, gi, gj, touch, hold;
    int32_t sub = RXP(t, RX_SUB) * 200, ring = RXP(t, RX_RING) * 258, nz = RXP(t, RX_NOISE) * 120;
    int32_t hki = rb_hpk(RXP(t, RX_IHPF)), hkj = rb_hpk(RXP(t, RX_JHPF));
    int32_t a1 = v->s[0], a2 = v->s[1], b1 = v->s[2], b2 = v->s[3], hi = v->s[4], hj = v->s[5], nst = v->s[6];
    int32_t dc = x->dc;
    uint32_t phi = v->ph[0], phj = v->ph[1], phs = v->ph[2], inci, incj, incs;
    tsvf_t f, g;

    /* RIBBON: the newest note held (or every voice) bent by K4, RANGE semitones at either end */
    if (RXP(t, RX_TGT) || rb_new[pi] == vi + 1u)
        bend = p[P_E3] * RXP(t, RX_RANGE) * 100 / 64;
    inci = rb_inc(m->pitch16, bend, m->fine, (uint32_t)RXP(t, RX_IFEET) % 3u);
    incj = rb_inc(m->pitch16, bend + p[P_E4], m->fine, (uint32_t)RXP(t, RX_JFEET) % 3u);
    incs = inci >> 1;
    /* the envelopes, once a block: the layers' filters, layer II's loudness */
    fi = rb_env(&x->fe[0], &x->fs[0], v->gate, RXP(t, RX_IFATK), RXP(t, RX_IFDEC), RXP(t, RX_IFSUS), p[P_REL]);
    fj = rb_env(&x->fe[1], &x->fs[1], v->gate, RXP(t, RX_JFATK), RXP(t, RX_JFDEC), RXP(t, RX_JFSUS), p[P_REL]);
    aj = rb_env(&x->ae, &x->as, v->gate, RXP(t, RX_JATK), RXP(t, RX_JDEC), RXP(t, RX_JSUS), RXP(t, RX_JREL));
    /* TOUCH: half the velocity, half the time held (full at HOLD), Q15; up to +40 cutoff steps at TOUCH 127 */
    if (v->gate && x->held < 0xFFFFu)
        x->held++;
    {
        int32_t hk = (int32_t)ENV_LIN[RXP(t, RX_HOLD) & 127];   /* (a HOLD of k: 2^24 / ENV_LIN[k] blocks, as GLIDE) */
        uint32_t hb = hk ? (uint32_t)(1 << 24) / (uint32_t)hk : 1u;
        hold = x->held >= hb ? 16384 : (int32_t)(x->held * 16384u / (hb ? hb : 1u));
    }
    touch = (int32_t)v->vel * 129 + hold;             /* 0 .. 32767 */
    /* BRILL: both layers' cutoff, with the track's modulation, KBD and TOUCH; each layer its envelope's depth */
    brill = (p[P_E0] << 8) + m->cutoff + ((((touch * p[P_E7]) >> 7) * 40) >> 7);
    brill += (RXP(t, RX_KBD) * ((m->pitch16 - 60 * 16) * 299 / 16)) >> 1;
    ci = brill + ((fi * RXP(t, RX_IFAMT) * 60) >> 13);
    cj = brill + ((fj * RXP(t, RX_JFAMT) * 60) >> 13);
    k = 8192 - p[P_E1] * 6600 / 127;                  /* RESO: the damping 2.0 .. 0.39 (short of oscillation) */
    rb_svf(&f, ci, k);
    rb_svf(&g, cj, k);
    /* BAL: -64 layer I alone .. 0 both .. 63 layer II alone; layer II through its own loudness envelope */
    gj = (p[P_E2] + 64) * 258;
    gi = 32767 - gj;
    gj = mulq15(gj, aj);
    for (i = 0; i < n; i++) {
        int32_t xi = rb_wave(wi, phi, inci, pwi), xj = rb_wave(wj, phj, incj, pwj), y, yi, yj, b = 0;
        if (sub)
            xi += mulq15(osc_pulse(phs, incs, 0x80000000u), sub);
        if (nz) {
            b = mulq15((int32_t)(noise32(&nst) >> 17) - 16384, nz);
            xi += b, xj += b;
        }
        phi += inci, phj += incj, phs += incs;
        xi >>= 1, xj >>= 1;                           /* (headroom: an oscillator, the sub and the breath) */
        hi += mulq15(xi - hi, hki);                   /* the high-passes (their low-pass taken away) */
        hj += mulq15(xj - hj, hkj);
        yi = rb_lp(&f, hki ? xi - hi : xi, &a1, &a2);
        yj = rb_lp(&g, hkj ? xj - hj : xj, &b1, &b2);
        y = mulq15(yi, gi) + mulq15(yj, gj);
        if (ring)
            y += mulq15(mulq15(yi, yj) << 1, ring);
        dc += (y * 256 - dc) >> 11;
        y -= dc >> 8;
        y = clamp(soft_knee(y, 12000) << 1, -32000, 32000);
        out[i] += voice_amp(y, m, i) << 1;
    }
    v->ph[0] = phi, v->ph[1] = phj, v->ph[2] = phs;
    v->s[0] = a1, v->s[1] = a2, v->s[2] = b1, v->s[3] = b2, v->s[4] = hi, v->s[5] = hj, v->s[6] = nst;
    x->dc = dc;
}

/* once per block, before the voices (and before any is handed to the second core): the part's arena claimed; the
 * newest voice held (RIBBON's target); the spring; ATTACK / RELEASE and the ENV page's A / R kept one value each */
static int16_t rb_mir[NTRK][4];
static void ribbon_block(track_t *t)
{
    uint32_t pi = (uint32_t)(t - trk) % NTRK, i, best = NVOICE;
    int16_t *l = rb_mir[pi];
    if (t >= &trk[0] && t < &trk[NPART])
        (void)eng_arena_of(t, ENGI_RIBBON);
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && t->v[i].stage != 4u && (best == NVOICE || t->v[i].age > t->v[best].age))
            best = i;
    if (best < NVOICE)
        rb_new[pi] = (uint8_t)(best + 1u);            /* (none held: the last one keeps its bend into its release) */
    if (t->p[P_E3] != rb_last[pi]) {                  /* K4 turned (or a sound loaded): the spring waits again */
        rb_idle[pi] = 0;
        rb_pos[pi] = (int32_t)t->p[P_E3] << 8;
    } else if (rb_idle[pi] < RB_IDLE_BLOCKS) {
        rb_idle[pi]++;
    } else if (RXP(t, RX_SPRING) && t->p[P_E3]) {     /* at rest 300 ms: back to the middle, the whole way in 175 ms */
        int32_t step = (64 << 8) / (int32_t)RB_BACK_BLOCKS, q = rb_pos[pi];
        q = q > 0 ? (q > step ? q - step : 0) : (q < -step ? q + step : 0);
        rb_pos[pi] = q;
        t->p[P_E3] = (int16_t)(q >= 0 ? (q + 255) >> 8 : -((-q + 255) >> 8));
    }
    rb_last[pi] = t->p[P_E3];
    if (t->p[P_E5] != l[0])
        t->p[P_ATK] = t->p[P_E5];
    else if (t->p[P_ATK] != l[1])
        t->p[P_E5] = t->p[P_ATK];
    if (t->p[P_E6] != l[2])
        t->p[P_REL] = t->p[P_E6];
    else if (t->p[P_REL] != l[3])
        t->p[P_E6] = t->p[P_REL];
    l[0] = t->p[P_E5], l[1] = t->p[P_ATK], l[2] = t->p[P_E6], l[3] = t->p[P_REL];
}

/*                    E: BRILL RESO BAL RIBBON  DETUNE ATTACK RELEASE TOUCH (ATTACK, RELEASE = the ADSR's A and R)
 * The level trims (P_ED_FX, 1/2 dB, in the mix): each note near one note of the DCO pad (tests/ribbon_test.c) */
static const preset_t RIBBON_PRESETS[] = {
    {"NOIR BRASS", {46, 30, 0, 0, 8, 34, 86, 70}, {34, 90, 118, 86}, 0, 0, FX(0, 30, 24, 80),
     XP(P_LRATE + 1, 24, P_LD_PIT + 1, 2)},
    {"VICTORY LEAD", {62, 22, 10, 0, 6, 10, 60, 64}, {10, 80, 116, 60}, 0, 1, FX(0, 20, 40, 60),
     XP(P_GLIDE + 1, 24, P_LRATE + 1, 34, P_LD_PIT + 1, 4)},
    {"MEMORY PAD", {50, 16, 0, 0, 10, 76, 104, 40}, {76, 96, 120, 104}, 0, 0, FX(0, 50, 24, 90),
     XP(P_LRATE + 1, 18, P_LD_FLT + 1, 6)},
    {"RIBBON BEND", {66, 26, -20, 0, 4, 4, 56, 50}, {4, 80, 114, 56}, 0, 1, FX(0, 10, 34, 50),
     XP(P_ED_FX + 1, -6, P_GLIDE + 1, 10)},
    {"RAIN BELL", {84, 10, 10, 0, 0, 0, 96, 30}, {0, 96, 0, 96}, 0, 0, FX(0, 20, 40, 80), XP(P_ED_FX + 1, -4)},
    {"FILM STRINGS", {54, 12, 0, 0, 12, 70, 90, 50}, {70, 90, 120, 90}, 0, 0, FX(0, 60, 20, 80),
     XP(P_LRATE + 1, 30, P_LD_PIT + 1, 2)},
    {"BREATH FLUTE", {58, 18, -64, 0, 0, 30, 60, 80}, {30, 80, 110, 60}, 0, 1, FX(0, 0, 30, 60),
     XP(P_ED_FX + 1, -12, P_LRATE + 1, 36, P_LD_PIT + 1, 3)},
    {"TOUCH BRASS", {38, 34, 0, 0, 6, 20, 60, 110}, {20, 80, 116, 60}, 0, 0, FX(0, 20, 20, 50), XP(P_ED_FX + 1, -6)},
};

/*                 X: IWAV IPW IHPF IFEET  JWAV JPW JHPF JFEET  IFATK IFDEC IFSUS IFAMT  JFATK JFDEC JFSUS JFAMT
 *                    JATK JDEC JSUS JREL  SUB RING NOISE KBD  RANGE TGT SPRING HOLD */
typedef struct {
    int8_t x[RX_COUNT];
} ribbon_xp_t;
static const ribbon_xp_t RIBBON_XP[NELEM(RIBBON_PRESETS)] = {
    /* NOIR BRASS: two saws, layer II an octave up and swelling later, the filters opening slowly; touch it to brighten */
    {{RW_SAW, 64, 0, 1, RW_SAW, 64, 20, 2, 60, 90, 90, 36, 80, 90, 80, 30, 70, 90, 110, 100, 30, 0, 0, 1, 12, 0, 1, 104}},
    /* VICTORY LEAD: a saw and a pulse, the ribbon an octave each way on the newest note */
    {{RW_SAW, 64, 0, 1, RW_PLS, 40, 0, 1, 10, 80, 80, 30, 20, 80, 80, 20, 0, 60, 127, 100, 0, 0, 0, 1, 12, 0, 1, 96}},
    /* MEMORY PAD: a pulse and a saw an octave up, slow both ways */
    {{RW_PLS, 50, 0, 1, RW_SAW, 64, 30, 2, 80, 100, 100, 20, 90, 100, 90, 16, 80, 100, 120, 110, 20, 0, 0, 1, 12, 0, 1, 108}},
    /* RIBBON BEND: a lead made for the ribbon: an octave each way, springing back */
    {{RW_SAW, 64, 0, 1, RW_SINE, 64, 0, 1, 0, 70, 90, 24, 0, 70, 90, 10, 0, 60, 127, 100, 40, 0, 0, 1, 12, 0, 1, 96}},
    /* RAIN BELL: two sines an octave apart (8' and 4') rung against each other (RING), a long fall */
    {{RW_SINE, 64, 0, 1, RW_SINE, 64, 0, 2, 0, 90, 0, 0, 0, 90, 0, 0, 0, 80, 0, 90, 0, 90, 0, 2, 12, 0, 1, 96}},
    /* FILM STRINGS: two saws, high-passed a little, a slow swell */
    {{RW_SAW, 64, 30, 1, RW_SAW, 64, 40, 2, 70, 90, 110, 10, 80, 90, 110, 10, 60, 90, 120, 100, 0, 0, 0, 1, 12, 0, 1, 104}},
    /* BREATH FLUTE: a sine and its breath, layer I alone */
    {{RW_SINE, 64, 20, 1, RW_SINE, 64, 0, 2, 30, 70, 100, 10, 30, 70, 100, 10, 30, 60, 100, 60, 0, 0, 40, 1, 2, 0, 1, 96}},
    /* TOUCH BRASS: dark until touched: TOUCH near the top, HOLD short */
    {{RW_SAW, 64, 0, 1, RW_SAW, 64, 0, 1, 20, 80, 90, 30, 20, 80, 90, 30, 0, 60, 127, 100, 0, 0, 0, 1, 12, 0, 1, 92}},
};

static void ribbon_load(track_t *t, int32_t pi)
{
    uint32_t k;
    if (pi < 0)
        return;
    for (k = 0; k < RX_COUNT; k++)
        t->xp[k] = RIBBON_XP[(uint32_t)pi % NELEM(RIBBON_XP)].x[k];
}
static const char *const RIBBON_PAGES[] = {"LAYER I", "LAYER II", "I FILTER", "II FILTER", "II AMP", "SUB RING", "RIBBON"};
static const xp_info_t RIBBON_XPI = {RIBBON_X, RX_COUNT, RIBBON_PAGES, ribbon_load};
_Static_assert(RX_COUNT <= NXP && (RX_COUNT + 3u) / 4u == NELEM(RIBBON_PAGES), "RIBBON: its pages and values");
_Static_assert(NELEM(RIBBON_XP) == NELEM(RIBBON_PRESETS), "RIBBON: a row of extra values per preset");

static const engine_t ENG_RIBBON = {
    .name = "RIBBON",
    .page_title = {"BRILL BAL", "TOUCH"},
    .edit = {
        {"BRILL", F_CUTOFF, 0, 127, 60, 0, 0},
        {"RESO", F_PCT, 0, 127, 20, 0, 0},
        {"BAL", F_BIPCT, -64, 63, 0, 0, 0},
        {"RIBBON", F_BIPCT, -64, 63, 0, 0, 0},
        {"DETUNE", F_INT, 0, 127, 6, 0, "ct"},
        {"ATTACK", F_TIME, 0, 127, 10, 0, 0},
        {"RELEASE", F_TIME, 0, 127, 60, 0, 0},
        {"TOUCH", F_PCT, 0, 127, 60, 0, 0},
    },
    .presets = RIBBON_PRESETS,
    .npresets = NELEM(RIBBON_PRESETS),
    .fil_page = 0,
    .note_on = ribbon_note_on,
    .render = ribbon_render,
    .color = 0xA81F,
    .macro = {P_E0, P_E1, P_E2, P_E3},
    .poly = 0,
    .block = ribbon_block,
};
