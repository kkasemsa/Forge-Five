/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 3dSam (Clipper) */
/* Clipper 0.7 on the FM-1 (the harness of tests/ui_pages_test.c): the sound's history and COMPARE (ui_synth.c hist_*),
 * the favourites (ui.c fav_*), KBD HOLD (seq.c kh_*), the arp's new modes and a sound loaded under a running arp.
 *   UNDO     a knob turned and left: one step (a sweep: one step); UNDO left goes back, right forward; a new change
 *            drops the steps forward; a sound loaded: the edits are a step (UNDO brings them back); RANDOM: a step;
 *            FMX: its patch goes with the steps
 *   COMPARE  ORIG: the sound as loaded; EDIT: the edits again; a change while comparing ends it, the edits a step back
 *   EDITED   HOME says so once the sound differs from what was loaded
 *   FAV      PRESETS, KNOB 4: the sound at the top of the list; left: out again (kept: tests/synth6_test.c)
 *   screens: DIR/hist-*.ppm */
#define UI_PAGES_HARNESS_ONLY
#include "ui_pages_test.c"

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
static void rest(void) { frames(70); }                  /* past HIST_REST (16 ms a frame) */

int main(int argc, char **argv)
{
    uint32_t i;
    track_t *t = &trk[0];
    int16_t e0, e1;
    outdir = argc > 1 ? argv[1] : "build/host";
    bank_resolve();
    panel = PANEL_DEFAULT;
    layers_init();
    settings.palette = 4;
    palette_set(4);
    host_tracks_init();
    for (i = 0; i < NPART; i++) { set_engine_of(&trk[i], i ? 1u : 0u); apply_preset_to(&trk[i], 0); trk[i].engine = trk[i].eng_req; }
    clipper_proj_neutral();
    go_home(); ui.force = 1; frames(4);

    /* ---- UNDO */
    rest();
    ok(hist_ready && !hist_edited && hist_n == 0u, "history: ready, nothing edited after power-on");
    e0 = t->p[P_E0 + 4];                                /* ANALOG CUT */
    go_page("EDIT 2");
    knob(0, -10);                                       /* EDIT 2 K1: the engine's 5th value (CUT) */
    e1 = t->p[P_E0 + 4];
    knob(0, -3); knob(0, -2);                           /* a sweep: one step */
    rest();
    ok(e1 != e0 && hist_n == 1u && hist_edited, "UNDO: a knob turned and left: one step, the sound EDITED");
    go_home(); frame(); ppm("hist-home-edited");
    go_page("EDIT 2");
    knob(1, t->p[P_E0 + 5] > 60 ? -6 : 6);             /* another value: a second step */
    rest();
    ok(hist_n == 2u, "UNDO: another change: a second step");
    go_page("UNDO");
    ppm("hist-undo");
    knob(0, -1);
    ok(t->p[P_E0 + 5] == hist[(hist_head + HIST_N - 1u) % HIST_N].s.p[P_E0 + 5] || hist_r == 1u,
       "UNDO left: one step back");
    knob(0, -1);
    rest();
    ok(t->p[P_E0 + 4] == e0 && hist_r == 2u && !hist_edited, "UNDO left again: the sound as loaded (not EDITED)");
    ppm("hist-undo-back");
    knob(0, 2);
    rest();
    ok(hist_r == 0u && t->p[P_E0 + 4] != e0 && hist_n == 2u, "UNDO right: forward again to the newest");
    knob(0, -1);
    rest();
    go_page("EDIT 2");
    knob(2, t->p[P_E0 + 6] > 60 ? -4 : 4);             /* a new change after UNDO: the step forward is dropped */
    rest();
    ok(hist_r == 0u, "UNDO: a new change drops the steps forward");

    /* ---- COMPARE */
    go_page("UNDO");
    knob(1, 1);
    frames(2);
    ok(hist_cmp_on && t->p[P_E0 + 4] == e0 && !hist_edited, "COMPARE ORIG: the sound as loaded");
    go_home(); frame(); ppm("hist-home-compare");
    go_page("UNDO");
    knob(1, -1);
    rest();
    ok(!hist_cmp_on && t->p[P_E0 + 4] != e0 && hist_edited, "COMPARE EDIT: the edits again");
    knob(1, 1);
    go_page("EDIT 2");
    knob(3, t->p[P_E0 + 7] > 60 ? -5 : 5);             /* a change while comparing: compare ends, the edits a step */
    rest();
    ok(!hist_cmp_on, "COMPARE: a change while comparing ends it");
    go_page("UNDO");
    knob(0, -1);
    rest();
    ok(hist_r == 1u && t->p[P_E0 + 4] != e0, "COMPARE: the edits one step back (UNDO brings them)");
    knob(0, 1);
    rest();

    /* ---- a sound loaded: the edits a step; RANDOM a step */
    {
        uint32_t n0 = hist_n, eng = t->eng_req;
        int16_t cut = t->p[P_E0 + 4];
        set_engine_of(t, ENGI_HYBRID);
        apply_preset_to(t, 2);
        frames(3);
        rest();
        ok(hist_n == n0 + 1u && !hist_edited, "a sound loaded: a step, not EDITED");
        go_page("UNDO");
        knob(0, -1);
        rest();
        ok(t->eng_req == eng && t->p[P_E0 + 4] == cut, "UNDO after a sound loaded: the edited sound back");
        knob(0, 1);
        rest();
        ok(t->eng_req == ENGI_HYBRID, "UNDO right: the loaded sound again");
    }
    {
        int16_t before[P_COUNT];
        memcpy(before, t->p, sizeof before);
        go_page("RANDOM");
        knob(2, 1);                                     /* RAND */
        rest();
        go_page("UNDO");
        knob(0, -1);
        rest();
        ok(!memcmp(before + P_E0, t->p + P_E0, 8 * sizeof(int16_t)), "RANDOM, then UNDO: the sound before it");
        knob(0, 1);
        rest();
    }
    /* ---- FMX: the patch goes with the steps */
    {
        uint8_t w0;
        set_engine_of(t, ENGI_FMX);
        apply_preset_to(t, 0);
        frames(4);
        rest();
        w0 = fmx_cur[0].op[1].wave;
        go_page("OP");
        knob(0, 1);                                     /* OP 2 */
        knob(1, 3);                                     /* its wave */
        rest();
        ok(fmx_cur[0].op[1].wave != w0 && hist_edited, "FMX: an operator's wave changed, EDITED");
        go_page("UNDO");
        knob(0, -1);
        frames(3);
        rest();
        ok(fmx_cur[0].op[1].wave == w0, "FMX: UNDO brings the patch back");
        knob(0, 1);
        frames(3);
        ok(fmx_cur[0].op[1].wave != w0, "FMX: UNDO right: the edit again");
    }

    /* ---- RANDOM by section (0.7): ENV moves the envelopes only; OPS on FMX: the operators, not the EDIT values */
    {
        int16_t before[P_COUNT];
        uint32_t k, other = 0, envd = 0;
        fmx_patch_t fb;
        set_engine_of(t, 0);
        apply_preset_to(t, 2);
        frames(3);
        memcpy(before, t->p, sizeof before);
        xv[X_RNDAMT] = 127;
        xv[X_RNDWHAT] = 3;                              /* ENV */
        syn_randomize();
        for (k = 0; k < P_COUNT; k++) {
            int envp = (k >= P_ATK && k <= P_REL) || (k >= P_ED_FLT && k <= P_ED_SHP) || (k >= P_E2ATK && k <= P_E2REL);
            if (t->p[k] != before[k])
                envp ? envd++ : other++;
        }
        ok(envd >= 3u && !other, "RANDOM WHAT ENV: the envelopes move, nothing else");
        set_engine_of(t, ENGI_FMX);
        apply_preset_to(t, 1);
        frames(4);
        memcpy(before, t->p, sizeof before);
        fb = fmx_cur[0];
        xv[X_RNDWHAT] = 6;                              /* OPS */
        syn_randomize();
        frames(2);
        if (FORGE_FIVE)                                 /* (no FMX: OPS randomizes nothing) */
            ok(!memcmp(&fb, &fmx_cur[0], sizeof fb), "RANDOM WHAT OPS without FMX (Forge Five): nothing moves");
        else
            ok(memcmp(&fb.op, &fmx_cur[0].op, sizeof fb.op) && !memcmp(before + P_E0, t->p + P_E0, 8 * sizeof(int16_t)) &&
               !memcmp(fb.out, fmx_cur[0].out, sizeof fb.out), "RANDOM WHAT OPS on FMX: the operators move, the carriers' outputs and EDIT stay");
        {
            int bad = 0;
            for (k = 0; k < FMX_NOP; k++)
                bad |= fmx_cur[0].op[k].coarse > 31u || fmx_cur[0].op[k].level > 127u;
            ok(!bad, "RANDOM WHAT OPS: every value in range");
        }
        xv[X_RNDWHAT] = 1;
        xv[X_RNDAMT] = 40;
    }

    /* ---- the favourites */
    {
        uint32_t total0, total1, cur;
        set_engine_of(t, ENGI_HYBRID);
        apply_preset_to(t, 3);
        frames(3);
        preset_pos(&total0);
        go_page("PRESETS");
        knob(3, 1);
        cur = preset_pos(&total1);
        ok(total1 == total0 + 1u && fav[0] != FAV_NONE && !strcmp(preset_kind(0), "FAV"), "FAV: KNOB 4 right: the sound at the top");
        ppm("hist-fav");
        {
            uint32_t k, e = preset_at(0, &k);
            ok(e == ENGI_HYBRID && k == 3u, "FAV: the first in the list is that sound");
        }
        fav[0] = (uint16_t)fav_key();
        knob(3, -1);
        preset_pos(&total1);
        ok(total1 == total0 && fav[0] == FAV_NONE, "FAV: KNOB 4 left: out of the list again");
        (void)cur;
    }

    /* ---- KBD HOLD, the arp's modes */
    {
        set_engine_of(t, 0);
        apply_preset_to(t, 0);
        frames(3);
        t->p[P_AMODE] = 0;
        t->p[P_AHOLD] = 1;
        frames(2);
        trk_all_off(t);
        key(7);                                         /* a key down and up: it goes on */
        frames(2);
        ok(kh_any(t) && t->v[0].active | t->v[1].active | t->v[2].active | t->v[3].active, "KBD HOLD: a key let go goes on sounding");
        go_home(); frame(); ppm("hist-kbdhold");
        t->p[P_AHOLD] = 0;
        frames(3);
        ok(!kh_any(t), "KBD HOLD off: the held notes end");
    }
    {
        static const char *const want[16] = {"OFF", "UP", "DN", "UPDN", "RND", "ORD", "REPEAT", "DNUP", "UP+8", "CONV",
                                             "DIVG", "PINKY", "THUMB", "WALK", "CHORD", "TURNG"};
        const param_desc_t *d = &TP[P_AMODE];
        int okm = d->max == 15;
        for (i = 0; i < 16u && okm; i++)
            okm = !strcmp(d->names[i], want[i]);
        ok(okm, "ARP: 16 modes (REPEAT DNUP UP+8 CONV DIVG PINKY THUMB WALK CHORD after UP .. ORD; 0.9: TURNG)");
    }
    {   /* a sound loaded under a running arp: the arp and its notes go on */
        t->p[P_AMODE] = 1;
        t->p[P_ARATE] = 2;
        frames(2);
        fm1_in.notes |= 1u << 7;
        frames(6);
        apply_preset_to(t, 1);
        frames(6);
        ok(t->p[P_AMODE] == 1 && t->nheld == 1u, "ARP: a sound loaded while it runs: the arp and its notes go on");
        fm1_in.notes &= ~(1u << 7);
        frames(4);
        t->p[P_AMODE] = 0;
        frames(2);
    }
#if FORGE_FIVE
    {   /* RIBBON's ribbon (K4 on EDIT 1) is played, not an edit: no step, the sound not EDITED; BRILL is an edit.
         * (SPRING off: these frames run the audio too, and the spring would bring it back to the middle) */
        uint32_t n0, h0;
        set_engine_of(t, ENGI_RIBBON);
        apply_preset_to(t, 1);
        t->xp[RX_SPRING] = 0;
        frames(3);
        go_page("UNDO");                                /* (the spring's switch set: a step of its own, first) */
        rest();
        n0 = hist_n, h0 = hist_head;
        go_page("EDIT 1");
        knob(3, 20);
        knob(3, -7);
        rest();
        ok(t->p[P_E3] == 13 && hist_n == n0 && hist_head == h0 && !hist_edited,
           "RIBBON: K4's ribbon turned (and left there): no UNDO step, not EDITED");
        knob(0, 6);
        rest();
        ok(hist_head != h0 && hist_edited, "RIBBON: BRILL turned: a step, EDITED");
    }
#endif
    printf(fails ? "hist_ui: %d FAILED\n" : "hist_ui: all passed\n", fails);
    return fails;
}
