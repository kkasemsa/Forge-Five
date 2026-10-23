/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 3dSam (Clipper) */
/* Clipper 0.8 (CLIPPER_SYNTH 1): JUNO and its chorus, VECTOR, the MOD SEQ, SPICE / DICE.
 *   JUNO     every preset bounded, audible, its voices free after the release; in tune; PW (a square: no even
 *            harmonics, narrow: they come); HPF 3 thins the bass; the chorus: OFF mono (left = right),
 *            I II I+II stereo and each its own; a mode changed under a held chord: no click; the INSERT's JUNO1 JUNO2
 *            JUN12 on another engine (MIX 0: mono again)
 *   VECTOR   every preset bounded and audible; a corner its wave alone (SINE: no harmonics; SAW: the 2nd at half);
 *            the centre all four; MOVE moves the timbre; a high note bounded (the narrow copy)
 *   MOD SEQ  KEY: step 0 at the first key, the next one a 1/16 later (120 BPM), LEN wraps; SYNC: the song's grid;
 *            SMTH glides; SEQ1 -> CUT changes the sound, SEQ3 -> AMP gates it; kept with the sound: projects (both
 *            parts, a 0.7 project: the shapes and defaults), user presets (the steps in note / flags, the values in
 *            the side), a record without steps: none
 *   SPICE    0: the arp as written; up: rests, octaves, velocities; the same DICE: the same variation; another DICE:
 *            another; the sequencer's steps too (the pattern never changes) */
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
    printf("synth8: %-96s %s\n", what, c ? "ok" : "FAIL");
    fails += !c;
}

#define NCAP (4096u * 16u)
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
    t->p[P_AMODE] = 0;
    t->p[P_ITYPE] = 0, t->p[P_IMIX] = 127;
}
static uint32_t sounding(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active;
    return n;
}
/* the magnitude of frequency f (Hz) in cl[a .. a + n) */
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
static double rms_l(uint32_t a, uint32_t n)
{
    double e = 0;
    uint32_t i;
    for (i = a; i < a + n && i < ncap; i++)
        e += (double)cl[i] * cl[i];
    return sqrt(e / n);
}
static double side_ratio(uint32_t a, uint32_t n)       /* |L - R| / |L + R| */
{
    double s = 0, m = 0;
    uint32_t i;
    for (i = a; i < a + n && i < ncap; i++) {
        double d = (double)cl[i] - cr[i], u = (double)cl[i] + cr[i];
        s += d * d, m += u * u;
    }
    return m > 0 ? sqrt(s / m) : 0;
}
static int32_t max_step(uint32_t a, uint32_t n)        /* the largest sample-to-sample move of the left channel */
{
    int32_t mx = 0;
    uint32_t i;
    for (i = a + 1u; i < a + n && i < ncap; i++) {
        int32_t d = cl[i] - cl[i - 1];
        d = d < 0 ? -d : d;
        mx = d > mx ? d : mx;
    }
    return mx;
}
static int bounded(void)
{
    uint32_t i;
    for (i = 0; i < ncap; i++)
        if (cl[i] > (1 << 23) || cl[i] < -(1 << 23) || cr[i] > (1 << 23) || cr[i] < -(1 << 23))
            return 0;
    return 1;
}

/* every preset of engine e: a chord held, then let go: bounded, audible, the voices free after */
static void presets_all(uint32_t e)
{
    const engine_t *en = ENGINES[e];
    uint32_t pi, good = 1, loud = 1, freed = 1;
    char w[200];
    for (pi = 0; pi < en->npresets; pi++) {
        static const uint8_t ch[3] = {48, 55, 64};
        uint32_t i;
        quiet();
        host_preset(&trk[0], e, pi);
        trk[0].p[P_AMODE] = 0;
        for (i = 0; i < 3u; i++)
            trk_note_on(&trk[0], ch[i], 100);
        run(1400);
        if (!bounded())
            good = 0, printf("  %s: out of bounds\n", en->presets[pi].name);
        {
            double r = 0;
            uint32_t w;
            for (w = 0; w < 8u; w++)                    /* (the loudest eighth: plucks fade) */
                r = rms_l(w * (ncap / 8u), ncap / 8u) > r ? rms_l(w * (ncap / 8u), ncap / 8u) : r;
            if (r < 300.0) {
                loud = 0;
                printf("  %s: quiet (%.0f)\n", en->presets[pi].name, r);
            }
        }
        for (i = 0; i < 3u; i++)
            trk_note_off(&trk[0], ch[i]);
        for (i = 0; i < 40u && sounding(&trk[0]); i++)   /* (long releases: up to 15 s) */
            run(500);
        if (!bounded())
            good = 0, printf("  %s: out of bounds after\n", en->presets[pi].name);
        if (sounding(&trk[0]))
            freed = 0, printf("  %s: %u voices left\n", en->presets[pi].name, (unsigned)sounding(&trk[0]));
    }
    snprintf(w, sizeof w, "%s: all %u presets bounded, audible, their voices free after the release", en->name,
             (unsigned)en->npresets);
    ok(good && loud && freed, w);
}

static void t_juno(void)
{
    track_t *t = &trk[0];
    presets_all(ENGI_JUNO);
    {   /* in tune: a saw at A4, the filter open */
        double m0, ml, mh;
        quiet();
        run(8000);                                      /* (the presets' reverb and delay tails die away: 6 s) */
        plain(t, ENGI_JUNO, 0);
        t->p[P_E0] = 0, t->p[P_E2] = 0, t->p[P_E3] = 0, t->p[P_E4] = 1, t->p[P_E5] = 127, t->p[P_E6] = 0, t->p[P_E7] = 0;
        t->p[P_LD_SHP] = 0;
        trk_note_on(t, 69, 100);
        run(500);
        m0 = mag(440, 8192, 8192), ml = mag(428, 8192, 8192), mh = mag(452, 8192, 8192);
        ok(m0 > 8 * ml && m0 > 8 * mh, "JUNO: in tune (a saw at A4: 440 Hz, not beside it)");
        ok(side_ratio(8192, 8192) < 1e-3, "JUNO: CHOR OFF: mono (left = right)");
    }
    {   /* PW: a square has no even harmonics, a narrow pulse has */
        double sq, nar;
        quiet();
        plain(t, ENGI_JUNO, 0);
        t->p[P_E0] = 1, t->p[P_E1] = 0, t->p[P_E2] = 0, t->p[P_E3] = 0, t->p[P_E4] = 1, t->p[P_E5] = 127, t->p[P_E6] = 0;
        t->p[P_E7] = 0, t->p[P_LD_SHP] = 0;
        trk_note_on(t, 57, 100);
        run(500);
        sq = mag(440, 8192, 8192) / mag(220, 8192, 8192);
        quiet();
        t->p[P_E1] = 110;
        trk_note_on(t, 57, 100);
        run(500);
        nar = mag(440, 8192, 8192) / mag(220, 8192, 8192);
        ok(sq < 0.05 && nar > 0.3, "JUNO: PW 0 a square (no 2nd harmonic), PW 110 a narrow pulse (its 2nd harmonic)");
    }
    {   /* HPF: 3 thins the bass */
        double f1, f3;
        quiet();
        plain(t, ENGI_JUNO, 0);
        t->p[P_E0] = 0, t->p[P_E2] = 0, t->p[P_E5] = 127, t->p[P_E6] = 0, t->p[P_E7] = 0, t->p[P_E4] = 1;
        trk_note_on(t, 33, 100);                        /* A1, 55 Hz */
        run(600);
        f1 = mag(55, 8192, 16384);
        quiet();
        t->p[P_E4] = 3;
        trk_note_on(t, 33, 100);
        run(600);
        f3 = mag(55, 8192, 16384);
        ok(f3 < 0.3 * f1, "JUNO: HPF 3 thins the bass (55 Hz at least 10 dB down from HPF 1)");
    }
    {   /* the chorus: I, II, I+II stereo and each its own */
        double sr[4];
        uint32_t m;
        int32_t again[3] = {0};
        static int32_t keep[3][4096];
        for (m = 1; m < 4u; m++) {
            quiet();
            plain(t, ENGI_JUNO, 0);
            t->p[P_E7] = (int16_t)m;
            trk_note_on(t, 57, 100);
            trk_note_on(t, 64, 100);
            run(1000);
            sr[m] = side_ratio(8192, 16384);
            memcpy(keep[m - 1u], cl + 16384, sizeof keep[0]);
            (void)again;
        }
        ok(sr[1] > 0.15 && sr[2] > 0.15 && sr[3] > 0.05, "JUNO: CHOR I, II, I+II: stereo (left - right a good share)");
        ok(memcmp(keep[0], keep[1], sizeof keep[0]) && memcmp(keep[1], keep[2], sizeof keep[0]),
           "JUNO: CHOR I, II, I+II: three different choruses");
    }
    {   /* a mode changed under a held chord: no click */
        int32_t steady, sw;
        quiet();
        plain(t, ENGI_JUNO, 0);
        t->p[P_E0] = 0, t->p[P_E5] = 60, t->p[P_E7] = 1;
        trk_note_on(t, 57, 100);
        trk_note_on(t, 61, 100);
        run(600);
        steady = max_step(4096, 8192);
        t->p[P_E7] = 3;
        run(60);
        sw = max_step(0, ncap);
        t->p[P_E7] = 0;
        run(60);
        sw = max_step(0, ncap) > sw ? max_step(0, ncap) : sw;
        ok(sw < steady * 3 / 2 + 64, "JUNO: CHOR changed under a held chord (I -> I+II -> OFF): no click");
    }
    {   /* the INSERT's JUNO types on ANALOG */
        double s1, s0;
        static int32_t a1[4096], a3[4096];
        quiet();
        plain(t, 0, 0);
        t->p[P_ITYPE] = IT_JUNO1, t->p[P_IA] = 127, t->p[P_IB] = 100, t->p[P_IC] = 0, t->p[P_IMIX] = 127;
        trk_note_on(t, 57, 100);
        run(800);
        s1 = side_ratio(8192, 16384);
        memcpy(a1, cl + 16384, sizeof a1);
        quiet();
        t->p[P_ITYPE] = IT_JUNO12;
        trk_note_on(t, 57, 100);
        run(800);
        memcpy(a3, cl + 16384, sizeof a3);
        quiet();
        t->p[P_ITYPE] = IT_JUNO2, t->p[P_IMIX] = 0;
        trk_note_on(t, 57, 100);
        run(800);
        s0 = side_ratio(8192, 16384);
        ok(s1 > 0.15 && s0 < 1e-3 && memcmp(a1, a3, sizeof a1) && !strcmp(N_ITYPE[IT_JUNO1], "BBD1"),
           "JUNO chorus as an INSERT (JUNO1, JUN12) on ANALOG: stereo, each its own; MIX 0: mono");
        t->p[P_ITYPE] = 0, t->p[P_IMIX] = 127;
    }
}

static void t_vector(void)
{
    track_t *t = &trk[0];
    presets_all(ENGI_VECTOR);
    {   /* a corner: its wave alone */
        double sine2, saw2, mid2, h1;
        quiet();
        plain(t, ENGI_VECTOR, 0);
        t->p[P_E0] = 0, t->p[P_E1] = 2, t->p[P_E2] = 3, t->p[P_E3] = 2;   /* A SINE, B SAW, C SQR, D SAW */
        t->p[P_E4] = -64, t->p[P_E5] = -64, t->p[P_E6] = VM_OFF, t->p[P_E7] = 127;
        trk_note_on(t, 57, 100);
        run(500);
        h1 = mag(220, 8192, 8192);
        sine2 = (mag(440, 8192, 8192) + mag(660, 8192, 8192)) / h1;
        quiet();
        t->p[P_E4] = 63;                               /* B */
        trk_note_on(t, 57, 100);
        run(500);
        saw2 = mag(440, 8192, 8192) / mag(220, 8192, 8192);
        quiet();
        t->p[P_E4] = 0, t->p[P_E5] = 0;                /* the centre */
        trk_note_on(t, 57, 100);
        run(500);
        mid2 = mag(440, 8192, 8192) / mag(220, 8192, 8192);
        ok(sine2 < 0.02 && saw2 > 0.35 && saw2 < 0.65 && mid2 > 0.1 && mid2 < saw2,
           "VECTOR: corner A (SINE) a sine, corner B (SAW) its 2nd harmonic at half, the centre between");
    }
    {   /* MOVE moves the timbre over the note; OFF holds it */
        double r0[2], r1[2];
        uint32_t mv;
        for (mv = 0; mv < 2u; mv++) {
            quiet();
            plain(t, ENGI_VECTOR, 0);
            t->p[P_E0] = 0, t->p[P_E1] = 2, t->p[P_E2] = 3, t->p[P_E3] = 13;
            t->p[P_E4] = 0, t->p[P_E5] = 0, t->p[P_E6] = mv ? VM_CIRCF : VM_OFF, t->p[P_E7] = 127;
            trk_note_on(t, 57, 100);
            run(2000);
            r0[mv] = mag(440, 4096, 8192) / mag(220, 4096, 8192);
            r1[mv] = mag(440, 4096 + 11025, 8192) / mag(220, 4096 + 11025, 8192);   /* a quarter of CIRC+'s turn on */
        }
        ok(fabs(r0[0] - r1[0]) < 0.02 * r0[0] + 0.005 && fabs(r0[1] - r1[1]) > 0.05,
           "VECTOR: MOVE OFF holds the mix, CIRC+ moves it (the 2nd harmonic's share changes)");
    }
    {   /* a high note: the narrow copy, bounded */
        quiet();
        plain(t, ENGI_VECTOR, 0);
        t->p[P_E6] = VM_OFF;
        trk_note_on(t, 100, 127);
        trk_note_on(t, 108, 127);
        run(500);
        ok(bounded() && rms_l(4096, 8192) > 100.0, "VECTOR: notes at the top of the keyboard bounded and audible");
    }
}

static void t_mseq(void)
{
    track_t *t = &trk[0];
    uint32_t i, good = 1;
    quiet();
    plain(t, 0, 0);
    song.g[G_BPM] = 120;
    t->p[P_MSRATE] = 2, t->p[P_MSLEN] = 16, t->p[P_MSSMTH] = 0, t->p[P_MSKEY] = 1;
    ok(t->ms[0][0] == -8 && t->ms[0][15] == 7 && t->ms[2][2] == -8, "MOD SEQ: a sound loaded brings the four shapes");
    run(100);
    trk_note_on(t, 57, 100);
    run(1);
    good &= t->ms_step == 0u && t->ms_cur[0] == mseq_q15(t->ms[0][0]);
    run(170);                                           /* 1/16 at 120 BPM: 5512.5 samples, 172.3 blocks */
    good &= t->ms_step == 0u;
    run(4);
    good &= t->ms_step == 1u && t->ms_cur[1] == mseq_q15(t->ms[1][1]);
    ok(good, "MOD SEQ KEY: step 1 at the first key, the next a 1/16 later (120 BPM), the values at once (SMTH 0)");
    t->p[P_MSLEN] = 4;
    run(172 * 4);
    ok(t->ms_step < 4u, "MOD SEQ: LEN 4 wraps at 4");
    trk_note_off(t, 57);
    run(20);
    trk_note_on(t, 60, 100);
    run(1);
    ok(t->ms_step == 0u, "MOD SEQ KEY: a new first key starts it again");
    {   /* SMTH: a glide between steps */
        int32_t a, b;
        t->p[P_MSSMTH] = 110, t->p[P_MSLEN] = 16;
        run(172 + 3);
        a = t->ms_cur[0];
        b = mseq_q15(t->ms[0][t->ms_step]);
        ok(a != b && a < b, "MOD SEQ: SMTH glides to the step's value");
        t->p[P_MSSMTH] = 0;
    }
    trk_note_off(t, 60);
    {   /* SYNC: the song's grid */
        uint32_t into, slen, g = 1;
        t->p[P_MSKEY] = 0, t->p[P_MSLEN] = 16;
        transport_req = 1;
        run(2);
        for (i = 0; i < 40u; i++) {
            run(37);
            g &= t->ms_step == grid_at(2, 0, &into, &slen) % 16u || into < 64u * 120u;
        }
        transport_req = 2;
        run(2);
        ok(g, "MOD SEQ SYNC: on the song's grid while it plays");
        t->p[P_MSKEY] = 1;
    }
    {   /* SEQ1 -> CUT changes the sound; SEQ3 (the rhythm) -> AMP gates it */
        static int32_t a[8192];
        double lo, hi;
        quiet();
        plain(t, 0, 0);
        t->p[P_E4] = 60;
        trk_note_on(t, 48, 100);
        run(800);
        memcpy(a, cl + 8192, sizeof a);
        quiet();
        t->p[P_M1SRC] = MS_SEQ1, t->p[P_M1DST] = MT_CUT, t->p[P_M1AMT] = 63;
        trk_note_on(t, 48, 100);
        run(800);
        ok(memcmp(a, cl + 8192, sizeof a) != 0, "MOD SEQ: SEQ1 -> CUT moves the sound");
        quiet();
        t->p[P_M1SRC] = MS_SEQ3, t->p[P_M1DST] = MT_AMP, t->p[P_M1AMT] = 63;
        t->p[P_MSRATE] = 5;                             /* 1/4: 22050 samples a step */
        trk_note_on(t, 48, 100);
        run(1400);                                      /* steps 0, 1 (7: up), 2 (-8: down) */
        lo = rms_l(44100 + 4000, 16000);
        hi = rms_l(4000, 16000);
        ok(hi > 3.0 * lo, "MOD SEQ: SEQ3 (the rhythm) -> AMP: its steps up loud, down quiet");
        t->p[P_M1SRC] = 0, t->p[P_M1AMT] = 0, t->p[P_MSRATE] = 2;
    }
}

static project_t pj;
static void t_storage(void)
{
    track_t *a = &trk[0], *b = &trk[1];
    uint32_t i, l, s, good = 1;
    int8_t ma[MSEQ_LANES][MSEQ_STEPS], mb[MSEQ_LANES][MSEQ_STEPS];
    quiet();
    plain(a, ENGI_JUNO, 0);
    plain(b, ENGI_VECTOR, 0);
    for (l = 0; l < MSEQ_LANES; l++)
        for (s = 0; s < MSEQ_STEPS; s++)
            a->ms[l][s] = ma[l][s] = (int8_t)((int32_t)((l * 7u + s * 5u) % 16u) - 8),
            b->ms[l][s] = mb[l][s] = (int8_t)((int32_t)((l * 3u + s * 11u + 4u) % 16u) - 8);
    a->p[P_MSRATE] = 8, a->p[P_MSLEN] = 1, a->p[P_MSSMTH] = 127, a->p[P_MSKEY] = 0, a->p[P_SPICE] = 127, a->p[P_DICE] = 99;
    b->p[P_MSRATE] = 0, b->p[P_MSLEN] = 16, b->p[P_MSSMTH] = 3, b->p[P_MSKEY] = 1, b->p[P_SPICE] = 1, b->p[P_DICE] = 0;
    a->p[P_ITYPE] = IT_JUNO2, a->p[P_IMIX] = 77, a->p[P_M2SRC] = MS_SEQ4;
    proj_capture(&pj);
    mseq_init(a), mseq_init(b);
    for (i = P_MSRATE; i <= P_DICE; i++)
        a->p[i] = b->p[i] = TP[i].def;
    a->p[P_ITYPE] = 0, a->p[P_M2SRC] = 0;
    proj_apply(&pj, 1);
    good = !memcmp(a->ms, ma, sizeof ma) && !memcmp(b->ms, mb, sizeof mb) && a->p[P_MSRATE] == 8 && a->p[P_MSLEN] == 1 &&
           a->p[P_MSSMTH] == 127 && a->p[P_MSKEY] == 0 && a->p[P_SPICE] == 127 && a->p[P_DICE] == 99 &&
           b->p[P_MSRATE] == 0 && b->p[P_MSLEN] == 16 && b->p[P_MSSMTH] == 3 && b->p[P_MSKEY] == 1 && b->p[P_SPICE] == 1 &&
           b->p[P_DICE] == 0 && a->p[P_ITYPE] == IT_JUNO2 && a->p[P_IMIX] == 77 && a->p[P_M2SRC] == MS_SEQ4 &&
           a->eng_req == ENGI_JUNO && b->eng_req == ENGI_VECTOR && sizeof pj == 3840u;
    ok(good, "project: both sounds' MOD SEQ (steps, RATE LEN SMTH START), SPICE, DICE kept; JUNO, VECTOR; the size unchanged");
    {   /* as 0.7 wrote it: no 0.8 bytes, no steps */
        uint8_t raw[3u * NLOCK];
        uint32_t k;
        proj_capture(&pj);
        for (k = 0; k < NLOCK; k++)
            raw[3u * k] = pj.t[2].lock[k].param, raw[3u * k + 1u] = (uint8_t)pj.t[2].lock[k].val,
            raw[3u * k + 2u] = (uint8_t)((uint16_t)pj.t[2].lock[k].val >> 8);
        memset(raw + PROJ_XM2 + 1u, 0, sizeof raw - PROJ_XM2 - 1u);
        for (k = 0; k < NLOCK; k++)
            pj.t[2].lock[k].param = raw[3u * k], pj.t[2].lock[k].val = (int16_t)(raw[3u * k + 1u] | raw[3u * k + 2u] << 8);
        for (k = 0; k < NLOCK; k++)
            pj.t[3].lock[k].step = LOCK_FREE, pj.t[3].lock[k].param = 0, pj.t[3].lock[k].val = 0;
        proj_apply(&pj, 1);
        ok(!memcmp(a->ms, MSEQ_INIT, sizeof ma) && a->p[P_MSLEN] == TP[P_MSLEN].def && a->p[P_SPICE] == 0 &&
           a->p[P_ITYPE] == IT_JUNO2 && a->p[P_IMIX] == 77, "a project of 0.7: its INSERT read, the MOD SEQ's shapes and defaults");
    }
    {   /* user presets */
        up_rec_t r;
        int16_t v[P_COUNT], out[P_COUNT], def[P_COUNT];
        int8_t got[MSEQ_LANES][MSEQ_STEPS];
        uint32_t same = 1;
        for (i = 0; i < P_COUNT; i++)
            def[i] = i >= P_E0 ? 0 : TP[i].def;
        for (i = 0; i < P_COUNT; i++)
            v[i] = (int16_t)(i >= P_E0 ? 40 + i : i < UP_E0V ? i * 3 : TP[i].min + (int32_t)(i * 7u) % (TP[i].max - TP[i].min + 1));
        memset(&r, 0, sizeof r);
        r.used = UP_USED, r.ver = UP_VER, r.name[0] = 'M';
        up_rec_set(&r, v);
        ok(!up_has_ms(&r), "user presets: a record without the MOD SEQ's steps says so");
        up_ms_put(&r, ma);
        up_params(&r, out, def);
        for (i = 0; i < P_COUNT; i++)
            same &= out[i] == v[i];
        up_ms_get(&r, got);
        ok(same && up_has_ms(&r) && !memcmp(got, ma, sizeof got) && r.np == 61u && r.p[53] == 40 + P_E0 && up_valid(&r),
           "user presets: the MOD SEQ's steps (note / flags) and values (the side) back exactly; SLOOP's view the same");
        memset(up_xs, 0, sizeof up_xs);                 /* (a record of 0.7: no side bytes) */
        up_params(&r, out, def);
        ok(out[P_MSLEN] == TP[P_MSLEN].def && out[P_SPICE] == 0 && out[P_MSKEY] == TP[P_MSKEY].def,
           "user presets: a record without side bytes: the MOD SEQ's and SPICE's defaults");
    }
}

/* the arp's notes over 32 steps: note, or 0 (a rest), and their velocities */
static void arp_take(uint32_t spice, uint32_t dice, uint8_t *notes, uint8_t *vels)
{
    track_t *t = &trk[0];
    uint32_t k = 0, b, last_idx;
    quiet();
    plain(t, 0, 0);
    t->p[P_SPICE] = (int16_t)spice, t->p[P_DICE] = (int16_t)dice;
    t->p[P_AMODE] = 1, t->p[P_ARATE] = 2, t->p[P_AGATE] = 64, t->p[P_APROB] = 127, t->p[P_AOCT] = 1;
    t->arp_idx = 0;
    input_on(t, 60, 100);
    input_on(t, 64, 100);
    input_on(t, 67, 100);
    last_idx = t->arp_idx;
    for (b = 0; b < 6000u && k < 32u; b++) {
        run(1);
        if (t->arp_idx != last_idx) {
            uint32_t i, vel = 0;
            last_idx = t->arp_idx;
            for (i = 0; i < NVOICE; i++)
                if (t->v[i].active && t->v[i].gate && t->v[i].note == t->arp_note)
                    vel = t->v[i].vel;
            notes[k] = t->arp_note;
            vels[k++] = (uint8_t)vel;
        }
    }
    input_off(t, 60);
    input_off(t, 64);
    input_off(t, 67);
    t->p[P_AMODE] = 0, t->p[P_SPICE] = 0;
    run(50);
}
static void t_spice(void)
{
    uint8_t n0[32], v0[32], n1[32], v1[32], n2[32], v2[32], n3[32], v3[32];
    uint32_t i, rest = 0, oct = 0, velv = 0, plain_ok = 1;
    arp_take(0, 0, n0, v0);
    for (i = 0; i < 32u; i++)
        plain_ok &= n0[i] == (uint8_t)(i % 3u == 0u ? 60 : i % 3u == 1u ? 64 : 67) && v0[i] == 100u;
    ok(plain_ok, "SPICE 0: the arp as written (UP, every note at velocity 100)");
    arp_take(127, 5, n1, v1);
    for (i = 0; i < 32u; i++) {
        rest += n1[i] == 0u;
        oct += n1[i] && n1[i] != n0[i] && (n1[i] == n0[i] + 12u || n1[i] + 12u == n0[i]);
        velv += n1[i] && v1[i] != 100u;
    }
    ok(rest >= 1u && rest <= 14u && oct >= 1u && velv >= 8u, "SPICE 127: some rests, some octaves, the velocities move");
    arp_take(127, 5, n2, v2);
    arp_take(127, 6, n3, v3);
    ok(!memcmp(n1, n2, 32) && !memcmp(v1, v2, 32), "SPICE: the same DICE, the same variation");
    ok(memcmp(n1, n3, 32) || memcmp(v1, v3, 32), "SPICE: another DICE, another variation");
    {   /* the first 16 and the next 16: the same (the variation loops over 16 notes); the notes cycle every 3 */
        uint32_t same = 1;
        for (i = 0; i < 16u; i++)
            same &= (n1[i] == 0u) == (n1[i + 16u] == 0u) && v1[i] == v1[i + 16u];
        ok(same, "SPICE: the variation comes round every 16 notes");
    }
    {   /* the sequencer: a pattern of C4s; with SPICE some steps rest or jump, the pattern itself unchanged */
        track_t *t = &trk[0];
        static const uint8_t c4[1] = {60};
        step_t before[16];
        uint32_t b, rests = 0, jumps = 0, plays = 0, s, last = 99;
        quiet();
        plain(t, 0, 0);
        steps_clear(t);
        t->p[P_SLEN] = 16, t->p[P_SDIV] = 2, t->p[P_SGATE] = 64;
        for (s = 0; s < 16u; s++)
            put_step(t, s, 1, c4, ST_NOTE, 0);
        memcpy(before, t->step, sizeof before);
        t->p[P_SPICE] = 127, t->p[P_DICE] = 9;
        transport_req = 1;
        for (b = 0; b < 16u * 173u + 40u; b++) {
            uint32_t q, st = at_step(t, &q), i2, n60 = 0, nother = 0;
            run(1);
            if (st == last || q < 30u || q > 45u)
                continue;
            last = st;
            for (i2 = 0; i2 < NVOICE; i2++)
                if (t->v[i2].active && t->v[i2].gate)
                    t->v[i2].note == 60 ? n60++ : nother++;
            plays += n60 > 0u;
            jumps += nother > 0u;
            rests += !n60 && !nother;
        }
        transport_req = 2;
        run(4);
        t->p[P_SPICE] = 0;
        ok(rests >= 1u && jumps >= 1u && plays >= 6u && !memcmp(before, t->step, sizeof before),
           "SPICE on the sequencer: steps rest or jump an octave, the rest play; the pattern unchanged");
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
    t_juno();
    t_vector();
    t_mseq();
    t_storage();
    t_spice();
    printf(fails ? "synth8: %d FAILED\n" : "synth8: all passed\n", fails);
    return fails;
}
