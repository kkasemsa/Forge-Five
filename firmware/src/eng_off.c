/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Khantipol Kasemsant; built on Clipper (isod89), SLOOP, Felucca (C) 2026 Leo Kuroshita, Hügelton Instruments */
/* Forge Five: ENG_OFF stands in ENGINES[] for every Clipper engine the build leaves out (core.h FF_ENG). It has
 * no sound, no presets and no page offers it; a project or user preset that names one of those engines loads as
 * silence (engines.c eng_ok: the stores map by number, the numbers stay). */
#include "ff_stubs.c"

static void off_note_on(track_t *t, voice_t *v) { (void)t; (void)v; }
static void off_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    (void)t; (void)v; (void)out; (void)n; (void)m;
}
static const engine_t ENG_OFF = {
    .name = "-",
    .page_title = {"-", "-"},
    .edit = {
        {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
    },
    .presets = 0,
    .npresets = 0,
    .fil_page = -1,
    .note_on = off_note_on,
    .render = off_render,
    .color = 0x7BEF,
    .macro = {P_E0, P_E1, P_E2, P_E3},
};
