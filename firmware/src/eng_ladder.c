/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Khantipol Kasemsant
 * Built on Felucca, Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* LADDER: a three-oscillator monosynth in the manner of the classic 1970s American portable synthesizer.
 * Written from scratch from the published behaviour of its circuit blocks; no code or data of any other synth.
 *
 * Signal path, as on the original's panel left to right:
 *   OSCILLATOR BANK  three VCOs. RANGE LO (sub-audio) 32' 16' 8' 4' 2'; OSC 2 and 3 have FREQ (+-7 semitones, finer
 *                    near the centre for slow beating); OSC 3 can leave the keyboard (O3 KBD OFF) and, at LO, is the
 *                    modulation oscillator (its FREQ then spans +-3 octaves: ~0.25 .. 16 Hz)
 *   MIXER            a level per oscillator and NOISE (white or pink). The mixer drives the filter's input
 *                    stage: a single oscillator at full stays clean, three at full saturate it (the classic "fat"),
 *                    OVERLOAD pushes it further (the external-input feedback trick)
 *   MODIFIERS        a four-pole transistor-ladder low-pass (zero-delay-feedback, a tanh input stage), CUTOFF,
 *                    EMPHASIS (self-oscillates near the top), CONTOUR AMOUNT and KBD tracking (OFF 1/3 2/3 FULL);
 *                    its own contour (ATTACK, DECAY, SUSTAIN; DECAY switch: the release follows DECAY when ON,
 *                    is short when OFF). The LOUDNESS contour is the track's ENV page (ATK DEC SUS REL)
 *   CONTROLLERS      MIX (OSC 3 <-> NOISE) at WHEEL (the larger of the knob and the part's mod wheel, CC1) to the
 *                    oscillators' pitch (OSC) and / or the cutoff (FILT). GLIDE is the track's (VOICE page)
 *   DRIFT            a slow random walk of each oscillator's tuning (0: dead stable)
 *
 * Parameters: the engine's eight (EDIT 1 FILTER, EDIT 2 CONTOUR) and twenty more as its engine patch (engine_xp.c:
 * track_t.xp[], stored as a blob in the part's 128-byte patch record), on five pages of their own (LADDER_XP:
 * OSC 1-2, OSC 2-3, MIX, MIX 2, CTRL). A preset sets them (ladder_load). */

#define F_ST8 (F_STEPS + 1)          /* params.c: an oscillator's FREQ, shown in semitones (ladder_cents) */
#define LADDER_PINK 62               /* pink's level against white (x / 16): measured, tests/ladder_test.c */
#define LADDER_SAT 20000             /* the filter's input stage: tanh of this ceiling (a full oscillator is 16383) */
#define LADDER_SATK 6711             /* 32768 / LADDER_SAT, Q12 (softclip's scale) */
#define LADDER_OUT 32                /* the voice's level (x / 16): near the other engines' (tests/ladder_test.c) */

static const char *const N_LD_RANGE[] = {"LO", "32'", "16'", "8'", "4'", "2'"};
static const char *const N_LD_WAVE[] = {"TRI", "SHARK", "SAW", "SQR", "WIDE", "NARW"};
static const char *const N_LD_WAVE3[] = {"TRI", "SHARK", "RSAW", "SQR", "WIDE", "NARW"};
static const char *const N_LD_KBD[] = {"OFF", "1/3", "2/3", "FULL"};
static const char *const N_LD_NOISE[] = {"WHITE", "PINK"};
static const char *const N_LD_SW[] = {"OFF", "ON"};

enum { LD_LO, LD_32, LD_16, LD_8, LD_4, LD_2 };
enum { LW_TRI, LW_SHARK, LW_SAW, LW_SQR, LW_WIDE, LW_NARW };

/* the twenty extra parameters, P_X0 + k (core.h) */
enum {
    LX_O1RNG, LX_O1WAV, LX_O2RNG, LX_O2WAV,           /* page OSC 1-2 */
    LX_O2FRQ, LX_O3RNG, LX_O3FRQ, LX_O3WAV,           /* page OSC 2-3 */
    LX_MIX1, LX_MIX2, LX_MIX3, LX_MIXN,               /* page MIXER */
    LX_NCOL, LX_OVLD, LX_O3KBD, LX_DRIFT,             /* page MIXER 2 */
    LX_MODMIX, LX_MODAMT, LX_OSCMOD, LX_FLTMOD,       /* page MOD */
    LX_COUNT
};
#define LXP(t, k) ((t)->xp[k])

static const param_desc_t LADDER_X[LX_COUNT] = {
    {"O1 RNG", F_ENUM, 0, 5, LD_8, N_LD_RANGE, 0},
    {"O1 WAV", F_ENUM, 0, 5, LW_SAW, N_LD_WAVE, 0},
    {"O2 RNG", F_ENUM, 0, 5, LD_8, N_LD_RANGE, 0},
    {"O2 WAV", F_ENUM, 0, 5, LW_SAW, N_LD_WAVE, 0},
    {"O2 FRQ", F_ST8, -56, 56, 0, 0, "st"},
    {"O3 RNG", F_ENUM, 0, 5, LD_8, N_LD_RANGE, 0},
    {"O3 FRQ", F_ST8, -56, 56, 0, 0, "st"},
    {"O3 WAV", F_ENUM, 0, 5, LW_TRI, N_LD_WAVE3, 0},
    {"OSC 1", F_PCT, 0, 127, 127, 0, 0},
    {"OSC 2", F_PCT, 0, 127, 0, 0, 0},
    {"OSC 3", F_PCT, 0, 127, 0, 0, 0},
    {"NOISE", F_PCT, 0, 127, 0, 0, 0},
    {"N COL", F_ENUM, 0, 1, 0, N_LD_NOISE, 0},
    {"OVLD", F_PCT, 0, 127, 0, 0, 0},
    {"O3 KBD", F_ENUM, 0, 1, 1, N_LD_SW, 0},
    {"DRIFT", F_PCT, 0, 127, 20, 0, 0},
    {"MIX", F_PCT, 0, 127, 0, 0, 0},
    {"WHEEL", F_PCT, 0, 127, 0, 0, 0},
    {"OSC", F_ENUM, 0, 1, 0, N_LD_SW, 0},
    {"FILT", F_ENUM, 0, 1, 0, N_LD_SW, 0},
};

/* an oscillator's FREQ (-56..56) in cents, square law: fine near the centre (1 step 0.2 ct), +-700 at the ends */
static inline int32_t ladder_cents(int32_t v)
{
    int32_t a = v < 0 ? -v : v, c = (a * a * 700 + 1568) / 3136;
    return v < 0 ? -c : c;
}

/* per voice, kept in the part's engine memory (engines.c ENG_MEM): the pink noise filter and the drift walks */
typedef struct {
    int32_t pk[3];                   /* pink noise (P. Kellet's economy filter) */
    int32_t dr[3];                   /* drift, 1/4096 of the phase increment (0.42 ct) << 8 */
    int32_t dt[3];                   /* drift targets */
    int32_t er[4];                   /* the ladder stages' rounding residues (error feedback: no dead band) */
    int32_t dc;                      /* the output's DC blocker (Q8) */
} ladder_vx_t;
typedef ladder_vx_t ladder_part_t[NVOICE];
ENG_MEM_FITS("LADDER", sizeof(ladder_part_t));
static ladder_vx_t ladder_spare;     /* a track outside trk[] (a host test's copy): state of its own */
static ladder_vx_t *ladder_vx(const track_t *t, const voice_t *v)
{
    if (t < &trk[0] || t >= &trk[NPART])
        return &ladder_spare;
    return &(*(ladder_part_t *)ENG_MEM(t - trk))[(uint32_t)(v - t->v) % NVOICE];
}

/* voice state: s[0] s[1] s[4] s[5] the ladder's four stages (kept over a retrigger), s[2] the filter contour's stage
 * (1 attack, 2 decay / sustain, 3 release), s[3] OSC 3's last sample (the modulation source between blocks),
 * s[6] the noise generator, s[7] the filter contour (Q24, kept: a retrigger rises from where it is) */
static void ladder_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->s[0] = v->s[1] = v->s[4] = v->s[5] = 0;
    v->s[7] = 0;
    v->s[2] = 1;
    v->s[3] = 0;
    if (!v->s[6])
        v->s[6] = 0x2545F491 + (int32_t)v->age * 7919;
    v->ph[1] = v->ph[0] + 0x2A000000u;                /* free-running oscillators: not in phase */
    v->ph[2] = v->ph[0] + 0x71000000u;
}

/* the filter contour, once per block: ENV_LIN / ENV_EXP as the track's ENV (1 ms .. 10 s) */
static int32_t ladder_contour(const track_t *t, voice_t *v)
{
    const int16_t *p = t->p;
    int32_t e = v->s[7], sus = (int32_t)p[P_E6] << 17;
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
    default:                                          /* release: DECAY ON its decay time, OFF ~5 ms */
        e -= mulq16(e, ENV_EXP[p[P_E7] ? p[P_E5] & 127 : 22]);
        break;
    }
    v->s[7] = e;
    return e >> 9;                                    /* Q15 */
}

/* an oscillator's phase increment for the block: pitch (1/16 semitone) + cents, the residual fine (1/4096), the
 * RANGE as octaves from 8' (LO: 7 below), capped below Nyquist */
static uint32_t ladder_inc(int32_t pitch16, int32_t ct, int32_t fine, uint32_t range)
{
    uint32_t inc = cents_inc(pitch16, ct, fine);
    if (range < LD_8)
        inc >>= range == LD_LO ? 7u : (uint32_t)(LD_8 - range);
    else if (range > LD_8) {
        uint32_t sh = range - LD_8;
        inc = inc > (0x70000000u >> sh) ? 0x70000000u : inc << sh;
    }
    return inc;
}

/* one sample of an oscillator's wave (Q15); pw the pulse waves' offset (osc_pulse) */
static inline int32_t ladder_wave(uint32_t w, uint32_t ph, uint32_t inc, int32_t pwm, int rev)
{
    switch (w) {
    case LW_TRI:
        return osc_tri(ph);
    case LW_SHARK:                                    /* the triangle bent towards the ramp: rounded, buzzier */
        return (osc_tri(ph) + osc_saw(ph, inc)) >> 1;
    case LW_SQR:
        return osc_pulse(ph, inc, 0x80000000u + (uint32_t)pwm);
    case LW_WIDE:
        return osc_pulse(ph, inc, 0x4CCCCCCDu + (uint32_t)pwm);     /* ~30 % */
    case LW_NARW:
        return osc_pulse(ph, inc, 0x1EB851ECu + (uint32_t)(pwm >> 1));   /* ~12 % */
    default:
        return rev ? -osc_saw(ph, inc) : osc_saw(ph, inc);
    }
}

/* the drift walks, once per block: towards a new random target now and then, slowly */
static void ladder_drift(voice_t *v, ladder_vx_t *x, int32_t amt, int32_t *fine)
{
    uint32_t k;
    for (k = 0; k < 3u; k++) {
        if ((noise32(&v->s[6]) & 255u) < 3u)          /* ~every 85 blocks (~60 ms) a new target */
            x->dt[k] = (int32_t)(noise32(&v->s[6]) >> 21) - 1024;   /* -1024..1023 */
        x->dr[k] += (x->dt[k] * 256 - x->dr[k]) >> 7;
        fine[k] = ((x->dr[k] >> 8) * amt) >> 11;      /* at 127: +-63 / 4096, ~ +-27 ct at the walk's edge */
    }
}

static void ladder_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    ladder_vx_t *x = ladder_vx(t, v);
    uint32_t i;
    uint32_t r1 = (uint32_t)LXP(t, LX_O1RNG) % 6u, r2 = (uint32_t)LXP(t, LX_O2RNG) % 6u, r3 = (uint32_t)LXP(t, LX_O3RNG) % 6u;
    uint32_t w1 = (uint32_t)LXP(t, LX_O1WAV) % 6u, w2 = (uint32_t)LXP(t, LX_O2WAV) % 6u, w3 = (uint32_t)LXP(t, LX_O3WAV) % 6u;
    int32_t c2 = ladder_cents(LXP(t, LX_O2FRQ)), c3 = ladder_cents(LXP(t, LX_O3FRQ));
    int32_t l1 = LXP(t, LX_MIX1) * 129, l2 = LXP(t, LX_MIX2) * 129, l3 = LXP(t, LX_MIX3) * 129;   /* Q14 */
    int32_t ln = LXP(t, LX_MIXN) * 129, pink = LXP(t, LX_NCOL);
    int32_t ovl = 16384 + LXP(t, LX_OVLD) * 387;      /* 1x .. 4x, Q14 */
    int32_t mmix = LXP(t, LX_MODMIX), mamt = LXP(t, LX_MODAMT) > t->mw ? LXP(t, LX_MODAMT) : t->mw;   /* WHEEL, or CC1 when higher */
    int32_t mdep = mamt * mamt / 127;                 /* the wheel, square law: light vibrato low down */
    int oscmod = LXP(t, LX_OSCMOD) && mdep, fltmod = LXP(t, LX_FLTMOD) && mdep;
    int32_t fine[3], pwm = (m->shape - (64 << 8)) << 15;
    int32_t p3, cut, envq, k, G, b, rd;
    int need3 = l3 || oscmod, needpk = pink && (ln || (mmix && (oscmod || fltmod)));
    int neednz = ln || (mmix && (oscmod || fltmod)) || p[P_E1] >= 100;   /* (the hiss: only where it starts the singing) */
    uint32_t inc1, inc2, inc3, ph0 = v->ph[0], ph1 = v->ph[1], ph2 = v->ph[2];
    int32_t s1 = v->s[0], s2 = v->s[1], s3 = v->s[4], s4 = v->s[5], nst = v->s[6];
    int32_t pk0 = x->pk[0], pk1 = x->pk[1], pk2 = x->pk[2];
    int32_t e1 = x->er[0], e2 = x->er[1], e3 = x->er[2], e4 = x->er[3], dc = x->dc;
    int32_t msrc = v->s[3], nzl = 0;

    ladder_drift(v, x, LXP(t, LX_DRIFT), fine);
    nst = v->s[6];
    envq = ladder_contour(t, v);

    /* the oscillators: OSC 3 off the keyboard plays as if from middle C; at LO its FREQ spans +-3 octaves */
    p3 = LXP(t, LX_O3KBD) ? m->pitch16 : 60 * 16;
    inc1 = ladder_inc(m->pitch16, 0, m->fine + fine[0], r1);
    inc2 = ladder_inc(m->pitch16, c2, m->fine + fine[1], r2);
    if (r3 == LD_LO)
        inc3 = ladder_inc(clamp(p3 + LXP(t, LX_O3FRQ) * 36 * 16 / 56, 0, 2047), 0, m->fine + fine[2], LD_LO);
    else
        inc3 = ladder_inc(p3, c3, m->fine + fine[2], r3);

    /* the cutoff in parameter steps (Q8; 1 step ~0.86 semitone): CUTOFF, the track's modulation (ENV / LFO DEST,
     * the matrix), KBD (FULL: one step per 0.86 semitone, the filter follows the key), CONTOUR (up to ~70 steps,
     * five octaves), FILT MOD (the modulation source at the block's start, +-28 steps: two octaves) */
    cut = (p[P_E0] << 8) + m->cutoff;
    cut += (int32_t)p[P_E3] * ((m->pitch16 - 60 * 16) * 299 / 16) / 3;
    cut += (envq * p[P_E2] * 70) >> 14;
    if (fltmod)
        cut += (msrc * mdep * 56) >> 14;
    cut = clamp(cut, 0, 127 << 8);
    {   /* the ladder's coefficients for the block (zero-delay feedback, V. Zavalishin's TPT one-poles):
         * g = tan(pi fc / FS) from SVF_G (Q12), G = g / (1 + g), b = 1 / (1 + g) (Q14), the feedback k (Q12, 0 .. 4.3:
         * the poles cross at 4; 4.8 at the top: a strong, quick whistle) and 1 / (1 + k G^4) (Q14; G^4 in 64 bits,
         * once a block: tiny at low cutoffs) */
        int32_t ci = cut >> 8, g = SVF_G[ci];
        int64_t G4;
        if (ci < 127)
            g += ((SVF_G[ci + 1] - g) * (cut & 255)) >> 8;
        G = (g << 14) / (4096 + g);
        b = 16384 - G;
        G4 = ((int64_t)G * G * G * G) >> 42;           /* Q14 */
        k = p[P_E1] * 155;                            /* EMPHASIS (Q12): sings from ~106 (8.3 of 10) up */
        rd = (int32_t)(((int64_t)16384 << 12) / (4096 + ((k * G4) >> 14)));   /* Q14 */
    }
    for (i = 0; i < n; i++) {
        int32_t a, bsig, c, s, u, y, vv, nz;
        uint32_t i1 = inc1, i2 = inc2;
        c = need3 ? ladder_wave(w3, ph2, inc3, pwm, 1) : 0;
        nz = neednz ? (int32_t)(noise32(&nst) >> 17) - 16384 : 0;   /* white, Q15 / 2 */
        if (needpk) {                                   /* Kellet's economy pink (Q15 coefficients) on white / 16: the */
            int32_t w = nz >> 4;                      /* states stay below ~44000, the products in 32 bits */
            pk0 = ((pk0 * 32690) >> 15) + ((w * 3245) >> 15);
            pk1 = ((pk1 * 31556) >> 15) + ((w * 9716) >> 15);
            pk2 = ((pk2 * 18678) >> 15) + ((w * 17247) >> 14);
            nz = clamp((pk0 + pk1 + pk2 + ((w * 6056) >> 15)) * LADDER_PINK >> 4, -16384, 16384);
        }
        nzl = nz;
        if (oscmod) {                                 /* OSC MOD: exponential, up to +-1 octave at full */
            int32_t d;
            msrc = ((c >> 1) * (127 - mmix) + nz * mmix) >> 7;   /* MOD MIX: OSC 3 .. NOISE, Q15 / 2 */
            d = (msrc * mdep) >> 10;                  /* ~ -2048..2048: +-1 octave in 1/2048 */
            uint32_t f = (uint32_t)(65536 + d * 22 + d * d / 266);   /* 2^(d / 2048), Q16 (a parabola: within 3 %) */
            i1 = (uint32_t)mulq16((int32_t)(inc1 >> 1), f) << 1;
            i2 = (uint32_t)mulq16((int32_t)(inc2 >> 1), f) << 1;
            i1 = i1 > 0x70000000u ? 0x70000000u : i1;
            i2 = i2 > 0x70000000u ? 0x70000000u : i2;
        }
        a = l1 ? ladder_wave(w1, ph0, i1, pwm, 0) : 0;
        bsig = l2 ? ladder_wave(w2, ph1, i2, pwm, 0) : 0;
        ph0 += i1;
        ph1 += i2;
        ph2 += inc3;
        /* the mixer: a full oscillator is 1/2 scale at the filter's input */
        s = ((a * l1) >> 15) + ((bsig * l2) >> 15) + ((c * l3) >> 15) + ((nz * ln) >> 14);
        s += nz >> 7;                                 /* the circuit's own hiss (-48 dB): what starts self-oscillation */
        if (ovl > 16384)
            s = (s * (ovl >> 4)) >> 10;
        /* the ladder: u = (in - k S) / (1 + k G^4), S the stages' response to no input (Horner: b (s4 + G (s3 +
         * G (s2 + G s1)))), the input stage a tanh of ceiling LADDER_SAT (a full oscillator is near it: more of them
         * or OVERLOAD saturate, they do not get much louder), then four TPT one-poles. Each stage's product keeps the
         * bits its shift drops for the next sample (error feedback): at low cutoffs a state moves by less than one unit
         * a sample, and a plain shift would stall it (no self-oscillation below ~1 kHz, a creeping offset) */
        {
            int32_t S = (b * (s4 + ((G * (s3 + ((G * (s2 + ((G * s1) >> 14))) >> 14))) >> 14))) >> 14;
            /* (bounds: |s| < 270000 and |k S| < 180000, so the products below stay in 32 bits; the tanh bounds u to
             * LADDER_SAT and a TPT one-pole's state to twice its input: the stages need no clamps) */
            u = (((s - ((k * (S >> 2)) >> 10)) >> 2) * rd) >> 12;
            u = (softclip(((u >> 2) * LADDER_SATK) >> 10) * LADDER_SAT) >> 15;
            vv = (u - s1) * G + e1; e1 = vv & 16383; vv >>= 14; y = vv + s1; s1 = y + vv;
            vv = (y - s2) * G + e2; e2 = vv & 16383; vv >>= 14; y = vv + s2; s2 = y + vv;
            vv = (y - s3) * G + e3; e3 = vv & 16383; vv >>= 14; y = vv + s3; s3 = y + vv;
            vv = (y - s4) * G + e4; e4 = vv & 16383; vv >>= 14; y = vv + s4; s4 = y + vv;
        }
        /* the loss of bass with EMPHASIS, partly made up (the original drops more), then the level */
        y = y + ((y * p[P_E1]) >> 8);
        /* the output's coupling: a ~3 Hz high-pass (octave-related oscillators through the tanh make DC, as on the
         * original, whose output is AC coupled too) */
        dc += (y * 256 - dc) >> 11;
        y -= dc >> 8;
        y = soft_knee((y * LADDER_OUT) >> 4, 44000);
        out[i] += voice_amp(y, m, i);
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->ph[2] = ph2;
    v->s[0] = s1;
    v->s[1] = s2;
    v->s[4] = s3;
    v->s[5] = s4;
    v->s[6] = nst;
    if (!oscmod)                                      /* the source FILT MOD reads next block: once a block is enough */
        msrc = ((ladder_wave(w3, ph2, inc3, pwm, 1) >> 1) * (127 - mmix) + nzl * mmix) >> 7;
    v->s[3] = msrc;
    x->pk[0] = pk0;
    x->pk[1] = pk1;
    x->pk[2] = pk2;
    x->er[0] = e1;
    x->er[1] = e2;
    x->er[2] = e3;
    x->er[3] = e4;
    x->dc = dc;
}

/* the extra parameters of a preset (LADDER_PRESETS order), the track's GLIDE last */
typedef struct {
    int8_t x[LX_COUNT];
    uint8_t glide;
} ladder_xp_t;

/*                      E: CUT EMPH CONT KBD  F.ATK F.DEC F.SUS DECAY
 *                      X: O1RNG O1WAV O2RNG O2WAV  O2FRQ O3RNG O3FRQ O3WAV  MIX1 MIX2 MIX3 MIXN  NCOL OVLD O3KBD DRIFT
 *                         MODMIX MODAMT OSCMOD FLTMOD  | GLIDE */
static const preset_t LADDER_PRESETS[] = {
    {"BERLIN BASS", {40, 50, 72, 1, 0, 52, 8, 1}, {0, 70, 100, 30}, 0, 1, FX(0, 0, 18, 12)},
    {"FAT LEAD", {68, 38, 46, 2, 6, 64, 70, 1}, {2, 70, 110, 45}, 0, 1, FX(0, 10, 40, 30)},
    {"PEDAL SUB", {26, 22, 48, 1, 0, 74, 40, 1}, {0, 80, 110, 50}, 0, 1, FX(0, 0, 0, 8)},
    {"SEQ PLUCK", {54, 80, 92, 1, 0, 46, 0, 0}, {0, 66, 0, 24}, 0, 1, FX(0, 0, 45, 25)},
    {"BRASS", {38, 26, 66, 2, 34, 70, 64, 1}, {24, 70, 104, 46}, 0, 1, FX(0, 20, 20, 40)},
    {"WHISTLE", {64, 127, 0, 3, 0, 0, 127, 1}, {10, 70, 120, 50}, 0, 1, FX(0, 0, 50, 50)},
    {"WOBBLE", {30, 70, 30, 1, 0, 60, 60, 1}, {0, 70, 110, 30}, 0, 1, FX(0, 0, 20, 10)},
    {"VIBRATO LEAD", {74, 30, 30, 2, 10, 70, 80, 1}, {6, 70, 110, 55}, 0, 1, FX(0, 10, 45, 45)},
    {"FM GROWL", {48, 44, 56, 1, 0, 60, 40, 1}, {0, 70, 100, 30}, 0, 1, FX(10, 0, 20, 15)},
    {"NOISE RISER", {10, 110, 116, 0, 106, 100, 120, 1}, {90, 70, 120, 80}, 0, 1, FX(0, 0, 40, 70)},
    {"OVERDRIVE", {44, 40, 58, 1, 0, 58, 30, 1}, {0, 70, 100, 30}, 0, 1, FX(0, 0, 10, 10)},
    {"DRONE", {36, 58, 40, 1, 80, 110, 70, 1}, {90, 90, 120, 100}, 0, 1, FX(0, 30, 40, 80)},
    {"FLUTE", {58, 12, 20, 2, 30, 60, 100, 1}, {40, 70, 110, 60}, 0, 1, FX(0, 20, 30, 50)},
    {"KICK BASS", {30, 60, 100, 2, 0, 46, 0, 0}, {0, 62, 0, 26}, 0, 1, FX(0, 0, 0, 0)},
};
static const ladder_xp_t LADDER_XP[NELEM(LADDER_PRESETS)] = {
    /* BERLIN BASS: three saws, OSC 2 a hair sharp, OSC 3 an octave down; sequencer bass */
    {{LD_16, LW_SAW, LD_16, LW_SAW, 3, LD_32, 0, LW_SAW, 127, 110, 80, 0, 0, 0, 1, 18, 0, 0, 0, 0}, 0},
    /* FAT LEAD: detuned saws and a square at 4', glide */
    {{LD_8, LW_SAW, LD_8, LW_SAW, -4, LD_4, 0, LW_SQR, 127, 120, 70, 0, 0, 0, 1, 24, 0, 0, 0, 0}, 38},
    /* PEDAL SUB: the bass pedal sound: two 32' saws and a 16' triangle, dark */
    {{LD_32, LW_SAW, LD_32, LW_SAW, 2, LD_16, 0, LW_TRI, 127, 100, 90, 0, 0, 0, 1, 12, 0, 0, 0, 0}, 0},
    /* SEQ PLUCK: resonant zap for 16th sequences, no sustain */
    {{LD_8, LW_SAW, LD_8, LW_SQR, 5, LD_16, 0, LW_SAW, 127, 110, 80, 0, 0, 20, 1, 16, 0, 0, 0, 0}, 0},
    /* BRASS: the filter opens slowly, two detuned saws */
    {{LD_8, LW_SAW, LD_8, LW_SAW, -5, LD_16, 0, LW_SAW, 127, 127, 50, 0, 0, 0, 1, 26, 0, 0, 0, 0}, 0},
    /* WHISTLE: the filter self-oscillating as a sine, no oscillators, the key tracking in full */
    {{LD_8, LW_TRI, LD_8, LW_TRI, 0, LD_8, 0, LW_TRI, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0}, 30},
    /* WOBBLE: OSC 3 a slow triangle on the cutoff (FILT MOD) */
    {{LD_16, LW_SAW, LD_16, LW_SQR, 4, LD_LO, 20, LW_TRI, 127, 100, 0, 0, 0, 0, 0, 14, 0, 96, 0, 1}, 0},
    /* VIBRATO LEAD: OSC 3 a ~5 Hz triangle on the pitch (OSC MOD), light */
    {{LD_8, LW_SAW, LD_8, LW_WIDE, 6, LD_LO, 30, LW_TRI, 127, 90, 0, 0, 0, 0, 0, 20, 0, 34, 1, 0}, 44},
    /* FM GROWL: OSC 3 at audio rate a fifth up, silent, frequency-modulating OSC 1 and 2 */
    {{LD_16, LW_SAW, LD_16, LW_SQR, 0, LD_8, 56, LW_SAW, 127, 100, 0, 0, 0, 0, 1, 10, 0, 44, 1, 0}, 0},
    /* NOISE RISER: pink noise through a resonant filter that opens over ~6 s */
    {{LD_8, LW_SAW, LD_8, LW_SAW, 0, LD_8, 0, LW_TRI, 0, 0, 0, 127, 1, 0, 1, 0, 0, 0, 0, 0}, 0},
    /* OVERDRIVE: everything up and OVERLOAD: the mixer feedback growl */
    {{LD_16, LW_SAW, LD_16, LW_SHARK, 7, LD_32, 0, LW_SQR, 127, 127, 127, 0, 0, 90, 1, 20, 0, 0, 0, 0}, 0},
    /* DRONE: 32' saws a fifth apart, slow contour, lots of drift */
    {{LD_32, LW_SAW, LD_32, LW_SAW, 56, LD_16, -2, LW_SHARK, 127, 110, 80, 10, 1, 0, 1, 60, 0, 0, 0, 0}, 0},
    /* FLUTE: triangle and a breath of noise, slow attack, a touch of vibrato */
    {{LD_8, LW_TRI, LD_4, LW_TRI, 1, LD_LO, 34, LW_TRI, 127, 30, 0, 26, 0, 0, 0, 16, 0, 22, 1, 0}, 20},
    /* KICK BASS: a short, deep thump with a click of contour */
    {{LD_16, LW_SQR, LD_32, LW_TRI, 0, LD_32, 0, LW_TRI, 127, 127, 0, 0, 0, 40, 1, 6, 0, 0, 0, 0}, 0},
};

/* a sound load (engine_xp.c xp_load, after the defaults): the twenty extra values and GLIDE of preset pi; pi < 0 (CLEAN
 * INIT): the defaults, already in */
static void ladder_load(track_t *t, int32_t pi)
{
    uint32_t k;
    if (pi < 0)
        return;
    for (k = 0; k < LX_COUNT; k++)
        t->xp[k] = LADDER_XP[(uint32_t)pi % NELEM(LADDER_XP)].x[k];
    t->p[P_GLIDE] = LADDER_XP[(uint32_t)pi % NELEM(LADDER_XP)].glide;
}
static const char *const LADDER_PAGES[] = {"OSC 1-2", "OSC 2-3", "MIX", "MIX 2", "CTRL"};
static const xp_info_t LADDER_XPI = {LADDER_X, LX_COUNT, LADDER_PAGES, ladder_load};
_Static_assert(LX_COUNT <= NXP && (LX_COUNT + 3u) / 4u == NELEM(LADDER_PAGES), "LADDER: its pages and values");

static const engine_t ENG_LADDER = {
    .name = "LADDER",
    .page_title = {"FILTER", "CONTOUR"},
    .edit = {
        {"CUTOFF", F_CUTOFF, 0, 127, 64, 0, 0},
        {"EMPH", F_PCT, 0, 127, 20, 0, 0},
        {"CONTOR", F_PCT, 0, 127, 50, 0, 0},
        {"KBD", F_ENUM, 0, 3, 1, N_LD_KBD, 0},
        {"F ATK", F_TIME, 0, 127, 0, 0, 0},
        {"F DEC", F_TIME, 0, 127, 60, 0, 0},
        {"F SUS", F_PCT, 0, 127, 60, 0, 0},
        {"DECAY", F_ENUM, 0, 1, 1, N_LD_SW, 0},
    },
    .presets = LADDER_PRESETS,
    .npresets = NELEM(LADDER_PRESETS),
    .note_on = ladder_note_on,
    .render = ladder_render,
    .knob = {P_E0, P_E1, P_E2, P_GLIDE},
    .keep = 0xF3,                /* the ladder (s0 s1 s4 s5), the noise (s6), the filter contour (s7) */
    .poly = 4,                   /* POLY and UNISON: at most four (a voice costs ~2.5 x ANALOG's; tests/ladder_test.c) */
    .mem = sizeof(ladder_part_t),
};
_Static_assert(NELEM(LADDER_XP) == NELEM(LADDER_PRESETS), "LADDER: a row of extra values per preset");
