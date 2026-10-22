/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 3dSam (Clipper) */
/* Clipper 0.9: no click where the sound is changed while it plays. Each case holds a chord, changes something, and
 * compares the largest second difference of the output (what a click leaves: a step or a corner) around the change
 * with the largest one of the steady sound before and after it (each the same sound: a click is a mark above both):
 *   a sound loaded (every engine's first presets, one into the next), VOICE steals (12 notes on 8 voices), MUTE on and
 *   off, LEVEL and PAN jumped, the INSERT's TYPE turned through every type, the master FILT jumped, MODEL (the duck),
 *   CHOR of JUNO, the MOD SEQ's SMTH to the cutoff, LFO 2 KEY, VINTAGE turned up, the TUNING changed (a pitch step:
 *   the phase goes on), the LAYER turned on and off; 1.0: SPREAD, BRITE, TIMBR, MOTN and SPACE jumped, the reverb's
 *   TYPE (ROOM PLATE HALL SHIMMER) and the delay's MODE (MONO PING TAPE) turned through.
 * Bound: the change's mark under 2 x the steady one + 400 (about -38 dB of the limiter's ceiling). */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int fails, cases;
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
static int32_t d2max(void)
{
    int32_t mx = 0;
    uint32_t i;
    for (i = 2; i < ncap; i++) {
        int32_t a = cl[i] - 2 * cl[i - 1] + cl[i - 2], b = cr[i] - 2 * cr[i - 1] + cr[i - 2];
        a = a < 0 ? -a : a, b = b < 0 ? -b : b;
        mx = a > mx ? a : mx, mx = b > mx ? b : mx;
    }
    return mx;
}
static void quiet(void)
{
    uint32_t i, k;
    for (i = 0; i < NPART; i++)
        trk_all_off(&trk[i]);
    run(500);
    for (i = 0; i < NPART; i++)
        for (k = 0; k < NVOICE; k++)
            trk[i].v[k].active = trk[i].v[k].gate = 0, trk[i].v[k].stage = 0, trk[i].v[k].env = trk[i].v[k].env_out = 0;
    run(400);
}
static void neutral(track_t *t)
{
    t->p[P_DLY] = t->p[P_REV] = 0;                     /* (the tails would mix the before into the after) */
    t->p[P_LD_PIT] = 0, t->p[P_AMODE] = 0, t->p[P_ATK] = t->p[P_ATK] > 40 ? 40 : t->p[P_ATK];
}
typedef void (*change_fn)(track_t *t, uint32_t k);
static int attacks_ok;           /* (sounds loaded: from silence, a new sound's own attack is not a click) */
/* hold a chord, the steady sound, k changes (each followed by settle blocks), the steady sound again */
static void check(const char *what, uint32_t eng, uint32_t pi, change_fn fn, uint32_t k, uint32_t settle)
{
    static const uint8_t ch[3] = {48, 55, 64};
    track_t *t = &trk[0];
    int32_t before, after, worst = 0, at = -1;
    uint32_t j, i;
    quiet();
    host_preset(t, eng, pi);
    neutral(t);
    for (i = 0; i < 3u; i++)
        trk_note_on(t, ch[i], 100);
    run(300);
    run(200);
    before = d2max();
    for (j = 0; j < k; j++) {
        int32_t s, steady;
        fn(t, j);
        run(settle);
        s = d2max();
        run(150);
        steady = d2max();
        if (s > 2 * (steady > before ? steady : before) + 400 && s > worst && !(attacks_ok && before < 8))
            worst = s, at = (int32_t)j;
        before = steady;
    }
    run(200);
    after = d2max();
    cases++;
    if (worst) {
        fails++;
        printf("clean: %-58s FAIL (change %d: %d, steady %d)\n", what, at, worst, after);
    }
}
static void ch_preset(track_t *t, uint32_t k)
{
    uint32_t e = t->eng_req;
    sound_fade_begin(t);
    host_preset_req(t, e, (t->preset + 1u + k) % ENGINES[e]->npresets);
    neutral(t);
}
static void ch_steal(track_t *t, uint32_t k) { trk_note_on(t, 70 + k, 100); }
static void ch_mute(track_t *t, uint32_t k) { t->p[P_MUTE] = (int16_t)!(k & 1u); }
static void ch_level(track_t *t, uint32_t k) { t->p[P_LEVEL] = (int16_t)(k & 1u ? 127 : 40); }
static void ch_pan(track_t *t, uint32_t k) { t->p[P_PAN] = (int16_t)(k & 1u ? 64 : -64); }
static void ch_dist(track_t *t, uint32_t k) { t->p[P_DIST] = (int16_t)(k & 1u ? 0 : 1 + 20 * (k >> 1)); }
static void ch_sendc(track_t *t, uint32_t k) { t->p[P_CHOR] = (int16_t)(k & 1u ? 0 : 127); }
static void ch_sendd(track_t *t, uint32_t k) { t->p[P_DLY] = (int16_t)(k & 1u ? 0 : 127); }
static void ch_sendr(track_t *t, uint32_t k) { t->p[P_REV] = (int16_t)(k & 1u ? 0 : 127); }
static void ch_dtime(track_t *t, uint32_t k) { t->p[P_DLY] = 100; song.g[G_DTIME] = (int16_t)(k % 7u); }
static void ch_rsize(track_t *t, uint32_t k) { t->p[P_REV] = 100; song.g[G_RSIZE] = (int16_t)(k & 1u ? 10 : 120); }
static void ch_crate(track_t *t, uint32_t k) { t->p[P_CHOR] = 100; song.g[G_CRATE] = (int16_t)(k & 1u ? 5 : 120); song.g[G_CDEPTH] = (int16_t)(k & 2u ? 20 : 127); }
static void ch_eq(track_t *t, uint32_t k) { (void)t; song.g[G_EQLOW] = (int16_t)(EQ_LOW_C + (k & 1u ? 12 : -12)); song.g[G_EQHIGH] = (int16_t)(EQ_HIGH_C + (k & 1u ? -12 : 12)); }
static void ch_wtab(track_t *t, uint32_t k) { t->p[P_E0] = (int16_t)((k * 5u + 3u) % 16u); }
static void ch_fmx(track_t *t, uint32_t k) { t->p[P_E0] = (int16_t)((k + 1u) % 16u); }
static void ch_engine(track_t *t, uint32_t k)
{
    static const uint8_t E[6] = {ENGI_JUNO, 0, ENGI_VECTOR, ENGI_WAVE, ENGI_HYBRID, ENGI_FMX};
    host_preset_req(t, E[k % 6u], 0);                  /* (as set_engine_of: the ISR fades the old engine out) */
    neutral(t);
}
static void ch_insert(track_t *t, uint32_t k) { t->p[P_ITYPE] = (int16_t)((k + 1u) % IT_N); t->p[P_IMIX] = 100; }
static void ch_filt(track_t *t, uint32_t k) { (void)t; song.g[G_FILT] = (int16_t)(k & 1u ? -50 : 40); }
static void ch_model(track_t *t, uint32_t k) { t->p[P_FMODEL] = (int16_t)(FM_SVF + (k + 1u) % FM_LAST); }
static void ch_chor(track_t *t, uint32_t k) { t->p[P_E7] = (int16_t)((k + 1u) % 4u); }
static void ch_vint(track_t *t, uint32_t k) { t->p[P_VINT] = (int16_t)(k & 1u ? 0 : 127); }
static void ch_tune(track_t *t, uint32_t k)
{
    (void)t;
    tune_cfg.sys = (uint8_t)((k + 1u) % TU_USER);
    tune_build();
}
static void ch_layer(track_t *t, uint32_t k)
{
    (void)t;
    trk[1].p[P_LYMODE] = (int16_t)(k & 1u ? LY_OFF : LY_LAYER);
}
static void ch_spread(track_t *t, uint32_t k) { t->p[P_SPREAD] = (int16_t)(k & 1u ? 0 : 127); }   /* (1.0) */
static void ch_brite(track_t *t, uint32_t k) { t->p[P_MAC1] = (int16_t)(k & 1u ? -64 : 63); }
static void ch_timbr(track_t *t, uint32_t k) { t->p[P_MAC2] = (int16_t)(k & 1u ? -64 : 63); }
static void ch_motn(track_t *t, uint32_t k) { t->p[P_MAC3] = (int16_t)(k & 1u ? 0 : 127); }
static void ch_space(track_t *t, uint32_t k) { t->p[P_REV] = 60; t->p[P_DLY] = 60; t->p[P_MAC4] = (int16_t)(k & 1u ? 0 : 127); }
static void ch_rtype(track_t *t, uint32_t k) { t->p[P_REV] = 100; song.g[G_RTYPE] = (int16_t)((k + 1u) % 4u); }
static void ch_dmode(track_t *t, uint32_t k) { t->p[P_DLY] = 100; song.g[G_DFDBK] = 90; song.g[G_DMODE] = (int16_t)((k + 1u) % 3u); }
static void ch_mseq(track_t *t, uint32_t k)
{
    t->p[P_M1SRC] = MS_SEQ2, t->p[P_M1DST] = MT_CUT, t->p[P_M1AMT] = 40;
    t->p[P_MSSMTH] = (int16_t)(k & 1u ? 120 : 60), t->p[P_MSRATE] = 4;
}

int main(void)
{
    uint32_t e, i;
    host_tracks_init();
    for (i = 0; i < NPART; i++)
        host_preset(&trk[i], i ? 1u : ENGI_HYBRID, 0);
    clipper_proj_neutral();
    trk[1].p[P_LYMODE] = LY_OFF;
    song.g[G_BPM] = 120;
    for (e = 0; e < NENGINES; e++) {                    /* a sound loaded under the chord, every engine */
        char w[80];                                     /* (0.9: SAMPLE and GRAIN too, on Clipper's own sources) */
        if (!ENGINES[e]->npresets)                      /* (Forge Five: an engine not built, ENG_OFF) */
            continue;
        attacks_ok = 1;
        snprintf(w, sizeof w, "%s: sounds loaded under a held chord", ENGINES[e]->name);
        check(w, e, 0, ch_preset, ENGINES[e]->npresets < 6u ? ENGINES[e]->npresets - 1u : 5u, 40);
        attacks_ok = 0;
    }
    check("ANALOG: VOICE steals (9 .. 12 notes on 8 voices)", 0, 12, ch_steal, 9, 30);
    check("ANALOG: MUTE on and off", 0, 13, ch_mute, 6, 30);
    check("ANALOG: LEVEL jumped (40 <-> 127)", 0, 13, ch_level, 6, 30);
    check("ANALOG: PAN jumped (left <-> right)", 0, 13, ch_pan, 6, 30);
    check("ANALOG: the INSERT's TYPE through every type", 0, 13, ch_insert, IT_N + 1u, 40);
    {   /* and each comes in when turned on under a held chord (FLANG / CHOR: after their line is full) */
        static const uint8_t ch[3] = {48, 55, 64};
        uint32_t ty, bad = 0;
        for (ty = IT_SOFT; ty <= IT_CHORUS; ty++) {
            quiet();
            host_preset(&trk[0], 0, 13);
            neutral(&trk[0]);
            trk_note_on(&trk[0], ch[0], 100), trk_note_on(&trk[0], ch[1], 100), trk_note_on(&trk[0], ch[2], 100);
            run(200);
            trk[0].p[P_ITYPE] = (int16_t)ty, trk[0].p[P_IMIX] = 100;
            run(200);
            bad += !(trk[0].ins_run && ins[0].w > 0 && ins[0].type == ty && !ins[0].fill);
        }
        trk[0].p[P_ITYPE] = 0;
        cases++;
        if (bad) {
            fails++;
            printf("clean: the INSERT turned on under a held chord: %u types never came in FAIL\n", bad);
        }
    }
    check("ANALOG: DIST on and off (1, 21, 41)", 0, 13, ch_dist, 6, 30);
    check("ANALOG: the CHOR send jumped (0 <-> 127)", 0, 13, ch_sendc, 6, 30);
    check("ANALOG: the DLY send jumped (0 <-> 127)", 0, 13, ch_sendd, 6, 30);
    check("ANALOG: the REV send jumped (0 <-> 127)", 0, 13, ch_sendr, 6, 30);
    check("ANALOG: the master FILT jumped", 0, 13, ch_filt, 6, 30);
    song.g[G_FILT] = 0;
    check("ANALOG: MODEL through every model", 0, 13, ch_model, FM_LAST, 40);
    check("WAVE: MODEL through every model", ENGI_WAVE, 2, ch_model, FM_LAST, 40);
    check("JUNO: CHOR through its modes", ENGI_JUNO, 0, ch_chor, 8, 40);
    check("ANALOG: VINTAGE on and off", 0, 13, ch_vint, 6, 30);
    check("ANALOG: the delay's TIME through its values (DLY send on)", 0, 13, ch_dtime, 8, 40);
    song.g[G_DTIME] = GP[G_DTIME].def;
    check("ANALOG: the reverb's SIZE jumped (REV send on)", 0, 13, ch_rsize, 6, 40);
    song.g[G_RSIZE] = GP[G_RSIZE].def;
    check("ANALOG: the chorus's RATE and DEPTH jumped (CHOR send on)", 0, 13, ch_crate, 6, 40);
    song.g[G_CRATE] = GP[G_CRATE].def, song.g[G_CDEPTH] = GP[G_CDEPTH].def;
    check("ANALOG: the master EQ jumped (+-12 dB)", 0, 13, ch_eq, 6, 40);
    song.g[G_EQLOW] = EQ_LOW_C, song.g[G_EQHIGH] = EQ_HIGH_C;
    check("WAVE: TABL turned under a held chord (a table built)", ENGI_WAVE, 0, ch_wtab, 6, 60);
    check("FMX: PTCH turned under a held chord", ENGI_FMX, 0, ch_fmx, 6, 60);
    check("an ENGINE switched under a held chord", 0, 13, ch_engine, 6, 60);
    check("ANALOG: the TUNING through every system", 0, 13, ch_tune, TU_USER, 30);
    tune_cfg.sys = TU_EQUAL;
    tune_build();
    check("HYBRID: the LAYER on and off", ENGI_HYBRID, 1, ch_layer, 6, 40);
    trk[1].p[P_LYMODE] = LY_OFF;
    check("JUNO: the MOD SEQ (SMTH 60 / 120) on the cutoff", ENGI_JUNO, 0, ch_mseq, 6, 60);
    check("ANALOG: SPREAD jumped 0 / 127 (1.0)", 0, 13, ch_spread, 6, 40);
    check("ANALOG: BRITE jumped -64 / +63 (1.0)", 0, 13, ch_brite, 6, 40);
    check("WAVE: TIMBR jumped -64 / +63 (1.0)", ENGI_WAVE, 2, ch_timbr, 6, 40);
    check("ANALOG: MOTN jumped 0 / 127 (1.0)", 0, 13, ch_motn, 6, 40);
    check("ANALOG: SPACE jumped 0 / 127 (REV, DLY sends on) (1.0)", 0, 13, ch_space, 6, 40);
    check("ANALOG: the reverb's TYPE through ROOM PLATE HALL SHIMMER (1.0)", 0, 13, ch_rtype, 8, 40);
    song.g[G_RTYPE] = GP[G_RTYPE].def;
    check("ANALOG: the delay's MODE through MONO PING TAPE (1.0)", 0, 13, ch_dmode, 6, 60);
    song.g[G_DMODE] = GP[G_DMODE].def, song.g[G_DFDBK] = GP[G_DFDBK].def;
    printf("clean: %d cases\n", cases);
    printf(fails ? "clean: %d FAILED\n" : "clean: all passed\n", fails);
    return fails;
}
