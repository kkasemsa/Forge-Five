/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Khantipol Kasemsant; after upreset.c, (C) 2026 Leo Kuroshita, Hügelton Instruments */
/* User presets, Forge Five (FORGE_FIVE; upreset.c's API, its own record): 32 slots in two storage.c objects
 * (OBJ_UPRESET0/1, 0xDC000..0xDFFFF), UP_PER_BANK records each, mirrored in RAM so browsing never reads flash.
 * A record: the engine, the name, every parameter as a byte (value - its minimum: every parameter's range is at
 * most 255 wide, up_rec_set checks), and the part's engine patch (engine_xp.c: a 72-byte blob). A record keeps
 * np = the P_COUNT it was stored with: a later firmware with more parameters reads it by place (ids never move:
 * common parameters are added just before P_E0, core.h), the new ones at their defaults; its E0..E7 are the last 8.
 * A bank of another shape (SLOOP's, Clipper's) reads as empty. No pattern: Forge Five has no sequencer (the editor's
 * UP_GET answers 16 x (0, 0) for it, UP_PUT ignores what it is sent there).
 *
 * With -DUP_HOST (host test) only the part above #ifndef UP_HOST is built; it needs core.h and engine_xp.c. */
#define UP_PER_BANK 17u
#define UP_PMAX 136u                             /* room for P_COUNT to grow (126 today) */
#define UP_XPB 72u                               /* the engine patch's blob: 3 + NXP bytes fit */
#define UP_USED 0xA5u
#define UP_VER 1u
#define UP_BANK_MAGIC 0x31465055u                /* "UPF1" */
typedef struct {
    uint8_t used, ver, engine, np;               /* UP_USED, UP_VER, engine, P_COUNT when stored */
    char name[12];                               /* ASCII 32..126, 0-padded (no 0 when 12 long) */
    uint8_t p[UP_PMAX];                          /* each value - its minimum (0..255) */
    uint8_t xp[UP_XPB];                          /* the engine patch (engine_xp.c xp_pack), zeros without one */
} up_rec_t;
typedef struct {
    uint32_t magic;
    uint16_t rsize, nslot;
    up_rec_t r[UP_PER_BANK];
} up_bank_t;
_Static_assert(sizeof(up_rec_t) == 224u, "user preset record layout (Forge Five)");
_Static_assert(sizeof(up_bank_t) <= 3840u && UP_PER_BANK * 2u >= UP_SLOTS, "two banks hold the slots, each in a sector");
_Static_assert(P_COUNT <= UP_PMAX && 3u + NXP <= UP_XPB, "user preset record: P_COUNT, the engine patch");

static up_bank_t up_bank[2] __attribute__((section(".pool")));   /* (up_gen: ui.c, bumped at every change) */
static up_rec_t *up_rec(uint32_t k) { return &up_bank[k / UP_PER_BANK].r[k % UP_PER_BANK]; }

static int up_valid(const up_rec_t *r)
{
    return r->used == UP_USED && r->ver == UP_VER && r->engine < NENGINES && r->np >= 8u && r->np <= UP_PMAX &&
           r->name[0];
}
static int up_used(uint32_t k) { return k < UP_SLOTS && up_valid(up_rec(k)); }

static const param_desc_t *up_desc(uint32_t e, uint32_t i)
{
    return i >= P_E0 ? &ENGINES[e % NENGINES]->edit[i - P_E0] : &TP[i];
}

/* today's values v (P_COUNT) into record r (np, p[]): each value - its minimum, clamped to its range */
static void up_rec_set(up_rec_t *r, const int16_t *v)
{
    uint32_t i;
    r->np = (uint8_t)P_COUNT;
    for (i = 0; i < P_COUNT; i++) {
        const param_desc_t *d = up_desc(r->engine, i);
        r->p[i] = (uint8_t)clamp(clamp(v[i], d->min, d->max) - d->min, 0, 255);
    }
}

/* the record's values in today's P_* order (mapped by place, see the top); def = the defaults */
static void up_params(const up_rec_t *r, int16_t *out, const int16_t *def)
{
    uint32_t i, nc = r->np - 8u, ncm = nc < (uint32_t)P_E0 ? nc : (uint32_t)P_E0;
    for (i = 0; i < P_E0; i++)
        out[i] = i < ncm ? (int16_t)(up_desc(r->engine, i)->min + r->p[i]) : def[i];
    for (i = 0; i < 8u; i++)
        out[P_E0 + i] = (int16_t)(up_desc(r->engine, P_E0 + i)->min + r->p[nc + i]);
}

static void up_bank_check(uint32_t b, int len)  /* after loading bank b (len bytes, -1 = none): wrong shape -> empty */
{
    up_bank_t *bk = &up_bank[b];
    if (len != (int)sizeof *bk || bk->magic != UP_BANK_MAGIC || bk->rsize != sizeof(up_rec_t) ||
        bk->nslot != UP_PER_BANK)
        memset(bk, 0, sizeof *bk);
}
static int up_bank_import(uint32_t b, const uint8_t *src, int len)   /* (a stored bank into bank b; nothing older) */
{
    if (len == (int)sizeof up_bank[b])
        memcpy(&up_bank[b], src, sizeof up_bank[b]);
    up_bank_check(b, len);
    return 0;
}

static int up_name_ok(const uint8_t *s, uint32_t n)   /* 1..12 printable ASCII */
{
    uint32_t i;
    if (!n || n > 12u)
        return 0;
    for (i = 0; i < n; i++)
        if (s[i] < 32u || s[i] > 126u)
            return 0;
    return 1;
}

static void up_name(uint32_t k, char *b)       /* upper case, 0-terminated: b holds 13 */
{
    const up_rec_t *r = up_rec(k);
    uint32_t i;
    for (i = 0; i < 12u && r->name[i]; i++)
        b[i] = r->name[i] >= 'a' && r->name[i] <= 'z' ? (char)(r->name[i] - 32) : r->name[i];
    b[i] = 0;
}

/* UP_PUT arguments: slot, engine, name, P_COUNT x v14, 16 x (note, flags: ignored) -> *r (values clamped); 0 ok, 1 bad
 * arguments. *slot gets the slot byte when there is one. */
static int up_parse(const uint8_t *a, uint32_t na, up_rec_t *r, uint32_t *slot)
{
    uint32_t i, n, k;
    if (na < 3u)
        return 1;
    *slot = a[0];
    for (n = 0; 2u + n < na && a[2 + n]; n++)
        ;
    k = 3u + n;                                  /* after the name's 0 */
    if (a[0] >= UP_SLOTS || a[1] >= NENGINES || 2u + n >= na || !up_name_ok(a + 2, n) ||
        na < k + 2u * P_COUNT + 32u)
        return 1;
    memset(r, 0, sizeof *r);
    r->used = UP_USED;
    r->ver = UP_VER;
    r->engine = a[1];
    for (i = 0; i < n; i++)
        r->name[i] = (char)a[2 + i];
    {
        int16_t v[P_COUNT];
        for (i = 0; i < P_COUNT; i++, k += 2u)
            v[i] = (int16_t)((int32_t)((a[k] & 127u) | (a[k + 1] & 127u) << 7) - 8192);
        up_rec_set(r, v);
    }
    return 0;
}

#ifndef UP_HOST
static void up_values(const up_rec_t *r, int16_t *v)   /* mapped and clamped for its engine */
{
    int16_t def[P_COUNT];
    uint32_t i;
    for (i = 0; i < P_COUNT; i++)
        def[i] = up_desc(r->engine, i)->def;
    up_params(r, v, def);
    for (i = 0; i < P_COUNT; i++)
        v[i] = (int16_t)clamp(v[i], up_desc(r->engine, i)->min, up_desc(r->engine, i)->max);
}

static void up_boot(void)                      /* persist_boot: the banks from flash */
{
#if FELUCCA_FLASH
    uint32_t b;
    for (b = 0; b < 2u; b++)
        up_bank_check(b, flash_ok ? st_load(OBJ_UPRESET0 + b, &up_bank[b], sizeof up_bank[b]) : -1);
#endif
}

/* record k = *r (0: erase), then the bank to flash: 0 ok, 2 flash error, 3 no flash (kept in RAM) */
static int up_put(uint32_t k, const up_rec_t *r)
{
    up_bank_t *bk = &up_bank[k / UP_PER_BANK];
    bk->magic = UP_BANK_MAGIC;
    bk->rsize = sizeof(up_rec_t);
    bk->nslot = UP_PER_BANK;
    if (r)
        *up_rec(k) = *r;
    else
        memset(up_rec(k), 0, sizeof(up_rec_t));
    if (!r) {
        uint32_t i;
        for (i = 0; i < NTRK; i++)
            if (trk[i].user == k + 1u)
                trk[i].user = 0;
    }
    up_gen++;
#if FELUCCA_FLASH
    if (flash_ok)
        return st_save(OBJ_UPRESET0 + k / UP_PER_BANK, bk, sizeof *bk) ? 2 : 0;
#endif
    return 3;
}

static void up_slot_label(char *b, uint32_t k)  /* "U07" */
{
    b[0] = 'U';
    b[1] = (char)('0' + (k + 1u) / 10u);
    b[2] = (char)('0' + (k + 1u) % 10u);
    b[3] = 0;
}

/* the selected part's sound -> slot k; name 0 or "": engine name + slot number ("HYBRID 07").
 * 1 = the drum track is selected (it has no sound to store) */
static int up_store(uint32_t k, const char *name)
{
    up_rec_t r;
    uint8_t blob[XP_BYTES];
    uint32_t i;
    if (is_drum(TSEL))
        return 1;
    memset(&r, 0, sizeof r);
    r.used = UP_USED;
    r.ver = UP_VER;
    r.engine = TSEL->eng_req;
    if (name && name[0]) {
        for (i = 0; i < 12u && name[i]; i++)
            r.name[i] = name[i];
    } else {
        char b[16], l[4];
        str_cpy(b, ENGINES[TSEL->eng_req]->name, 9);
        up_slot_label(l, k);
        str_cpy(b + str_len(b), " ", 2);
        str_cpy(b + str_len(b), l + 1, 3);
        for (i = 0; i < 12u && b[i]; i++)
            r.name[i] = b[i];
    }
    up_rec_set(&r, TSEL->p);
    xp_pack(TSEL, blob);
    memcpy(r.xp, blob, UP_XPB);
    return up_put(k, &r);
}

/* slot k -> part t's sound: engine and every parameter except its mix and the part's own (param_kept), and its
 * engine patch. 0 ok, 1 empty (or the drum track) */
static int up_load_to(track_t *t, uint32_t k)
{
    const up_rec_t *r;
    int16_t v[P_COUNT];
    uint8_t blob[XP_BYTES];
    uint32_t i;
    int keep_arp = arp_user_on(t);
    if (!up_used(k) || is_drum(t))
        return 1;
    r = up_rec(k);
    up_values(r, v);
    for (i = 0; i < P_COUNT; i++)
        if (param_kept(i) || (keep_arp && i >= P_AMODE && i <= P_AORDER) ||
            (i == P_AHOLD && !t->p[P_AMODE] && t->p[P_AHOLD]))
            v[i] = t->p[i];
    if (!v[P_AMODE] && !t->p[P_AMODE])
        v[P_AHOLD] = t->p[P_AHOLD];
    if (t == &trk[0])
        hist_before_load();
    sound_fade_begin(t);
    if (keep_arp)
        arp_keep_req |= (uint8_t)(1u << trk_index(t));
    arp_snd[trk_index(t) % NTRK] = (uint8_t)(keep_arp ? 0 : v[P_AMODE]);
    panic_req |= (uint8_t)(1u << trk_index(t));
    fm1_irq_off();
    t->eng_req = r->engine;
    for (i = 0; i < P_COUNT; i++)
        t->p[i] = v[i];
    memset(blob, 0, sizeof blob);
    memcpy(blob, r->xp, UP_XPB);
    xp_unpack(t, blob);
    mseq_init(t);
    t->preset = 0;
    fm1_irq_on();
    t->user = (uint8_t)(k + 1u);
    sync_reload = 1;
    ui.force = 1;
    return 0;
}
static int up_load(uint32_t k) { return up_load_to(TSEL, k); }

static uint32_t up_count(void)                 /* used slots */
{
    uint32_t k, n = 0;
    for (k = 0; k < UP_SLOTS; k++)
        n += (uint32_t)up_used(k);
    return n;
}
static uint32_t up_nth(uint32_t n)             /* slot of the n-th used one (n < up_count()) */
{
    uint32_t k;
    for (k = 0; k < UP_SLOTS; k++)
        if (up_used(k) && !n--)
            return k;
    return 0;
}
static uint32_t up_rank(uint32_t slot)         /* used slots before it */
{
    uint32_t k, n = 0;
    for (k = 0; k < slot && k < UP_SLOTS; k++)
        n += (uint32_t)up_used(k);
    return n;
}

/* SAVE > USER page actions, with the message in the top bar */
static void up_ui(uint32_t op, uint32_t k)     /* 0 load, 1 erase, 2 save */
{
    char l[4];
    int rc;
    up_slot_label(l, k);
    if (op != 1u && is_drum(TSEL)) {
        ui_message("DRUM TRACK: NO SOUND");
        return;
    }
    if (op < 2u && !up_used(k)) {
        ui_message("EMPTY SLOT");
        return;
    }
    if (op == 0u) {
        up_load(k);
        ui_say("LOADED ", l);
        return;
    }
    if (song.playing || transport_req) {               /* a flash erase stops the audio ~50 ms */
        ui_message("STOP BEFORE SAVE");
        return;
    }
    rc = op == 1u ? up_put(k, 0) : up_store(k, 0);
    if (rc == 3)
        ui_message(op == 1u ? "ERASED (RAM)" : "SAVED (RAM)");
    else if (rc)
        ui_message(op == 1u ? "ERASE ERROR" : "SAVE ERROR");
    else
        ui_say(op == 1u ? "ERASED " : "SAVED ", l);
    ui.force = 1;
}
#endif
