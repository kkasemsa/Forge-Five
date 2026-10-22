/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Khantipol Kasemsant; built on Felucca, (C) 2026 Leo Kuroshita, Hügelton Instruments */
/* The engine patch (this fork, v0.2): a part's engine keeps up to NXP extra values of its own in track_t.xp[], beside
 * the eight of EDIT 1 / 2, so an engine can carry its whole front panel (LADDER: 20; the classic polys to come: more).
 * Felucca's formats have no room for more track parameters (P_COUNT is full: project.c), but every project and every
 * FM6 user preset already stores one 128-byte 7-bit record per part, the FM6 patch (project.c PROJ_FM6_OFF, up_fm6.c).
 * An engine with extra values stores them THERE, as a blob the FM6 patch cannot be mistaken for:
 *   byte 0  XP_MARK (126: an FM6 packed patch's byte 0 is OP6's EG rate, 0..99)
 *   byte 1  the engine's number     byte 2  how many values     bytes 3..  each value - its minimum (0..127)
 * An FM6 part stores its FM6 patch as always, bit for bit; a part of any other engine without extra values stores its
 * (idle) FM6 patch as before. On a load, a blob of another engine, or none where one is expected, gives the defaults.
 * The RAM copy of the FM6 patch is never touched by a blob: switching engines on a part loses nothing until a save.
 * The values are edited on their own pages (params.c SC_XP: the page's ids index xp[]), named per engine
 * (xp_info), without motion / locks / MIDI LEARN / the editor (v0.2: the device only). Undo copies them (ui.c). */
#define NXP 64
#define XP_BYTES 128u                    /* the record: 128 7-bit bytes, as eng_fm6.c FM6_PACKED (checked there) */
#define XP_MARK 126u
#define XP_PAGE_N 8u                     /* at most 8 pages of 4 (32 values on pages; the rest reachable by the editor later) */

typedef struct {
    const param_desc_t *d;           /* the values' descriptors, n of them (ranges at most 127 wide) */
    uint8_t n;
    const char *const *titles;       /* the pages' titles, (n + 3) / 4 of them */
    void (*load)(track_t *t, int32_t pi);   /* a sound load: the values of preset pi, pi < 0 their defaults */
} xp_info_t;

static const xp_info_t *eng_xp(uint32_t e);   /* engines.c: the engine's, 0 = it has none */

static inline const xp_info_t *trk_xp(const track_t *t) { return eng_xp(eng_idx(t->eng_req)); }

/* the values' defaults (an engine switch, CLEAN INIT) */
static void xp_defaults(track_t *t)
{
    const xp_info_t *x = trk_xp(t);
    uint32_t k;
    for (k = 0; k < NXP; k++)
        t->xp[k] = x && k < x->n ? x->d[k].def : 0;
}

/* a sound load (ui.c apply_preset_to): preset pi's values, or the defaults for an engine without a load hook */
static void xp_load(track_t *t, int32_t pi)
{
    const xp_info_t *x = trk_xp(t);
    xp_defaults(t);
    if (x && x->load)
        x->load(t, pi);
}

/* the part's blob into b (128 bytes) when its engine has extra values: 1; else 0 (b untouched: the FM6 patch goes) */
static int xp_pack(const track_t *t, uint8_t *b)
{
    const xp_info_t *x = trk_xp(t);
    uint32_t k;
    if (!x)
        return 0;
    memset(b, 0, XP_BYTES);
    b[0] = XP_MARK;
    b[1] = (uint8_t)eng_idx(t->eng_req);
    b[2] = x->n;
    for (k = 0; k < x->n && 3u + k < XP_BYTES; k++)
        b[3 + k] = (uint8_t)clamp(t->xp[k] - x->d[k].min, 0, 127);
    return 1;
}

/* a stored record b into the part's values: 1 = it was this engine's blob (every value into its range), 0 = not a
 * blob of this engine (an FM6 patch, another engine's, older firmware's): the defaults */
static int xp_unpack(track_t *t, const uint8_t *b)
{
    const xp_info_t *x = trk_xp(t);
    uint32_t k, n;
    xp_defaults(t);
    if (!x || b[0] != XP_MARK || b[1] != eng_idx(t->eng_req))
        return 0;
    n = b[2] < x->n ? b[2] : x->n;                 /* (a record of fewer values: the rest at their defaults) */
    for (k = 0; k < n && 3u + k < XP_BYTES; k++)
        t->xp[k] = (int16_t)clamp(x->d[k].min + (int32_t)(b[3 + k] & 127u), x->d[k].min, x->d[k].max);
    return 1;
}

/* 1 = record b is some engine's blob (project.c: an FM6 part never gets one as its patch) */
static inline int xp_is_blob(const uint8_t *b) { return b[0] == XP_MARK; }

/* the descriptor and the value of the part's extra value k (params.c page_desc; 0 = the engine has fewer) */
static const param_desc_t *xp_desc(track_t *t, uint32_t k, int16_t **vp)
{
    const xp_info_t *x = trk_xp(t);
    if (!x || k >= x->n || k >= NXP) {
        *vp = 0;
        return 0;
    }
    *vp = &t->xp[k];
    return &x->d[k];
}

/* the title of the part's extra page i (ui_graph.c page_title), 0 = none */
static const char *xp_title(const track_t *t, uint32_t i)
{
    const xp_info_t *x = trk_xp(t);
    return x && x->titles && i < (x->n + 3u) / 4u ? x->titles[i] : 0;
}
