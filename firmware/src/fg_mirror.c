/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 the Forge Phoenix contributors; Forge Five's port */
/* Forge Five: the screen mirror of Forge Phoenix FP 0.5.11 (its fg_mirror.c), the same encoder, ring, pace and
 * commands; the hooks are Forge Five's own (felucca.c: gfx.c cv_blit / cv_blit_from, lcd.c lcd_fill, main.c around
 * ui_draw, usb.c ep1_tx and its bus reset, editor.c ed_handle). Forge Five has no LCD-wait serving of the editor's
 * reads, so the mirror waits for room only while the panel's DMA runs (time the frame waits anyway), never longer:
 * an owed whole redraw that does not fit goes over a few passes (what each got into the ring stays known).
 * web/EDITOR_PROTOCOL.md "FORGE extension" has the protocol as this firmware answers it.
 *
 * FORGE extension: the screen mirror. FG_SCREEN (122) turns it on, FG_WATCH (118) bit 3 lets its pushes out, and every
 * canvas the UI sends to the panel (gfx.c cv_blit / cv_blit_from) and every solid fill (lcd.c lcd_fill) also goes to the
 * host as FG_RECT (123) pushes: only what was drawn, so a still page costs nothing.
 *
 * FG_RECT, after F0 7D 46 4C 7B: x, y, w, h, row (2 bytes each, 7 bits a byte, low first), flags, then pack7 of the
 * pixels' RLE (at most MIR_MSG_MAX bytes before pack7). The rect is in screen pixels (mode 0: 240 x 240) or in half
 * pixels (mode 1: 120 x 120, each pixel the mean of a 2 x 2 block); row = the rect's row this message starts at; the
 * message's pixels fill the rect's rows from there, left to right. flags: bit 0 the rect's last message, bit 1 the end
 * of a draw pass (the host shows its picture then; such a message may carry no rect: w = h = 0), bit 2 the stream is in
 * mode 1 (half pixels: on every message of it, so a host always knows the mode, whoever chose it). RLE: a byte 0..63 =
 * n + 1 pixels follow; 64..127 = the next pixel n - 63 times. A pixel = 2 bytes, RGB565, high byte first. A rect wider
 * than MIR_STRIP pixels goes as strips of at most MIR_STRIP (so one row's RLE always fits a message).
 *
 * Transport: complete messages go into mir_q (USB-MIDI events, in the pool), drained by usb.c ep1_tx in the TIMER5 ISR
 * after the editor's SysEx and the MIDI out (mir_drain); a message once started is finished before anything else goes.
 * Only what the host does not have goes (mir_rect_new: a hash table of what it has); a message that does not fit is
 * dropped and what it carried stays owed (stale), sent by the next draw of it or a whole redraw (ui.force). The
 * encoder runs in the UI (main loop), right after the blit started its DMA: it reads the canvas while the panel does,
 * and may wait for room only while that DMA runs (or, for the owed whole redraw, up to MIR_FORCE_MS), answering the
 * editor's reads meanwhile. Nothing runs in the audio ISR. */

#define MIR_MSG_MAX 256u                /* RLE bytes a message (before pack7) */
#define MIR_STRIP 120u                  /* the widest rect a message carries: one row's RLE <= 2 + 240 bytes */
#ifndef MIR_RING
#define MIR_RING 2048u                  /* USB-MIDI events (4 bytes each): 8 KB of the pool */
#endif
#define MIR_MIN_MS 33u                  /* at most ~30 draw passes a second */
#define MIR_FORCE_GAP_MS 500u           /* the owed whole redraws (ui.force, the panel's too): at most two a second */
#define MIR_ROLL_MS 66u                 /* the hash table: a row cleared this often (mir_rect_new): all in 16 s */
#define MIR_IDLE_MS 5000u               /* no FL request for this long: the mirror goes off (the host left) */
#define MIR_LEASE_MS 5000u              /* a host's lease: this long after its last FG_SCREEN with its host byte */
#define MIR_HOSTS 7u                    /* lease bits: 0 a host-less (older) app, 1..6 apps that name themselves (the
                                         * mask goes out in one 7-bit byte) */

/* MIR_BSS: the mirror's own variables in a section of their own (still .bss: app.ld takes .bss*). The compiler merges
 * a unit's small globals into blocks addressed from one base; new globals in those blocks moved the audio ISR's
 * addresses and grew its code (fm1_alnk0_irq +122 bytes, target_budget.py 8335 -> 8621), though it reads none of them.
 * A global with a section of its own is never merged */
#define MIR_BSS __attribute__((section(".bss.mir")))
static uint32_t mir_q[MIR_RING] __attribute__((section(".pool")));
static volatile uint32_t mir_qw MIR_BSS, mir_qr MIR_BSS;   /* written by the UI, read by the TIMER5 ISR */
static volatile uint8_t mir_open MIR_BSS;       /* ISR: a message is part sent */

static struct {
    uint8_t on, mode, watch;            /* FG_SCREEN on, its mode; FG_WATCH's mask */
    uint8_t in_pass, send, sent;        /* inside ui_draw (main.c); this pass goes out; something of it went */
    uint8_t dirty;                      /* something drawn was not sent (stale rects): a whole redraw may be owed */
    uint8_t forced;                     /* this pass is that redraw (ui.force set for it) */
    uint32_t last_pass;                 /* fm1_ms: the last pass sent */
    uint32_t dirty_t0, skip_t;          /* fm1_ms: dirty since; the last draw not sent */
    uint32_t resets;                    /* usb.resets when it went on */
    uint32_t msgs, bytes, drops, passes;   /* counters (FG_SCREEN op 3: stats) */
    uint8_t lease;                      /* the apps it is on for: bit h (bit 0: a host-less app); on = lease != 0 */
    uint32_t lease_t[MIR_HOSTS];        /* fm1_ms: host h's last FG_SCREEN */
    uint32_t force_t;                   /* fm1_ms: the last owed whole redraw's start */
    uint32_t roll_t;                    /* fm1_ms: the hash table's last row cleared (mir_rect_new) */
    uint8_t roll;                       /* .. which */
    uint8_t whole;                      /* this forced pass: nothing of it dropped (so far) */
} mir MIR_BSS;

/* ---- the encoder (pure: host tests drive it) ---- */

/* (scratch: in the pool, not .bss) */
static uint8_t mir_rle[MIR_MSG_MAX] __attribute__((section(".pool")));
static uint32_t mir_rn MIR_BSS;
static uint8_t mir_msg[32 + MIR_MSG_MAX + MIR_MSG_MAX / 7u + 2u] __attribute__((section(".pool")));
static uint16_t mir_row[MIR_STRIP] __attribute__((section(".pool")));

/* one row of n pixels (native RGB565) as RLE into o; returns its length (<= 2 + 2n) */
static uint32_t mir_rle_row(const uint16_t *px, uint32_t n, uint8_t *o)
{
    uint32_t i = 0, k = 0;
    while (i < n) {
        uint32_t run = 1, j;
        while (i + run < n && run < 64u && px[i + run] == px[i])
            run++;
        if (run >= 2u) {
            o[k++] = (uint8_t)(63u + run);
            o[k++] = (uint8_t)(px[i] >> 8);
            o[k++] = (uint8_t)px[i];
            i += run;
            continue;
        }
        for (j = i; j < n && j - i < 64u && !(j + 1u < n && px[j + 1u] == px[j]); j++)
            ;
        if (j == i)
            j = i + 1u;
        o[k++] = (uint8_t)(j - i - 1u);
        for (; i < j; i++) {
            o[k++] = (uint8_t)(px[i] >> 8);
            o[k++] = (uint8_t)px[i];
        }
    }
    return k;
}

static uint32_t mir_pack7(const uint8_t *in, uint32_t n, uint8_t *o)
{
    uint32_t i, k = 0;
    for (i = 0; i < n; i += 7u) {
        uint32_t g = n - i < 7u ? n - i : 7u, j, m = 0;
        for (j = 0; j < g; j++)
            m |= (uint32_t)(in[i + j] >> 7) << j;
        o[k++] = (uint8_t)m;
        for (j = 0; j < g; j++)
            o[k++] = in[i + j] & 0x7Fu;
    }
    return k;
}

static void mir_u14(uint8_t *o, uint32_t v) { o[0] = (uint8_t)(v & 0x7Fu); o[1] = (uint8_t)((v >> 7) & 0x7Fu); }

/* one F0..F7 FG_RECT into mir_msg: its length */
static uint32_t mir_frame(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t row, uint32_t flags,
                          const uint8_t *rle, uint32_t rn)
{
    uint32_t k = 0;
    mir_msg[k++] = 0xF0;
    mir_msg[k++] = 0x7D;
    mir_msg[k++] = 0x46;
    mir_msg[k++] = 0x4C;
    mir_msg[k++] = 123u;
    mir_u14(mir_msg + k, x); k += 2u;
    mir_u14(mir_msg + k, y); k += 2u;
    mir_u14(mir_msg + k, w); k += 2u;
    mir_u14(mir_msg + k, h); k += 2u;
    mir_u14(mir_msg + k, row); k += 2u;
    mir_msg[k++] = (uint8_t)(flags & 0x7Fu);
    k += mir_pack7(rle, rn, mir_msg + k);
    mir_msg[k++] = 0xF7;
    return k;
}

#ifndef MIR_OUT
#define MIR_OUT(p, n) mir_put(p, n)     /* host tests capture the stream instead */
#endif
static int mir_put(const uint8_t *p, uint32_t n);

/* the pixel source of a rect: a canvas (cv_px, its rows from cr0, width cw; pixels stored high byte first) or a colour */
typedef struct {
    const uint16_t *px;                 /* 0: solid */
    uint32_t cw, cx, cy;                /* canvas width; the rect's origin in the canvas */
    uint16_t c;                         /* solid colour (native) */
} mir_src_t;

static uint16_t mir_cv(const mir_src_t *s, uint32_t x, uint32_t y)   /* native colour of rect pixel x, y */
{
    uint16_t v;
    if (!s->px)
        return s->c;
    v = s->px[(s->cy + y) * s->cw + s->cx + x];
    return (uint16_t)((v >> 8) | (v << 8));
}

/* a rect's geometry in the mode's pixels: X, Y, W, H (mode 1: the half pixels it touches; ox, oy their screen origin) */
typedef struct {
    uint32_t x, y, w, h;                /* the rect, screen pixels */
    uint32_t X, Y, W, H, ox, oy, mode;
} mir_geo_t;
static void mir_geo(mir_geo_t *g, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t mode)
{
    g->x = x; g->y = y; g->w = w; g->h = h; g->mode = mode;
    if (mode) {                         /* the half-size pixels the rect touches */
        g->X = x / 2u;
        g->Y = y / 2u;
        g->W = (x + w - 1u) / 2u - g->X + 1u;
        g->H = (y + h - 1u) / 2u - g->Y + 1u;
        g->ox = 2u * g->X;              /* the blocks start at even screen pixels */
        g->oy = 2u * g->Y;
    } else {
        g->X = x; g->Y = y; g->W = w; g->H = h;
        g->ox = x; g->oy = y;
    }
}
/* mir_row[0 .. sw) = the rect's row r, its columns c0 .. (the mode's pixels, relative to X, Y) */
static void mir_fetch(const mir_src_t *s, const mir_geo_t *g, uint32_t c0, uint32_t r, uint32_t sw)
{
    uint32_t i;
    if (g->mode) {                      /* the mean of the block's pixels that lie in the rect */
        for (i = 0; i < sw; i++) {
            uint32_t sr = 0, sg = 0, sb = 0, m = 0, dx, dy;
            for (dy = 0; dy < 2u; dy++)
                for (dx = 0; dx < 2u; dx++) {
                    uint32_t sx = 2u * (c0 + i) + dx + g->ox, sy = 2u * r + dy + g->oy;
                    uint16_t c;
                    if (sx < g->x || sy < g->y || sx >= g->x + g->w || sy >= g->y + g->h)
                        continue;
                    c = mir_cv(s, sx - g->x, sy - g->y);
                    sr += c >> 11;
                    sg += (c >> 5) & 0x3Fu;
                    sb += c & 0x1Fu;
                    m++;
                }
            mir_row[i] = (uint16_t)(((sr + m / 2u) / m) << 11 | ((sg + m / 2u) / m) << 5 | ((sb + m / 2u) / m));
        }
    } else {
        for (i = 0; i < sw; i++)
            mir_row[i] = mir_cv(s, c0 + i, r);
    }
}
/* the part of a rect at columns cA .. cA + cw, rows rA .. rA + rh (the mode's pixels, relative to X, Y) out as FG_RECT
 * messages: a rect of its own (x, y, w, h: that part); 0 if one did not fit (the rest not sent) */
static uint32_t mir_fc MIR_BSS, mir_fc2 MIR_BSS, mir_fr MIR_BSS;   /* mir_emit failed: in the strip at columns
                                                                      * mir_fc .. mir_fc2, from its row mir_fr
                                                                      * (relative to rA) on */
static int mir_emit(const mir_src_t *s, const mir_geo_t *g, uint32_t cA, uint32_t cw, uint32_t rA, uint32_t rh)
{
    static uint8_t rb[2u + 2u * MIR_STRIP] __attribute__((section(".pool")));
    uint32_t c0, r, n, rows0, mode = g->mode;
    for (c0 = cA; c0 < cA + cw; c0 += MIR_STRIP) {
        uint32_t sw = cA + cw - c0 < MIR_STRIP ? cA + cw - c0 : MIR_STRIP;
        mir_rn = 0;
        rows0 = 0;
        for (r = 0; r < rh; r++) {
            mir_fetch(s, g, c0, rA + r, sw);
            n = mir_rle_row(mir_row, sw, rb);
            if (mir_rn + n > MIR_MSG_MAX) {     /* this row starts the next message */
                if (!MIR_OUT(mir_msg, mir_frame(g->X + c0, g->Y + rA, sw, rh, rows0, mode ? 4u : 0u, mir_rle, mir_rn)))
                    goto fail;
                mir_rn = 0;
                rows0 = r;
            }
            memcpy(mir_rle + mir_rn, rb, n);
            mir_rn += n;
        }
        if (!MIR_OUT(mir_msg, mir_frame(g->X + c0, g->Y + rA, sw, rh, rows0, mode ? 5u : 1u, mir_rle, mir_rn)))
            goto fail;
        continue;
fail:
        mir_fc = c0;
        mir_fc2 = c0 + sw;
        mir_fr = rows0;
        return 0;
    }
    return 1;
}
/* a rect (screen pixels x, y, w, h) out whole as FG_RECT messages; 0 if one did not fit (the rest not sent) */
static int mir_rect(const mir_src_t *s, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t mode)
{
    mir_geo_t g;
    if (!w || !h)
        return 1;
    mir_geo(&g, x, y, w, h, mode);
    return mir_emit(s, &g, 0, g.W, 0, g.H);
}

/* What the host has: a 16-bit hash of every block of MIR_BW pixels of a row (the mode's pixels: mode 0 240 rows of 16
 * blocks, mode 1 120 rows of 8), 0 = not known. A block the rect covers whole whose hash is the table's is not sent
 * again; one it covers in part goes and becomes unknown (mode 1: also a half pixel at an odd edge, a part mean). The
 * table changes only with what went into the ring: when a message does not fit, the blocks it and the messages after
 * it would have carried become unknown (the host may have the old pixels or some of the new). FG_SCREEN op 2, a mode
 * change, a new lease and the mirror going on clear it; MIR_ROLL_MS clears a row of it at a time (a 16-bit hash may
 * match a changed block: such a block heals the next time it is drawn after its row was cleared) */
#define MIR_BW 15u
#define MIR_NB (240u / MIR_BW)
static uint16_t mir_h[240u * MIR_NB] __attribute__((section(".pool")));
static uint16_t mir_m[240] __attribute__((section(".pool")));  /* (scratch, a rect's rows: the blocks to send, */
static uint16_t mir_d[240] __attribute__((section(".pool")));  /* .. and those sent) */
static uint16_t mir_hash(const uint16_t *p, uint32_t n)
{
    uint32_t h = 2166136261u;
    while (n--)
        h = (h ^ *p++) * 16777619u;
    h = (h ^ (h >> 16)) & 0xFFFFu;
    return (uint16_t)(h ? h : 1u);
}
static void mir_forget(void) { memset(mir_h, 0, sizeof mir_h); }
static int mir_full(const mir_geo_t *g, uint32_t r, uint32_t b)   /* block b of the rect's row r: covered whole */
{
    uint32_t bx = b * MIR_BW;
    if (g->mode && ((r == 0u && (g->y & 1u)) || (r == g->H - 1u && ((g->y + g->h) & 1u))))
        return 0;                       /* (mode 1: a row of part means) */
    return bx >= g->X && bx + MIR_BW <= g->X + g->W &&
           !(g->mode && (((g->x & 1u) && bx == g->X) || (((g->x + g->w) & 1u) && bx + MIR_BW == g->X + g->W)));
}

/* Stale: rects drawn and not sent (a pass not sent, a message that did not fit), screen pixels. The host has the old
 * pixels there, the panel may not. A draw that then goes out whole and covers one makes it fresh (the UI redraws the
 * same canvases, so a page that keeps drawing heals by itself); while any is stale a whole redraw is owed
 * (mir_pass_begin), and one that went whole clears them all. More than MIR_STALE_N: all of it owed */
#define MIR_STALE_N 16u
static struct { uint16_t x, y, w, h; } mir_sr[MIR_STALE_N] MIR_BSS;
static uint8_t mir_srn MIR_BSS, mir_srall MIR_BSS;
static int mir_in(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t i)   /* stale rect i inside x, y, w, h */
{
    return mir_sr[i].x >= x && mir_sr[i].y >= y && mir_sr[i].x + mir_sr[i].w <= x + w && mir_sr[i].y + mir_sr[i].h <= y + h;
}
static void mir_fresh(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    uint32_t i = 0;
    while (i < mir_srn)
        if (mir_in(x, y, w, h, i))
            mir_sr[i] = mir_sr[--mir_srn];
        else
            i++;
}
static void mir_stale(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    uint32_t i;
    for (i = 0; i < mir_srn; i++)
        if (x >= mir_sr[i].x && y >= mir_sr[i].y && x + w <= mir_sr[i].x + mir_sr[i].w && y + h <= mir_sr[i].y + mir_sr[i].h)
            return;                     /* (inside one already) */
    mir_fresh(x, y, w, h);              /* (those inside this one: this one instead) */
    if (mir_srn >= MIR_STALE_N) {
        mir_srall = 1;
        return;
    }
    mir_sr[mir_srn].x = (uint16_t)x;
    mir_sr[mir_srn].y = (uint16_t)y;
    mir_sr[mir_srn].w = (uint16_t)w;
    mir_sr[mir_srn].h = (uint16_t)h;
    mir_srn++;
}
static void mir_stale_clear(void) { mir_srn = 0; mir_srall = 0; }
static int mir_stale_any(void) { return mir_srn || mir_srall; }

/* a rect out as FG_RECT messages, only what the host does not have: each group of side-by-side blocks to send in a
 * row, as one rect down the rows below that have all of it to send too; 0 if a message did not fit */
static int mir_rect_new(const mir_src_t *s, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t mode)
{
    mir_geo_t g;
    uint32_t r, b, b0, b1, q;
    if (!w || !h)
        return 1;
    mir_geo(&g, x, y, w, h, mode);
    if (g.X + g.W > (mode ? 120u : 240u) || g.Y + g.H > (mode ? 120u : 240u)) {   /* (off the panel: whole, not known) */
        for (r = g.Y; r < g.Y + g.H && r < 240u; r++)
            for (b = g.X / MIR_BW; b <= (g.X + g.W - 1u) / MIR_BW && b < MIR_NB; b++)
                mir_h[r * MIR_NB + b] = 0;
        if (mir_rect(s, x, y, w, h, mode)) {
            mir_fresh(x, y, w, h);
            return 1;
        }
        mir_stale(x, y, w, h);
        return 0;
    }
    b0 = g.X / MIR_BW;
    b1 = (g.X + g.W - 1u) / MIR_BW;
    for (r = 0; r < g.H; r++) {
        uint32_t m = 0;
        for (b = b0; b <= b1; b++) {
            uint16_t *e = &mir_h[(g.Y + r) * MIR_NB + b];
            if (mir_full(&g, r, b)) {
                uint16_t v;
                mir_fetch(s, &g, b * MIR_BW - g.X, r, MIR_BW);
                v = mir_hash(mir_row, MIR_BW);
                if (v == *e)
                    continue;
                *e = v;
            } else {
                *e = 0;
            }
            m |= 1u << (b - b0);
        }
        mir_m[r] = (uint16_t)m;
        mir_d[r] = 0;
    }
    for (r = 0; r < g.H; r++) {
        uint32_t m = mir_m[r];
        while (m & ~(uint32_t)mir_d[r]) {   /* a group: side-by-side blocks lo .. hi, down the rows that have it all */
            uint32_t p = m & ~(uint32_t)mir_d[r], lo = (uint32_t)__builtin_ctz(p), hi = lo, gm, c0, c1, qb;
            while (p >> (hi + 1u) & 1u)
                hi++;
            gm = ((2u << hi) - 1u) & ~((1u << lo) - 1u);
            for (qb = r + 1u; qb < g.H && ((mir_m[qb] & ~(uint32_t)mir_d[qb]) & gm) == gm; qb++)
                ;
            c0 = (b0 + lo) * MIR_BW > g.X ? (b0 + lo) * MIR_BW - g.X : 0u;
            c1 = (b0 + hi + 1u) * MIR_BW < g.X + g.W ? (b0 + hi + 1u) * MIR_BW - g.X : g.W;
            if (!mir_emit(s, &g, c0, c1 - c0, r, qb - r)) {
                for (q = r; q < qb; q++)        /* what of it went: the rows before the failed message's, and the
                                                 * strips before its strip */
                    for (b = b0 + lo; b <= b0 + hi; b++)
                        if ((b + 1u) * MIR_BW - g.X <= (q - r >= mir_fr ? mir_fc : mir_fc2))
                            mir_d[q] |= (uint16_t)(1u << (b - b0));
                goto fail;
            }
            for (q = r; q < qb; q++)
                mir_d[q] |= (uint16_t)gm;
        }
    }
    mir_fresh(x, y, w, h);
    return 1;
fail:
    for (r = 0; r < g.H; r++)          /* what did not go: not known */
        for (b = b0; b <= b1; b++)
            if ((mir_m[r] & ~mir_d[r]) >> (b - b0) & 1u)
                mir_h[(g.Y + r) * MIR_NB + b] = 0;
    mir_stale(x, y, w, h);
    return 0;
}

#ifndef MIR_HOST                        /* (tests/screen_test.c drives the encoder alone) */
/* ---- the ring: whole messages in, the TIMER5 ISR out ---- */

static uint32_t mir_room(void) { return MIR_RING - (mir_qw - mir_qr); }

/* waiting for room: only inside the main loop's ui_draw, and only while the panel's DMA still runs (time the frame
 * waits anyway: lcd.c lcd_sync would wait for it next). Forge Five answers no editor request inside ui_draw, so the
 * owed whole redraw does not wait longer either (Forge Phoenix: up to 250 ms, answering the editor's reads) */
static int mir_wait(uint32_t ev)
{
    while (mir_room() < ev) {
        if (!mir.in_pass || !usb.config)
            return 0;
        if (!(lcd_busy && !fm1_lcd_done()))
            return 0;
#ifdef MIR_WAIT_HOOK
        MIR_WAIT_HOOK();                /* (host tests: the ISR's drain) */
#endif
    }
    return 1;
}

static int mir_put(const uint8_t *p, uint32_t n)   /* one F0..F7 message into the ring, whole, or 0 */
{
    uint32_t ev = (n + 2u) / 3u, i = 0, w;
    if (mir_room() < ev && !mir_wait(ev))
        return 0;
    w = mir_qw;
    while (i < n) {                     /* as usb.c ota_wire_send: CIN 4 continues, 5..7 end with 1..3 bytes */
        uint32_t k = n - i >= 3u ? 3u : n - i;
        uint32_t cin = k == 3u && i + 3u < n ? 4u : k == 3u ? 7u : 4u + k;
        mir_q[w % MIR_RING] = cin | (uint32_t)p[i] << 8 | (k > 1u ? (uint32_t)p[i + 1] << 16 : 0u) |
                              (k > 2u ? (uint32_t)p[i + 2] << 24 : 0u);
        w++;
        i += k;
    }
    RING_PUBLISH();                     /* the events before the index: the ISR sees the message whole */
    mir_qw = w;
    mir.msgs++;
    mir.bytes += n;
    return 1;
}

/* usb.c ep1_tx (TIMER5 ISR): events of the ring into the packet from byte n; a started message is finished first
 * (start: only when the caller allows, i.e. no editor SysEx or MIDI out is waiting) */
static uint32_t mir_drain(uint8_t *b, uint32_t n, int may_start)
{
    while (n < 64u && mir_qr != mir_qw && (mir_open || may_start)) {
        uint32_t pkt;
        RING_PUBLISH();
        pkt = mir_q[mir_qr % MIR_RING];
        b[n] = (uint8_t)pkt;
        b[n + 1u] = (uint8_t)(pkt >> 8);
        b[n + 2u] = (uint8_t)(pkt >> 16);
        b[n + 3u] = (uint8_t)(pkt >> 24);
        n += 4u;
        mir_qr++;
        mir_open = (pkt & 0xFu) == 4u;
    }
    return n;
}
static int mir_pending(void) { return mir_qr != mir_qw; }
static int mir_busy(void) { return mir_open; }
static void mir_bus_reset(void)         /* usb.c, a bus reset (TIMER5 ISR) */
{
    mir_qr = mir_qw;
    mir_open = 0;
}

/* ---- the UI side ---- */

static int mir_live(void)
{
    return mir.on && (mir.watch & 8u) && usb.config && !usb.suspended;
}

static void mir_check(void)             /* main loop: off when the last app is gone */
{
    uint32_t h;
    if (!mir.on)
        return;
    if (!usb.config || usb.resets != mir.resets) {
        mir.lease = 0;
    } else {
        if ((mir.lease & 1u) && fm1_ms - ed_w.last_ms > MIR_IDLE_MS)   /* a host-less app: any request keeps it */
            mir.lease &= (uint8_t)~1u;
        for (h = 1; h < MIR_HOSTS; h++)                                /* an app with a lease: its own FG_SCREEN */
            if ((mir.lease >> h & 1u) && fm1_ms - mir.lease_t[h] > MIR_LEASE_MS)
                mir.lease &= (uint8_t)~(1u << h);
    }
    if (!mir.lease) {
        mir.on = 0;
        mir.watch = 0;
    }
}

static void mir_skip(void)              /* drawn, not sent (its blocks: mir_stale): a whole redraw may be owed */
{
    if (!mir.dirty)
        mir.dirty_t0 = fm1_ms;
    mir.dirty = 1;
    mir.skip_t = fm1_ms;
}

static void mir_drop(void)              /* a message did not fit (its blocks: stale): nothing more of this pass */
{
    mir.send = 0;
    mir.whole = 0;
    mir.drops++;
    mir_skip();
}

static int mir_end_pass(void)           /* the end-of-pass mark (no rect) */
{
    return MIR_OUT(mir_msg, mir_frame(0, 0, 0, 0, 0, mir.mode ? 6u : 2u, mir_rle, 0));
}

static void mir_blit(uint32_t x, uint32_t y, uint32_t r0)   /* gfx.c: canvas rows r0.. now on the panel at y + r0 */
{
    mir_src_t s;
    uint32_t m0;
    if (!mir_live() || r0 >= cv_h)
        return;
    if (!mir.send) {
        mir_stale(x, y + r0, cv_w, cv_h - r0);
        mir_skip();
        return;
    }
    s.px = cv_px;
    s.cw = cv_w;
    s.cx = 0;
    s.cy = r0;
    s.c = 0;
    m0 = mir.msgs;
    if (!mir_rect_new(&s, x, y + r0, cv_w, cv_h - r0, mir.mode))
        mir_drop();
    else if (mir.msgs != m0)
        mir.sent = 1;                   /* (all of it on the host already: no end of pass for it) */
}

static void mir_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)   /* lcd.c, clipped */
{
    mir_src_t s;
    uint32_t m0;
    if (!mir_live())
        return;
    s.px = 0;
    s.cw = s.cx = s.cy = 0;
    s.c = c;
    if (!mir.in_pass) {                 /* (a fill outside the UI's frame: boot, OTA, UBOOT) its own pass */
        if (!mir_rect_new(&s, x, y, w, h, mir.mode) || !mir_end_pass())
            mir_skip();
        return;
    }
    if (!mir.send) {
        mir_stale(x, y, w, h);
        mir_skip();
        return;
    }
    m0 = mir.msgs;
    if (!mir_rect_new(&s, x, y, w, h, mir.mode))
        mir_drop();
    else if (mir.msgs != m0)
        mir.sent = 1;
}

/* main.c, around ui_draw: a pass goes out at most every MIR_MIN_MS and only with three quarters of the ring free (the
 * link has caught up: what goes is fresh), and then only what the host does not have (mir_rect_new: a page redrawing
 * what it already showed costs nothing). A draw not sent leaves its blocks stale; the next draw of them that goes out
 * sends them (a page that keeps redrawing heals by itself). Blocks still stale once the drawing settles (150 ms without
 * a skipped draw) or after a second of it owe a whole redraw (ui.force), with half the ring free, at most every
 * MIR_FORCE_GAP_MS. It too sends only what the host lacks, so it gets through at any link rate, if need be over a few
 * (what each got into the ring stays known); ordinary passes go on meanwhile */
static void mir_owe_all(void)           /* FG_SCREEN op 2, a mode change, a new lease: the whole screen at the next pass */
{
    mir_forget();
    mir_srall = 1;
    mir_skip();
    mir.dirty_t0 = fm1_ms - 1000u;
    mir.skip_t = fm1_ms - 1000u;
    mir.force_t = fm1_ms - MIR_FORCE_GAP_MS;
}

static void mir_pass_begin(void)
{
    uint32_t now = fm1_ms;
    mir_check();
    mir.in_pass = 1;
    mir.sent = 0;
    mir.forced = 0;
    mir.send = 0;
    if (!mir_live())
        return;
    if (now - mir.roll_t >= MIR_ROLL_MS) {
        mir.roll_t = now;
        mir.roll = (uint8_t)((mir.roll + 1u) % (mir.mode ? 120u : 240u));
        memset(mir_h + mir.roll * MIR_NB, 0, MIR_NB * sizeof mir_h[0]);
    }
    if (mir.dirty && !mir_stale_any())
        mir.dirty = 0;                  /* (the stale rects were drawn again and sent) */
    if (mir.dirty && (now - mir.skip_t >= 150u || now - mir.dirty_t0 >= 1000u) &&
        now - mir.force_t >= MIR_FORCE_GAP_MS && mir_room() >= MIR_RING / 2u) {
        ui.force = 1;
        mir.forced = 1;
        mir.whole = 1;
        mir.dirty = 0;
        mir.send = 1;
        mir.force_t = now;              /* (mir_wait: the forced pass's time from here) */
        return;
    }
    if (now - mir.last_pass >= MIR_MIN_MS && mir_room() >= MIR_RING * 3u / 4u)
        mir.send = 1;
}

static void mir_pass_end(void)
{
    if (mir.sent) {
        if (!mir_end_pass())
            mir_skip();
        mir.last_pass = fm1_ms;
        mir.passes++;
    }
    if (mir.forced && mir.whole)        /* the whole screen went: nothing is stale */
        mir_stale_clear();
    mir.in_pass = 0;
    mir.send = 0;
    mir.forced = 0;
}

/* ---- the FORGE extension's commands this firmware answers (editor.c ed_handle); others get no reply ---- */
enum { FG_ID = 112, FG_WATCH = 118, FG_SCREEN = 122 };
#define FG_FIRMWARE 3u                  /* FG_ID: 1 FORGE ONE, 2 Forge Phoenix, 3 Forge Five */
#define FG_LINEAGE 1u                   /* .. 0 Felucca, 1 Clipper (Forge Five is built on Clipper) */
#define FG_CAPS (2u | 32u | 64u)        /* FG_ID caps: bit 1 the screen mirror, bit 5 its leases and FG_RECT's mode bit,
                                         * bit 6 it heals its own losses (no op 2 needed after drops). No generator,
                                         * input or playhead (bits 0, 2, 3) */
#define FG_WATCH_BITS 8u                /* FG_WATCH: the screen only */

static void fg_u32(uint32_t v)          /* 5 bytes, 7 bits each, low first */
{
    uint32_t i;
    for (i = 0; i < 5u; i++, v >>= 7)
        ed_b(v & 0x7Fu);
}

static int fg_handle(uint32_t cmd, const uint8_t *a, uint32_t na)
{
    switch (cmd) {
    case FG_ID:                         /* -> firmware, lineage, extension version (1), caps v14, name */
        ed_b(FG_FIRMWARE);
        ed_b(FG_LINEAGE);
        ed_b(1);
        ed_v(FG_CAPS);
        ed_str(FELUCCA_VERSION, 24);
        return 1;
    case FG_WATCH: {                    /* mask [, which] -> the mask now set (bit 3: the screen's pushes, the only bit
                                         * this firmware takes). With which: only its bits change */
        uint32_t which = na >= 2u ? a[1] & 15u : 15u, m;
        m = (na >= 1u ? ((uint32_t)mir.watch & ~which) | (a[0] & which) : mir.watch) & FG_WATCH_BITS;
        mir.watch = (uint8_t)m;
        ed_b(m & 15u);
        return 1;
    }
    case FG_SCREEN: {                   /* op [, mode [, host]] -> op, rc (op 3: then msgs, bytes, drops, passes, mode,
                                         * leases). host 1..6: that app's lease (op 1 takes it, op 0 gives it back,
                                         * any FG_SCREEN with it renews it); none: as FP 0.5.7 (op 0 turns it off) */
        uint32_t op = na ? a[0] : 0xFFu, rc = 0, h = na >= 3u ? a[2] : 0u;
        if (op > 3u || h >= MIR_HOSTS || (op == 1u && (na < 2u || a[1] > 1u))) {
            rc = 1;
        } else {
            if (h && (mir.lease >> h & 1u))
                mir.lease_t[h] = fm1_ms;
            if (op == 0u) {
                mir.lease = h ? (uint8_t)(mir.lease & ~(1u << h)) : 0u;
                if (!mir.lease)
                    mir.on = 0;
            } else if (op == 1u) {
                if (!mir.on) {
                    mir.resets = usb.resets;
                    mir.dirty = 0;
                    mir_forget();
                    mir_stale_clear();
                } else if (mir.mode != a[1] || !(mir.lease >> h & 1u)) {
                    mir_owe_all();      /* another app's mode, or another app: the whole screen again */
                }
                mir.lease |= (uint8_t)(1u << h);
                mir.lease_t[h] = fm1_ms;
                mir.on = 1;
                mir.mode = a[1];
            } else if (!mir.on) {
                rc = 2;                 /* op 2, op 3: off */
            } else if (op == 2u) {      /* the whole screen at the next pass, all of it (the hash table cleared) */
                mir_owe_all();
            }
        }
        ed_b(op & 0x7Fu);
        ed_b(rc);
        if (op == 3u) {
            fg_u32(mir.msgs);
            fg_u32(mir.bytes);
            fg_u32(mir.drops);
            fg_u32(mir.passes);
            ed_b(mir.mode);
            ed_b(mir.lease & 0x7Fu);
        }
        return 1;
    }
    default:
        return 0;
    }
}
#endif
