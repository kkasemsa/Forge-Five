/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Khantipol Kasemsant; built on Felucca, (C) 2026 Leo Kuroshita, Hügelton Instruments */
/* Forge Five: the MULTI page (GLO's first), the part's VOICES (P_MAXV) and the cost-unit voice budget (voice.c
 * voices_cost / voice_fits, engines.c eng_weight), through the UI host (tests/ui_test.c's harness).
 *   build/host/multi_test */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int mbad;
static void mcheck(const char *what, int ok)
{
    printf("multi: %-88s %s\n", what, ok ? "ok" : "FAIL");
    mbad += !ok;
}
static uint32_t part_sounding(uint32_t p)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += trk[p].v[i].active && trk[p].v[i].stage != 4u;
    return n;
}
static void audio(uint32_t n)                      /* n audio blocks: engine switches land, stolen voices fade */
{
    static int32_t o[2 * CTL];
    while (n--)
        mix_block(o, CTL);
}
static void notes_on(uint32_t p, uint32_t n, uint32_t base)
{
    uint32_t k;
    for (k = 0; k < n; k++) {
        trk_note_on(&trk[p], base + k, 100);
        audio(2);
    }
}

int main(void)
{
    uint32_t k;
    setvbuf(stdout, NULL, _IONBF, 0);
    ui_power_on();

    /* the page */
    press(B_GLO); frames(320);
    mcheck("GLO opens MULTI (graph GR_MULTI, SC_TRK: the MIXER's four cards under the knobs)", str_eq(cur_page()->title, "MULTI") &&
           cur_page()->graph == GR_MULTI && cur_page()->scope == SC_TRK);
    track_select(0);
    {
        uint32_t e0 = trk[0].eng_req;
        turn(EN_K1, 1);
        mcheck("KNOB 1 steps the part's engine (ANALOG -> FM6, the display order)", trk[0].eng_req == ENGI_FM6 && e0 == 0u);
        turn(EN_K1, 1);
        mcheck("  again: LADDER", trk[0].eng_req == ENGI_LADDER);
        turn(EN_K1, -1);
        turn(EN_K1, -1);
        mcheck("  back: ANALOG", trk[0].eng_req == 0u);
    }
    mcheck("MIDI IN is CH1-4 at power-on (the parts on channels 1..4)", song.g[G_ROUTE] == 0 && mch_get(0) == 0u && mch_get(1) == 1u);
    turn(EN_K2, 4);
    mcheck("KNOB 2: part 1's channel 1 -> 5; a turn puts MIDI IN on TRACK (the others keep their channels 2..4)",
           song.g[G_ROUTE] == ROUTE_TRACK && mch_get(0) == 4u && mch_get(1) == 1u && mch_get(2) == 2u && mch_get(3) == 3u);
    turn(EN_K2, 20);
    mcheck("  up to OFF (16 channels, OMNI, OFF; no wrap)", mch_get(0) == MCH_OFF);
    turn(EN_K2, -1);
    mcheck("  back one: OMNI", mch_get(0) == MCH_OMNI);
    turn(EN_K2, -1);
    mcheck("  back one: channel 16", mch_get(0) == 15u);
    turn(EN_K2, -15);
    turn(EN_K3, 3);
    mcheck("KNOB 3: VOICES ALL -> 3 (P_MAXV)", trk[0].p[P_MAXV] == 3 && str_eq(N_MAXV[3], "3"));
    turn(EN_K3, 20);
    mcheck("  at most 8", trk[0].p[P_MAXV] == 8);
    turn(EN_K3, -8);
    {
        int16_t l = trk[0].p[P_LEVEL];
        turn(EN_K4, -5);
        mcheck("KNOB 4: the part's LEVEL", trk[0].p[P_LEVEL] < l);
        turn(EN_K4, 5);
    }
    turn(EN_ALGO, 1);
    mcheck("ALGORITHM: the next part (its cards and knobs)", song.sel == 1u);
    track_select(0);

    /* VOICES is the part's, not the sound's: a preset load keeps it */
    trk[0].p[P_MAXV] = 2;
    apply_preset(3);
    mcheck("a sound load keeps VOICES (param_kept)", trk[0].p[P_MAXV] == 2);
    set_engine(0);
    mcheck("  an engine change too", trk[0].p[P_MAXV] == 2);
    mcheck("  never automation (motion_param)", !motion_param(P_MAXV));

    /* the cap: a POLY part of VOICES 2 holds two notes, the third steals */
    ui_power_on();
    track_select(0);
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_MAXV] = 2;
    notes_on(0, 3, 60);
    mcheck("VOICES 2 on a POLY part: three keys, two voices sounding (the oldest stolen)", part_sounding(0) == 2u &&
           trk_nvoice(&trk[0]) == 2u);
    trk[0].p[P_MAXV] = 0;
    mcheck("  ALL: the engine's cap (ANALOG: 8)", trk_nvoice(&trk[0]) == NVOICE);
    set_engine(ENGI_LADDER);
    audio(XF_BLOCKS + 4);                              /* (the ISR switches the engine after the fade) */
    mcheck("  LADDER: its own cap of 4", trk_nvoice(&trk[0]) == 4u);
    trk[0].p[P_MAXV] = 6;
    mcheck("  VOICES above the engine's cap: the cap", trk_nvoice(&trk[0]) == 4u);

    /* the budget in weights: 8 units; ANALOG 1.0, LADDER 2.5 */
    ui_power_on();
    mcheck("weights: Felucca's engines 1.0 (16), LADDER 2.5 (40); the budget 8.0", eng_weight(0) == 16u && eng_weight(ENGI_FM6) == 16u &&
           eng_weight(ENGI_LADDER) == 40u && VOICE_BUDGET == 128u);
    for (k = 0; k < NTRK; k++) {
        trk[k].p[P_VOICE] = V_POLY;
        trk[k].p[P_REL] = 127;
    }
    notes_on(0, 8, 48);
    mcheck("eight ANALOG notes: eight voices, the budget full (as Felucca)", part_sounding(0) == 8u && voices_cost() == 128u);
    ui_power_on();
    for (k = 0; k < NTRK; k++) {
        trk[k].p[P_VOICE] = V_POLY;
        trk[k].p[P_REL] = 127;
    }
    track_select(1);
    set_engine(ENGI_LADDER);
    audio(XF_BLOCKS + 4);
    trk[1].p[P_VOICE] = V_POLY;
    notes_on(1, 4, 48);
    mcheck("LADDER POLY alone: three voices of 2.5 fit (7.5), the fourth steals one", part_sounding(1) == 3u && voices_cost() == 120u);
    ui_power_on();
    for (k = 0; k < NTRK; k++) {
        trk[k].p[P_VOICE] = V_POLY;
        trk[k].p[P_REL] = 127;
    }
    track_select(1);
    set_engine(ENGI_LADDER);
    audio(XF_BLOCKS + 4);
    trk[1].p[P_VOICE] = V_LEGATO;
    notes_on(0, 6, 48);                                /* six light voices: 6.0 */
    mcheck("six ANALOG voices (6.0) sound", part_sounding(0) == 6u);
    trk_note_on(&trk[1], 36, 100);                     /* a heavy one: 2.5 does not fit beside 6.0 */
    audio(2);
    mcheck("a LADDER bass (2.5) on top: one light voice gives way (6.0 + 2.5 > 8: 5 + 2.5 = 7.5 fit)",
           part_sounding(1) == 1u && part_sounding(0) == 5u && voices_cost() == 120u);
    notes_on(0, 2, 60);
    mcheck("two more ANALOG notes: the budget stays within 8.0, the LADDER bass keeps sounding (MONO protected)",
           voices_cost() <= 128u && part_sounding(1) == 1u && trk[1].v[0].gate);

    /* a project keeps the Multi: engines, channels, VOICES, levels */
    ui_power_on();
    track_select(0); set_engine(ENGI_LADDER); trk[0].p[P_MAXV] = 1; trk[0].p[P_LEVEL] = 90;
    track_select(1); set_engine(ENGI_FM6); trk[1].p[P_MAXV] = 4;
    song.g[G_ROUTE] = ROUTE_TRACK;
    mch_set(0, 9); mch_set(1, 10); mch_set(2, MCH_OFF); mch_set(3, MCH_OMNI);
    mcheck("project save (slot 3) of the Multi", project_save_as(3, "MULTI") == 0);
    track_select(0); set_engine(0); trk[0].p[P_MAXV] = 0; trk[0].p[P_LEVEL] = 100;
    track_select(1); set_engine(0); trk[1].p[P_MAXV] = 0;
    song.g[G_ROUTE] = 0;
    for (k = 0; k < NTRK; k++) mch_set(k, k);
    project_load(3);
    mcheck("  project load: the parts' engines, VOICES and levels back", trk[0].eng_req == ENGI_LADDER && trk[0].p[P_MAXV] == 1 &&
           trk[0].p[P_LEVEL] == 90 && trk[1].eng_req == ENGI_FM6 && trk[1].p[P_MAXV] == 4);
    mcheck("  .. and MIDI IN TRACK with the channels 10, 11, OFF, OMNI", song.g[G_ROUTE] == ROUTE_TRACK && mch_get(0) == 9u &&
           mch_get(1) == 10u && mch_get(2) == MCH_OFF && mch_get(3) == MCH_OMNI);

    printf("multi: %s (%d failed)\n", mbad ? "FAIL" : "all ok", mbad);
    return mbad ? 1 : 0;
}
