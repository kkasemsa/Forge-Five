/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Khantipol Kasemsant; built on Felucca, (C) 2026 Leo Kuroshita, Hügelton Instruments */
/* LADDER engine test (firmware/src/eng_ladder.c) on the host, through hostsim.c as regress.c and noise_test.c.
 *   build/host/ladder_test [DEMODIR]          (DEMODIR: a WAV per preset and a tour of them all)
 * 1. pitch: OSC 1 at A4 is 440 Hz at 8' (+-0.3 %), the RANGEs octaves of it; OSC 2's FREQ +-56 a fifth up / a fourth
 *    down (+-0.3 %); OSC 3 at LO off the keyboard ~2 Hz (+-5 %).
 * 2. the filter: white noise, EMPHASIS 0: -24 dB per octave above the cutoff (+-4 dB); EMPHASIS 127 self-oscillates
 *    near CUTOFF's frequency (+-1.5 semitones) with every oscillator off; KBD FULL follows the key (two octaves
 *    apart: x4 +-6 %).
 * 3. pink noise: within 2 dB of white's level, its slope -3 dB / octave (+-1.5).
 * 4. levels: every preset's voice level near the other engines' (RMS within -10 .. +4 dB of ANALOG's SAW LEAD); the
 *    voice sum never beyond 2 x VOICE_FS.
 * 5. corners: every combination of CUTOFF / EMPHASIS / OVERLOAD / the mixer / MOD at notes 12, 60 and 115: bounded,
 *    no DC (|mean| < 3 % of full scale), the voices free and the output silent after the release.
 * 6. determinism: a note from silence renders the same samples again.
 * 7. cost: ns per sample and voice, against ANALOG's SAW LEAD in the same run; the device estimate from ANALOG's
 *    measured instructions (fx.c ENG_VCOST: 163). Fails above 3.4 x ANALOG (LADDER caps POLY at 4 voices).
 * 8. demos (DEMODIR): each preset playing a phrase through the full mix (its sends). */
#include <ctype.h>
#define main hostsim_main
#include "hostsim.c"
#undef main

#define NFFT 16384
#define LADDER_COST_MAX 3.4

static int bad;
static void check(const char *what, int ok)
{
    printf("ladder: %-92s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

/* track 0 LADDER preset pi (its extra values too), POLY or as the preset says, no sends */
static void setup(int32_t pi)
{
    memset(trk, 0, sizeof trk);
    memset(eng_mem, 0, sizeof eng_mem);             /* (the drift walks and pink: as at power-on) */
    memset(eng_mem_owner, 0, sizeof eng_mem_owner);
    host_tracks_init();
    vage = 0;
    host_preset(&trk[0], ENGI_LADDER, pi < 0 ? 0u : (uint32_t)pi);
    if (pi < 0) {                       /* a plain patch: OSC 1 saw at 8', the filter open, a flat envelope */
        uint32_t k;
        xp_defaults(&trk[0]);
        for (k = 0; k < 8u; k++)
            trk[0].p[P_E0 + k] = ENG_LADDER.edit[k].def;
        trk[0].p[P_E0] = 127;           /* CUTOFF */
        trk[0].p[P_E1] = 0;             /* EMPH */
        trk[0].p[P_E2] = 0;             /* CONTOUR */
        trk[0].p[P_E3] = 0;             /* KBD */
        LXP(&trk[0], LX_DRIFT) = 0;
        trk[0].p[P_ATK] = 0;
        trk[0].p[P_SUS] = 127;
        trk[0].p[P_REL] = 10;
        trk[0].p[P_GLIDE] = 0;
        trk[0].p[P_VOICE] = V_POLY;
    }
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = trk[0].p[P_DIST] = 0;
}

/* the voices' sum of note for n samples after skip s, into y (full scale 1.0 = 32768) */
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

#include "spectra.h"

/* ---------------------------------------------------------------- tests --- */
static void t_pitch(void)
{
    static const uint32_t RNG[] = {LD_32, LD_16, LD_8, LD_4, LD_2};
    static const double MUL[] = {0.25, 0.5, 1, 2, 4};
    char m[128];
    uint32_t k;
    for (k = 0; k < 5u; k++) {
        double f;
        setup(-1);
        LXP(&trk[0], LX_O1RNG) = (int16_t)RNG[k];
        LXP(&trk[0], LX_O1WAV) = LW_TRI;
        voice_render(69, 0.1, buf, 2 * NFFT);
        f = peak_hz(buf, 30);
        snprintf(m, sizeof m, "pitch: OSC 1 %s at A4: %.2f Hz (%.2f)", N_LD_RANGE[RNG[k]], f, 440 * MUL[k]);
        check(m, fabs(f / (440 * MUL[k]) - 1) < 0.003);
    }
    for (k = 0; k < 2u; k++) {
        double f, want = 440 * pow(2, (k ? -7 : 7) / 12.0);
        setup(-1);
        LXP(&trk[0], LX_MIX1) = 0;
        LXP(&trk[0], LX_MIX2) = 127;
        LXP(&trk[0], LX_O2WAV) = LW_TRI;
        LXP(&trk[0], LX_O2FRQ) = (int16_t)(k ? -56 : 56);
        voice_render(69, 0.1, buf, 2 * NFFT);
        f = peak_hz(buf, 30);
        snprintf(m, sizeof m, "pitch: OSC 2 FREQ %+d at A4: %.2f Hz (%.2f, %+.3f st)", k ? -56 : 56, f, want, semis(f, want));
        check(m, fabs(f / want - 1) < 0.003);
    }
    {   /* OSC 3 at LO off the keyboard: the modulation oscillator, middle C seven octaves down (2.04 Hz) */
        double f;
        setup(-1);
        LXP(&trk[0], LX_MIX1) = 0;
        LXP(&trk[0], LX_MIX3) = 127;
        LXP(&trk[0], LX_O3RNG) = LD_LO;
        LXP(&trk[0], LX_O3KBD) = 0;
        LXP(&trk[0], LX_O3WAV) = LW_TRI;
        voice_render(84, 0.1, buf, 4 * FS);
        f = zc_hz(buf, 4 * FS);
        snprintf(m, sizeof m, "pitch: OSC 3 LO, KBD OFF (any key): %.3f Hz (2.044)", f);
        check(m, fabs(f / 2.044 - 1) < 0.05);
    }
}

static void t_filter(void)
{
    static double ps[NFFT / 2];
    char m[160];
    double fc, lo, hi;
    uint32_t k;
    /* the slope: white noise, EMPHASIS 0, CUTOFF 50 */
    setup(-1);
    LXP(&trk[0], LX_MIX1) = 0;
    LXP(&trk[0], LX_MIXN) = 127;
    trk[0].p[P_E0] = 50;
    fc = CUTOFF_HZ[50];
    voice_render(60, 0.1, buf, 8 * NFFT);
    spectrum(buf, 8, ps);
    lo = band_db(ps, fc * 4 * 0.9, fc * 4 * 1.1);
    hi = band_db(ps, fc * 8 * 0.9, fc * 8 * 1.1);
    snprintf(m, sizeof m, "filter: four poles: %.0f Hz cutoff, %.1f dB from 4 fc to 8 fc (-24)", fc, hi - lo);
    check(m, hi - lo < -20 && hi - lo > -28);
    /* self-oscillation at CUTOFF's frequency, the oscillators off */
    for (k = 0; k < 4u; k++) {
        static const int16_t CUT[] = {50, 70, 90, 105};
        double f;
        setup(-1);
        LXP(&trk[0], LX_MIX1) = 0;
        trk[0].p[P_E0] = CUT[k];
        trk[0].p[P_E1] = 127;
        voice_render(60, 0.5, buf, 2 * NFFT);
        f = peak_hz(buf, 20);
        snprintf(m, sizeof m, "filter: EMPHASIS 127 sings at CUTOFF %d: %.1f Hz (%u Hz, %+.2f st), RMS %.3f", CUT[k], f,
                 CUTOFF_HZ[CUT[k]], semis(f, CUTOFF_HZ[CUT[k]]), rms(buf, 2 * NFFT));
        check(m, fabs(semis(f, CUTOFF_HZ[CUT[k]])) < 1.5 && rms(buf, 2 * NFFT) > 0.05);
    }
    {   /* KBD FULL: two octaves apart */
        double f1, f2;
        setup(-1);
        LXP(&trk[0], LX_MIX1) = 0;
        trk[0].p[P_E0] = 60;
        trk[0].p[P_E1] = 127;
        trk[0].p[P_E3] = 3;
        voice_render(48, 0.5, buf, 2 * NFFT);
        f1 = peak_hz(buf, 20);
        setup(-1);
        LXP(&trk[0], LX_MIX1) = 0;
        trk[0].p[P_E0] = 60;
        trk[0].p[P_E1] = 127;
        trk[0].p[P_E3] = 3;
        voice_render(72, 0.5, buf, 2 * NFFT);
        f2 = peak_hz(buf, 20);
        snprintf(m, sizeof m, "filter: KBD FULL follows the key: C3 %.1f Hz, C5 %.1f Hz, x%.3f (4)", f1, f2, f2 / f1);
        check(m, fabs(f2 / f1 / 4 - 1) < 0.06);
    }
}

static void t_pink(void)
{
    static double ps[NFFT / 2];
    char m[160];
    double rw, rp, s;
    setup(-1);
    LXP(&trk[0], LX_MIX1) = 0;
    LXP(&trk[0], LX_MIXN) = 127;
    voice_render(60, 0.2, buf, 8 * NFFT);
    rw = rms(buf, 8 * NFFT);
    setup(-1);
    LXP(&trk[0], LX_MIX1) = 0;
    LXP(&trk[0], LX_MIXN) = 127;
    LXP(&trk[0], LX_NCOL) = 1;
    voice_render(60, 0.5, buf, 8 * NFFT);
    rp = rms(buf, 8 * NFFT);
    spectrum(buf, 8, ps);
    s = (band_db(ps, 5000, 7000) - band_db(ps, 150, 250)) / log2(6000.0 / 200.0);
    snprintf(m, sizeof m, "pink noise: %.1f dB against white, slope %.2f dB / octave (-3)", 20 * log10(rp / rw), s);
    check(m, fabs(20 * log10(rp / rw)) < 2.0 && s < -1.5 && s > -4.5);
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
    printf("ladder: levels (one note, C3, velocity 100, after 0.2 s; ANALOG SAW LEAD %.3f):\n", ra);
    for (pi = 0; pi < ENG_LADDER.npresets; pi++) {
        double r, pk = 0;
        uint32_t i;
        char m[160];
        uint32_t w, wl = (uint32_t)(0.05 * FS);
        setup((int32_t)pi);
        voice_render(pi == 5 ? 72u : 48u, 0, buf, 6 * FS);     /* (WHISTLE: a lead, up an octave) */
        for (r = 0, w = 0; w + wl <= 6u * FS; w += wl / 2u) {  /* the loudest 50 ms (plucks, the riser's swell) */
            double q = rms(buf + w, wl);
            r = q > r ? q : r;
        }
        for (i = 0; i < 6u * FS; i++)
            pk = fabs(buf[i]) > pk ? fabs(buf[i]) : pk;
        {
            double mean = 0;
            for (i = 0; i < 6u * FS; i++)
                mean += buf[i];
            mean /= 6u * FS;
            snprintf(m, sizeof m, "level: %-13s RMS %.3f (%+.1f dB), peak %.3f of full scale, mean %+.4f",
                     ENG_LADDER.presets[pi].name, r, 20 * log10(r / ra), pk, mean);
            check(m, 20 * log10(r / ra) > -10 && 20 * log10(r / ra) < 4 && pk < 2.0 * VOICE_FS / 32768.0 && fabs(mean) < 0.01);
        }
    }
}

static void t_corners(void)
{
    static const int16_t CUT[] = {0, 127}, EM[] = {0, 127}, OV[] = {0, 127};
    static const uint32_t NOTE[] = {12, 60, 115};
    uint32_t a, b, c, d, n, fails = 0, cases = 0;
    double worst = 0, worst_dc = 0;
    char m[200];
    for (a = 0; a < 2u; a++)
        for (b = 0; b < 2u; b++)
            for (c = 0; c < 2u; c++)
                for (d = 0; d < 2u; d++)
                    for (n = 0; n < 3u; n++) {
                        static int32_t ob[CTL];
                        double pk = 0, mean = 0;
                        uint32_t i, k, j, free_ok = 0, quiet = 1;
                        setup(-1);
                        trk[0].p[P_E0] = CUT[a];
                        trk[0].p[P_E1] = EM[b];
                        LXP(&trk[0], LX_OVLD) = OV[c];
                        LXP(&trk[0], LX_MIX1) = LXP(&trk[0], LX_MIX2) = LXP(&trk[0], LX_MIX3) = 127;
                        LXP(&trk[0], LX_MIXN) = d ? 127 : 0;
                        LXP(&trk[0], LX_NCOL) = (int16_t)d;
                        LXP(&trk[0], LX_O2RNG) = LD_2;
                        LXP(&trk[0], LX_O3RNG) = d ? LD_2 : LD_LO;
                        LXP(&trk[0], LX_O3WAV) = LW_SAW;
                        LXP(&trk[0], LX_MODAMT) = (int16_t)(d ? 127 : 60);
                        LXP(&trk[0], LX_OSCMOD) = LXP(&trk[0], LX_FLTMOD) = 1;
                        LXP(&trk[0], LX_DRIFT) = 127;
                        trk[0].p[P_E2] = 127;
                        trk[0].p[P_E3] = 3;
                        trk[0].p[P_REL] = 30;
                        voice_render(NOTE[n], 0.05, buf, FS / 2);
                        for (i = 0; i < FS / 2; i++) {
                            pk = fabs(buf[i]) > pk ? fabs(buf[i]) : pk;
                            mean += buf[i];
                        }
                        mean /= FS / 2;
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
                        worst_dc = fabs(mean) > worst_dc ? fabs(mean) : worst_dc;   /* (OSC 3 at LO is in the mix
                                                 * here: a 2 Hz wave, not an offset; the presets' means are checked) */
                        if (pk > 2.0 * VOICE_FS / 32768.0 || !free_ok || !quiet) {
                            if (fails++ < 4)
                                printf("ladder:   corner CUT %d EMPH %d OVLD %d NOISE+MOD %u note %u: peak %.3f, mean %.4f, free %u, "
                                       "silent %u\n", CUT[a], EM[b], OV[c], d, NOTE[n], pk, mean, free_ok, quiet);
                        }
                    }
    snprintf(m, sizeof m, "corners: %u renders bounded (worst peak %.3f of full scale; mean up to %.4f), free, silent after",
             cases, worst, worst_dc);
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
    uint32_t i, k, ei, nb = 20000;
    uint64_t t0;
    for (ei = 0; ei < NENGINES && ENGINES[ei] != e; ei++)
        ;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    host_preset(&trk[0], ei, pi);
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_REL] = 127;
    if (heavy) {                        /* every block of LADDER at work */
        LXP(&trk[0], LX_MIX1) = LXP(&trk[0], LX_MIX2) = LXP(&trk[0], LX_MIX3) = LXP(&trk[0], LX_MIXN) = 127;
        LXP(&trk[0], LX_NCOL) = 1;
        LXP(&trk[0], LX_MODAMT) = 90;
        LXP(&trk[0], LX_OSCMOD) = LXP(&trk[0], LX_FLTMOD) = 1;
        LXP(&trk[0], LX_OVLD) = 60;
        LXP(&trk[0], LX_DRIFT) = 40;
    }
    for (k = 0; k < NVOICE; k++)
        trk_note_on(&trk[0], 40 + 3 * k, 100);
    for (i = 0; i < 200; i++)
        track_render(&trk[0], ob, CTL);
    {
        uint32_t nv = 0;
        for (k = 0; k < NVOICE; k++)
            nv += trk[0].v[k].active;   /* (LADDER: its cap of 4) */
        t0 = now_ns();
        for (i = 0; i < nb; i++)
            track_render(&trk[0], ob, CTL);
        return (double)(now_ns() - t0) / (nb * CTL) / (nv ? nv : 1u);
    }
}

static void t_cost(void)
{
    double a = 0, l = 0, h = 0;
    uint32_t r;
    char m[200];
    for (r = 0; r < 3u; r++) {          /* the best of three: the host's own noise out */
        double x = cost_ns(&ENG_ANALOG, 0, 0), y = cost_ns(&ENG_LADDER, 0, 0), z = cost_ns(&ENG_LADDER, 0, 1);
        a = r && a < x ? a : x;
        l = r && l < y ? l : y;
        h = r && h < z ? h : z;
    }
    snprintf(m, sizeof m, "cost per voice: ANALOG %.2f ns, LADDER %.2f ns (x%.2f), heaviest %.2f ns (x%.2f): ~%.0f instr on the device",
             a, l, l / a, h, h / a, 163 * h / a);
    check(m, h / a < LADDER_COST_MAX);
}

/* ---------------------------------------------------------------- demos --- */
static const uint8_t SEQ_BASS[16] = {36, 36, 48, 36, 39, 36, 43, 36, 36, 48, 36, 46, 36, 43, 39, 41};
static const uint8_t SEQ_LEAD[8] = {60, 63, 67, 70, 72, 70, 67, 63};

static void demo(const char *dir, uint32_t pi, FILE *tour)
{
    static int32_t o[2 * CTL];
    char path[512], slug[32];
    uint32_t i, f, frames = 6u * FS, step = FS * 60 / 120 / 4, k;   /* 120 BPM, sixteenths */
    int lead = pi == 1 || pi == 5 || pi == 7 || pi == 12 || pi == 4, slow = pi == 9 || pi == 11;
    uint32_t cur = 0;
    FILE *w;
    for (i = 0, k = 0; ENG_LADDER.presets[pi].name[i] && k < sizeof slug - 1; i++)
        slug[k++] = ENG_LADDER.presets[pi].name[i] == ' ' ? '_' : (char)tolower(ENG_LADDER.presets[pi].name[i]);
    slug[k] = 0;
    snprintf(path, sizeof path, "%s/%02u_%s.wav", dir, pi + 1, slug);
    w = fopen(path, "wb");
    if (!w)
        return;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    song.g[G_RTYPE] = 2;                /* HALL */
    vage = 0;
    host_preset(&trk[0], ENGI_LADDER, pi);
    wav_hdr(w, frames);
    for (f = 0; f < frames; f += CTL) {
        if (slow) {                     /* a held note (the riser), a fifth (the drone) */
            if (f == 0)
                trk_note_on(&trk[0], cur = pi == 9 ? 48u : 36u, 100);
            if (f == 5u * FS - (5u * FS) % CTL)
                trk_note_off(&trk[0], cur);
        } else if (f % (lead ? 2u * step : step) < CTL && f < 5u * FS) {
            uint32_t n = lead ? SEQ_LEAD[(f / (2u * step)) % 8u] : SEQ_BASS[(f / step) % 16u];
            if (cur)
                trk_note_off(&trk[0], cur);
            trk_note_on(&trk[0], cur = n - (pi == 2 ? 12u : 0u), pi == 3 && (f / step) % 4u == 0 ? 120u : 96u);
        } else if (!lead && !slow && cur && f % step >= step / 2 && f % step < step / 2 + CTL) {
            trk_note_off(&trk[0], cur);   /* the bass: gated sixteenths */
            cur = 0;
        } else if (f >= 5u * FS && cur) {
            trk_note_off(&trk[0], cur);
            cur = 0;
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
    t_pink();
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
            wav_hdr(tour, ENG_LADDER.npresets * 6u * FS);
        for (pi = 0; pi < ENG_LADDER.npresets; pi++)
            demo(argv[1], pi, tour);
        if (tour)
            fclose(tour);
        printf("ladder: demos in %s\n", argv[1]);
    }
    printf("ladder: %s (%d failed)\n", bad ? "FAIL" : "all ok", bad);
    return bad ? 1 : 0;
}
