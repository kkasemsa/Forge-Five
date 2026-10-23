/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Khantipol Kasemsant; built on Clipper (isod89 / 3dSam), SLOOP and Felucca */
/* CHORD-D engine test (firmware/src/eng_chordd.c, the chord memory in voice.c) on the host, through hostsim.c.
 *   build/host/chordd_test [DEMODIR]          (DEMODIR: a WAV per preset and a tour of them all)
 * 1. chord memory MEM: one key starts the chord's notes, its note-off releases them, even after the chord changed
 *    meanwhile; a chord past note 127 drops the notes above it; OFF: one note.
 * 2. LEARN: the keys held together, let go: the chord is learnt (intervals from the lowest, MEM); one key alone: still
 *    LEARN; then one key plays it.
 * 3. SPLIT: CHORD-D on part B plays its chords above the split; part A below plays single notes.
 * 4. VCA GATE: the amplifier open while the key is held (a decaying envelope on the filter only); ENV: it decays.
 * 5. CHORUS: OFF no stereo; I, II, I+II stereo (the BBD's two taps); WIDTH 0 mono again (side 40 dB down).
 * 6. RELEASE (EDIT 2) and the ENV page's R are one value.
 * 7. the engine patch: 16 values; a preset's chord survives xp_pack / xp_unpack.
 * 8. levels: every preset's key (its chord), its loudest 50 ms with its trim (P_ED_FX, in the mix), within 4 dB of a
 *    JUNO pad note's; corners: every preset at five notes, RESO and CUTOFF up, under full scale a voice;
 *    three chord keys never past the voice budget.
 * 9. shared notes: two chords sharing notes, one key let go: the shared notes stay; the same key twice; 20 keys.
 * 10. golden: every preset's first half second of a key, and the chorus's three modes through the mix, hash as
 *    tests/chordd_golden.txt says (GOLDEN_UPDATE=1 rewrites).
 * 11. demos (DEMODIR): each preset playing a phrase through the full mix (its sends). */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("chordd: %-92s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static void setup(track_t *t, uint32_t e, uint32_t pi)
{
    memset(eng_arena, 0, sizeof eng_arena);
    memset(eng_arena_own, 0, sizeof eng_arena_own);
    host_preset(t, e, pi);
    t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = t->p[P_DIST] = 0;
}
static void fresh(void)
{
    uint32_t i;
    memset(trk, 0, sizeof trk);
    for (i = 0; i < NPART; i++)                         /* (the chord memory's keys too, as a panic clears them) */
        cm_reset(i);
    host_tracks_init();
    trk[1].p[P_LYMODE] = LY_OFF;
}

static int sounding_note(uint32_t p, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (trk[p].v[i].active && trk[p].v[i].note == note && trk[p].v[i].stage && trk[p].v[i].stage < 3u)
            return 1;
    return 0;
}
static uint32_t held(uint32_t p)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += trk[p].v[i].active && trk[p].v[i].stage && trk[p].v[i].stage < 3u;
    return n;
}
static void blocks(uint32_t n)
{
    static int32_t o[CTL * 2];
    while (n--)
        mix_block(o, CTL);
}

/* the part's voice sum: RMS and peak (of 32768) over n samples, after skip */
static void part_level(uint32_t p, uint32_t skip, uint32_t n, double *rms, double *peak)
{
    static int32_t b[CTL];
    double s = 0, pk = 0;
    uint32_t i, k;
    for (i = 0; i < skip; i += CTL) {
        memset(b, 0, sizeof b);
        track_render(&trk[p], b, CTL);
    }
    for (i = 0; i < n; i += CTL) {
        memset(b, 0, sizeof b);
        track_render(&trk[p], b, CTL);
        for (k = 0; k < CTL; k++) {
            double x = b[k] / 32768.0;
            s += x * x;
            if (fabs(x) > pk)
                pk = fabs(x);
        }
    }
    *rms = sqrt(s / n);
    *peak = pk;
}

/* the loudest 50 ms of the part's voice sum (RMS) over 1.5 s from the key: a stab and a pad alike */
static double part_loudest(uint32_t p)
{
    double best = 0, r, pk;
    uint32_t w;
    for (w = 0; w < 30u; w++) {
        part_level(p, 0, FS / 20, &r, &pk);
        if (r > best)
            best = r;
    }
    return best;
}

static void t_memory(void)
{
    uint32_t ok;
    fresh();
    setup(&trk[0], ENGI_CHORDD, 0);                     /* DUB STAB: a minor seventh, MEM */
    trk_note_on(&trk[0], 48, 100);
    blocks(2);
    ok = sounding_note(0, 48) && sounding_note(0, 51) && sounding_note(0, 55) && sounding_note(0, 58) && held(0) == 4u;
    check("MEM: one key (48) plays the chord: 48 51 55 58", ok);
    trk[0].xp[CX_N2] = 4;                               /* the chord changes while the key is down */
    trk_note_off(&trk[0], 48);
    blocks(2);
    check("MEM: its note-off lets the notes it started go, though the chord changed meanwhile", held(0) == 0u);
    trk[0].xp[CX_N2] = 3;
    trk_note_on(&trk[0], 120, 100);
    blocks(2);
    check("MEM: a chord at 120: 120 123, nothing above 127 (127 + 3, + 7 dropped)",
          sounding_note(0, 120) && sounding_note(0, 123) && sounding_note(0, 127) && held(0) == 3u);
    trk_note_off(&trk[0], 120);
    blocks(2);
    trk[0].xp[CX_CHORD] = CM_OFF;
    trk_note_on(&trk[0], 60, 100);
    blocks(2);
    check("OFF: one key, one note", held(0) == 1u && sounding_note(0, 60));
    trk_note_off(&trk[0], 60);
    blocks(2);
}

static void t_learn(void)
{
    fresh();
    setup(&trk[0], ENGI_CHORDD, 4);                     /* CHOR STRINGS: chord OFF */
    trk[0].xp[CX_CHORD] = CM_LEARN;
    trk_note_on(&trk[0], 62, 100);
    trk_note_off(&trk[0], 62);
    check("LEARN: one key alone is not a chord: still LEARN", trk[0].xp[CX_CHORD] == CM_LEARN);
    trk_note_on(&trk[0], 57, 100);                      /* A, C, E, G held, out of order */
    trk_note_on(&trk[0], 64, 100);
    trk_note_on(&trk[0], 60, 100);
    trk_note_on(&trk[0], 67, 100);
    blocks(2);
    check("LEARN: the keys sound while it listens", held(0) == 4u);
    trk_note_off(&trk[0], 64);
    trk_note_off(&trk[0], 57);
    check("LEARN: keys still down: nothing learnt yet", trk[0].xp[CX_CHORD] == CM_LEARN);
    trk_note_off(&trk[0], 60);
    trk_note_off(&trk[0], 67);
    check("LEARN: the last key up: MEM, N2 m3, N3 5, N4 m7, N5 .. N7 none",
          trk[0].xp[CX_CHORD] == CM_MEM && trk[0].xp[CX_N2] == 3 && trk[0].xp[CX_N3] == 7 && trk[0].xp[CX_N4] == 10 &&
              !trk[0].xp[CX_N5] && !trk[0].xp[CX_N6] && !trk[0].xp[CX_N7]);
    blocks(400);
    trk_note_on(&trk[0], 50, 100);
    blocks(2);
    check("LEARN: then one key (50) plays it: 50 53 57 60",
          sounding_note(0, 50) && sounding_note(0, 53) && sounding_note(0, 57) && sounding_note(0, 60) && held(0) == 4u);
    trk_note_off(&trk[0], 50);
    blocks(2);
}

static void t_split(void)
{
    fresh();
    setup(&trk[0], ENGI_HYBRID, 0);
    setup(&trk[1], ENGI_CHORDD, 0);
    trk[1].p[P_LYMODE] = LY_SPLIT;
    trk[1].p[P_LYSPLIT] = 60 + 4;                       /* C4 (stored + 4) */
    trk[1].p[P_LYOCT] = 2;
    trk_note_on(&trk[0], 64, 100);
    trk_note_on(&trk[0], 40, 100);
    blocks(2);
    check("SPLIT: part B's CHORD-D above the split plays its chord (64 67 71 74), part A below one note (40)",
          held(1) == 4u && sounding_note(1, 64) && sounding_note(1, 74) && held(0) == 1u && sounding_note(0, 40));
    trk_note_off(&trk[0], 64);
    trk_note_off(&trk[0], 40);
    blocks(2);
    check("SPLIT: both let go", !held(0) && !held(1));
}

static void t_gate(void)
{
    double r1, r2, pk;
    fresh();
    setup(&trk[0], ENGI_CHORDD, 4);
    trk[0].p[P_ATK] = 0, trk[0].p[P_DEC] = 40, trk[0].p[P_SUS] = 0;
    trk[0].p[P_E6] = 0;                                 /* (the filter still: the amplifier alone) */
    trk[0].xp[CX_VCA] = 0;
    trk_note_on(&trk[0], 60, 100);
    part_level(0, CTL * 8, FS / 20, &r1, &pk);
    part_level(0, FS / 2, FS / 20, &r2, &pk);
    check("VCA ENV: a decay to 0 sustain: the note fades (> 30 dB down at 0.6 s)", r2 < r1 * 0.03);
    trk_note_off(&trk[0], 60);
    blocks(600);
    fresh();
    setup(&trk[0], ENGI_CHORDD, 4);
    trk[0].p[P_ATK] = 0, trk[0].p[P_DEC] = 40, trk[0].p[P_SUS] = 0;
    trk[0].p[P_E6] = 0;
    trk[0].xp[CX_VCA] = 1;
    trk_note_on(&trk[0], 60, 100);
    part_level(0, CTL * 8, FS / 20, &r1, &pk);
    part_level(0, FS / 2, FS / 20, &r2, &pk);
    check("VCA GATE: the same envelope, the amplifier open while held (within 1 dB at 0.6 s)",
          r1 > 0.001 && fabs(20 * log10(r2 / r1)) < 1.0);
    trk_note_off(&trk[0], 60);
    part_level(0, FS, FS / 20, &r2, &pk);
    check("VCA GATE: let go, it ends with the release", r2 < r1 * 0.001 && !held(0));
}

static double stereo_diff(uint32_t chorus, int32_t width)
{
    static int32_t o[CTL * 2];
    double d = 0, s = 0;
    uint32_t i, k;
    fresh();
    memset(jc, 0, sizeof jc);
    setup(&trk[0], ENGI_CHORDD, 4);
    trk[0].p[P_E3] = (int16_t)chorus;
    trk[0].xp[CX_WIDTH] = (int16_t)width;
    trk[0].p[P_SPREAD] = 0;
    trk_note_on(&trk[0], 60, 100);
    for (i = 0; i < 200u; i++) {
        mix_block(o, CTL);
        if (i >= 100u)
            for (k = 0; k < CTL; k++) {
                double l = o[2 * k], r = o[2 * k + 1];
                d += (l - r) * (l - r);
                s += (l + r) * (l + r);
            }
    }
    trk_note_off(&trk[0], 60);
    return s > 0 ? sqrt(d / s) : 0;
}
static void t_chorus(void)
{
    double off = stereo_diff(0, 127), c1 = stereo_diff(1, 127), c2 = stereo_diff(2, 127), c3 = stereo_diff(3, 127);
    double narrow = stereo_diff(1, 0);
    char m[160];
    snprintf(m, sizeof m, "CHORUS: OFF mono (side/mid %.4f); I %.3f, II %.3f, I+II %.3f stereo; WIDTH 0 %.4f",
             off, c1, c2, c3, narrow);
    check(m, off < 0.001 && c1 > 0.02 && c2 > 0.02 && c3 > 0.005 && narrow < 0.01);   /* (WIDTH 0: -40 dB and below) */
}

static void t_release(void)
{
    fresh();
    setup(&trk[0], ENGI_CHORDD, 0);
    blocks(1);
    check("RELEASE: a preset sets EDIT 2's RELEASE and the ENV page's R alike", trk[0].p[P_E7] == trk[0].p[P_REL]);
    trk[0].p[P_E7] = 90;
    blocks(1);
    check("RELEASE: EDIT 2 turned to 90: R follows", trk[0].p[P_REL] == 90);
    trk[0].p[P_REL] = 20;
    blocks(1);
    check("RELEASE: R turned to 20: EDIT 2 follows", trk[0].p[P_E7] == 20);
}

static void t_patch(void)
{
    static uint8_t b[XP_BYTES];
    uint32_t k, same = 1;
    int16_t keep[NXP];
    fresh();
    setup(&trk[0], ENGI_CHORDD, 1);                     /* ECHO CHORD: a minor ninth */
    memcpy(keep, trk[0].xp, sizeof keep);
    check("engine patch: CHORD-D has 16 values on 4 pages", CHORDD_XPI.n == 16u && xp_title(&trk[0], 3) &&
                                                               !xp_title(&trk[0], 4));
    xp_pack(&trk[0], b);
    xp_defaults(&trk[0]);
    xp_unpack(&trk[0], b);
    for (k = 0; k < NXP; k++)
        same &= trk[0].xp[k] == keep[k];
    check("engine patch: the chord (MEM, m3 5 m7 9) and the rest round-trip", same && trk[0].xp[CX_N5] == 14);
}

static void t_levels(void)
{
    double ref, pk, r;
    uint32_t pi, fails = 0, ok;
    fresh();
    setup(&trk[0], ENGI_JUNO, 0);                       /* JUNO's pad, one note */
    trk_note_on(&trk[0], 60, 100);
    ref = part_loudest(0);
    trk_note_off(&trk[0], 60);
    for (pi = 0; pi < ENG_CHORDD.npresets; pi++) {
        uint32_t nn, key;
        double db;
        fresh();
        setup(&trk[0], ENGI_CHORDD, pi);
        nn = 1 + (uint32_t)__builtin_popcount(chordd_mask(&trk[0]));
        key = trk[0].p[P_VOICE] == V_POLY ? 60u : 36u;
        trk_note_on(&trk[0], key, 100);
        r = part_loudest(0);
        trk_note_off(&trk[0], key);
        db = 20 * log10(r / ref) + trk[0].p[P_ED_FX] * 0.5;   /* (the trim: in the mix, 1/2 dB steps) */
        printf("chordd:   %-14s %+.1f dB in the mix against one pad note (%u notes, trim %+.1f dB)\n",
               ENG_CHORDD.presets[pi].name, db, nn, trk[0].p[P_ED_FX] * 0.5);
        if (db < -4 || db > 4)
            fails++;
    }
    (void)pk;
    check("levels: every preset's key (its chord), its loudest 50 ms in the mix, within 4 dB of a pad note", !fails);
    fresh();
    setup(&trk[0], ENGI_CHORDD, 0);
    trk_note_on(&trk[0], 48, 100), blocks(2);
    trk_note_on(&trk[0], 53, 100), blocks(2);
    trk_note_on(&trk[0], 55, 100), blocks(2);
    ok = voices_cost() <= VOICE_BUDGET && held(0) <= NVOICE && sounding_note(0, 55) && sounding_note(0, 65);
    check("budget: three chord keys (12 notes) never past the budget; the newest chord sounds", ok);
}

static void t_bounded(void)
{
    double r, pk, worst = 0;
    uint32_t pi, nt, k, fails = 0;
    static const uint8_t NOTES[] = {24, 48, 72, 96, 115};
    char m[120];
    for (pi = 0; pi < ENG_CHORDD.npresets; pi++)
        for (nt = 0; nt < NELEM(NOTES); nt++) {
            uint32_t res;
            for (res = 0; res < 2u; res++) {
                fresh();
                setup(&trk[0], ENGI_CHORDD, pi);
                trk[0].p[P_E1] = res ? 127 : trk[0].p[P_E1];
                trk[0].p[P_E0] = res ? 127 : trk[0].p[P_E0];
                uint32_t nn = 1 + (uint32_t)__builtin_popcount(chordd_mask(&trk[0]));
                trk_note_on(&trk[0], NOTES[nt], 127);
                part_level(0, 0, FS / 4, &r, &pk);
                pk /= nn;                               /* (a voice's share of the chord) */
                if (pk > worst)
                    worst = pk;
                fails += !(pk < 1.25) || r != r;            /* (RESO at full rings past a voice's full scale) */
                trk_note_off(&trk[0], NOTES[nt]);
                {   /* the release runs out (DEEP PAD's is long): at most 20 s, then silence */
                    uint32_t q, act = 1;
                    for (q = 0; q < 20u * FS / CTL && act; q++) {
                        part_level(0, 0, CTL, &r, &pk);
                        for (act = 0, k = 0; k < NVOICE; k++)
                            act |= trk[0].v[k].active;
                    }
                }
                part_level(0, 0, CTL, &r, &pk);
                if (pk > 0.0005 || held(0))
                    printf("chordd: corners: %s note %u%s: %.5f after the release, %u held\n", ENG_CHORDD.presets[pi].name,
                           NOTES[nt], res ? " RESO up" : "", pk, held(0));
                fails += pk > 0.0005 || held(0);
            }
        }
    snprintf(m, sizeof m, "corners: every preset at 5 notes, RESO and CUTOFF up: under 1.25 x full scale a voice (worst %.2f), silent once released", worst);
    check(m, !fails);
}

/* the golden renders: each preset's voice sum (track_render), and the chorus in its three modes through the mix
 * (stereo: the BBD after the voices, fx.c juno_chorus), a key of DUB STAB, the sends off */
#define NGOLD (32u + 3u)
static const char *const GCHOR[3] = {"CHORUS I", "CHORUS II", "CHORUS I+II"};
static void t_golden(void)
{
    static int32_t b[CTL], o[CTL * 2];
    static uint64_t h[NGOLD];
    static const char *name[NGOLD];
    char line[160];
    const char *path = "tests/chordd_golden.txt";
    uint32_t pi, i, k, n = 0, changed = 0, missing = 0;
    FILE *f;
    for (pi = 0; pi < ENG_CHORDD.npresets && pi < 32u; pi++, n++) {
        uint64_t x = 1469598103934665603ull;
        fresh();
        memset(jc, 0, sizeof jc);
        setup(&trk[0], ENGI_CHORDD, pi);
        trk_note_on(&trk[0], 48 + (pi % 12u), 100);
        for (i = 0; i < FS / 2 / CTL; i++) {
            memset(b, 0, sizeof b);
            track_render(&trk[0], b, CTL);
            for (k = 0; k < CTL; k++)
                x = (x ^ (uint32_t)b[k]) * 1099511628211ull;
        }
        h[n] = x, name[n] = ENG_CHORDD.presets[pi].name;
    }
    for (pi = 0; pi < 3u; pi++, n++) {
        uint64_t x = 1469598103934665603ull;
        fresh();
        memset(jc, 0, sizeof jc);
        setup(&trk[0], ENGI_CHORDD, 0);
        trk[0].p[P_E3] = (int16_t)(pi + 1u);
        trk[0].p[P_SPREAD] = 0;
        trk_note_on(&trk[0], 48, 100);
        for (i = 0; i < FS / 2 / CTL; i++) {
            mix_block(o, CTL);
            for (k = 0; k < 2u * CTL; k++)
                x = (x ^ (uint32_t)o[k]) * 1099511628211ull;
        }
        trk_note_off(&trk[0], 48);
        h[n] = x, name[n] = GCHOR[pi];
    }
    if (getenv("GOLDEN_UPDATE")) {
        f = fopen(path, "w");
        if (f) {
            fprintf(f, "# CHORD-D: FNV-1a of half a second of a key: each preset's voices, the chorus's modes through the mix "
                       "(tests/chordd_test.c t_golden)\n");
            for (i = 0; i < n; i++)
                fprintf(f, "%s|%016llx\n", name[i], (unsigned long long)h[i]);
            fclose(f);
        }
        check("golden: tests/chordd_golden.txt rewritten", f != 0);
        return;
    }
    f = fopen(path, "r");
    if (!f) {
        check("golden: tests/chordd_golden.txt present (GOLDEN_UPDATE=1 writes it)", 0);
        return;
    }
    for (i = 0; i < n; i++) {
        unsigned long long want = 0;
        int found = 0;
        size_t ln = strlen(name[i]);
        rewind(f);
        while (fgets(line, sizeof line, f))
            if (!strncmp(line, name[i], ln) && line[ln] == '|' && sscanf(line + ln + 1, "%llx", &want) == 1) {
                found = 1;
                break;
            }
        if (!found) {
            printf("chordd: golden: %s has no entry\n", name[i]);
            missing++;
        } else if (want != h[i]) {
            printf("chordd: golden: %s changed (%016llx, was %016llx)\n", name[i], (unsigned long long)h[i], want);
            changed++;
        }
    }
    fclose(f);
    snprintf(line, sizeof line, "golden: %u presets and the chorus's 3 modes render as tests/chordd_golden.txt says "
             "(%u changed, %u missing)", ENG_CHORDD.npresets, changed, missing);
    check(line, !changed && !missing);
}

/* two chords sharing notes: a key let go keeps the notes the other still holds; the same key twice; a table's worth */
static void t_shared(void)
{
    uint32_t k;
    fresh();
    setup(&trk[0], ENGI_CHORDD, 0);                     /* DUB STAB: m7 */
    trk_note_on(&trk[0], 48, 100);                      /* 48 51 55 58 */
    trk_note_on(&trk[0], 51, 100);                      /* 51 54 58 61: 51 and 58 shared */
    blocks(2);
    trk_note_off(&trk[0], 48);
    blocks(2);
    check("shared notes: key 48 let go: 51 and 58 still sound (key 51's chord holds them), 48 and 55 released",
          sounding_note(0, 51) && sounding_note(0, 58) && sounding_note(0, 54) && sounding_note(0, 61) &&
              !sounding_note(0, 48) && !sounding_note(0, 55));
    trk_note_off(&trk[0], 51);
    blocks(2);
    check("shared notes: key 51 let go too: all released", !held(0));
    trk_note_on(&trk[0], 60, 100);
    trk_note_on(&trk[0], 60, 90);                       /* (the same key twice, no note-off between) */
    blocks(2);
    trk_note_off(&trk[0], 60);
    blocks(2);
    check("the same key twice, then one note-off: its chord released, nothing left", !held(0));
    trk[0].xp[CX_CHORD] = CM_OFF;                       /* (one note a key: the table, not the voices, the limit) */
    for (k = 0; k < 20u; k++)
        trk_note_on(&trk[0], 30 + 3 * k, 100);
    blocks(2);
    for (k = 0; k < 20u; k++)
        trk_note_off(&trk[0], 30 + 3 * k);
    blocks(2);
    check("20 keys, more than the 16 a part keeps: all let go, nothing left", !held(0));
}

/* a phrase through the full mix: a dub chord stab pattern at 120 (a pad: a chord a bar), its sends on */
static void demo(const char *dir, uint32_t pi, FILE *tour)
{
    static int32_t o[CTL * 2];
    static const uint8_t ROOT[4] = {48, 48, 51, 46};
    char path[512], nm[40];
    uint32_t i, j, step = FS / 8, len = FS * 6, on = 0, cur = 0, slow;
    FILE *w;
    for (i = 0; ENG_CHORDD.presets[pi].name[i] && i < 39u; i++)
        nm[i] = isalnum((unsigned char)ENG_CHORDD.presets[pi].name[i]) ? (char)tolower(ENG_CHORDD.presets[pi].name[i]) : '_';
    nm[i] = 0;
    snprintf(path, sizeof path, "%s/%02u_%s.wav", dir, pi + 1u, nm);
    w = fopen(path, "wb");
    if (!w)
        return;
    wav_hdr(w, len);
    fresh();
    memset(jc, 0, sizeof jc);
    host_preset(&trk[0], ENGI_CHORDD, pi);
    slow = ENG_CHORDD.presets[pi].env[0] > 40u;         /* (a pad: whole-bar chords, its attack heard) */
    for (i = 0; i < len; i += CTL) {
        uint32_t s = i / step;
        if (i % step < CTL) {
            if (on) {
                trk_note_off(&trk[0], cur);
                on = 0;
            }
            if (i < FS * 4 && (slow ? s % 8u == 0u : s % 8u == 2u || s % 8u == 5u || s % 16u == 7u)) {
                cur = trk[0].p[P_VOICE] == V_POLY ? ROOT[(s / 8u) % 4u] + 12u : ROOT[(s / 8u) % 4u] - 12u;
                trk_note_on(&trk[0], cur, 100);
                on = 1;
            }
        } else if (on && (slow ? s % 8u == 7u : 1) && i % step >= step / 2) {
            trk_note_off(&trk[0], cur);
            on = 0;
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
    t_golden();                                         /* (first: from the power-on state. VINTAGE and the voices'
                                                         * random values draw from seeds every test before would move) */
    t_memory();
    t_shared();
    t_learn();
    t_split();
    t_gate();
    t_chorus();
    t_release();
    t_patch();
    t_levels();
    t_bounded();
    if (argc > 1) {
        char path[512];
        uint32_t pi;
        FILE *tour;
        snprintf(path, sizeof path, "%s/00_tour.wav", argv[1]);
        tour = fopen(path, "wb");
        if (tour)
            wav_hdr(tour, ENG_CHORDD.npresets * 6u * FS);
        for (pi = 0; pi < ENG_CHORDD.npresets; pi++)
            demo(argv[1], pi, tour);
        if (tour)
            fclose(tour);
        printf("chordd: demos in %s\n", argv[1]);
    }
    printf("chordd: %s (%d failed)\n", bad ? "FAIL" : "all ok", bad);
    return bad ? 1 : 0;
}
