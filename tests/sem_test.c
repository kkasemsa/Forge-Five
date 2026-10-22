/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Khantipol Kasemsant; built on Felucca, (C) 2026 Leo Kuroshita, Hügelton Instruments */
/* SEM engine test (firmware/src/eng_sem.c) on the host, through hostsim.c as ladder_test.c.
 *   build/host/sem_test [DEMODIR]
 * 1. pitch: OSC 1 at A4 440 Hz at 8', 220 at 16', 880 at 4' (+-0.3 %); OSC 2 FREQ +56 a fifth up; DETUNE 100 ct a
 *    semitone; SYNC: OSC 2 at FREQ +30 st still repeats at OSC 1's period (its fundamental OSC 1's).
 * 2. the filter: white noise, MODE 0 (LP): -12 dB per octave above the cutoff (+-3 dB); MODE 127 (HP): +12 below;
 *    MODE 64: a notch at the cutoff (the band at fc 15 dB under its neighbours an octave away); TYPE 4P: -24 dB per
 *    octave; TYPE BP: a peak at fc, falling both ways; RESO 127 never self-oscillates (no oscillators: silence).
 * 3. the filter envelope: F AMT +63 at F ATK 0, F DEC 127 opens the filter: the first 50 ms brighter than the last
 *    (spectral centroid); F AMT -64 the other way.
 * 4. levels: every preset within -10 .. +4 dB of ANALOG's SAW LEAD; peaks under 2 x VOICE_FS; no DC.
 * 5. corners: CUTOFF / RESO / MODE / TYPE / SYNC / X-MOD / NOISE at notes 12, 60, 115: bounded, free and silent after.
 * 6. determinism. 7. cost: against ANALOG, fails above 2.0 x (the weight is 1.5).
 * 8. demos (DEMODIR): each preset playing a phrase through the mix. */
#include <ctype.h>
#define main hostsim_main
#include "hostsim.c"
#undef main

#define NFFT 16384
#define SEM_COST_MAX 2.0
#include "spectra.h"

static int bad;
static void check(const char *what, int ok)
{
    printf("sem: %-92s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static void setup(int32_t pi)
{
    memset(trk, 0, sizeof trk);
    memset(eng_mem, 0, sizeof eng_mem);
    memset(eng_mem_owner, 0, sizeof eng_mem_owner);
    host_tracks_init();
    vage = 0;
    host_preset(&trk[0], ENGI_SEM, pi < 0 ? 0u : (uint32_t)pi);
    if (pi < 0) {                       /* a plain patch: OSC 1 saw at 8', the filter open and flat, a flat envelope */
        uint32_t k;
        xp_defaults(&trk[0]);
        for (k = 0; k < 8u; k++)
            trk[0].p[P_E0 + k] = ENG_SEM.edit[k].def;
        trk[0].p[P_E0] = 127;           /* CUTOFF */
        trk[0].p[P_E1] = 0;             /* RESO */
        trk[0].p[P_E2] = 0;             /* MODE LP */
        trk[0].p[P_E3] = 0;             /* TYPE 2P */
        SXP(&trk[0], SX_FAMT) = 0;
        SXP(&trk[0], SX_KBD) = 0;
        SXP(&trk[0], SX_O2LVL) = 0;
        SXP(&trk[0], SX_DTN) = 0;
        trk[0].p[P_ATK] = 0;
        trk[0].p[P_SUS] = 127;
        trk[0].p[P_REL] = 10;
        trk[0].p[P_VOICE] = V_POLY;
    }
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = trk[0].p[P_DIST] = 0;
}

static void voice_render(uint32_t note, double skip_s, double *y, uint32_t n)
{
    static int32_t b[CTL];
    uint32_t i, k, skip = (uint32_t)(skip_s * FS) / CTL;
    trk_note_on(&trk[0], note, 100);
    for (i = 0; i < skip; i++)
        track_render(&trk[0], b, CTL);
    for (i = 0; i < n; i += CTL) {
        track_render(&trk[0], b, CTL);
        for (k = 0; k < CTL && i + k < n; k++)
            y[i + k] = b[k] / 32768.0;
    }
}

static void noise_only(void)
{
    SXP(&trk[0], SX_O1LVL) = 0;
    SXP(&trk[0], SX_O2LVL) = 0;
    SXP(&trk[0], SX_NOISE) = 127;
}

static void t_pitch(void)
{
    static const uint32_t RNG[] = {0, 1, 2};
    static const double MUL[] = {0.5, 1, 2};
    char m[128];
    uint32_t k;
    for (k = 0; k < 3u; k++) {
        double f;
        setup(-1);
        SXP(&trk[0], SX_O1RNG) = (int16_t)RNG[k];
        SXP(&trk[0], SX_O1WAV) = SW_TRI;
        voice_render(69, 0.1, buf, 2 * NFFT);
        f = peak_hz(buf, 30);
        snprintf(m, sizeof m, "pitch: OSC 1 %s at A4: %.2f Hz (%.2f)", N_SEM_RANGE[RNG[k]], f, 440 * MUL[k]);
        check(m, fabs(f / (440 * MUL[k]) - 1) < 0.003);
    }
    {
        double f, want = 440 * pow(2, 7 / 12.0);
        setup(-1);
        SXP(&trk[0], SX_O1LVL) = 0;
        SXP(&trk[0], SX_O2LVL) = 127;
        SXP(&trk[0], SX_O2WAV) = SW_TRI;
        SXP(&trk[0], SX_O2FRQ) = 56;
        voice_render(69, 0.1, buf, 2 * NFFT);
        f = peak_hz(buf, 30);
        snprintf(m, sizeof m, "pitch: OSC 2 FREQ +56 at A4: %.2f Hz (%.2f, a fifth)", f, want);
        check(m, fabs(f / want - 1) < 0.003);
        setup(-1);
        SXP(&trk[0], SX_O1LVL) = 0;
        SXP(&trk[0], SX_O2LVL) = 127;
        SXP(&trk[0], SX_O2WAV) = SW_TRI;
        SXP(&trk[0], SX_DTN) = 100;
        voice_render(69, 0.1, buf, 2 * NFFT);
        f = peak_hz(buf, 30);
        want = 440 * pow(2, 1 / 12.0);
        snprintf(m, sizeof m, "pitch: OSC 2 DETUNE 100 ct at A4: %.2f Hz (%.2f, a semitone)", f, want);
        check(m, fabs(f / want - 1) < 0.003);
    }
    {   /* SYNC: OSC 2 far above, its period OSC 1's */
        double f;
        setup(-1);
        SXP(&trk[0], SX_O1LVL) = 0;
        SXP(&trk[0], SX_O2LVL) = 127;
        SXP(&trk[0], SX_O2FRQ) = 48;                   /* ~+5 semitones */
        SXP(&trk[0], SX_SYNC) = 1;
        voice_render(57, 0.1, buf, 4 * FS);
        f = zc_hz(buf, 4 * FS);
        snprintf(m, sizeof m, "SYNC: OSC 2 at FREQ +48 repeats at OSC 1's A3: %.2f Hz by zero crossings (220)", f);
        check(m, fabs(f / 220 - 1) < 0.02);
    }
}

static void t_filter(void)
{
    static double ps[NFFT / 2];
    char m[160];
    double fc, lo, hi, mid;
    uint32_t k;
    /* LP slope */
    setup(-1);
    noise_only();
    trk[0].p[P_E0] = 60;
    fc = CUTOFF_HZ[60];
    voice_render(60, 0.1, buf, 8 * NFFT);
    spectrum(buf, 8, ps);
    lo = band_db(ps, fc * 4 * 0.9, fc * 4 * 1.1);
    hi = band_db(ps, fc * 8 * 0.9, fc * 8 * 1.1);
    snprintf(m, sizeof m, "filter: LP (MODE 0): %.0f Hz cutoff, %.1f dB from 4 fc to 8 fc (-12)", fc, hi - lo);
    check(m, hi - lo < -9 && hi - lo > -15);
    /* HP slope */
    setup(-1);
    noise_only();
    trk[0].p[P_E0] = 80;
    trk[0].p[P_E2] = 127;
    fc = CUTOFF_HZ[80];
    voice_render(60, 0.1, buf, 8 * NFFT);
    spectrum(buf, 8, ps);
    lo = band_db(ps, fc / 8 * 0.9, fc / 8 * 1.1);
    hi = band_db(ps, fc / 4 * 0.9, fc / 4 * 1.1);
    snprintf(m, sizeof m, "filter: HP (MODE 127): %.0f Hz, %.1f dB from fc/8 to fc/4 (+12)", fc, hi - lo);
    check(m, hi - lo > 9 && hi - lo < 15);
    /* notch */
    setup(-1);
    noise_only();
    trk[0].p[P_E0] = 70;
    trk[0].p[P_E2] = 64;
    trk[0].p[P_E1] = 60;
    fc = CUTOFF_HZ[70];
    voice_render(60, 0.1, buf, 8 * NFFT);
    spectrum(buf, 8, ps);
    mid = band_db(ps, fc * 0.97, fc * 1.03);
    lo = band_db(ps, fc / 2 * 0.9, fc / 2 * 1.1);
    hi = band_db(ps, fc * 2 * 0.9, fc * 2 * 1.1);
    snprintf(m, sizeof m, "filter: NOTCH (MODE 64) at %.0f Hz: %.1f dB under the octave below, %.1f under the one above (> 12)", fc, lo - mid, hi - mid);
    check(m, lo - mid > 12 && hi - mid > 12);
    /* 4P */
    setup(-1);
    noise_only();
    trk[0].p[P_E0] = 60;
    trk[0].p[P_E3] = 1;
    fc = CUTOFF_HZ[60];
    voice_render(60, 0.1, buf, 8 * NFFT);
    spectrum(buf, 8, ps);
    lo = band_db(ps, fc * 2 * 0.9, fc * 2 * 1.1);
    hi = band_db(ps, fc * 4 * 0.9, fc * 4 * 1.1);                   /* (4 fc .. 8 fc is at the noise floor already) */
    snprintf(m, sizeof m, "filter: TYPE 4P: %.1f dB from 2 fc to 4 fc (-24)", hi - lo);
    check(m, hi - lo < -20 && hi - lo > -28);
    /* BP */
    setup(-1);
    noise_only();
    trk[0].p[P_E0] = 70;
    trk[0].p[P_E3] = 2;
    trk[0].p[P_E1] = 80;
    fc = CUTOFF_HZ[70];
    voice_render(60, 0.1, buf, 8 * NFFT);
    spectrum(buf, 8, ps);
    mid = band_db(ps, fc * 0.95, fc * 1.05);
    lo = band_db(ps, fc / 4 * 0.9, fc / 4 * 1.1);
    hi = band_db(ps, fc * 4 * 0.9, fc * 4 * 1.1);
    snprintf(m, sizeof m, "filter: TYPE BP at %.0f Hz: %.1f dB over two octaves below, %.1f over two above (> 9: 6 dB/oct)", fc, mid - lo, mid - hi);
    check(m, mid - lo > 9 && mid - hi > 9);
    /* no self-oscillation */
    for (k = 0; k < 2u; k++) {
        setup(-1);
        SXP(&trk[0], SX_O1LVL) = 0;
        trk[0].p[P_E0] = k ? 100 : 50;
        trk[0].p[P_E1] = 127;
        voice_render(60, 0.5, buf, FS);
        snprintf(m, sizeof m, "filter: RESO 127 with no source at CUTOFF %d: silence, RMS %.5f (the SEM never sings)", k ? 100 : 50, rms(buf, FS));
        check(m, rms(buf, FS) < 0.001);
    }
}

static double centroid(const double *y, uint32_t n)
{
    static double ps[NFFT / 2];
    static double w[NFFT];
    uint32_t i;
    double num = 0, den = 0;
    for (i = 0; i < NFFT; i++)
        w[i] = i < n ? y[i] : 0;
    spectrum(w, 1, ps);
    for (i = 1; i < NFFT / 2; i++) {
        num += ps[i] * i;
        den += ps[i];
    }
    return den > 0 ? num / den * FS / NFFT : 0;
}
static void t_fenv(void)
{
    char m[160];
    double c0, c1;
    setup(-1);
    trk[0].p[P_E0] = 40;
    trk[0].p[P_E4] = 0;
    trk[0].p[P_E5] = 110;
    trk[0].p[P_E6] = 0;
    SXP(&trk[0], SX_FAMT) = 63;
    voice_render(48, 0, buf, 2 * FS);
    c0 = centroid(buf, (uint32_t)(0.05 * FS));
    c1 = centroid(buf + FS, (uint32_t)(0.3 * FS));
    snprintf(m, sizeof m, "filter envelope: F AMT +63, decaying: centroid %.0f Hz in the first 50 ms, %.0f Hz a second in", c0, c1);
    check(m, c0 > c1 * 1.5);
    setup(-1);
    trk[0].p[P_E0] = 90;
    trk[0].p[P_E4] = 0;
    trk[0].p[P_E5] = 110;
    trk[0].p[P_E6] = 0;
    SXP(&trk[0], SX_FAMT) = -64;
    voice_render(48, 0, buf, 2 * FS);
    c0 = centroid(buf, (uint32_t)(0.05 * FS));
    c1 = centroid(buf + FS, (uint32_t)(0.3 * FS));
    snprintf(m, sizeof m, "filter envelope: F AMT -64: %.0f Hz first, %.0f Hz a second in (the other way)", c0, c1);
    check(m, c1 > c0 * 1.25);
}

static double analog_rms(void)
{
    uint32_t e;
    for (e = 0; e < NENGINES && ENGINES[e] != &ENG_ANALOG; e++)
        ;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    host_preset(&trk[0], e, 0);
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = trk[0].p[P_DIST] = 0;
    voice_render(48, 0.2, buf, FS);
    return rms(buf, FS);
}

static void t_levels(void)
{
    double ra = analog_rms();
    uint32_t pi;
    printf("sem: levels (one note, C3, velocity 100; ANALOG SAW LEAD %.3f):\n", ra);
    for (pi = 0; pi < ENG_SEM.npresets; pi++) {
        double r = 0, pk = 0, mean = 0;
        uint32_t i, w, wl = (uint32_t)(0.05 * FS);
        char m[160];
        setup((int32_t)pi);
        voice_render(pi == 13 ? 60u : 48u, 0, buf, 6 * FS);
        for (w = 0; w + wl <= 6u * FS; w += wl / 2u) {
            double q = rms(buf + w, wl);
            r = q > r ? q : r;
        }
        for (i = 0; i < 6u * FS; i++) {
            pk = fabs(buf[i]) > pk ? fabs(buf[i]) : pk;
            mean += buf[i];
        }
        mean /= 6u * FS;
        snprintf(m, sizeof m, "level: %-13s RMS %.3f (%+.1f dB), peak %.3f of full scale, mean %+.4f", ENG_SEM.presets[pi].name, r,
                 20 * log10(r / ra), pk, mean);
        check(m, 20 * log10(r / ra) > -10 && 20 * log10(r / ra) < 4 && pk < 2.0 * VOICE_FS / 32768.0 && fabs(mean) < 0.01);
    }
}

static void t_corners(void)
{
    static const int16_t CUT[] = {0, 127}, RS[] = {0, 127}, MD[] = {0, 64, 127};
    static const uint32_t NOTE[] = {12, 60, 115};
    uint32_t a, b, c, d, n, fails = 0, cases = 0;
    double worst = 0;
    char m[200];
    for (a = 0; a < 2u; a++)
        for (b = 0; b < 2u; b++)
            for (c = 0; c < 3u; c++)
                for (d = 0; d < 3u; d++)
                    for (n = 0; n < 3u; n++) {
                        static int32_t ob[CTL];
                        double pk = 0;
                        uint32_t i, k, j, free_ok, quiet = 1;
                        setup(-1);
                        trk[0].p[P_E0] = CUT[a];
                        trk[0].p[P_E1] = RS[b];
                        trk[0].p[P_E2] = MD[c];
                        trk[0].p[P_E3] = (int16_t)d;
                        SXP(&trk[0], SX_O1WAV) = SW_BOTH;
                        SXP(&trk[0], SX_O2WAV) = SW_PLS;
                        SXP(&trk[0], SX_O1LVL) = SXP(&trk[0], SX_O2LVL) = 127;
                        SXP(&trk[0], SX_NOISE) = 127;
                        SXP(&trk[0], SX_SYNC) = (int16_t)(n & 1u);
                        SXP(&trk[0], SX_O2FRQ) = 40;
                        SXP(&trk[0], SX_XMOD) = 127;
                        SXP(&trk[0], SX_PWM) = 127;
                        SXP(&trk[0], SX_FAMT) = 63;
                        SXP(&trk[0], SX_KBD) = 2;
                        trk[0].p[P_REL] = 30;
                        voice_render(NOTE[n], 0.05, buf, FS / 2);
                        for (i = 0; i < FS / 2; i++)
                            pk = fabs(buf[i]) > pk ? fabs(buf[i]) : pk;
                        trk_note_off(&trk[0], NOTE[n]);
                        for (j = 0; j < 3u * FS / CTL; j++) {
                            track_render(&trk[0], ob, CTL);
                            if (j > FS / CTL)
                                for (k = 0; k < CTL; k++)
                                    quiet &= ob[k] == 0;
                        }
                        free_ok = 1;
                        for (k = 0; k < NVOICE; k++)
                            free_ok &= !trk[0].v[k].active;
                        cases++;
                        worst = pk > worst ? pk : worst;
                        if (pk > 2.0 * VOICE_FS / 32768.0 || !free_ok || !quiet) {
                            if (fails++ < 4)
                                printf("sem:   corner CUT %d RESO %d MODE %d TYPE %u note %u: peak %.3f, free %u, silent %u\n",
                                       CUT[a], RS[b], MD[c], d, NOTE[n], pk, free_ok, quiet);
                        }
                    }
    snprintf(m, sizeof m, "corners: %u renders bounded (worst peak %.3f of full scale), free, silent after", cases, worst);
    check(m, !fails);
}

static void t_determinism(void)
{
    static double a[FS / 2];
    uint32_t i, same = 1;
    setup(0);
    voice_render(40, 0, a, FS / 2);
    setup(0);
    voice_render(40, 0, buf, FS / 2);
    for (i = 0; i < FS / 2; i++)
        same &= a[i] == buf[i];
    check("determinism: a note from silence renders the same samples again", same);
}

static double cost_ns(const engine_t *e, uint32_t pi, int heavy)
{
    static int32_t ob[CTL];
    uint32_t i, k, ei, nb = 20000, nv = 0;
    uint64_t t0;
    for (ei = 0; ei < NENGINES && ENGINES[ei] != e; ei++)
        ;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    host_preset(&trk[0], ei, pi);
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_MAXV] = 0;
    trk[0].p[P_REL] = 127;
    if (heavy) {
        trk[0].p[P_E3] = 1;
        SXP(&trk[0], SX_O1WAV) = SW_BOTH;
        SXP(&trk[0], SX_O2WAV) = SW_BOTH;
        SXP(&trk[0], SX_NOISE) = 100;
        SXP(&trk[0], SX_SYNC) = 1;
        SXP(&trk[0], SX_XMOD) = 60;
        SXP(&trk[0], SX_PWM) = 60;
    }
    for (k = 0; k < NVOICE; k++)
        trk_note_on(&trk[0], 40 + 3 * k, 100);
    for (i = 0; i < 200; i++)
        track_render(&trk[0], ob, CTL);
    for (k = 0; k < NVOICE; k++)
        nv += trk[0].v[k].active;
    t0 = now_ns();
    for (i = 0; i < nb; i++)
        track_render(&trk[0], ob, CTL);
    return (double)(now_ns() - t0) / (nb * CTL) / (nv ? nv : 1u);
}

static void t_cost(void)
{
    double a = 0, l = 0, h = 0;
    uint32_t r;
    char m[200];
    for (r = 0; r < 3u; r++) {
        double x = cost_ns(&ENG_ANALOG, 0, 0), y = cost_ns(&ENG_SEM, 0, 0), z = cost_ns(&ENG_SEM, 0, 1);
        a = r && a < x ? a : x;
        l = r && l < y ? l : y;
        h = r && h < z ? h : z;
    }
    snprintf(m, sizeof m, "cost per voice: ANALOG %.2f ns, SEM %.2f ns (x%.2f), heaviest %.2f ns (x%.2f): ~%.0f instr on the device",
             a, l, l / a, h, h / a, 163 * h / a);
    check(m, h / a < SEM_COST_MAX);
}

/* demos: a chord phrase for the pads and stabs, a line for the leads and basses */
static const uint8_t CHORD_A[4] = {57, 60, 64, 67}, CHORD_B[4] = {55, 59, 62, 67}, CHORD_C[4] = {53, 57, 60, 65};
static const uint8_t LINE[8] = {45, 48, 52, 57, 55, 52, 48, 47};
static void demo(const char *dir, uint32_t pi, FILE *tour)
{
    static int32_t o[2 * CTL];
    char path[512], slug[32];
    uint32_t i, f, frames = 6u * FS, bar = FS * 2, step = FS / 4, k;
    int line = pi == 4 || pi == 6 || pi == 8 || pi == 11 || pi == 9 || pi == 15, hold = pi == 14;
    const uint8_t *cur = 0;
    uint32_t curn = 0;
    FILE *w;
    for (i = 0, k = 0; ENG_SEM.presets[pi].name[i] && k < sizeof slug - 1; i++)
        slug[k++] = ENG_SEM.presets[pi].name[i] == ' ' ? '_' : (char)tolower(ENG_SEM.presets[pi].name[i]);
    slug[k] = 0;
    snprintf(path, sizeof path, "%s/%02u_%s.wav", dir, pi + 1, slug);
    w = fopen(path, "wb");
    if (!w)
        return;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    song.g[G_RTYPE] = 2;
    vage = 0;
    host_preset(&trk[0], ENGI_SEM, pi);
    wav_hdr(w, frames);
    for (f = 0; f < frames; f += CTL) {
        if (hold) {
            if (f == 0)
                for (k = 0; k < 2u; k++) trk_note_on(&trk[0], CHORD_A[k * 2], 100);
            if (f == 5u * FS - (5u * FS) % CTL)
                for (k = 0; k < 2u; k++) trk_note_off(&trk[0], CHORD_A[k * 2]);
        } else if (line) {
            if (f % step < CTL && f < 5u * FS) {
                if (curn) trk_note_off(&trk[0], curn);
                trk_note_on(&trk[0], curn = LINE[(f / step) % 8u], 100);
            } else if (curn && f % step >= step * 3 / 4 && f % step < step * 3 / 4 + CTL) {
                trk_note_off(&trk[0], curn); curn = 0;
            } else if (f >= 5u * FS && curn) {
                trk_note_off(&trk[0], curn); curn = 0;
            }
        } else {
            if (f % bar < CTL && f < 5u * FS) {
                const uint8_t *c = (f / bar) % 3u == 0u ? CHORD_A : (f / bar) % 3u == 1u ? CHORD_B : CHORD_C;
                if (cur) for (k = 0; k < 4u; k++) trk_note_off(&trk[0], cur[k]);
                for (k = 0; k < 4u; k++) trk_note_on(&trk[0], c[k], 96);
                cur = c;
            } else if (f >= 5u * FS - (5u * FS) % CTL && f < 5u * FS - (5u * FS) % CTL + CTL && cur) {
                for (k = 0; k < 4u; k++) trk_note_off(&trk[0], cur[k]);
                cur = 0;
            }
        }
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            wav_put(w, o[2 * i], o[2 * i + 1]);
            if (tour)
                wav_put(tour, o[2 * i], o[2 * i + 1]);
        }
    }
    fclose(w);
}

int main(int argc, char **argv)
{
    song.master_q12 = 4096;
    t_pitch();
    t_filter();
    t_fenv();
    t_levels();
    t_corners();
    t_determinism();
    t_cost();
    if (argc > 1) {
        char path[512];
        uint32_t pi;
        FILE *tour;
        snprintf(path, sizeof path, "%s/00_tour.wav", argv[1]);
        tour = fopen(path, "wb");
        if (tour)
            wav_hdr(tour, ENG_SEM.npresets * 6u * FS);
        for (pi = 0; pi < ENG_SEM.npresets; pi++)
            demo(argv[1], pi, tour);
        if (tour)
            fclose(tour);
        printf("sem: demos in %s\n", argv[1]);
    }
    printf("sem: %s (%d failed)\n", bad ? "FAIL" : "all ok", bad);
    return bad ? 1 : 0;
}
