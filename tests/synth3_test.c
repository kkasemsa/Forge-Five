/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 3dSam (Clipper) */
/* Clipper 0.3 (CLIPPER_SYNTH 1), on the harness of tests/ui_pages_test.c:
 *   EQ        LOW / MID / HIGH boost and cut their band (+-1.5 dB of the setting), leave the others (+-1.5 dB);
 *             flat: the signal untouched (bit for bit); a band turned: no click (the gain glides)
 *   COMP      a loud and a quiet tone 20 dB apart come out closer (COMP 127: under 13 dB); COMP 0 untouched
 *   UNI2/UNI4 two notes on 4 / 8 voices, each note's voices detuned apart; a note-off lets all of them go
 *   MOD       VEL>F (a hard note brighter than a soft one), MW > VIB (the pitch wobbles), AT > CUT (brighter)
 *   LAYER     LAYER: a note on both sounds (the layer an octave up with OCT +1); SPLIT: below SPLIT the layer,
 *             from it up the main sound; a note held while the mode changes still lets go where it went; OFF: the
 *             layer silent; SOUND turned on the page loads the layer's sound; SWAP trades the two sounds (twice:
 *             back as they were)
 *   MORPH     SET A, SET B, MORPH half way: the values half way, the switches at B's; 0 and 127: A and B exactly
 *   RANDOM    RAND moves the engine's values (AMT 127: most of them), inside their ranges; BACK: the sound as it was
 *   the pages: EQ+COMP, MOD, LAYER, LAYER 2, MORPH, RANDOM (screens to DIR/synth3-*.ppm) */
#define UI_PAGES_HARNESS_ONLY
#include "ui_pages_test.c"
#include <math.h>

static void ok(int c, const char *what) { check(c, what); }
static int page_idx(const char *t)
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++)
        if (!strcmp(PAGES[i].title, t))
            return (int)i;
    return -1;
}
static void go_page(const char *t)
{
    int i = page_idx(t);
    if (i < 0)
        return;
    open_family(PAGES[i].fam);
    ui.page = (uint8_t)i;
    ui.fam_last[PAGES[i].fam] = (uint8_t)i;
    page_entered();
    ui.force = 1;
    frame();
}
static void knob(uint32_t k, int32_t s) { encs[panel.enc[EN_K1 + k]] = s; frame(); }
static uint32_t nvoices(uint32_t p)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += trk[p].v[i].active && trk[p].v[i].gate;
    return n;
}

/* ---- the bus alone: a sine through bus_process, its level after the settle (dB) */
static double bus_gain(double f, double amp)
{
    static int32_t l[CTL], r[CTL];
    double ph = 0, ein = 0, eout = 0;
    uint32_t b, i;
    for (b = 0; b < 1200u; b++) {
        for (i = 0; i < CTL; i++) {
            l[i] = r[i] = (int32_t)(amp * sin(ph));
            ph += 2 * M_PI * f / FS;
        }
        if (b >= 800u)
            for (i = 0; i < CTL; i++)
                ein += (double)l[i] * l[i];
        bus_process(l, r, CTL);
        if (b >= 800u)
            for (i = 0; i < CTL; i++)
                eout += (double)l[i] * l[i];
    }
    return 10 * log10((eout + 1e-9) / (ein + 1e-9));
}
static void bus_flat(void)
{
    song.g[G_EQLOW] = EQ_LOW_C, song.g[G_EQMID] = EQ_MID_C, song.g[G_EQHIGH] = EQ_HIGH_C, song.g[G_COMP] = 0;
    {
        uint32_t k;
        double d = 0;
        for (k = 0; k < 400u; k++)                   /* (the gains glide back) */
            d += bus_gain(1000, 1000) * 0;
        (void)d;
    }
}
static void t_eq(void)
{
    char w[160];
    double lo, mid, hi, x;
    bus_flat();
    song.g[G_EQLOW] = EQ_LOW_C + 12;
    lo = bus_gain(60, 3000), x = bus_gain(3000, 3000);
    snprintf(w, sizeof w, "EQ LOW +12: 60 Hz %+.1f dB, 3 kHz %+.1f dB", lo, x);
    ok(fabs(lo - 12) < 1.5 && fabs(x) < 1.5, w);
    song.g[G_EQLOW] = EQ_LOW_C - 12;
    lo = bus_gain(60, 3000);
    snprintf(w, sizeof w, "EQ LOW -12: 60 Hz %+.1f dB", lo);
    ok(fabs(lo + 12) < 1.5, w);
    song.g[G_EQLOW] = EQ_LOW_C, song.g[G_EQHIGH] = EQ_HIGH_C + 12;
    hi = bus_gain(14000, 3000), x = bus_gain(200, 3000);
    snprintf(w, sizeof w, "EQ HIGH +12: 14 kHz %+.1f dB, 200 Hz %+.1f dB", hi, x);
    ok(fabs(hi - 12) < 1.5 && fabs(x) < 1.5, w);
    song.g[G_EQHIGH] = EQ_HIGH_C, song.g[G_EQMID] = EQ_MID_C + 10;
    mid = bus_gain(1000, 3000), x = bus_gain(60, 3000);
    snprintf(w, sizeof w, "EQ MID +10: 1 kHz %+.1f dB, 60 Hz %+.1f dB, 14 kHz %+.1f dB", mid, x, bus_gain(14000, 3000));
    ok(fabs(mid - 10) < 1.5 && fabs(x) < 1.5 && fabs(bus_gain(14000, 3000)) < 1.5, w);
    song.g[G_EQMID] = EQ_MID_C;
    bus_gain(1000, 3000);
    {   /* flat: untouched */
        static int32_t l[CTL], r[CTL], l0[CTL];
        uint32_t i, b, same = 1;
        for (b = 0; b < 400u; b++) {
            for (i = 0; i < CTL; i++)
                l[i] = r[i] = l0[i] = (int32_t)(rand() % 60000) - 30000;
            bus_process(l, r, CTL);
            if (b > 300u)
                for (i = 0; i < CTL; i++)
                    same &= l[i] == l0[i];
        }
        ok(same, "EQ flat, COMP 0: the signal untouched, bit for bit");
    }
    {   /* a band turned while a tone plays: no step (the largest sample-to-sample change stays a tone's) */
        static int32_t l[CTL], r[CTL];
        double ph = 0, prev = 0, worst = 0;
        uint32_t b, i;
        for (b = 0; b < 300u; b++) {
            if (b == 150u)
                song.g[G_EQLOW] = EQ_LOW_C + 12;
            for (i = 0; i < CTL; i++) {
                l[i] = r[i] = (int32_t)(4000 * sin(ph));
                ph += 2 * M_PI * 100 / FS;
            }
            bus_process(l, r, CTL);
            for (i = 0; i < CTL; i++) {
                double d = fabs(l[i] - prev);
                if (b > 10u && d > worst)
                    worst = d;
                prev = l[i];
            }
        }
        snprintf(w, sizeof w, "EQ LOW 0 -> +12 under a 100 Hz tone: no click (largest step %.0f, the tone's ~%.0f)", worst,
                 4 * 4000 * 2 * M_PI * 100 / FS);
        ok(worst < 4 * 4000 * 2 * M_PI * 100 / FS * 1.3, w);
        song.g[G_EQLOW] = EQ_LOW_C;
    }
}
static void t_comp(void)
{
    char w[160];
    double hi0, lo0, hi1, lo1;
    bus_flat();
    hi0 = bus_gain(500, 16000), lo0 = bus_gain(500, 1600);
    song.g[G_COMP] = 127;
    hi1 = bus_gain(500, 16000) + 20, lo1 = bus_gain(500, 1600);
    snprintf(w, sizeof w, "COMP 0: unity (%+.2f, %+.2f dB); COMP 127: 20 dB apart in, %.1f dB out", hi0, lo0, hi1 - lo1);
    ok(fabs(hi0) < 0.05 && fabs(lo0) < 0.05 && hi1 - lo1 < 13 && hi1 - lo1 > 3, w);
    song.g[G_COMP] = 60;
    {
        double a = bus_gain(500, 16000) + 20, b = bus_gain(500, 1600);
        snprintf(w, sizeof w, "COMP 60: %.1f dB out (between)", a - b);
        ok(a - b < 20 && a - b > hi1 - lo1, w);
    }
    song.g[G_COMP] = 0;
    bus_gain(500, 1600);
}

/* ---- voices */
static void sound(uint32_t t, uint32_t e, uint32_t pi)
{
    set_engine_of(&trk[t], e);
    apply_preset_to(&trk[t], pi);
    trk[t].engine = trk[t].eng_req;
    frames(2);
}
static void t_unison(void)
{
    char w[160];
    uint32_t i, a = 0, b = 0;
    int32_t fa[4], nf = 0;
    trk[1].p[P_LYMODE] = LY_OFF;
    sound(0, 0u, 0);
    trk[0].p[P_VOICE] = V_PUNI2, trk[0].p[P_DETUNE] = 80;
    trk_note_on(&trk[0], 60, 100), trk_note_on(&trk[0], 64, 100);
    frames(2);
    for (i = 0; i < NVOICE; i++)
        if (trk[0].v[i].active && trk[0].v[i].gate) {
            a += trk[0].v[i].note == 60;
            b += trk[0].v[i].note == 64;
            if (trk[0].v[i].note == 60 && nf < 4)
                fa[nf++] = trk[0].v[i].fine;
        }
    snprintf(w, sizeof w, "UNI2: two notes on %u + %u voices, the note's two detuned apart (%d, %d)", a, b, (int)fa[0], nf > 1 ? (int)fa[1] : 0);
    ok(a == 2 && b == 2 && nf == 2 && fa[0] == -fa[1] && fa[0] != 0, w);
    trk_note_off(&trk[0], 60), trk_note_off(&trk[0], 64);
    frames(2);
    ok(nvoices(0) == 0, "UNI2: the note-offs let every voice go");
    frames(40);
    trk[0].p[P_VOICE] = V_PUNI4;
    trk_note_on(&trk[0], 60, 100), trk_note_on(&trk[0], 67, 100);
    frames(2);
    snprintf(w, sizeof w, "UNI4: two notes on %u voices", nvoices(0));
    ok(nvoices(0) == 8, w);
    trk_note_off(&trk[0], 60), trk_note_off(&trk[0], 67);
    frames(2);
    ok(nvoices(0) == 0, "UNI4: the note-offs let every voice go");
    trk[0].p[P_VOICE] = V_POLY;
    frames(40);
}

/* the selected sound's output for a held note, blocks of mix (the audio between frames) */
static double capt[4096 * 4];
static void capture(uint32_t nb)
{
    static int32_t o[CTL * 2];
    uint32_t b, i;
    for (b = 0; b < nb; b++) {
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            if (b * CTL + i < 4096u * 4u)
                capt[b * CTL + i] = o[2 * i];
    }
}
static double bright(uint32_t a, uint32_t n)
{
    double e = 0, d = 0;
    uint32_t i;
    for (i = a + 1; i < a + n; i++)
        e += capt[i] * capt[i], d += (capt[i] - capt[i - 1]) * (capt[i] - capt[i - 1]);
    return sqrt(d / (e + 1e-9));
}
static double zc_spread(uint32_t a, uint32_t n)   /* the periods' spread (std / mean) between rising zero crossings */
{
    double last = -1, s = 0, s2 = 0, k = 0, lp = 0, prev = 0;
    uint32_t i;
    for (i = a; i < a + n; i++) {
        lp += (capt[i] - lp) * 0.1;
        if (prev < 0 && lp >= 0) {
            if (last >= 0) {
                double p = i - last;
                s += p, s2 += p * p, k++;
            }
            last = i;
        }
        prev = lp;
    }
    return k > 2 ? sqrt(s2 / k - (s / k) * (s / k)) / (s / k) : 0;
}
static void t_mod(void)
{
    char w[160];
    double b0, b1;
    sound(0, 0u, 0);
    trk[0].p[P_DIST] = trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    trk[0].p[P_E0] = 0, trk[0].p[P_E2] = 0, trk[0].p[P_E3] = 0;   /* ANALOG: a saw alone, no noise */
    trk[0].p[P_ED_PIT] = trk[0].p[P_LD_PIT] = trk[0].p[P_LD_FLT] = 0;
    trk[0].p[P_E4] = 40;                               /* ANALOG CUT: closed down */
    trk[0].p[P_ED_FLT] = 0, trk[0].p[P_VELF] = 63;
    trk[0].p[P_ATK] = 0, trk[0].p[P_SUS] = 127;
    trk_note_on(&trk[0], 48, 30), capture(40), b0 = bright(512, 600), trk_note_off(&trk[0], 48), capture(80);
    trk_note_on(&trk[0], 48, 127), capture(40), b1 = bright(512, 600), trk_note_off(&trk[0], 48), capture(80);
    snprintf(w, sizeof w, "VEL>F +63: a hard note brighter than a soft one (%.3f vs %.3f)", b1, b0);
    ok(b1 > b0 * 1.5, w);
    trk[0].p[P_VELF] = 0;
    trk[0].p[P_MWDST] = MD_VIB, trk[0].p[P_E4] = 127, trk[0].p[P_E0] = 3;   /* a sine */
    part_ctl[0].mw = 0;
    trk_note_on(&trk[0], 57, 100), capture(400), b0 = zc_spread(2048, 4096 * 3);
    part_ctl[0].mw = 127;
    capture(400), b1 = zc_spread(2048, 4096 * 3), trk_note_off(&trk[0], 57), capture(80);
    part_ctl[0].mw = 0;
    snprintf(w, sizeof w, "MW > VIB: the pitch wobbles with the wheel up (period spread %.4f vs %.4f)", b1, b0);
    ok(b1 > b0 * 3 && b1 > 0.004, w);
    trk[0].p[P_ATDST] = MD_CUT, trk[0].p[P_E4] = 40, trk[0].p[P_E0] = 0;
    part_ctl[0].at = 0;
    trk_note_on(&trk[0], 48, 100), capture(40), b0 = bright(512, 600);
    part_ctl[0].at = 127;
    capture(40), b1 = bright(512, 600), trk_note_off(&trk[0], 48), capture(80);
    part_ctl[0].at = 0;
    snprintf(w, sizeof w, "AT > CUT: pressure opens the filter (%.3f vs %.3f)", b1, b0);
    ok(b1 > b0 * 1.5, w);
}

static uint32_t held_gate(uint32_t p, uint32_t note)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += trk[p].v[i].active && trk[p].v[i].gate && trk[p].v[i].note == note;
    return n;
}
static void t_layer(void)
{
    char w[160];
    sound(0, 0u, 0);
    sound(1, host_eng_built(ENGI_STRINGS), 0);           /* (Forge Five: HYBRID) */
    trk[0].p[P_VOICE] = V_POLY, trk[1].p[P_VOICE] = V_POLY;
    trk[1].p[P_LYMODE] = LY_LAYER, trk[1].p[P_LYOCT] = 3, trk[1].p[P_LYSPLIT] = 64;
    trk_note_on(&trk[0], 60, 100);
    frames(2);
    ok(held_gate(0, 60) == 1 && held_gate(1, 72) == 1, "LAYER, OCT +1: a note on both sounds, the layer's an octave up");
    ok(!trk_silent(&trk[1]) && trk_silent(&trk[2]), "LAYER: track 2 sounds, track 3 does not");
    trk[1].p[P_LYMODE] = LY_SPLIT;                      /* the mode changes under the held note */
    trk_note_off(&trk[0], 60);
    frames(2);
    ok(!held_gate(0, 60) && !held_gate(1, 72), "a note held while the mode changes lets go where it went");
    trk[1].p[P_LYOCT] = 2;
    trk_note_on(&trk[0], 55, 100), trk_note_on(&trk[0], 62, 100);
    frames(2);
    snprintf(w, sizeof w, "SPLIT at C4: G3 on the layer only, D4 on the main sound only");
    ok(held_gate(1, 55) == 1 && !held_gate(0, 55) && held_gate(0, 62) == 1 && !held_gate(1, 62), w);
    trk_note_off(&trk[0], 55), trk_note_off(&trk[0], 62);
    frames(2);
    ok(!held_gate(0, 62) && !held_gate(1, 55), "SPLIT: the note-offs");
    trk[1].p[P_LYMODE] = LY_OFF;
    trk_note_on(&trk[0], 60, 100);
    frames(2);
    ok(held_gate(0, 60) == 1 && nvoices(1) == 0 && trk_silent(&trk[1]), "OFF: the main sound alone, the layer silent");
    trk_note_off(&trk[0], 60);
    frames(30);
    {   /* the pages: SOUND turned loads track 2's sound; SWAP twice */
        uint32_t e0, p0, e1, p1;
        go_page("LAYER");
        knob(0, 1);                                     /* MODE: LAYER */
        ok(trk[1].p[P_LYMODE] == LY_LAYER, "LAYER page, KNOB 1: MODE LAYER");
        e1 = trk[1].eng_req, p1 = trk[1].preset;
        knob(1, 3);
        ok(trk[1].eng_req != e1 || trk[1].preset != p1, "LAYER page, KNOB 2: SOUND loads another sound on track 2");
        knob(2, -4);
        snprintf(w, sizeof w, "LAYER page, KNOB 3: SPLIT down 4 (%d)", trk[1].p[P_LYSPLIT] - 4);
        ok(trk[1].p[P_LYSPLIT] == 60, w);
        ppm("synth3-layer");
        go_page("LAYER 2");
        ppm("synth3-layer2");
        e0 = trk[0].eng_req, p0 = trk[0].preset, e1 = trk[1].eng_req, p1 = trk[1].preset;
        knob(3, 1);
        frames(20);
        ok(trk[0].eng_req == e1 && trk[0].preset == p1 && trk[1].eng_req == e0 && trk[1].preset == p0,
           "SWAP: the two sounds trade places");
        knob(3, 1);
        frames(20);
        ok(trk[0].eng_req == e0 && trk[0].preset == p0 && trk[1].eng_req == e1 && trk[1].preset == p1,
           "SWAP again: back as they were");
        go_page("LAYER");
        knob(0, -1);
        ok(trk[1].p[P_LYMODE] == LY_OFF, "LAYER page: MODE OFF");
    }
}

static void t_morph(void)
{
    char w[160];
    int16_t a4, b4, a0, b0;
    uint32_t mid_ok;
    sound(0, 0u, 0);
    trk[1].p[P_LYMODE] = LY_OFF;
    go_page("MORPH");
    knob(1, 1);                                         /* SET A */
    a4 = trk[0].p[P_E4], a0 = trk[0].p[P_E0];
    trk[0].p[P_E4] = (int16_t)(a4 > 60 ? 10 : 120);
    trk[0].p[P_E0] = (int16_t)(a0 ? 0 : 2);
    trk[0].p[P_ATK] = 100;
    b4 = trk[0].p[P_E4], b0 = trk[0].p[P_E0];
    knob(2, 1);                                         /* SET B */
    ok(syn_a.ok && syn_b.ok && xv[X_MORPH] == 127, "MORPH: SET A, SET B (MORPH at B)");
    knob(0, -127);
    ok(trk[0].p[P_E4] == a4 && trk[0].p[P_E0] == a0 && trk[0].p[P_ATK] == syn_a.p[P_ATK], "MORPH 0: A exactly");
    xv[X_MORPH] = 0;
    {
        int16_t *vp = &xv[X_MORPH];
        *vp = 64;
        syn_changed(X_MORPH, vp);
    }
    mid_ok = abs(trk[0].p[P_E4] - (a4 + b4) / 2) <= 1 && trk[0].p[P_E0] == b0;
    snprintf(w, sizeof w, "MORPH half way: CUT %d between %d and %d, WAVE at B's (%d)", trk[0].p[P_E4], a4, b4, trk[0].p[P_E0]);
    ok(mid_ok, w);
    knob(0, 127);
    ok(trk[0].p[P_E4] == b4 && trk[0].p[P_ATK] == 100, "MORPH 127: B exactly");
    ppm("synth3-morph");
    syn_morph_req = 0;                                  /* MIDI CC 20 */
    frame();
    ok(trk[0].p[P_E4] == a4 && xv[X_MORPH] == 0, "MIDI CC 20 moves MORPH");
}

static void t_random(void)
{
    char w[160];
    int16_t before[P_COUNT];
    uint32_t i, moved = 0, inside = 1;
    sound(0, host_eng_built(ENGI_ACID), 0);
    memcpy(before, trk[0].p, sizeof before);
    go_page("RANDOM");
    knob(0, 127);                                       /* AMT 127 */
    knob(2, 1);                                         /* RAND */
    for (i = P_E0; i <= P_E7; i++) {
        const param_desc_t *d = &ENGINES[host_eng_built(ENGI_ACID)]->edit[i - P_E0];
        moved += trk[0].p[i] != before[i];
        inside &= trk[0].p[i] >= d->min && trk[0].p[i] <= d->max;
    }
    for (i = 0; i < P_E0; i++)
        inside &= trk[0].p[i] >= TP[i].min && trk[0].p[i] <= TP[i].max;
    snprintf(w, sizeof w, "RAND (AMT 127): %u of the 8 values moved, all inside their ranges", moved);
    ok(moved >= 5 && inside && trk[0].p[P_LEVEL] == before[P_LEVEL] && trk[0].p[P_SLEN] == before[P_SLEN], w);
    ppm("synth3-random");
    knob(3, 1);                                         /* BACK */
    ok(!memcmp(trk[0].p, before, sizeof before), "BACK: the sound as it was");
}


/* a preset changed under a held note (0.3): the old sound fades out with its own values, no step; the same for a
 * WAVE table change (and the new table built in a few passes of the main loop) */
static double max_step(uint32_t a, uint32_t b)
{
    double m = 0;
    uint32_t i;
    for (i = a + 1u; i < b; i++)
        if (fabs(capt[i] - capt[i - 1]) > m)
            m = fabs(capt[i] - capt[i - 1]);
    return m;
}
static void t_fade(void)
{
    char w[160];
    static const char *const TO[] = {"SUB BASS", "SINE PAD", "FUNK BASS", "TRAP PLUCK", "MINI LEAD"};
    uint32_t k, worst_ok = 1;
    double worst = 0;
    trk[1].p[P_LYMODE] = LY_OFF;
    for (k = 0; k < 5u; k++) {
        double before, after;
        uint32_t pi, i;
        sound(0, 0u, 0);
        trk[0].p[P_E0] = 3, trk[0].p[P_E4] = 127, trk[0].p[P_E6] = 0, trk[0].p[P_SUS] = 127, trk[0].p[P_REL] = 100;
        trk[0].p[P_ATK] = 0, trk[0].p[P_ED_PIT] = trk[0].p[P_LD_PIT] = 0, trk[0].p[P_VOICE] = V_POLY;
        trk[0].p[P_DLY] = trk[0].p[P_REV] = trk[0].p[P_CHOR] = trk[0].p[P_DIST] = 0;
        trk_note_on(&trk[0], 57, 120);
        capture(300);
        before = max_step(200 * CTL, 300 * CTL);
        for (pi = 0, i = 0; i < ENGINES[0]->npresets; i++)
            if (!strcmp(ENGINES[0]->presets[i].name, TO[k]))
                pi = i;
        apply_preset_to(&trk[0], pi);
        capture(120);
        after = max_step(0, 120 * CTL);
        trk_note_off(&trk[0], 57);
        frames(20);
        if (after / before > worst)
            worst = after / before;
        worst_ok &= after < before * 1.3;
    }
    snprintf(w, sizeof w, "a preset changed under a held sine: no step (the largest x%.2f of the sine's own)", worst);
    ok(worst_ok, w);
    {   /* WAVE: a table change under a held note; the new table */
        uint32_t pa = 0, pb = 0, i, passes = 0;
        double before, after;
        for (i = 0; i < ENGINES[ENGI_WAVE]->npresets; i++) {
            if (!strcmp(ENGINES[ENGI_WAVE]->presets[i].name, "WT BASIC")) pa = i;
            if (!strcmp(ENGINES[ENGI_WAVE]->presets[i].name, "VOCAL WT")) pb = i;
        }
        sound(0, ENGI_WAVE, pa);
        while (wt_have != (int16_t)(trk[0].p[P_E0] % WT_NALL) && passes < 100)
            wt_service(), passes++;
        trk[0].p[P_DLY] = trk[0].p[P_REV] = trk[0].p[P_CHOR] = 0, trk[0].p[P_SUS] = 127, trk[0].p[P_REL] = 100;
        trk_note_on(&trk[0], 45, 110);
        capture(300);
        before = max_step(200 * CTL, 300 * CTL);
        apply_preset_to(&trk[0], pb);
        {   /* the audio runs, the main loop builds between blocks (as on the FM-1) */
            static int32_t o[CTL * 2];
            uint32_t bl, j;
            for (bl = 0; bl < 120u; bl++) {
                mix_block(o, CTL);
                for (j = 0; j < CTL; j++)
                    capt[bl * CTL + j] = o[2 * j];
                if (bl >= 10u) {
                    wt_service();
                    if (wt_have != (int16_t)(trk[0].p[P_E0] % WT_NALL))
                        passes++;
                }
            }
        }
        after = max_step(0, 120 * CTL);
        trk_note_off(&trk[0], 45);
        frames(20);
        snprintf(w, sizeof w, "WAVE: a table change under a held note fades (x%.2f of the saw's own step)", after / before);
        ok(after < before * 1.3, w);
    }
    {   /* the build: passes of wt_service for a whole table */
        uint32_t n = 0;
        wt_have = -1, wt_building = -1;
        trk[0].p[P_E0] = 4;
        while (wt_have != 4 && n < 100)
            wt_service(), n++;
        snprintf(w, sizeof w, "WAVE: a whole table built in %u passes of the main loop", n);
        ok(n <= 4u, w);
    }
}

static void t_pages(void)
{
    static const char *const T[] = {"EQ+COMP", "MOD", "LAYER", "LAYER 2", "MORPH", "RANDOM"};
    uint32_t i, all = 1;
    for (i = 0; i < 6u; i++)
        all &= page_idx(T[i]) >= 0;
    ok(all, "the pages: EQ+COMP, MOD, LAYER, LAYER 2, MORPH, RANDOM");
    go_page("EQ+COMP");
    knob(0, 3), knob(3, 40);
    ok(song.g[G_EQLOW] == EQ_LOW_C + 3 && song.g[G_COMP] > 0, "EQ+COMP page: LOW +3 dB, COMP up");
    ppm("synth3-eqcomp");
    song.g[G_EQLOW] = EQ_LOW_C, song.g[G_COMP] = 0;
    go_page("MOD");
    ppm("synth3-mod");
    go_page("VOICE");
    knob(0, 4);
    ok(trk[0].p[P_VOICE] == V_PUNI2 || trk[0].p[P_VOICE] == V_PUNI4, "VOICE page: UNI2 / UNI4 reachable");
    trk[0].p[P_VOICE] = V_POLY;
    go_home();
    frames(2);
}

/* Clipper 0.4: the pages LFO 2, ENV 2, MATRIX (its knobs: SLOT, SRC, DST, AMT on the slot turned to), MPE kept */
static void t_pages4(void)
{
    track_t *t = &trk[0];
    char w[160];
    uint32_t k;
    ok(page_idx("LFO 2") >= 0 && page_idx("ENV 2") >= 0 && page_idx("MATRIX") >= 0, "0.4: the pages LFO 2, ENV 2, MATRIX");
    for (k = 0; k < MX_SLOTS; k++)
        t->p[mx_pid(k)] = MS_OFF, t->p[mx_pid(k) + 1u] = MT_OFF, t->p[mx_pid(k) + 2u] = 0;
    go_page("MATRIX");
    knob(1, 2), knob(2, 2), knob(3, 30);
    snprintf(w, sizeof w, "MATRIX: slot 1 turned to LFO2 > CUT +30 (%d %d %d)", t->p[P_M1SRC], t->p[P_M1DST], t->p[P_M1AMT]);
    ok(t->p[P_M1SRC] == MS_LFO2 && t->p[P_M1DST] == MT_CUT && t->p[P_M1AMT] == 30, w);
    knob(0, 2), knob(1, 5), knob(2, MT_E1 + 2);
    snprintf(w, sizeof w, "MATRIX: SLOT 3, its own values (VEL > E3), slot 1 kept (%d %d)", t->p[P_M3SRC], t->p[P_M3DST]);
    ok(t->p[P_M3SRC] == MS_VEL && t->p[P_M3DST] == MT_E1 + 2 && t->p[P_M1SRC] == MS_LFO2, w);
    knob(3, -20);
    ppm("synth4-matrix");
    go_page("LFO 2");
    knob(2, 1);
    ok(t->p[P_L2MODE] == L2_KEY, "LFO 2 page: MODE KEY");
    ppm("synth4-lfo2");
    go_page("ENV 2");
    t->p[P_E2SUS] = 0;
    knob(2, 3);
    snprintf(w, sizeof w, "ENV 2 page: SUS turned up (%d)", t->p[P_E2SUS]);
    ok(t->p[P_E2SUS] > 0, w);
    ppm("synth4-env2");
    for (k = 0; k < MX_SLOTS; k++)
        t->p[mx_pid(k)] = MS_OFF, t->p[mx_pid(k) + 1u] = MT_OFF, t->p[mx_pid(k) + 2u] = 0;
    t->p[P_L2MODE] = L2_FREE;
    mpe_mode = 4;
    k = lights_word();
    mpe_mode = 0;
    lights_from_word(k);
    ok(mpe_mode == 4, "MPE (menu SYSTEM): kept with the settings");
    mpe_mode = 0;
    go_home();
    frames(2);
}

int main(int argc, char **argv)
{
    uint32_t i;
    outdir = argc > 1 ? argv[1] : "build/host";
    bank_resolve();
    panel = PANEL_DEFAULT;
    layers_init();
    settings.palette = 4;
    palette_set(4);
    host_tracks_init();
    for (i = 0; i < NPART; i++) { set_engine_of(&trk[i], TRK_DEF[i][0]); apply_preset_to(&trk[i], TRK_DEF[i][1]); trk[i].engine = trk[i].eng_req; }
    clipper_proj_neutral();
    song.g[G_BPM] = 120;
    go_home(); ui.force = 1; frames(3);
    t_eq();
    t_comp();
    t_unison();
    t_mod();
    t_layer();
    t_morph();
    t_random();
    t_fade();
    t_pages();
    t_pages4();
    printf(fails ? "synth3: %d FAILED\n" : "synth3: all passed\n", fails);
    return fails;
}
