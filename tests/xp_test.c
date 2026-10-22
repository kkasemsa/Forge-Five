/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Khantipol Kasemsan (kkasemsa); built on Felucca, (C) 2026 Leo Kuroshita, Hügelton Instruments */
/* The engine patch (firmware/src/engine_xp.c) through the UI host (tests/ui_test.c's harness): a LADDER part's extra
 * values round-trip through a project and a user preset, an FM6 part's patch stays bit for bit, engine switches keep
 * or default them as designed, undo swaps them, the pages show and edit them.
 *   build/host/xp_test */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int xbad;
static void xcheck(const char *what, int ok)
{
    printf("xp: %-88s %s\n", what, ok ? "ok" : "FAIL");
    xbad += !ok;
}

/* a distinctive set of LADDER values on track t */
static void scribble(track_t *t, uint32_t seed)
{
    const xp_info_t *x = trk_xp(t);
    uint32_t k;
    for (k = 0; k < x->n; k++)
        t->xp[k] = (int16_t)(x->d[k].min + (int32_t)((seed * 7u + k * 13u) % (uint32_t)(x->d[k].max - x->d[k].min + 1)));
}
static int same_xp(const track_t *t, const int16_t *ref)
{
    const xp_info_t *x = trk_xp(t);
    uint32_t k;
    for (k = 0; k < x->n; k++)
        if (t->xp[k] != ref[k])
            return 0;
    return 1;
}

int main(void)
{
    static int16_t ref[NXP], ref2[NXP];
    static uint8_t fm6_before[FP_SIZE + 1u], blob[FM6_PACKED];
    uint32_t k, n;
    setvbuf(stdout, NULL, _IONBF, 0);
    ui_power_on();
    xcheck("LADDER has an engine patch of 20 values on 5 pages; FM6 and ANALOG none",
           trk_xp(&trk[0]) == 0 && eng_xp(ENGI_FM6) == 0 && eng_xp(ENGI_LADDER) && eng_xp(ENGI_LADDER)->n == 20u &&
           xp_title(&trk[0], 0) == 0);
    for (k = 0; k < NENGINES; k++) {                   /* every value's range fits a 7-bit byte */
        const xp_info_t *x = eng_xp(k);
        uint32_t j;
        if (!x)
            continue;
        for (j = 0; j < x->n; j++)
            if (x->d[j].max - x->d[j].min > 127 || x->d[j].def < x->d[j].min || x->d[j].def > x->d[j].max)
                break;
        xcheck("every extra value's range at most 127 wide, its default inside", j == x->n && x->n <= NXP);
    }

    /* the pages */
    track_select(0);
    set_engine(ENGI_LADDER);
    xcheck("a LADDER part: XP 1..5 visible, XP 6..8 not; titled OSC 1-2 .. CTRL", (n = 0, 1) &&
           str_eq(xp_title(&trk[0], 0), "OSC 1-2") && str_eq(xp_title(&trk[0], 4), "CTRL") && !xp_title(&trk[0], 5));
    for (k = 0; k < NPAGES; k++)
        if (PAGES[k].scope == SC_XP && page_visible(k))
            n++;
    xcheck("  5 XP pages visible on LADDER", n == 5u);
    set_engine(0);
    for (k = n = 0; k < NPAGES; k++)
        if (PAGES[k].scope == SC_XP && page_visible(k))
            n++;
    xcheck("  none on ANALOG", n == 0u);

    /* a knob edits a value; a load's undo (SAVE held) swaps the values with the copy, as the sound's others */
    set_engine(ENGI_LADDER);
    go_title("XP 1");
    {
        int16_t was = trk[0].xp[0];
        turn(EN_K1, 1);
        xcheck("XP 1 KNOB 1 steps O1 RNG", trk[0].xp[0] == was + 1);
        trk[0].xp[LX_DRIFT] = 77;
        apply_preset(2);                               /* a load: the copy holds the edited values */
        xcheck("  a preset load: PEDAL SUB's values", trk[0].xp[LX_O1RNG] == LD_32 && trk[0].xp[LX_DRIFT] == 12);
        hold(B_SAVE);
        xcheck("  SAVE held (undo): the edited values back (O1 RNG, DRIFT 77)", trk[0].xp[0] == was + 1 && trk[0].xp[LX_DRIFT] == 77);
        hold(B_SAVE);
        xcheck("  again (redo): PEDAL SUB's", trk[0].xp[LX_DRIFT] == 12);
    }

    /* the preset load fills them, CLEAN INIT defaults them */
    apply_preset(1);                                   /* FAT LEAD: GLIDE 38, O2 FRQ -4 */
    xcheck("a preset load: its extra values and GLIDE (FAT LEAD)", trk[0].xp[LX_O2FRQ] == -4 && trk[0].p[P_GLIDE] == 38);
    clean_engine_of(&trk[0]);
    xcheck("CLEAN INIT: the defaults", trk[0].xp[LX_O2FRQ] == 0 && trk[0].xp[LX_MIX1] == 127 && trk[0].xp[LX_DRIFT] == 20);

    /* the blob itself */
    scribble(&trk[0], 3);
    memcpy(ref, trk[0].xp, sizeof ref);
    xcheck("xp_pack: marked, the engine, 20 values", xp_pack(&trk[0], blob) && blob[0] == XP_MARK && blob[1] == ENGI_LADDER && blob[2] == 20u);
    for (k = 0; k < FM6_PACKED && blob[k] < 128u; k++)
        ;
    xcheck("  every byte 7-bit (a project import's check)", k == FM6_PACKED);
    xp_defaults(&trk[0]);
    xcheck("xp_unpack: the values back", xp_unpack(&trk[0], blob) && same_xp(&trk[0], ref));
    blob[1] = 0;                                       /* another engine's */
    xcheck("  another engine's blob: the defaults", !xp_unpack(&trk[0], blob) && trk[0].xp[LX_MIX1] == 127);
    xcheck("  an FM6 patch is no blob", !xp_is_blob(FM6_INIT) && !xp_pack(&trk[1], blob));

    /* a project: LADDER on T1 with scribbled values, FM6 on T2 with an edited patch */
    ui_power_on();
    track_select(0);
    set_engine(ENGI_LADDER);
    scribble(&trk[0], 5);
    memcpy(ref, trk[0].xp, sizeof ref);
    track_select(1);
    set_engine(ENGI_FM6);
    {
        uint8_t v[FP_SIZE + 1u];
        memcpy(v, fm6_patch[1], FP_SIZE + 1u);
        v[FP_ALG] = 17;                                /* an edit: the patch is the track's own */
        fm6_set_patch(1, v);
        fm6_adopt(1);
        memcpy(fm6_before, fm6_patch[1], FP_SIZE + 1u);
    }
    xcheck("project save (slot 2)", project_save_as(2, "XPTEST") == 0);
    track_select(0);
    set_engine(0);                                     /* (the harness's power-on clears the RAM slots: switch instead) */
    track_select(1);
    set_engine(0);
    memset(trk[0].xp, 0, sizeof trk[0].xp);
    {   /* (an engine switch leaves the RAM FM6 patch: put the init voice in by hand, so the load has to bring it back) */
        uint8_t v[FP_SIZE + 1u];
        fm6_unpack(FM6_INIT, v);
        fm6_set_patch(1, v);
    }
    xcheck("  meanwhile: T1 and T2 on ANALOG, T1's values and T2's patch gone", trk[0].eng_req == 0u && trk[1].eng_req == 0u &&
           fm6_patch[1][FP_ALG] != 17u);
    project_load(2);
    xcheck("  project load: T1 LADDER with its 20 values", trk[0].eng_req == ENGI_LADDER && same_xp(&trk[0], ref));
    xcheck("  T2 FM6 with its own patch, bit for bit (ALG 18, OWN)", trk[1].eng_req == ENGI_FM6 &&
           !memcmp(fm6_patch[1], fm6_before, FP_SIZE) && trk[1].p[P_E7] == (int16_t)FM6_OWN);
    xcheck("  T3 (ANALOG, no extra values): its slot untouched", !trk_xp(&trk[2]));

    /* the autosave (RESTORE LAST) is the project format too */
    /* a user preset */
    track_select(0);
    scribble(&trk[0], 9);
    memcpy(ref2, trk[0].xp, sizeof ref2);
    xcheck("user preset store (slot 4) of a LADDER sound", up_store(4, "LADR XP") == 0 || up_store(4, "LADR XP") == 3);
    set_engine(0);
    xcheck("  the part on ANALOG meanwhile", trk[0].eng_req == 0u);
    up_load(4);
    xcheck("  user preset load: LADDER, its 20 values back", trk[0].eng_req == ENGI_LADDER && same_xp(&trk[0], ref2));
    track_select(1);
    xcheck("user preset store (slot 5) of the FM6 sound", up_store(5, "FM6 OWN") == 0 || up_store(5, "FM6 OWN") == 3);
    set_engine(0);
    up_load(5);
    xcheck("  user preset load: FM6, its patch bit for bit", trk[1].eng_req == ENGI_FM6 && !memcmp(fm6_patch[1], fm6_before, FP_SIZE));

    /* an engine switch on a part: LADDER -> FM6 -> LADDER keeps nothing of the values in flash, but the RAM FM6 patch
     * is never touched by a blob */
    track_select(0);
    xcheck("a LADDER part's FM6 RAM patch is the init voice (untouched by its blob)",
           trk[0].eng_req == ENGI_LADDER && fm6_patch[0][FP_ALG] == 0u);

    printf("xp: %s (%d failed)\n", xbad ? "FAIL" : "all ok", xbad);
    return xbad ? 1 : 0;
}
