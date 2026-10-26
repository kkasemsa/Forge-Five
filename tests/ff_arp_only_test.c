/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Khantipol Kasemsant; built on Clipper (isod89 / 3dSam), SLOOP and Felucca */
/* Forge Five without the step sequencer (spec v0.3, core.h FF_SEQ 0), on the harness of tests/ui_pages_test.c (the
 * real UI on a framebuffer, the audio between frames):
 *   a pattern in the instrument's steps does not play, with PLAY or without; REC does nothing (no take, no wait)
 *   the arpeggiator plays (UP over a held chord: a voice's note moves), ARP held is the note repeat
 *   SEQ, EDIT, SEL and GLO held open no layer and the keys play through them; SEQ tapped opens no page; EDIT, SEL and
 *   GLO tapped open their pages
 *   no NOTES or PATTERN page on any walk; TOOLS without CLEAR SEQ
 *   BERLIN BASS and ARP ZAP (was SEQ PLUCK) bring the arp on
 *   screens to DIR/ff-*.ppm. */
#define UI_PAGES_HARNESS_ONLY
#include "ui_pages_test.c"

static uint32_t voices_part(uint32_t p)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += trk[p].v[i].active && trk[p].v[i].stage != 4u;
    return n;
}
static uint32_t note_mask(uint32_t p)                    /* the notes sounding on part p (bit per note % 32) */
{
    uint32_t i, m = 0;
    for (i = 0; i < NVOICE; i++)
        if (trk[p].v[i].active && trk[p].v[i].gate)
            m |= 1u << (trk[p].v[i].note & 31u);
    return m;
}
static int preset_of(uint32_t e, const char *name)
{
    uint32_t k;
    for (k = 0; k < ENGINES[e]->npresets; k++)
        if (!strcmp(ENGINES[e]->presets[k].name, name))
            return (int)k;
    return -1;
}

int main(int argc, char **argv)
{
    uint32_t i, j;
    outdir = argc > 1 ? argv[1] : "build/host";
    check(FORGE_FIVE == 1 && FF_SEQ == 0, "built as Forge Five, no step sequencer (FF_SEQ 0)");
    bank_resolve();
    panel = PANEL_DEFAULT;
    layers_init();
    host_tracks_init();
    for (i = 0; i < NPART; i++) { set_engine_of(&trk[i], TRK_DEF[i][0]); apply_preset_to(&trk[i], TRK_DEF[i][1]); trk[i].engine = trk[i].eng_req; }
    song.g[G_BPM] = 120;
    go_home(); ui.force = 1; frames(3);

    /* a pattern does not play, stopped or with PLAY; REC does nothing */
    {
        track_t *t = TSEL;
        uint8_t n60[1] = {60};
        t->p[P_SLEN] = 16; t->p[P_AMODE] = 0;
        for (j = 0; j < NSTEP; j++) step_clear(&t->step[j]);
        for (j = 0; j < 16u; j += 2u) put_step(t, j, 1, n60, ST_NOTE, 0);
        tap(B_PLAY); frames(90);                          /* (1.5 s at 120: three bars of steps) */
        check(song.playing && voices_part(0) == 0u, "a pattern in the steps, PLAY: transport runs, no note plays");
        tap(B_REC); frames(10);
        check(!song.rec && !rec_wait && !ft_on, "REC: no take, no wait, no free take");
        tap(B_PLAY); frames(4);
        tap(B_REC); frames(10);
        check(!song.rec && !rec_wait && !ft_on && voices_part(0) == 0u, "REC stopped: nothing armed, nothing plays");
        press(B_REC); frames(80); release(B_REC); frames(2);
        check(step_on(&t->step[0]), "REC held: the pattern is not cleared (REC does nothing)");
        for (j = 0; j < NSTEP; j++) step_clear(&t->step[j]);
    }
    /* the arpeggiator plays; ARP held is the note repeat */
    {
        track_t *t = TSEL;
        uint32_t seen = 0, k;
        t->p[P_AMODE] = 1; t->p[P_ARATE] = 2; t->p[P_AOCT] = 1; t->p[P_AGATE] = 64;
        fm1_in.notes |= 1u << 0 | 1u << 4 | 1u << 7; frame();   /* (three keys held: a chord) */
        for (k = 0; k < 60u; k++) {
            frame();
            seen |= note_mask(0);
        }
        fm1_in.notes = 0; frames(20);
        check(__builtin_popcount(seen) >= 3, "ARP UP over a held chord: its notes play in turn");
        t->p[P_AMODE] = 0; frames(4);
        press(B_ARP); frames(12);
        check(ui.layer == LY_ROLL, "ARP held: the note repeat layer");
        fm1_in.notes |= 1u << 4; frames(30);
        seen = voices_part(0);
        fm1_in.notes = 0; frames(2);
        release(B_ARP); frames(20);
        check(seen > 0u, "ARP held + a key: it repeats");
    }
    /* SEQ, EDIT, SEL and GLO held: no layer, the keys play */
    {
        static const struct { uint32_t b; const char *n; uint32_t fam; } B[] = {
            {B_SEQ, "SEQ", 0}, {B_EDIT, "EDIT", FAM_EDIT}, {B_SCL, "SEL", FAM_SCL}, {B_GLO, "GLO", FAM_GLO}};
        char what[120];
        for (i = 0; i < NELEM(B); i++) {
            uint32_t v;
            go_home(); frames(2);
            press(B[i].b); frames(12);
            fm1_in.notes |= 1u << 7; frames(6);
            v = voices_part(0);
            snprintf(what, sizeof what, "%s held: no layer (%u), a key plays (%u voices)", B[i].n, ui.layer, v);
            check(ui.layer == LY_PLAY && v > 0u, what);
            fm1_in.notes = 0; frames(2);
            release(B[i].b); frames(30);
            go_home(); frames(2);
            tap(B[i].b); frames(2);
            if (B[i].b == B_SEQ)
                check(ui.home, "SEQ tapped: no page (free for Phase 7)");
            else {
                snprintf(what, sizeof what, "%s tapped: its pages", B[i].n);
                check(!ui.home && cur_page()->fam == B[i].fam, what);
            }
        }
    }
    /* no NOTES or PATTERN page; TOOLS without CLEAR SEQ */
    {
        uint32_t seq_pages = 0, clr = 0;
        for (i = 0; i < NPAGES; i++) {
            if (PAGES[i].fam == FAM_SEQ)
                seq_pages += !page_hidden(i);
            if (!strcmp(PAGES[i].title, "TOOLS"))
                for (j = 0; j < 4u; j++)
                    clr += PAGES[i].id[j] == G_CLRSEQ;
        }
        check(!seq_pages && !clr, "no NOTES or PATTERN page; TOOLS without CLEAR SEQ");
    }
    /* the presets made for the sequencer: the arp instead */
    {
        int b = preset_of(ENGI_LADDER, "BERLIN BASS"), z = preset_of(ENGI_LADDER, "ARP ZAP");
        int ok = b >= 0 && z >= 0 && preset_of(ENGI_LADDER, "SEQ PLUCK") < 0;
        if (ok) {
            set_engine_of(TSEL, ENGI_LADDER); apply_preset_to(TSEL, (uint32_t)b); frames(2);
            ok &= TSEL->p[P_AMODE] == 1;
            set_engine_of(TSEL, ENGI_LADDER); apply_preset_to(TSEL, (uint32_t)z); frames(2);
            ok &= TSEL->p[P_AMODE] == 3;
        }
        check(ok, "BERLIN BASS (UP) and ARP ZAP (UPDN, was SEQ PLUCK) bring the arp on");
        ui.force = 1; go_home(); frames(2); ppm("ff-home-arp");
    }
    printf("ff_arp_only: %s\n", fails ? "FAILED" : "all passed");
    return fails != 0;
}
