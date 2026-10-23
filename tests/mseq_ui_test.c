/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 3dSam (Clipper) */
/* Clipper 0.8 on the FM-1 (the harness of tests/ui_pages_test.c): the MOD SEQ's pages, SPICE, JUNO and VECTOR.
 *   MOD SEQ    LANE and STEP choose, VALUE writes the step (-8..7); SHAPE fills the lane over LEN (RAMP+, TRI, SQR,
 *              FLAT, INVRT, SHIFT, RND); the graph: the lane, the step being edited, the one playing
 *   MOD SEQ 2  RATE LEN SMTH START (the sound's parameters); a step changed and left: one UNDO step (UNDO: back)
 *   SPICE      ARP family: SPICE, DICE
 *   SWAP       the layer's SWAP trades the MOD SEQs too
 *   browser    JUNO and VECTOR in the engine list and their presets in PRESETS; the INSERT's JUNO1 .. JUN12
 *   screens: DIR/mseq-*.ppm */
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
static int go_page(const char *t)
{
    int i = page_idx(t);
    if (i < 0)
        return 0;
    open_family(PAGES[i].fam);
    ui.page = (uint8_t)i;
    ui.fam_last[PAGES[i].fam] = (uint8_t)i;
    page_entered();
    ui.force = 1;
    frame();
    return 1;
}
static void knob(uint32_t k, int32_t s) { encs[panel.enc[EN_K1 + k]] = s; frame(); }
static void rest(void) { frames(70); }

int main(int argc, char **argv)
{
    uint32_t i;
    track_t *t = &trk[0];
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

    ok(!memcmp(t->ms, MSEQ_INIT, sizeof t->ms), "MOD SEQ: a sound loaded brings the four shapes");
    ok(go_page("MOD SEQ") && PAGES[ui.page].fam == FAM_LFO, "MOD SEQ: a page of the LFO family");
    ppm("mseq-page");
    knob(0, 1);                                         /* LANE 2 */
    knob(1, 4);                                         /* STEP 5 */
    ok(xv[X_QLANE] == 2 && xv[X_QSTEP] == 5, "MOD SEQ: LANE and STEP choose the step");
    {
        int8_t was = t->ms[1][4];
        knob(2, was < 7 ? 1 : -1);
        ok(t->ms[1][4] == was + (was < 7 ? 1 : -1), "MOD SEQ: VALUE writes the step");
        knob(2, 40);
        ok(t->ms[1][4] == 7, "MOD SEQ: VALUE stops at 7");
        knob(2, -40);
        ok(t->ms[1][4] == -8, "MOD SEQ: VALUE stops at -8");
    }
    ppm("mseq-edit");
    rest();
    {   /* one UNDO step: back */
        uint32_t n0 = hist_n;
        go_page("MOD SEQ");
        knob(2, 5);
        rest();
        ok(hist_n == n0 + 1u, "MOD SEQ: a step changed and left: one UNDO step");
        go_page("UNDO");
        knob(0, -1);
        rest();
        ok(t->ms[1][4] == -8, "MOD SEQ: UNDO brings the step back");
        knob(0, 1);
        rest();
        ok(t->ms[1][4] == -3, "MOD SEQ: UNDO right: the edit again");
    }
    {   /* SHAPE */
        int good;
        go_page("MOD SEQ");
        knob(3, 1);                                     /* RAMP+ */
        good = t->ms[1][0] == -8 && t->ms[1][15] == 7 && t->ms[1][7] < t->ms[1][8];
        ok(good, "MOD SEQ SHAPE RAMP+: -8 to 7 over the 16 steps");
        t->p[P_MSLEN] = 8;
        knob(3, 4);                                     /* SQR */
        good = t->ms[1][0] == 7 && t->ms[1][3] == 7 && t->ms[1][4] == -8 && t->ms[1][7] == -8 && t->ms[1][15] == 7;
        ok(good, "MOD SEQ SHAPE SQR over LEN 8 (the steps past LEN kept)");
        knob(3, 2);                                     /* FLAT (one turn of two detents: the last one applies) */
        good = 1;
        for (i = 0; i < 8u; i++)
            good &= t->ms[1][i] == 0;
        ok(good, "MOD SEQ SHAPE FLAT: every step 0");
        knob(3, 1);                                     /* INVRT of flat: flat */
        ok(t->ms[1][3] == 0 && xv[X_QFILL] == QF_INV, "MOD SEQ SHAPE INVRT");
        knob(3, -5);                                    /* TRI */
        ok(t->ms[1][0] == -8 && t->ms[1][4] > 5, "MOD SEQ SHAPE TRI: up and down");
        {
            int8_t a = t->ms[1][1];
            knob(3, 6);                                 /* .. SHIFT */
            ok(t->ms[1][0] == a && xv[X_QFILL] == QF_SHIFT, "MOD SEQ SHAPE SHIFT: one step to the left");
        }
        t->p[P_MSLEN] = 16;
        ppm("mseq-shape");
    }
    ok(go_page("MOD SEQ 2") && PAGES[ui.page].id[0] == P_MSRATE, "MOD SEQ 2: RATE LEN SMTH START");
    knob(1, -8);
    ok(t->p[P_MSLEN] == 8, "MOD SEQ 2: LEN turned");
    ppm("mseq-page2");
    t->p[P_MSLEN] = 16;
    ok(go_page("SPICE") && PAGES[ui.page].fam == FAM_ARP && PAGES[ui.page].id[0] == P_SPICE, "SPICE: a page of the ARP family");
    knob(0, 20);
    knob(1, 3);
    ok(t->p[P_SPICE] == 20 && t->p[P_DICE] == 3, "SPICE: SPICE and DICE turned");
    ppm("mseq-spice");
    t->p[P_SPICE] = 0;
    {   /* SWAP trades the MOD SEQs */
        int8_t a = t->ms[1][1], b;
        trk[1].ms[1][1] = (int8_t)(a == 5 ? 4 : 5);
        b = trk[1].ms[1][1];
        layer_swap();
        frame();
        ok(trk[0].ms[1][1] == b && trk[1].ms[1][1] == a, "LAYER SWAP: the MOD SEQs trade places too");
        layer_swap();
        frame();
    }
    {   /* the matrix names the lanes */
        ok(!strcmp(N_MSRC[MS_SEQ1], "SEQ1") && !strcmp(N_MSRC[MS_SEQ4], "SEQ4") && MS_SEQ4 < 16 &&
           MS_NSRC <= 32, "MATRIX: SEQ1..SEQ4 among the sources (4 bits in the stores; 1.0: a fifth in the tail)");
    }
    {   /* JUNO and VECTOR in the browser and the engine list */
        uint32_t total, n, juno = 0, vec = 0;
        preset_pos(&total);
        for (n = 0; n < total; n++) {
            uint32_t k, e = preset_at(n, &k);
            juno += e == ENGI_JUNO;
            vec += e == ENGI_VECTOR;
        }
        ok(juno == ENGINES[ENGI_JUNO]->npresets && vec == ENGINES[ENGI_VECTOR]->npresets,
           "PRESETS: every JUNO and VECTOR preset in the list");
        ok(!strcmp(N_ENGNAME[ENGI_JUNO], "DCO") && !strcmp(N_ENGNAME[ENGI_VECTOR], "VECTOR"), "ENG: JUNO and VECTOR named");
        set_engine_of(t, ENGI_JUNO);
        apply_preset_to(t, 0);
        frames(3);
        go_home();
        frame();
        ppm("mseq-juno-home");
        go_page("EDIT 2");
        ppm("mseq-juno-edit2");
        set_engine_of(t, ENGI_VECTOR);
        apply_preset_to(t, 0);
        frames(3);
        go_page("EDIT 1");
        ppm("mseq-vector-edit1");
        go_page("EDIT 2");
        ppm("mseq-vector-edit2");
        go_home();
        frame();
        ppm("mseq-vector-home");
        ok(t->engine == ENGI_VECTOR, "VECTOR: loaded and playing");
    }
    {   /* the INSERT's JUNO chorus types */
        go_page("INSERT");
        t->p[P_ITYPE] = IT_JUNO1;
        ui.force = 1;
        frame();
        ppm("mseq-insert-juno");
        ok(!strcmp(N_ITYPE[IT_JUNO12], "BBD12") && !strcmp(ins_desc(IT_JUNO1, 0)->label, "WIDTH"),
           "INSERT: JUNO1 JUNO2 JUN12, their A B C: WIDTH TONE NOISE");
        t->p[P_ITYPE] = 0;
    }
    {   /* 0.9: TUNING (GLO), VINTAGE (VOICE 2), the filter MODELs (FILTER), TURNG's labels (SPICE) */
        set_engine_of(t, 0);
        apply_preset_to(t, 0);
        frames(3);
        ok(go_page("TUNING") && PAGES[ui.page].fam == FAM_GLO, "TUNING: a page of the GLO family");
        knob(0, 1);
        ok(tune_cfg.sys == TU_JUST && tun_on, "TUNING: KNOB 1 to JUST: the table on");
        knob(1, 2);
        ok(tune_cfg.root == 2u, "TUNING: KNOB 2: ROOT D");
        ppm("tune-page");
        knob(0, -1);
        ok(tune_cfg.sys == TU_EQUAL && !tun_on, "TUNING: back to EQUAL: off");
        knob(1, -2);
        go_page("VOICE 2");
        ok(PAGES[ui.page].id[3] == P_VINT, "VINTAGE: VOICE 2's fourth knob");
        knob(3, 30);
        ok(t->p[P_VINT] == 30, "VINTAGE turned");
        ppm("vint-page");
        t->p[P_VINT] = 0;
        go_page("FILTER");
        knob(1, 2);
        ok(t->p[P_FMODEL] == FM_MS20, "FILTER MODL: SVF MOOG MS20 ..");
        ppm("filter-ms20");
        knob(1, 6);
        ok(t->p[P_FMODEL] == FM_HP, "FILTER MODL: up to HP");
        knob(1, -10);
        ok(t->p[P_FMODEL] == FM_SVF, "FILTER MODL: down to SVF");
        t->p[P_AMODE] = AM_TURING;
        go_page("SPICE");
        ok(!strcmp(track_desc(t, P_SPICE)->label, "MUTAT") && !strcmp(track_desc(t, P_DICE)->label, "SEED") &&
           !strcmp(track_desc(t, P_SLOOP)->label, "LOOP"), "SPICE page in TURNG: MUTAT SEED LOOP");
        ppm("turing-spice");
        t->p[P_AMODE] = 0;
        frame();
        ok(!strcmp(track_desc(t, P_SPICE)->label, "SPICE"), "SPICE page otherwise: SPICE DICE LOOP");
    }
    printf(fails ? "mseq_ui: %d FAILED\n" : "mseq_ui: all passed\n", fails);
    return fails;
}
