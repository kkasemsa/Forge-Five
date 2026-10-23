/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Khantipol Kasemsant; built on Clipper (isod89 / 3dSam), SLOOP and Felucca */
/* BRASS engine test (firmware/src/eng_brass.c) on the host, through hostsim.c.
 *   build/host/brass_test [DEMODIR]          (DEMODIR: a WAV per preset and a tour of them all)
 * 1. golden (first, from the power-on state): every preset's first half second of a note hashes as
 *    tests/brass_golden.txt says (GOLDEN_UPDATE=1 rewrites it).
 * 2. pitch: OSC A at A4 440 Hz at 8' (+-0.3 %), 16' and 4' its octaves; B FREQ +-56 a fifth up / down; DETUNE 100 a
 *    semitone up (+-0.3 %).
 * 3. SYNC: A at 4' synced to B at 8': the output repeats at B's 440 Hz (its band 20 dB up on the unsynced A's).
 * 4. the filter, white noise: 2P -12 dB / octave above the cutoff (+-4 dB); 4P twice 2P's slope (2 fc .. 4 fc); MODE 127 a high-pass (+12 dB /
 *    octave below); MODE 64 a notch at the cutoff; BP a peak there.
 * 5. poly-mod: OSC B into A's frequency (sidebands: A's fundamental 6 dB less of the whole), into A's width and into
 *    the cutoff (the sound changes: 20 % of its RMS); PM ENV into a synced A: the sweep (the spectrum's centre moves).
 * 6. AGE: 0 holds A at 440 Hz (+-0.2 ct); 127 drifts it within +-12 cents and moves; the voices' offsets differ and
 *    belong to the voice (a render repeats).
 * 7. ATTACK / RELEASE (EDIT 2) and the ENV page's A / R are one value each.
 * 8. TYPE switched under a held chord at RESO 127: bounded.
 * 9. the engine patch: 24 values on 6 pages, a preset's round-trip.
 * 10. levels: every preset's note within 4 dB of the DCO pad's in the mix; corners: every preset at five notes with
 *    RESO, CUTOFF, PMOD, PM ENV, every destination, SYNC and AGE up: within 2 x VOICE_FS a voice (its clamp),
 *    silent once released.
 * 11. cost: ns per sample and voice against ANALOG's in the same run (the budget's weight 1.5 waits for the unit).
 * 12. demos (DEMODIR). */
#include <ctype.h>
#define main hostsim_main
#include "hostsim.c"
#undef main

#define NFFT 16384
#define BRASS_COST_MAX 4.0              /* x Clipper's ANALOG on the host, the heaviest setting: a sanity bound */

static int bad;
static void check(const char *what, int ok)
{
    printf("brass: %-92s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

/* track 0 BRASS preset pi; pi < 0: a plain patch (A's saw at 8' alone, the filter open, a flat envelope, AGE 0) */
static void setup(int32_t pi)
{
    uint32_t i;
    memset(trk, 0, sizeof trk);
    memset(eng_arena, 0, sizeof eng_arena);
    memset(eng_arena_own, 0, sizeof eng_arena_own);
    memset(br_last, 0, sizeof br_last);
    for (i = 0; i < NPART; i++)
        cm_reset(i);
    host_tracks_init();
    host_preset(&trk[0], ENGI_BRASS, pi < 0 ? 0u : (uint32_t)pi);
    if (pi < 0) {
        uint32_t k;
        xp_defaults(&trk[0]);
        for (k = 0; k < 8u; k++)
            trk[0].p[P_E0 + k] = ENG_BRASS.edit[k].def;
        trk[0].p[P_E0] = 127;           /* CUTOFF */
        trk[0].p[P_E1] = 0;             /* RESO */
        trk[0].p[P_E2] = 0;             /* ENV */
        trk[0].p[P_E3] = 0;             /* DETUNE */
        trk[0].p[P_E4] = 0;             /* PMOD */
        trk[0].p[P_E6] = trk[0].p[P_ATK] = 0;
        trk[0].p[P_E7] = trk[0].p[P_REL] = 10;
        trk[0].xp[BX_BLVL] = 0;
        trk[0].xp[BX_AGE] = 0;
        trk[0].xp[BX_PMFRQ] = 0;
        trk[0].xp[BX_KBD] = 0;
        trk[0].p[P_SUS] = 127;
        trk[0].p[P_GLIDE] = 0;
        trk[0].p[P_VINT] = 0;
        trk[0].p[P_VOICE] = V_POLY;
    }
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = trk[0].p[P_DIST] = 0;
}

static void voice_render(uint32_t note, double skip_s, double *y, uint32_t n)
{
    static int32_t b[CTL];
    uint32_t i, k, skip = (uint32_t)(skip_s * FS) / CTL;
    trk_note_on(&trk[0], note, 100);
    for (i = 0; i < skip; i++) {
        memset(b, 0, sizeof b);
        track_render(&trk[0], b, CTL);
    }
    for (i = 0; i < n; i += CTL) {
        memset(b, 0, sizeof b);
        track_render(&trk[0], b, CTL);
        for (k = 0; k < CTL && i + k < n; k++)
            y[i + k] = b[k] / 32768.0;
    }
}

#include "spectra.h"

/* ---------------------------------------------------------------- golden --- */
static void t_golden(void)
{
    static int32_t b[CTL];
    static uint64_t h[32];
    char line[160];
    const char *path = "tests/brass_golden.txt";
    uint32_t pi, i, k, np = ENG_BRASS.npresets, changed = 0, missing = 0;
    FILE *f;
    for (pi = 0; pi < np && pi < 32u; pi++) {
        uint64_t x = 1469598103934665603ull;
        setup((int32_t)pi);
        trk_note_on(&trk[0], 48 + (pi % 24u), 100);
        for (i = 0; i < FS / 2 / CTL; i++) {
            memset(b, 0, sizeof b);
            track_render(&trk[0], b, CTL);
            for (k = 0; k < CTL; k++)
                x = (x ^ (uint32_t)b[k]) * 1099511628211ull;
        }
        h[pi] = x;
    }
    if (getenv("GOLDEN_UPDATE")) {
        f = fopen(path, "w");
        if (f) {
            fprintf(f, "# BRASS presets: FNV-1a of half a second of a note (tests/brass_test.c t_golden)\n");
            for (pi = 0; pi < np && pi < 32u; pi++)
                fprintf(f, "%s|%016llx\n", ENG_BRASS.presets[pi].name, (unsigned long long)h[pi]);
            fclose(f);
        }
        check("golden: tests/brass_golden.txt rewritten", f != 0);
        return;
    }
    f = fopen(path, "r");
    if (!f) {
        check("golden: tests/brass_golden.txt present (GOLDEN_UPDATE=1 writes it)", 0);
        return;
    }
    for (pi = 0; pi < np && pi < 32u; pi++) {
        unsigned long long want = 0;
        int found = 0;
        size_t ln = strlen(ENG_BRASS.presets[pi].name);
        rewind(f);
        while (fgets(line, sizeof line, f))
            if (!strncmp(line, ENG_BRASS.presets[pi].name, ln) && line[ln] == '|' &&
                sscanf(line + ln + 1, "%llx", &want) == 1) {
                found = 1;
                break;
            }
        if (!found) {
            printf("brass: golden: %s has no entry\n", ENG_BRASS.presets[pi].name);
            missing++;
        } else if (want != h[pi]) {
            printf("brass: golden: %s changed (%016llx, was %016llx)\n", ENG_BRASS.presets[pi].name,
                   (unsigned long long)h[pi], want);
            changed++;
        }
    }
    fclose(f);
    snprintf(line, sizeof line, "golden: %u presets render as tests/brass_golden.txt says (%u changed, %u missing)", np,
             changed, missing);
    check(line, !changed && !missing);
}

/* ----------------------------------------------------------------- pitch --- */
static void t_pitch(void)
{
    static const double MUL[3] = {0.5, 1, 2};
    char m[160];
    uint32_t k;
    for (k = 0; k < 3u; k++) {
        double f;
        setup(-1);
        trk[0].xp[BX_AWAV] = BW_TRI;
        trk[0].xp[BX_ARNG] = (int16_t)k;
        voice_render(69, 0.1, buf, 2 * NFFT);
        f = peak_hz(buf, 30);
        snprintf(m, sizeof m, "pitch: OSC A %s at A4: %.2f Hz (%.2f)", N_BR_RANGE[k], f, 440 * MUL[k]);
        check(m, fabs(f / (440 * MUL[k]) - 1) < 0.003);
    }
    for (k = 0; k < 3u; k++) {
        static const int16_t FRQ[3] = {56, -56, 0};
        double f, want = 440 * pow(2, (k == 0 ? 7 : k == 1 ? -7 : 1) / 12.0);
        setup(-1);
        trk[0].xp[BX_ALVL] = 0;
        trk[0].xp[BX_BLVL] = 127;
        trk[0].xp[BX_BWAV] = BW_TRI;
        trk[0].xp[BX_BFRQ] = FRQ[k];
        trk[0].p[P_E3] = k == 2 ? 100 : 0;
        voice_render(69, 0.1, buf, 2 * NFFT);
        f = peak_hz(buf, 30);
        snprintf(m, sizeof m, "pitch: OSC B %s: %.2f Hz (%.2f, %+.3f st)",
                 k == 0 ? "FREQ +56" : k == 1 ? "FREQ -56" : "DETUNE 100", f, want, semis(f, want));
        check(m, fabs(f / want - 1) < 0.003);
    }
}

/* ------------------------------------------------------------------ sync --- */
static void t_sync(void)
{
    static double ps[NFFT / 2];
    double on, off;
    char m[160];
    uint32_t s;
    for (s = 0; s < 2u; s++) {
        setup(-1);
        trk[0].xp[BX_ARNG] = 2;                         /* A at 4': 1760 Hz alone */
        trk[0].xp[BX_SYNC] = (int16_t)s;
        voice_render(69, 0.1, buf, 4 * NFFT);
        spectrum(buf, 4, ps);
        if (s)
            on = band_db(ps, 430, 450);
        else
            off = band_db(ps, 430, 450);
    }
    snprintf(m, sizeof m, "SYNC: A at 4' synced to B at 8': 440 Hz %.1f dB up on the unsynced A's", on - off);
    check(m, on - off > 20);
}

/* ---------------------------------------------------------------- filter --- */
static void noise_through(int32_t type, int32_t mode, int32_t cut, int32_t reso, double *ps)
{
    setup(-1);
    trk[0].xp[BX_ALVL] = 0;
    trk[0].xp[BX_NOISE] = 127;
    trk[0].xp[BX_MODE] = (int16_t)mode;
    trk[0].p[P_E5] = (int16_t)type;
    trk[0].p[P_E0] = (int16_t)cut;
    trk[0].p[P_E1] = (int16_t)reso;
    voice_render(60, 0.1, buf, 8 * NFFT);
    spectrum(buf, 8, ps);
}
static void t_filter(void)
{
    static double ps[NFFT / 2];
    double fc = CUTOFF_HZ[50], a, b, c;
    char m[200];
    noise_through(BT_2P, 0, 50, 0, ps);
    a = band_db(ps, fc * 4 * 0.9, fc * 4 * 1.1), b = band_db(ps, fc * 8 * 0.9, fc * 8 * 1.1);
    snprintf(m, sizeof m, "filter: 2P low-pass: %.0f Hz cutoff, %.1f dB from 4 fc to 8 fc (-12)", fc, b - a);
    check(m, b - a < -8 && b - a > -16);
    {   /* 4P against 2P over the same octave, 2 fc .. 4 fc (higher up the noise is a few LSBs after four poles:
         * the integer filters' floor, not their slope) */
        double d2, d4;
        noise_through(BT_2P, 0, 50, 0, ps);
        d2 = band_db(ps, fc * 4 * 0.9, fc * 4 * 1.1) - band_db(ps, fc * 2 * 0.9, fc * 2 * 1.1);
        noise_through(BT_4P, 0, 50, 0, ps);
        d4 = band_db(ps, fc * 4 * 0.9, fc * 4 * 1.1) - band_db(ps, fc * 2 * 0.9, fc * 2 * 1.1);
        snprintf(m, sizeof m, "filter: 4P low-pass: %.1f dB from 2 fc to 4 fc, 2P %.1f: four poles, twice the slope", d4, d2);
        check(m, d4 < -18 && d4 < 1.6 * d2);
    }
    noise_through(BT_2P, 127, 70, 0, ps);
    fc = CUTOFF_HZ[70];
    a = band_db(ps, fc / 8 * 0.9, fc / 8 * 1.1), b = band_db(ps, fc / 4 * 0.9, fc / 4 * 1.1);
    snprintf(m, sizeof m, "filter: MODE 127 high-pass: %.1f dB from fc / 4 down to fc / 8 (-12)", a - b);
    check(m, a - b < -8 && a - b > -16);
    noise_through(BT_2P, 64, 70, 0, ps);
    a = band_db(ps, fc * 0.97, fc * 1.03), b = band_db(ps, fc / 4 * 0.9, fc / 4 * 1.1), c = band_db(ps, fc * 4 * 0.9, fc * 4 * 1.1);
    snprintf(m, sizeof m, "filter: MODE 64 a notch at the cutoff: %.1f dB under fc / 4, %.1f under 4 fc", b - a, c - a);
    check(m, b - a > 10 && c - a > 10);
    noise_through(BT_BP, 0, 70, 60, ps);
    a = band_db(ps, fc * 0.95, fc * 1.05), b = band_db(ps, fc / 8 * 0.9, fc / 8 * 1.1), c = band_db(ps, fc * 8 * 0.9, fc * 8 * 1.1);
    snprintf(m, sizeof m, "filter: BP peaks at the cutoff: %.1f dB over fc / 8, %.1f over 8 fc", a - b, a - c);
    check(m, a - b > 12 && a - c > 12);
}

/* -------------------------------------------------------------- poly-mod --- */
static double centroid(const double *ps, double f0, double f1)
{
    uint32_t a = (uint32_t)(f0 * NFFT / FS), b = (uint32_t)(f1 * NFFT / FS), i;
    double s = 0, w = 0;
    for (i = a; i <= b; i++)
        s += ps[i] * i, w += ps[i];
    return w > 0 ? s / w * FS / NFFT : 0;
}
static void t_pmod(void)
{
    static double ps[NFFT / 2], ref[2 * NFFT];
    char m[200];
    double r0, r1, a, d, s, e0, e1;
    uint32_t i;
    /* into A's frequency: B a fifth up, silent, at PMOD 0 and 100: A's fundamental's share of the whole */
    for (i = 0; i < 2u; i++) {
        double tot = 0;
        uint32_t k;
        setup(-1);
        trk[0].xp[BX_AWAV] = trk[0].xp[BX_BWAV] = BW_TRI;
        trk[0].xp[BX_BFRQ] = 56;
        trk[0].xp[BX_PMFRQ] = 1;
        trk[0].p[P_E4] = i ? 100 : 0;
        voice_render(57, 0.1, buf, 4 * NFFT);
        spectrum(buf, 4, ps);
        for (k = 1; k < NFFT / 2; k++)
            tot += ps[k];
        a = band_db(ps, 215, 225) + 10 * log10((225.0 - 215) * NFFT / FS + 1) - 10 * log10(tot);
        if (i)
            r1 = a;
        else
            r0 = a;
    }
    snprintf(m, sizeof m, "poly-mod -> FREQ A: A's fundamental %.1f dB of the whole at PMOD 0, %.1f at 100 (sidebands)", r0, r1);
    check(m, r0 - r1 > 6);
    /* into A's width */
    for (i = 0; i < 2u; i++) {
        setup(-1);
        trk[0].xp[BX_AWAV] = BW_PLS;
        trk[0].xp[BX_BFRQ] = 20;
        trk[0].xp[BX_PMPW] = 1;
        trk[0].p[P_E4] = i ? 100 : 0;
        voice_render(57, 0.1, i ? buf : ref, 2 * NFFT);
    }
    for (i = 0, d = 0, s = 0; i < 2 * NFFT; i++)
        d += (buf[i] - ref[i]) * (buf[i] - ref[i]), s += ref[i] * ref[i];
    snprintf(m, sizeof m, "poly-mod -> PW A: the pulse changes (difference %.0f %% of its RMS)", 100 * sqrt(d / s));
    check(m, sqrt(d / s) > 0.2);
    /* into the cutoff: the sound changes (OSC B moving the cutoff at audio rate) */
    for (i = 0; i < 2u; i++) {
        setup(-1);
        trk[0].p[P_E0] = 40;
        trk[0].xp[BX_BWAV] = BW_SAW;
        trk[0].xp[BX_BFRQ] = 3;
        trk[0].xp[BX_PMFLT] = (int16_t)i;
        trk[0].p[P_E4] = 110;
        voice_render(45, 0.1, i ? buf : ref, 2 * NFFT);
    }
    for (i = 0, d = 0, s = 0; i < 2 * NFFT; i++)
        d += (buf[i] - ref[i]) * (buf[i] - ref[i]), s += ref[i] * ref[i];
    snprintf(m, sizeof m, "poly-mod -> FILT: OSC B on the cutoff: the sound changes (difference %.0f %% of its RMS)",
             100 * sqrt(d / s));
    check(m, sqrt(d / s) > 0.2);
    /* PM ENV into a synced A: the sweep */
    setup(-1);
    trk[0].xp[BX_SYNC] = 1;
    trk[0].xp[BX_PMFRQ] = 1;
    trk[0].xp[BX_PMENV] = 63;
    trk[0].xp[BX_FATK] = 0, trk[0].xp[BX_FDEC] = 90, trk[0].xp[BX_FSUS] = 0;
    voice_render(57, 0.0, buf, 6 * NFFT);
    spectrum(buf, 1, ps);
    e0 = centroid(ps, 100, 15000);
    spectrum(buf + 5 * NFFT, 1, ps);
    e1 = centroid(ps, 100, 15000);
    snprintf(m, sizeof m, "poly-mod PM ENV -> a synced A: the sweep: spectrum's centre %.0f Hz early, %.0f Hz later", e0, e1);
    check(m, e0 > 1.3 * e1);
}

/* ------------------------------------------------------------------- AGE --- */
static void t_age(void)
{
    char m[200];
    double f[12], lo = 1e9, hi = 0;
    uint32_t w, k, j, same = 1, differ = 0;
    static double y1[NFFT];
    for (k = 0; k < 2u; k++) {
        setup(-1);
        trk[0].xp[BX_AWAV] = BW_TRI;
        trk[0].xp[BX_AGE] = (int16_t)(k ? 127 : 0);
        trk_note_on(&trk[0], 69, 100);
        lo = 1e9, hi = 0;
        for (w = 0; w < 12u; w++) {                     /* 12 windows of 0.37 s: 4.5 s */
            static int32_t b[CTL];
            uint32_t i, q;
            for (i = 0; i < NFFT; i += CTL) {
                memset(b, 0, sizeof b);
                track_render(&trk[0], b, CTL);
                for (q = 0; q < CTL; q++)
                    buf[i + q] = b[q] / 32768.0;
            }
            memcpy(buf + NFFT, buf, NFFT * sizeof(double));
            f[w] = peak_hz(buf, 30);
            lo = f[w] < lo ? f[w] : lo, hi = f[w] > hi ? f[w] : hi;
        }
        if (!k) {
            snprintf(m, sizeof m, "AGE 0: A holds 440 Hz (%.3f .. %.3f, within 0.2 ct)", lo, hi);
            check(m, fabs(semis(lo, 440)) < 0.002 && fabs(semis(hi, 440)) < 0.002);
        } else {
            snprintf(m, sizeof m, "AGE 127: A drifts within 12 ct (%+.1f .. %+.1f ct) and moves (%.1f ct)",
                     100 * semis(lo, 440), 100 * semis(hi, 440), 100 * semis(hi, lo));
            check(m, fabs(semis(lo, 440)) < 0.12 && fabs(semis(hi, 440)) < 0.12 && semis(hi, lo) > 0.005);
        }
    }
    for (k = 0; k < NVOICE; k++)                        /* the voices' offsets: their own, and the same each time */
        for (j = k + 1u; j < NVOICE; j++)
            differ += brass_card(&trk[0], &trk[0].v[k], 1) != brass_card(&trk[0], &trk[0].v[j], 1);
    for (k = 0; k < 2u; k++) {
        setup(4);                                       /* STACK BRASS: AGE 50 */
        trk[0].xp[BX_AGE] = 127;
        voice_render(57, 0.2, k ? buf : y1, NFFT);
    }
    for (k = 0; k < NFFT; k++)
        same &= buf[k] == y1[k];
    snprintf(m, sizeof m, "AGE: the %u voices' offsets differ (%u of %u pairs); a voice renders the same again", NVOICE,
             differ, NVOICE * (NVOICE - 1) / 2);
    check(m, differ >= NVOICE * (NVOICE - 1) / 2 - 2 && same);
}

/* ------------------------------------------------------- ATTACK, RELEASE --- */
static void t_mirror(void)
{
    static int32_t o[CTL * 2];
    setup(0);
    mix_block(o, CTL);
    check("ATTACK / RELEASE: a preset sets EDIT 2's and the ENV page's alike",
          trk[0].p[P_E6] == trk[0].p[P_ATK] && trk[0].p[P_E7] == trk[0].p[P_REL]);
    trk[0].p[P_E6] = 70, trk[0].p[P_E7] = 90;
    mix_block(o, CTL);
    check("ATTACK / RELEASE: EDIT 2 turned: A and R follow", trk[0].p[P_ATK] == 70 && trk[0].p[P_REL] == 90);
    trk[0].p[P_ATK] = 5, trk[0].p[P_REL] = 20;
    mix_block(o, CTL);
    check("ATTACK / RELEASE: A and R turned: EDIT 2 follows", trk[0].p[P_E6] == 5 && trk[0].p[P_E7] == 20);
}

/* ------------------------------------------------------- TYPE under notes --- */
static void t_type_switch(void)
{
    static int32_t b[CTL];
    double pk = 0;
    uint32_t i, k, bad_ = 0;
    char m[160];
    setup(0);
    trk[0].p[P_E1] = 127;
    for (k = 0; k < 3u; k++)
        trk_note_on(&trk[0], 48 + 4 * k, 110);
    for (i = 0; i < 3000u; i++) {
        if (!(i % 150u))
            trk[0].p[P_E5] = (int16_t)((i / 150u) % 3u);
        memset(b, 0, sizeof b);
        track_render(&trk[0], b, CTL);
        for (k = 0; k < CTL; k++) {
            double x = fabs(b[k] / 32768.0) / 3;
            pk = x > pk ? x : pk;
            bad_ += x != x;
        }
    }
    snprintf(m, sizeof m, "TYPE 2P / 4P / BP switched under a held chord at RESO 127: bounded (peak %.2f a voice)", pk);
    check(m, !bad_ && pk < 1.25);
}

/* --------------------------------------------------------- engine patch --- */
static void t_patch(void)
{
    static uint8_t b[XP_BYTES];
    int16_t keep[NXP];
    uint32_t k, same = 1;
    setup(3);                                           /* POLYMOD BELL */
    memcpy(keep, trk[0].xp, sizeof keep);
    check("engine patch: BRASS has 24 values on 6 pages", BRASS_XPI.n == 24u && xp_title(&trk[0], 5) && !xp_title(&trk[0], 6));
    xp_pack(&trk[0], b);
    xp_defaults(&trk[0]);
    xp_unpack(&trk[0], b);
    for (k = 0; k < NXP; k++)
        same &= trk[0].xp[k] == keep[k];
    check("engine patch: a preset's values round-trip (B FREQ +56 and PM ENV signed among them)", same);
}

/* ------------------------------------------------------- levels, corners --- */
static double part_loudest(void)
{
    static int32_t b[CTL];
    double best = 0;
    uint32_t w, i, k;
    for (w = 0; w < 30u; w++) {                         /* the loudest 50 ms of 1.5 s */
        double s = 0;
        for (i = 0; i < FS / 20; i += CTL) {
            memset(b, 0, sizeof b);
            track_render(&trk[0], b, CTL);
            for (k = 0; k < CTL; k++)
                s += (b[k] / 32768.0) * (b[k] / 32768.0);
        }
        s = sqrt(s / (FS / 20));
        best = s > best ? s : best;
    }
    return best;
}
static void t_levels(void)
{
    double ref, r;
    uint32_t pi, fails = 0;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    host_preset(&trk[0], ENGI_JUNO, 0);                 /* the DCO engine's pad, one note */
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    trk_note_on(&trk[0], 60, 100);
    ref = part_loudest();
    for (pi = 0; pi < ENG_BRASS.npresets; pi++) {
        double db;
        uint32_t key;
        setup((int32_t)pi);
        key = trk[0].p[P_VOICE] == V_POLY ? 60u : 48u;
        trk_note_on(&trk[0], key, 100);
        r = part_loudest();
        db = 20 * log10(r / ref) + trk[0].p[P_ED_FX] * 0.5;
        printf("brass:   %-14s %+.1f dB in the mix against one pad note (trim %+.1f dB)\n", ENG_BRASS.presets[pi].name, db,
               trk[0].p[P_ED_FX] * 0.5);
        fails += db < -4 || db > 4;
    }
    check("levels: every preset's note, its loudest 50 ms in the mix, within 4 dB of a pad note", !fails);
}
static void t_corners(void)
{
    static const uint8_t NOTES[] = {24, 48, 72, 96, 115};
    double worst = 0;
    uint32_t pi, nt, ty, k, fails = 0;
    char m[200];
    for (pi = 0; pi < ENG_BRASS.npresets; pi++)
        for (nt = 0; nt < NELEM(NOTES); nt++)
            for (ty = 0; ty < 3u; ty++) {
                static int32_t b[CTL];
                double pk = 0;
                uint32_t i, act = 1;
                setup((int32_t)pi);
                trk[0].p[P_E0] = 127, trk[0].p[P_E1] = 127, trk[0].p[P_E4] = 127, trk[0].p[P_E5] = (int16_t)ty;
                trk[0].xp[BX_PMENV] = 63, trk[0].xp[BX_PMFRQ] = trk[0].xp[BX_PMPW] = trk[0].xp[BX_PMFLT] = 1;
                trk[0].xp[BX_SYNC] = 1, trk[0].xp[BX_AGE] = 127, trk[0].xp[BX_NOISE] = 127;
                trk_note_on(&trk[0], NOTES[nt], 127);
                for (i = 0; i < FS / 4 / CTL; i++) {
                    memset(b, 0, sizeof b);
                    track_render(&trk[0], b, CTL);
                    for (k = 0; k < CTL; k++) {
                        double x = fabs(b[k] / 32768.0);
                        pk = x > pk ? x : pk;
                        fails += x != x;
                    }
                }
                worst = pk > worst ? pk : worst;
                fails += !(pk < 2.0 * VOICE_FS / 32768);   /* (the engine's own ceiling: its clamp) */
                trk_note_off(&trk[0], NOTES[nt]);
                for (i = 0; i < 20u * FS / CTL && act; i++) {
                    memset(b, 0, sizeof b);
                    track_render(&trk[0], b, CTL);
                    for (act = 0, k = 0; k < NVOICE; k++)
                        act |= trk[0].v[k].active;
                }
                memset(b, 0, sizeof b);
                track_render(&trk[0], b, CTL);
                for (k = 0; k < CTL; k++)
                    if (fabs(b[k] / 32768.0) > 0.0005) {
                        fails++;
                        break;
                    }
            }
    snprintf(m, sizeof m, "corners: every preset x 5 notes x 3 TYPEs, all up: within 2 x VOICE_FS (worst %.2f), silent once released", worst);
    check(m, !fails);
}

/* ------------------------------------------------------------------ cost --- */
static double cost_ns(uint32_t ei, uint32_t pi, int heavy)   /* heavy: 1 all, 2 4P, 3 PM FILT, 4 PM FREQ+PW, 5 SYNC+NOISE+AGE */
{
    static int32_t ob[CTL];
    uint32_t i, k, nb = 20000, nv = 0;
    uint64_t t0;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    host_preset(&trk[0], ei, pi);
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_REL] = trk[0].p[P_E7] = 127;
    if (heavy) {                                        /* every part of BRASS at work, or one of them */
        trk[0].xp[BX_PMFRQ] = 0;
        if (heavy == 1 || heavy == 2)
            trk[0].p[P_E5] = BT_4P;
        if (heavy == 1 || heavy == 3)
            trk[0].p[P_E4] = 100, trk[0].xp[BX_PMFLT] = 1;
        if (heavy == 1 || heavy == 4)
            trk[0].p[P_E4] = 100, trk[0].xp[BX_PMFRQ] = trk[0].xp[BX_PMPW] = 1, trk[0].xp[BX_PMENV] = 40;
        if (heavy == 1 || heavy == 5)
            trk[0].xp[BX_SYNC] = 1, trk[0].xp[BX_NOISE] = 60, trk[0].xp[BX_AGE] = 100;
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
        double x = cost_ns(0, 0, 0), y = cost_ns(ENGI_BRASS, 0, 0), z = cost_ns(ENGI_BRASS, 0, 1);
        a = r && a < x ? a : x;
        l = r && l < y ? l : y;
        h = r && h < z ? h : z;
    }
    snprintf(m, sizeof m, "cost per voice: ANALOG %.2f ns, BRASS %.2f ns (x%.2f), heaviest %.2f ns (x%.2f): ~%.0f instr on the device",
             a, l, l / a, h, h / a, 163 * h / a);
    check(m, h / a < BRASS_COST_MAX);
    {   /* each part's own share (a note, not a check: the unit decides the weight) */
        static const char *const PART[4] = {"4P", "PM FILT", "PM FREQ + PW", "SYNC + NOISE + AGE"};
        uint32_t q;
        for (q = 0; q < 4u; q++) {
            double c = cost_ns(ENGI_BRASS, 0, (int)q + 2), c2 = cost_ns(ENGI_BRASS, 0, (int)q + 2);
            c = c2 < c ? c2 : c;
            printf("brass:   cost with %-20s x%.2f ANALOG\n", PART[q], c / a);
        }
    }
}

/* ---------------------------------------------------------------- demos --- */
static void demo(const char *dir, uint32_t pi, FILE *tour)
{
    static int32_t o[2 * CTL];
    static const uint8_t CH[4][3] = {{48, 55, 63}, {46, 53, 62}, {44, 51, 60}, {46, 53, 62}};
    static const uint8_t LEAD[8] = {60, 63, 67, 70, 72, 70, 67, 63};
    char path[512], nm[40];
    uint32_t i, j, frames = 6u * FS, step = FS / 4, held[3] = {0}, nh = 0, mono, slow;
    FILE *w;
    for (i = 0; ENG_BRASS.presets[pi].name[i] && i < 39u; i++)
        nm[i] = isalnum((unsigned char)ENG_BRASS.presets[pi].name[i]) ? (char)tolower(ENG_BRASS.presets[pi].name[i]) : '_';
    nm[i] = 0;
    snprintf(path, sizeof path, "%s/%02u_%s.wav", dir, pi + 1u, nm);
    w = fopen(path, "wb");
    if (!w)
        return;
    wav_hdr(w, frames);
    memset(trk, 0, sizeof trk);
    memset(eng_arena_own, 0, sizeof eng_arena_own);
    host_tracks_init();
    host_preset(&trk[0], ENGI_BRASS, pi);
    mono = trk[0].p[P_VOICE] != V_POLY;
    slow = ENG_BRASS.presets[pi].env[0] > 40u;
    for (i = 0; i < frames; i += CTL) {
        uint32_t s = i / step;
        if (i % step < CTL) {
            if (mono) {                                 /* a lead line: eighths */
                for (j = 0; j < nh; j++)
                    trk_note_off(&trk[0], held[j]);
                nh = 0;
                if (i < FS * 5) {
                    held[0] = LEAD[s % 8u] - (pi == 9u || pi == 10u ? 24u : 0u);
                    trk_note_on(&trk[0], held[0], 100);
                    nh = 1;
                }
            } else if (slow ? s % 8u == 0u : (s % 4u == 0u || s % 4u == 3u)) {   /* chords: a bar each, or stabs */
                for (j = 0; j < nh; j++)
                    trk_note_off(&trk[0], held[j]);
                nh = 0;
                if (i < FS * 5)
                    for (j = 0; j < 3u; j++) {
                        held[j] = CH[(s / 8u) % 4u][j] + (pi == 3u ? 12u : 0u);
                        trk_note_on(&trk[0], held[j], 100);
                        nh = 3;
                    }
            } else if (!slow && nh) {
                for (j = 0; j < nh; j++)
                    trk_note_off(&trk[0], held[j]);
                nh = 0;
            }
        }
        mix_block(o, CTL);
        for (j = 0; j < CTL; j++) {
            wav_put(w, o[2 * j], o[2 * j + 1]);
            if (tour)
                wav_put(tour, o[2 * j], o[2 * j + 1]);
        }
    }
    fclose(w);
}

int main(int argc, char **argv)
{
    song.master_q12 = 4096;
    t_golden();
    t_pitch();
    t_sync();
    t_filter();
    t_pmod();
    t_age();
    t_mirror();
    t_type_switch();
    t_patch();
    t_levels();
    t_corners();
    t_cost();
    if (argc > 1) {
        char path[512];
        uint32_t pi;
        FILE *tour;
        snprintf(path, sizeof path, "%s/00_tour.wav", argv[1]);
        tour = fopen(path, "wb");
        if (tour)
            wav_hdr(tour, ENG_BRASS.npresets * 6u * FS);
        for (pi = 0; pi < ENG_BRASS.npresets; pi++)
            demo(argv[1], pi, tour);
        if (tour)
            fclose(tour);
        printf("brass: demos in %s\n", argv[1]);
    }
    printf("brass: %s (%d failed)\n", bad ? "FAIL" : "all ok", bad);
    return bad ? 1 : 0;
}
