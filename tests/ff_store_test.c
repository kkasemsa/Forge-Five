/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Khantipol Kasemsant */
/* Forge Five's stores (FORGE_FIVE, CLIPPER_SYNTH 1): the project record FFP1 (project.c) and the user preset record
 * UPF1 (upreset_ff.c): every parameter of each part round-trips (P_MAXV among them), the engine patch bytes go with
 * them, no steps, a Clipper or SLOOP record is not read; the VOICES cap (P_MAXV, voice.c trk_nvoice); the shared
 * budget in units (voices_cost, VOICE_BUDGET).
 *   build/host/ff_store_test */
#define main hostsim_main
#include <stddef.h>
#include "hostsim.c"
#undef main
#define PROJ_HOST 1
static const uint8_t TRK_DEF[NPART][2] = {{ENGI_HYBRID, 0}, {1, 0}, {4, 5}};
static uint32_t trk_def_engine(uint32_t i) { return i < NPART ? TRK_DEF[i][0] : 0u; }
#include "../firmware/src/project.c"
#define UP_HOST 1
#include "../firmware/src/upreset.c"

static int fails;
static void ok(int c, const char *what)
{
    printf("ff_store: %-92s %s\n", what, c ? "ok" : "FAIL");
    fails += !c;
}
static void blocks(uint32_t n)
{
    static int32_t o[CTL * 2];
    while (n--)
        mix_block(o, CTL);
}
static uint32_t sounding(uint32_t p)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += trk[p].v[i].active && trk[p].v[i].stage != 4u;
    return n;
}

int main(void)
{
    static project_t pj;
    uint32_t i, k, same;
    host_tracks_init();
    for (i = 0; i < NPART; i++)
        host_preset(&trk[i], i ? ENGI_JUNO : ENGI_HYBRID, i);

    /* ---- the project: FFP1, every parameter by place */
    ok(PROJ_MAGIC == 0x31504646u && sizeof(pj) <= 3840u && PROJ_NP == (uint32_t)P_COUNT && PROJ_E0 == (uint32_t)P_E0,
       "project: FFP1, P_COUNT parameters a part as they are, one flash object");
    trk[0].p[P_MAXV] = 3, trk[1].p[P_MAXV] = 5;
    trk[0].p[P_M12AMT] = -17, trk[1].p[P_E2SUS] = 99, trk[0].p[P_SPREAD] = 44, trk[1].p[P_LYMODE] = LY_SPLIT;
    trk[0].p[P_E4] = 31;
    for (i = 0; i < NXP; i++)
        trk[0].xp[i] = (int16_t)(i * 3);               /* (no engine has extra values yet: stored as zeros) */
    proj_capture(&pj);
    ok(pj.magic == PROJ_MAGIC && pj.size == sizeof pj && proj_ok(&pj) && pj.t[0].p[P_MAXV] == 3 && pj.t[1].p[P_MAXV] == 5 &&
       pj.t[0].p[P_M12AMT] == -17 && pj.t[1].p[P_E2SUS] == 99 && pj.t[0].p[P_E4] == 31 && pj.t[1].p[P_LYMODE] == LY_SPLIT &&
       pj.t[0].engine == ENGI_HYBRID && pj.t[1].engine == ENGI_JUNO && pj.t[1].preset == 1u,
       "project: capture keeps every parameter by its id (VOICES, the matrix's slot 12, ENV 2, the layer, EDIT)");
    for (k = 0; k < PROJ_XP && !pj.t[0].xp[k]; k++)
        ;
    ok(k == PROJ_XP, "project: a part of an engine without extra values stores an empty engine patch");
    for (i = 0; i < NPART; i++) {
        trk[i].p[P_MAXV] = 0, trk[i].p[P_M12AMT] = 0, trk[i].p[P_E2SUS] = 0, trk[i].p[P_SPREAD] = 0, trk[i].p[P_LYMODE] = 0;
        trk[i].eng_req = trk[i].engine = 0;
        trk[i].step[3].n = 1;
        trk[i].lock[0].step = 2;
    }
    proj_apply(&pj, 1);
    ok(trk[0].p[P_MAXV] == 3 && trk[1].p[P_MAXV] == 5 && trk[0].p[P_M12AMT] == -17 && trk[1].p[P_E2SUS] == 99 &&
       trk[0].p[P_SPREAD] == 44 && trk[1].p[P_LYMODE] == LY_SPLIT && trk[0].p[P_E4] == 31 &&
       trk[0].eng_req == ENGI_HYBRID && trk[1].eng_req == ENGI_JUNO && trk[1].preset == 1u,
       "project: apply brings them all back, the engines and presets too");
    ok(!trk[0].step[3].n && trk[0].lock[0].step == LOCK_FREE, "project: no steps or locks are stored: a load clears them");
    {   /* a record of another firmware: not a project here */
        static uint8_t raw[3840];
        static project_t q;
        memcpy(raw, &pj, sizeof pj);
        raw[0] = 'F', raw[1] = 'U', raw[2] = 'N', raw[3] = '5';
        same = !proj_import(&q, raw, (int)sizeof pj);
        memcpy(raw, &pj, sizeof pj);
        same &= proj_import(&q, raw, (int)sizeof pj) && q.t[0].p[P_MAXV] == 3;
        ok(same, "project: a FUN5 (Clipper / SLOOP) record is not a project here; FFP1 imports as it is");
    }

    /* ---- user presets: UPF1 */
    ok(UP_BANK_MAGIC == 0x31465055u && sizeof(up_rec_t) == 224u && UP_PER_BANK * 2u >= UP_SLOTS && sizeof(up_bank_t) <= 3840u,
       "user presets: UPF1, 224-byte records, 17 a bank, two banks for the 32 slots");
    {
        up_rec_t r;
        int16_t v[P_COUNT], out[P_COUNT], def[P_COUNT];
        memset(&r, 0, sizeof r);
        r.used = UP_USED, r.ver = UP_VER, r.engine = ENGI_HYBRID;
        r.name[0] = 'A';
        for (i = 0; i < P_COUNT; i++) {
            const param_desc_t *d = up_desc(r.engine, i);
            v[i] = (int16_t)(d->min + (int32_t)((i * 37u) % (uint32_t)(d->max - d->min + 1)));
            def[i] = d->def;
        }
        up_rec_set(&r, v);
        up_params(&r, out, def);
        for (i = 0, same = 1; i < P_COUNT; i++)
            same &= out[i] == v[i];
        ok(same && r.np == (uint8_t)P_COUNT && up_valid(&r), "user presets: every parameter round-trips through the byte record");
        for (i = 0, same = 1; i < P_COUNT; i++) {           /* every range at most 255 wide: the byte holds it */
            const param_desc_t *d = up_desc(r.engine, i);
            same &= d->max - d->min <= 255;
        }
        ok(same, "user presets: every parameter's range fits a byte");
        {   /* a record of fewer parameters (an older firmware): read by place, the rest at their defaults */
            up_rec_t o = r;
            uint32_t nc = (uint32_t)P_E0 - 3u;
            o.np = (uint8_t)(nc + 8u);
            for (k = 0; k < 8u; k++)
                o.p[nc + k] = r.p[P_E0 + k];
            up_params(&o, out, def);
            same = 1;
            for (i = 0; i < nc; i++)
                same &= out[i] == v[i];
            for (i = nc; i < P_E0; i++)
                same &= out[i] == def[i];
            for (i = 0; i < 8u; i++)
                same &= out[P_E0 + i] == v[P_E0 + i];
            ok(same, "user presets: a record of fewer common parameters reads by place, the new ones at their defaults, EDIT last");
        }
    }
    {   /* UP_PUT's bytes */
        static uint8_t a[4 + 2 * P_COUNT + 40];
        up_rec_t r;
        uint32_t slot, n = 0;
        a[n++] = 4, a[n++] = ENGI_JUNO;
        a[n++] = 'J', a[n++] = 'X', a[n++] = 0;
        for (i = 0; i < P_COUNT; i++) {
            int32_t x = up_desc(ENGI_JUNO, i)->max + 8192;
            a[n++] = (uint8_t)(x & 127), a[n++] = (uint8_t)((x >> 7) & 127);
        }
        for (i = 0; i < 32u; i++)
            a[n++] = 5;
        ok(!up_parse(a, n, &r, &slot) && slot == 4u && r.engine == ENGI_JUNO && r.name[0] == 'J' && r.name[2] == 0 &&
           r.p[P_LEVEL] == (uint8_t)(TP[P_LEVEL].max - TP[P_LEVEL].min),
           "user presets: UP_PUT parses (slot, engine, name, P_COUNT values; the pattern bytes ignored)");
        ok(up_parse(a, n - 40u, &r, &slot) == 1, "user presets: too short a UP_PUT is refused");
    }
    {   /* a Clipper / SLOOP bank is empty here */
        static uint8_t raw[3840];
        memset(raw, 0, sizeof raw);
        raw[0] = 'U', raw[1] = 'P', raw[2] = 'B', raw[3] = '1';
        up_bank_import(0, raw, 8 + 16 * 192);
        ok(!up_used(0) && !up_used(16), "user presets: a UPB1 bank (Clipper, SLOOP) reads as empty");
    }

    /* ---- VOICES and the budget */
    host_tracks_init();
    for (i = 0; i < NPART; i++)
        host_preset(&trk[i], ENGI_HYBRID, 0);
    trk[1].p[P_LYMODE] = LY_OFF;
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_MAXV] = 0;
    ok(trk_nvoice(&trk[0]) == NVOICE, "VOICES ALL: the engine's cap (HYBRID: every voice)");
    trk[0].p[P_MAXV] = 3;
    ok(trk_nvoice(&trk[0]) == 3u, "VOICES 3: three");
    for (k = 0; k < 5u; k++) {
        trk_note_on(&trk[0], 60 + k, 100);
        blocks(2);
    }
    ok(sounding(0) == 3u, "VOICES 3 on a POLY part: five keys, three voices sounding (the others stolen)");
    trk[0].p[P_MAXV] = 0;
    ok(eng_weight(ENGI_HYBRID) == 16u && VOICE_BUDGET == (uint32_t)NVOICE * 16u && voices_cost() == 3u * 16u,
       "the budget: NVOICE units of 16; Clipper's engines one unit a voice; three voices cost 3.0");
    for (k = 0; k < 8u; k++) {
        trk_note_on(&trk[0], 72 + k, 100);
        blocks(2);
    }
    ok(sounding(0) <= NVOICE && voices_cost() <= VOICE_BUDGET, "eleven keys: never past the budget");
    /* the weights measured on the FM-1 (engines.c eng_weight): LADDER 1.25, BRASS, CHORD-D and RIBBON one unit */
    ok(eng_weight(ENGI_LADDER) == 20u && eng_weight(ENGI_BRASS) == 16u && eng_weight(ENGI_CHORDD) == 16u &&
           eng_weight(ENGI_RIBBON) == 16u,
       "the weights: LADDER 1.25 units, BRASS, CHORD-D and RIBBON 1.0 (measured on the FM-1)");
    ok(eng_weight(ENGI_WAVEP) == 16u, "the weights: WAVE-P 1.0 units (measured on the FM-1: ~4-4.5 % a voice)");
    memset(trk, 0, sizeof trk);                         /* (the HYBRID keys above still held: from silence) */
    host_tracks_init();
    for (i = 0; i < NPART; i++)
        host_preset(&trk[i], ENGI_LADDER, 0);
    trk[1].p[P_LYMODE] = LY_LAYER;                      /* (both parts LADDER: past one part's cap of four) */
    trk[0].p[P_VOICE] = trk[1].p[P_VOICE] = V_POLY;
    for (k = 0; k < 6u; k++) {
        trk_note_on(&trk[0], 48 + 2 * k, 100);
        blocks(2);
    }
    ok(sounding(0) + sounding(1) <= 6u && voices_cost() <= VOICE_BUDGET && voices_cost() >= 6u * 20u - 20u,
       "the budget with LADDER on both parts: six voices at most (1.25 each: 7.5 of the 8 units)");
    {   /* random keys on two parts of mixed weights, LAYER and SPLIT: never past the budget after a note-on (a voice
         * given up for another part and fading out was restarted unchecked: 30 % of the note-ons went over) */
        static const uint32_t E[4] = {ENGI_LADDER, ENGI_HYBRID, ENGI_BRASS, ENGI_CHORDD};
        uint32_t trial, over = 0, notes = 0, rs = 12345u;
#define FF_RND(n) ((rs = rs * 1664525u + 1013904223u), (rs >> 8) % (n))
        for (trial = 0; trial < 300u; trial++) {
            uint8_t down[128] = {0};
            memset(trk, 0, sizeof trk);
            host_tracks_init();
            host_preset(&trk[0], E[FF_RND(4u)], 0);
            host_preset(&trk[1], E[FF_RND(4u)], 0);
            trk[0].xp[CX_CHORD] = trk[1].xp[CX_CHORD] = 0;   /* (a CHORD-D: its keys alone) */
            trk[1].p[P_LYMODE] = FF_RND(2u) ? LY_LAYER : LY_SPLIT;
            trk[0].p[P_VOICE] = trk[1].p[P_VOICE] = V_POLY;
            for (k = 0; k < 40u; k++) {
                uint32_t n = 36u + FF_RND(40u);
                if (down[n]) {
                    trk_note_off(&trk[0], n);
                    down[n] = 0;
                } else {
                    trk_note_on(&trk[0], n, 100);
                    down[n] = 1;
                    notes++;
                    over += voices_cost() > VOICE_BUDGET;
                }
                blocks(1u + FF_RND(3u));
            }
        }
#undef FF_RND
        printf("ff_store:   (%u note-ons on two parts of mixed weights, %u past the budget)\n", notes, over);
        ok(!over, "the budget: random keys on two parts of mixed weights, LAYER and SPLIT: never past it");
    }

    printf("ff_store: %s\n", fails ? "FAILED" : "all passed");
    return fails ? 1 : 0;
}
