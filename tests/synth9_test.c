/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 3dSam (Clipper) */
/* Clipper 0.9 (CLIPPER_SYNTH 1): the filter MODELs, VINTAGE, the TUNING, the arp's TURNG, LOOP; and the sound's
 * cleanliness (no click, no blow-up) where 0.9 touches it.
 *   MODELS   every model bounded at full resonance on a loud chord (ANALOG, WAVE, VECTOR); each one its response:
 *            the low-passes keep the bass and lose the top, HP the other way round, BP a peak, NOTCH a dip; MS20 and
 *            DIODE sing at full resonance; a MODEL changed under a held chord: no click (the duck)
 *   VINTAGE  0: every voice in tune; 127: each voice its own tuning, within +-15 cents, and a slow drift
 *   TUNING   EQUAL: in tune; JUST: E a pure third (-13.7 cents); 19-EDO: one key a 19th of an octave; a USER scale; ROOT
 *   TURNG    MUTAT 0: a loop of LOOP notes, the held notes only; another SEED, another loop; MUTAT up: it changes
 *   LOOP     SPICE's variation comes round every LOOP notes
 *   STORE    projects (VINTAGE, LOOP of both sounds; a 0.8 project: their defaults), user presets (the side's 8 values;
 *            a 0.8 side: its 6, the 2 new at defaults)
 *   CLEAN    a cutoff swept by the knob's steps on every model: no click; a full mix at its loudest: no sample at the
 *            rail */
#define main hostsim_main
#include <stddef.h>
#include "hostsim.c"
#undef main
#define PROJ_HOST 1
static const uint8_t TRK_DEF[NPART][2] = {{ENGI_HYBRID, 0}, {1, 0}, {4, 5}};   /* (ui.c, Clipper) */
static uint32_t trk_def_engine(uint32_t i) { return i < NPART ? TRK_DEF[i][0] : 0u; }
#include "../firmware/src/project.c"
#define UP_HOST 1
#include "../firmware/src/upreset.c"

static int fails;
static void ok(int c, const char *what)
{
    printf("synth9: %-96s %s\n", what, c ? "ok" : "FAIL");
    fails += !c;
}

#define NCAP (4096u * 24u)
static int32_t cl[NCAP], cr[NCAP];
static uint32_t ncap;
static void run(uint32_t nb)
{
    static int32_t o[CTL * 2];
    uint32_t b, i;
    ncap = 0;
    for (b = 0; b < nb; b++) {
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            if (ncap < NCAP) {
                cl[ncap] = o[2 * i];
                cr[ncap++] = o[2 * i + 1];
            }
    }
}
static void quiet(void)
{
    uint32_t i, k;
    for (i = 0; i < NPART; i++)
        trk_all_off(&trk[i]);
    run(400);
    for (i = 0; i < NPART; i++)
        for (k = 0; k < NVOICE; k++)
            trk[i].v[k].active = trk[i].v[k].gate = 0, trk[i].v[k].stage = 0, trk[i].v[k].env = trk[i].v[k].env_out = 0;
    run(300);
}
static void plain(track_t *t, uint32_t eng, uint32_t pi)
{
    host_preset(t, eng, pi);
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    t->p[P_LD_PIT] = t->p[P_LD_FLT] = t->p[P_LD_SHP] = t->p[P_LD_AMP] = t->p[P_ED_PIT] = t->p[P_ED_FLT] = 0;
    t->p[P_ATK] = 0, t->p[P_SUS] = 127, t->p[P_REL] = 10, t->p[P_VOICE] = V_POLY, t->p[P_PAN] = 0, t->p[P_GLIDE] = 0;
    t->p[P_MWDST] = t->p[P_ATDST] = MD_OFF;
    t->p[P_SPREAD] = 0;                                 /* (1.0: the pads come with some) */
    t->p[P_AMODE] = 0, t->p[P_VINT] = 0, t->p[P_FMODEL] = FM_SVF;
    t->p[P_ITYPE] = 0, t->p[P_IMIX] = 127;
}
static double mag(double f, uint32_t a, uint32_t n)
{
    double re = 0, im = 0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        double w = 0.5 - 0.5 * cos(2 * M_PI * i / n), x = cl[a + i] * w;
        re += x * cos(2 * M_PI * f * i / FS);
        im += x * sin(2 * M_PI * f * i / FS);
    }
    return sqrt(re * re + im * im) / n;
}
/* the frequency of the strongest partial near f (+-4 %), to a hundredth of a cent or so */
static double freq_near(double f, uint32_t a, uint32_t n)
{
    double best = f, bm = 0, step = f * 0.0005, x;
    for (x = f * 0.96; x <= f * 1.04; x += step) {
        double m = mag(x, a, n);
        if (m > bm)
            bm = m, best = x;
    }
    for (step /= 20.0, x = best - 20 * step; x <= best + 20 * step; x += step) {
        double m = mag(x, a, n);
        if (m > bm)
            bm = m, best = x;
    }
    return best;
}
static double cents(double f, double ref) { return 1200.0 * log2(f / ref); }
static int bounded(void)
{
    uint32_t i;
    for (i = 0; i < ncap; i++)
        if (cl[i] >= (1 << 23) || cl[i] <= -(1 << 23) || cr[i] >= (1 << 23) || cr[i] <= -(1 << 23))
            return 0;
    return 1;
}
static int32_t d2max(uint32_t a, uint32_t n)           /* the largest second difference: a click's mark */
{
    int32_t mx = 0;
    uint32_t i;
    for (i = a + 2u; i < a + n && i < ncap; i++) {
        int32_t d = cl[i] - 2 * cl[i - 1] + cl[i - 2];
        d = d < 0 ? -d : d;
        mx = d > mx ? d : mx;
    }
    return mx;
}
static double rms(uint32_t a, uint32_t n)
{
    double e = 0;
    uint32_t i;
    for (i = a; i < a + n && i < ncap; i++)
        e += (double)cl[i] * cl[i];
    return sqrt(e / n);
}

static const char *const MN[9] = {"", "SVF", "LADDR", "SKEY", "DIODE", "2POLE", "NOTCH", "BP", "HP"};
static void t_models(void)
{
    static const uint32_t engs[3] = {0, ENGI_WAVE, ENGI_VECTOR};
    static const uint8_t ch[4] = {36, 48, 55, 64};
    track_t *t = &trk[0];
    uint32_t e, m, i, good = 1;
    char w[200];
    for (e = 0; e < 3u; e++)                            /* full resonance, a loud chord, every model: bounded */
        for (m = FM_SVF; m <= FM_LAST; m++) {
            quiet();
            plain(t, engs[e], 0);
            t->p[P_FMODEL] = (int16_t)m;
            if (engs[e] != ENGI_VECTOR)
                t->p[P_E5] = 127;                       /* (RES) */
            for (i = 0; i < 4u; i++)
                trk_note_on(t, ch[i], 127);
            run(1200);
            if (!bounded()) {
                good = 0;
                printf("  %s %s: out of bounds\n", ENGINES[engs[e]]->name, MN[m]);
            }
        }
    ok(good, "MODELS: every one at full resonance on a loud chord (ANALOG, WAVE, VECTOR): bounded");
    {   /* the responses: a saw at A2 (110 Hz), the cutoff ~1 kHz: the 2nd harmonic against the 20th (2.2 kHz) */
        double lo[9], hi[9], h5[9];
        for (m = FM_SVF; m <= FM_LAST; m++) {
            quiet();
            plain(t, 0, 0);
            t->p[P_E0] = 0, t->p[P_E2] = 0, t->p[P_E3] = 0, t->p[P_E4] = 70, t->p[P_E5] = 40, t->p[P_E6] = 0, t->p[P_E7] = 0;
            t->p[P_FMODEL] = (int16_t)m;
            trk_note_on(t, 45, 100);
            run(700);
            lo[m] = mag(220, 8192, 8192) / 0.5;         /* (the saw's own falls as 1 / k) */
            hi[m] = mag(2200, 8192, 8192) / 0.05;
            h5[m] = mag(550, 8192, 8192) / 0.2;
        }
        good = 1;
        for (m = FM_SVF; m <= FM_SEM; m++)
            if (!(lo[m] > (m == FM_MOOG || m == FM_DIODE ? 4.0 : 3.0) * hi[m])) {
                good = 0;
                printf("  %s: low %.1f high %.1f\n", MN[m], lo[m], hi[m]);
            }
        ok(good, "MODELS: SVF MOOG MS20 DIODE SEM low-pass (the bass kept, the top 10 dB down, 12 for MOOG and DIODE)");
        ok(hi[FM_HP] > 4.0 * lo[FM_HP], "MODELS: HP the other way round");
        ok(lo[FM_NOTCH] > 0.3 * lo[FM_SVF] && hi[FM_NOTCH] > 2.0 * hi[FM_SVF], "MODELS: NOTCH keeps both ends");
        ok(lo[FM_BP] < 0.5 * lo[FM_SVF] && hi[FM_BP] < 0.85 * hi[FM_HP] && h5[FM_BP] > lo[FM_BP],
           "MODELS: BP a band (the bass down against SVF, the top against HP, the middle up)");
    }
    {   /* MS20 and DIODE sing at full resonance (a quiet note: the resonance rings on) */
        double r[2];
        for (i = 0; i < 2u; i++) {
            quiet();
            plain(t, 0, 0);
            t->p[P_E0] = 3, t->p[P_E4] = 70, t->p[P_E5] = 127;   /* a sine: the ring is the filter's */
            t->p[P_FMODEL] = (int16_t)(i ? FM_DIODE : FM_MS20);
            trk_note_on(t, 33, 20);
            run(800);
            r[i] = rms(16384, 8192);
        }
        ok(r[0] > 200 && r[1] > 200, "MODELS: MS20 and DIODE ring at full resonance");
    }
    {   /* a MODEL changed under a held chord: no click */
        int32_t steady, sw = 0;
        quiet();
        plain(t, 0, 0);
        t->p[P_E4] = 80, t->p[P_E5] = 60;
        for (i = 0; i < 3u; i++)
            trk_note_on(t, ch[i + 1u], 100);
        run(600);
        steady = d2max(4096, 12000);
        for (m = FM_SVF + 1u; m <= FM_LAST + 1u; m++) {
            int32_t a, b2;
            run(200);
            a = d2max(0, ncap);
            t->p[P_FMODEL] = (int16_t)(m > FM_LAST ? FM_SVF : m);
            run(16);
            b2 = d2max(0, ncap);
            run(200);
            a = a > d2max(0, ncap) ? a : d2max(0, ncap);   /* (the steady sound before and after) */
            if (b2 > a * 3 / 2 + 64) {
                sw = b2;
                printf("  to %s: %d (steady %d)\n", MN[m > FM_LAST ? FM_SVF : m], b2, a);
            }
        }
        (void)steady;
        ok(!sw, "MODELS: changed under a held chord, every one in turn: no click (a 6 ms S-curve duck)");
    }
}

static void t_vintage(void)
{
    track_t *t = &trk[0];
    uint32_t i;
    double f[8], spread0 = 0, spread = 0, mx = 0;
    for (i = 0; i < 8u; i++) {                          /* VINTAGE 0: one note on each voice in turn: in tune */
        quiet();
        plain(t, 0, 0);
        t->p[P_E0] = 3, t->p[P_E4] = 127, t->p[P_E5] = 0;
        t->v[i].age = 0;
        trk_note_on(t, 69, 100);
        run(450);
        f[i] = cents(freq_near(440, 4096, 8192), 440);
        spread0 = fabs(f[i]) > spread0 ? fabs(f[i]) : spread0;
    }
    ok(spread0 < 0.3, "VINTAGE 0: every note in tune (within 0.3 cent)");
    {
        double c[NVOICE];
        uint32_t k, n = 0;
        quiet();
        plain(t, 0, 0);
        t->p[P_E0] = 3, t->p[P_E4] = 127, t->p[P_E5] = 0, t->p[P_VINT] = 127;
        for (k = 0; k < NVOICE; k++) {                  /* each voice its own card: the same note on each */
            uint32_t j;
            for (j = 0; j < NVOICE; j++)
                t->v[j].active = 0;
            trk_note_on(t, 69, 100);
            run(450);
            c[n++] = cents(freq_near(440, 4096, 8192), 440);
            trk_note_off(t, 69);
            run(60);
        }
        for (k = 0; k < n; k++) {
            mx = fabs(c[k]) > mx ? fabs(c[k]) : mx;
            for (i = 0; i < n; i++)
                spread = fabs(c[k] - c[i]) > spread ? fabs(c[k] - c[i]) : spread;
        }
        ok(spread > 2.0 && mx < 15.0, "VINTAGE 127: the voices each their own tuning (spread over 2 cents, none past 15)");
    }
    {   /* a held note drifts slowly */
        double a, b;
        quiet();
        plain(t, 0, 0);
        t->p[P_E0] = 3, t->p[P_E4] = 127, t->p[P_E5] = 0, t->p[P_VINT] = 127;
        trk_note_on(t, 69, 100);
        run(2900);
        a = cents(freq_near(440, 2048, 8192), 440);
        b = cents(freq_near(440, ncap - 8192, 8192), 440);
        run(2900);
        b = fabs(cents(freq_near(440, ncap - 8192, 8192), 440) - a) > fabs(b - a) ? cents(freq_near(440, ncap - 8192, 8192), 440) : b;
        ok(fabs(b - a) > 0.2 && fabs(b - a) < 12.0, "VINTAGE 127: a held note drifts, slowly and a little");
        t->p[P_VINT] = 0;
    }
}

static void t_tuning(void)
{
    track_t *t = &trk[0];
    double e, j, k19, u;
    quiet();
    plain(t, 0, 0);
    t->p[P_E0] = 3, t->p[P_E4] = 127, t->p[P_E5] = 0;
    tune_cfg.sys = TU_EQUAL, tune_cfg.root = 0;
    tune_build();
    trk_note_on(t, 64, 100);
    run(450);
    e = cents(freq_near(329.628, 4096, 8192), 329.628);
    quiet();
    tune_cfg.sys = TU_JUST;
    tune_build();
    trk_note_on(t, 64, 100);
    run(450);
    j = cents(freq_near(329.628, 4096, 8192), 329.628);
    ok(fabs(e) < 0.3 && fabs(j + 13.7) < 0.6 && tun_on, "TUNING: EQUAL in tune; JUST: E4 a pure third (-13.7 cents)");
    quiet();
    tune_cfg.sys = TU_19;
    tune_build();
    trk_note_on(t, 61, 100);
    run(450);
    k19 = cents(freq_near(261.626 * pow(2, 1 / 19.0), 4096, 8192), 261.626);
    ok(fabs(k19 - 1200.0 / 19) < 0.6, "TUNING: 19-EDO: C#4 a 19th of an octave above C4");
    quiet();
    tune_cfg.sys = TU_USER, tune_cfg.n = 3, tune_cfg.period = 19200;   /* 0, 400, 700 cents */
    tune_cfg.deg[1] = 400 * 16, tune_cfg.deg[2] = 700 * 16, tune_cfg.root = 2;   /* from D4 */
    tune_build();
    trk_note_on(t, 64, 100);                            /* D4 + 2 keys: the third degree, 700 cents above D4 */
    run(450);
    u = cents(freq_near(293.665 * pow(2, 7 / 12.0), 4096, 8192), 293.665);
    ok(fabs(u - 700) < 0.6, "TUNING: a USER scale of 3 degrees from ROOT D: E4 is its 3rd degree (D + 700 cents)");
    quiet();
    tune_cfg.sys = TU_RAST, tune_cfg.root = 0;
    tune_build();
    trk_note_on(t, 71, 100);
    run(450);
    u = cents(freq_near(493.883, 4096, 8192), 493.883);
    ok(fabs(u + 50) < 0.6, "TUNING: RAST: B half flat (-50 cents)");
    trk_note_off(t, 71);
    {   /* (1.0) A4: the reference pitch, 400 .. 480 Hz, under every system */
        static const int hz[3] = {432, 415, 480};
        double c[3], jj;
        uint32_t i;
        tune_cfg.sys = TU_EQUAL;
        for (i = 0; i < 3u; i++) {
            quiet();
            tune_cfg.a4 = (int8_t)(hz[i] - 440);
            tune_build();
            trk_note_on(t, 69, 100);
            run(450);
            c[i] = cents(freq_near(hz[i], 4096, 8192), hz[i]);
            trk_note_off(t, 69);
        }
        quiet();
        tune_cfg.sys = TU_JUST, tune_cfg.a4 = 432 - 440;
        tune_build();
        trk_note_on(t, 64, 100);
        run(450);
        jj = cents(freq_near(329.628 * 432 / 440, 4096, 8192), 329.628 * 432 / 440);
        trk_note_off(t, 64);
        printf("synth9:   A4 432 / 415 / 480 Hz: A4 off by %.2f / %.2f / %.2f cents; JUST at 432: E4 %.2f\n", c[0], c[1], c[2], jj);
        ok(fabs(c[0]) < 0.6 && fabs(c[1]) < 0.6 && fabs(c[2]) < 0.6 && fabs(jj + 13.7) < 0.8 && tun_on,
           "TUNING: A4 at 432, 415, 480 Hz (EQUAL: on with a reference other than 440); JUST on top of it");
        tune_cfg.a4 = 0;
    }
    tune_cfg.sys = TU_EQUAL;
    tune_build();
    ok(!tun_on, "TUNING: EQUAL at 440: off (nothing touched)");
    quiet();
}

/* the arp's notes over k steps */
static void arp_take(uint32_t mode, uint32_t mut, uint32_t seed, uint32_t loop, uint8_t *notes, uint32_t k)
{
    track_t *t = &trk[0];
    uint32_t n = 0, b, last;
    quiet();
    plain(t, 0, 0);
    t->p[P_AMODE] = (int16_t)mode, t->p[P_ARATE] = 2, t->p[P_AGATE] = 64, t->p[P_APROB] = 127, t->p[P_AOCT] = 2;
    t->p[P_SPICE] = (int16_t)mut, t->p[P_DICE] = (int16_t)seed, t->p[P_SLOOP] = (int16_t)loop;
    t->arp_idx = 0, t->tm_seed = 0;
    input_on(t, 60, 100);
    input_on(t, 63, 100);
    input_on(t, 67, 100);
    last = t->arp_idx;
    for (b = 0; b < 12000u && n < k; b++) {
        run(1);
        if (t->arp_idx != last)
            last = t->arp_idx, notes[n++] = t->arp_note;
    }
    input_off(t, 60);
    input_off(t, 63);
    input_off(t, 67);
    t->p[P_AMODE] = 0, t->p[P_SPICE] = 0, t->p[P_DICE] = 0, t->p[P_SLOOP] = 16;
    run(30);
}
static void t_turing(void)
{
    uint8_t a[48], b[48], c[48], d[48];
    uint32_t i, per = 1, inkey = 1, diff = 0;
    arp_take(AM_TURING, 0, 3, 8, a, 48);
    for (i = 8; i < 48u; i++)
        per &= a[i] == a[i - 8u];
    for (i = 0; i < 48u; i++) {
        uint32_t pc = a[i] % 12u;
        inkey &= a[i] >= 60 && a[i] < 84 && (pc == 0u || pc == 3u || pc == 7u);
    }
    ok(per && inkey, "TURNG MUTAT 0, LOOP 8: a loop of 8 notes, every one a held note (over OCT 2)");
    arp_take(AM_TURING, 0, 4, 8, b, 48);
    ok(memcmp(a, b, 48) != 0, "TURNG: another SEED, another loop");
    arp_take(AM_TURING, 0, 3, 8, c, 48);
    ok(!memcmp(a, c, 48), "TURNG: the same SEED, the same loop");
    arp_take(AM_TURING, 100, 3, 8, d, 48);
    for (i = 8; i < 48u; i++)
        diff += d[i] != d[i - 8u];
    ok(diff >= 4u, "TURNG MUTAT 100: the loop changes as it goes");
    {   /* LOOP: SPICE's variation every LOOP notes (UP over 3 notes and LOOP 6: a period of 6) */
        uint8_t s[48];
        uint32_t same = 1;
        arp_take(AM_UP, 127, 5, 6, s, 48);
        for (i = 6; i < 48u; i++)
            same &= s[i] == s[i - 6u];
        ok(same, "LOOP 6: SPICE's variation comes round every 6 notes");
    }
}

static project_t pj;
static void t_store(void)
{
    track_t *a = &trk[0], *b = &trk[1];
    quiet();
    plain(a, 0, 0);
    plain(b, ENGI_JUNO, 0);
    a->p[P_VINT] = 99, a->p[P_SLOOP] = 5, b->p[P_VINT] = 17, b->p[P_SLOOP] = 12, a->p[P_FMODEL] = FM_DIODE;
    a->p[P_MSLEN] = 7, a->p[P_DICE] = 44;
    proj_capture(&pj);
    a->p[P_VINT] = b->p[P_VINT] = 0, a->p[P_SLOOP] = b->p[P_SLOOP] = 16, a->p[P_FMODEL] = FM_SVF, a->p[P_MSLEN] = 16;
    proj_apply(&pj, 1);
    ok(a->p[P_VINT] == 99 && a->p[P_SLOOP] == 5 && b->p[P_VINT] == 17 && b->p[P_SLOOP] == 12 && a->p[P_FMODEL] == FM_DIODE &&
       a->p[P_MSLEN] == 7 && a->p[P_DICE] == 44 && sizeof pj == 3840u,
       "project: VINTAGE, LOOP of both sounds and the MODEL kept (with 0.8's), the size unchanged");
    {   /* as 0.8 wrote it: track 4's 0.9 bytes and mark not there */
        uint32_t k;
        proj_capture(&pj);
        for (k = 0; k < NLOCK; k++) {
            plock_t *l = &pj.t[3].lock[k];
            uint32_t n0 = 3u * k;
            if (n0 + 2u >= PROJ_X9_AT) {
                if (n0 >= PROJ_X9_AT)
                    l->param = 0;
                if (n0 + 1u >= PROJ_X9_AT)
                    l->val = (int16_t)((uint16_t)l->val & (n0 + 1u >= PROJ_X9_AT ? 0x0000u : 0x00FFu));
                if (n0 + 1u < PROJ_X9_AT)
                    l->val = (int16_t)((uint16_t)l->val & 0x00FFu);
            }
        }
        proj_apply(&pj, 1);
        ok(a->p[P_VINT] == 0 && a->p[P_SLOOP] == 16 && a->p[P_MSLEN] == 7 && a->ms[0][0] == MSEQ_INIT[0][0],
           "a project of 0.8: its MOD SEQ read, VINTAGE and LOOP at their defaults");
    }
    {   /* user presets */
        up_rec_t r;
        int16_t v[P_COUNT], out[P_COUNT], def[P_COUNT];
        uint32_t i, same = 1;
        for (i = 0; i < P_COUNT; i++)
            def[i] = i >= P_E0 ? 0 : TP[i].def;
        for (i = 0; i < P_COUNT; i++)
            v[i] = (int16_t)(i >= P_E0 ? 40 + i : i < UP_E0V ? i * 3 : TP[i].min + (int32_t)(i * 7u) % (TP[i].max - TP[i].min + 1));
        memset(&r, 0, sizeof r);
        r.used = UP_USED, r.ver = UP_VER, r.name[0] = 'N';
        up_rec_set(&r, v);
        up_params(&r, out, def);
        for (i = 0; i < P_COUNT; i++)
            same &= out[i] == v[i];
        ok(same && up_xs[0] == 2u, "user presets: every value back, VINTAGE and LOOP with them (the side's 8)");
        up_xs[0] = 1;                                   /* (0.8's side: its 6 values) */
        up_params(&r, out, def);
        ok(out[P_MSLEN] == v[P_MSLEN] && out[P_DICE] == v[P_DICE] && out[P_VINT] == TP[P_VINT].def &&
           out[P_SLOOP] == TP[P_SLOOP].def, "user presets: a 0.8 side: its 6 values, VINTAGE and LOOP at their defaults");
    }
}

static void t_clean(void)
{
    track_t *t = &trk[0];
    uint32_t m, i, bad = 0;
    for (m = FM_SVF; m <= FM_LAST; m++) {               /* the cutoff swept by the knob, a step a UI frame */
        int32_t steady, sweep = 0;
        quiet();
        plain(t, 0, 0);
        t->p[P_E4] = 40, t->p[P_E5] = 70, t->p[P_FMODEL] = (int16_t)m;
        trk_note_on(t, 48, 100);
        trk_note_on(t, 55, 100);
        run(500);
        steady = d2max(2048, 12000);
        for (i = 0; i < 80u; i++) {
            int32_t d;
            t->p[P_E4] = (int16_t)(40 + i);
            run(22);
            d = d2max(0, ncap);
            sweep = d > sweep ? d : sweep;
        }
        run(400);
        steady = d2max(2048, 9000) > steady ? d2max(2048, 9000) : steady;   /* (open: the sound brighter) */
        if (sweep > steady * 2 + 200) {
            bad++;
            printf("  %s: sweep %d steady %d\n", MN[m], sweep, steady);
        }
    }
    ok(!bad, "CLEAN: the cutoff swept by the knob on every model (a step each UI frame): no click");
    {   /* the loudest mix: two parts, eight voices of a hot saw chord, every send, the INSERT, VINTAGE */
        uint32_t rail = 0, k;
        quiet();
        plain(t, 0, 0);
        plain(&trk[1], ENGI_JUNO, 0);
        trk[1].p[P_LYMODE] = LY_LAYER;
        t->p[P_E6] = 127, t->p[P_E5] = 100, t->p[P_FMODEL] = FM_MS20, t->p[P_VINT] = 127, t->p[P_LEVEL] = 127;
        t->p[P_DIST] = 127, t->p[P_CHOR] = 127, t->p[P_DLY] = 127, t->p[P_REV] = 127, t->p[P_ITYPE] = 4;
        trk[1].p[P_LEVEL] = 127;
        song.master_q12 = 4088;                         /* (the MASTER knob all the way up) */
        for (k = 0; k < 8u; k++)
            trk_note_on(t, 36 + 5 * k, 127);
        run(3000);
        for (k = 0; k < ncap; k++)
            rail += cl[k] >= (1 << 23) - 1 || cl[k] <= -(1 << 23) || cr[k] >= (1 << 23) - 1 || cr[k] <= -(1 << 23);
        ok(!rail && bounded(), "CLEAN: the loudest mix (2 parts, 8 hot voices, every send, FUZZ, MS20): no sample at the rail");
        song.master_q12 = 4096;
        trk[1].p[P_LYMODE] = LY_OFF;
        quiet();
    }
}

int main(void)
{
    uint32_t i;
    host_tracks_init();
    for (i = 0; i < NPART; i++)
        host_preset(&trk[i], TRK_DEF[i][0], TRK_DEF[i][1]);
    clipper_proj_neutral();
    trk[1].p[P_LYMODE] = LY_OFF;
    song.g[G_BPM] = 120;
    t_models();
    t_vintage();
    t_tuning();
    t_turing();
    t_store();
    t_clean();
    printf(fails ? "synth9: %d FAILED\n" : "synth9: all passed\n", fails);
    return fails;
}
