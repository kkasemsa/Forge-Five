/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 the Forge Phoenix contributors; Forge Five's port */
/* Forge Five: the screen mirror (firmware/src/fg_mirror.c, the FORGE extension FG_ID, FG_WATCH, FG_SCREEN, FG_RECT) on
 * the real UI of tests/ui_pages_test.c (the audio between frames, as on the device), through the mirror's real ring,
 * pace, drops, owed redraws and commands, with a link that drains the ring as usb.c ep1_tx does at a given rate and a
 * clock that moves as the main loop's would (the panel's DMA at 12 MHz SPI). The host side decodes FG_RECT from the
 * format alone (web/EDITOR_PROTOCOL.md) and behaves as an app does (a renewal every 1.5 s; a whole redraw every 10 s,
 * or, as the older apps, also after each drop).
 *   commands   FG_ID (firmware 3, lineage 1, caps 98, the version string), FG_WATCH (bit 3 only; which), FG_SCREEN
 *              (bad arguments, op 3 while off, two leases, one given back, one expiring, a host-less op 0, op 3's
 *              counters, mode and leases, a mode change by the other host: bit 2 on every message after it)
 *   pages      every page of a walk (HOME, the families, the menu, the visualiser), both modes: the host's picture is
 *              the panel's after each (mode 0: every pixel; mode 1: every half pixel but the part means at odd edges,
 *              which a whole redraw on an idle link gives as well)
 *   load       HOME with the arpeggiator playing a held chord, and the visualiser (it draws every frame), both modes,
 *              96 and 54 KB/s, both app rules: KB/s, pictures a second, bytes a picture, drops (printed); after the
 *              load stops, the host's picture is the panel's
 * FF_SCREEN_PPM=1: the host's and the panel's pictures of each load run to build/host/ff_screen_*.ppm. */
#include <stdint.h>
static void ff_mir_blit(uint32_t x, uint32_t y, uint32_t r0);
static void ff_mir_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c);
static void ld_sync(void);
#define MIR_BLIT(x, y, r0) ff_mir_blit(x, y, r0)
#define MIR_FILL(x, y, w, h, c) ff_mir_fill(x, y, w, h, c)
#define UI_SYNC_HOOK() ld_sync()
#define UI_PAGES_HARNESS_ONLY
#include "ui_pages_test.c"

/* ---- the device's side the mirror needs (felucca.c has them from lcd.c, editor.c) ---- */
static uint64_t t_us;                   /* the clock: fm1_ms follows it */
static uint64_t dma_end;                /* the panel's DMA busy until */
static uint8_t lcd_busy;
static uint32_t fm1_lcd_done(void) { return t_us >= dma_end; }
static struct { uint32_t last_ms; } ed_w;
static uint8_t ed_out[256];
static uint32_t ed_n;
static void ed_b(uint32_t v) { if (ed_n < sizeof ed_out) ed_out[ed_n++] = (uint8_t)(v & 0x7Fu); }
static void ed_v(int32_t v) { uint32_t u = (uint32_t)(v + 8192); ed_b(u); ed_b(u >> 7); }
static void ed_str(const char *s, uint32_t max)
{
    uint32_t i;
    for (i = 0; s && s[i] && i < max; i++)
        ed_b((uint8_t)s[i] & 0x7Fu);
    ed_b(0);
}
static void link_run(uint32_t us);
static void adv(uint32_t us)
{
    t_us += us;
    fm1_ms = (uint32_t)(t_us / 1000u);
    link_run(us);
}
static void ld_dma(uint32_t px)         /* 12 MHz SPI: 16 bits a pixel, 4/3 us */
{
    if (dma_end < t_us)
        dma_end = t_us;
    dma_end += (uint64_t)px * 4u / 3u;
    lcd_busy = 1;
}
static void ld_sync(void)               /* lcd.c lcd_sync: the next access waits for the running transfer */
{
    if (t_us < dma_end)
        adv((uint32_t)(dma_end - t_us));
    lcd_busy = 0;
}
static void mir_take(void) { adv(50); }
#define MIR_WAIT_HOOK() mir_take()
#include "../firmware/src/fg_mirror.c"

static void ff_mir_blit(uint32_t x, uint32_t y, uint32_t r0)   /* gfx.c cv_blit: the DMA starts, then the mirror */
{
    ld_sync();
    ld_dma(cv_w * (cv_h > r0 ? cv_h - r0 : 0u));
    mir_blit(x, y, r0);
}
static void ff_mir_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)   /* lcd.c lcd_fill */
{
    mir_fill(x, y, w, h, c);
    ld_sync();
    ld_dma(w * h);
}

/* ---- the host: the link, the decoder, the app ---- */
static uint32_t link_bps;               /* payload bytes a second (a USB-MIDI event: up to 3) */
static double link_credit;
static uint8_t mw[4096];
static uint32_t mw_n;
static uint16_t dimg[240 * 240];        /* the host's picture (mode 1: 120 x 120 in its corner) */
static uint32_t h_passes, h_msgs, h_bytes, h_bad, h_mode, h_mode_msgs[2];

static void h_message(const uint8_t *m, uint32_t n)   /* one F0..F7 FG_RECT onto dimg, from the format alone */
{
    uint32_t x, y, w, h, row, flags, rn = 0, k, q, side;
    static uint8_t rle[512];
    h_msgs++;
    h_bytes += n;
    if (n < 17u || m[0] != 0xF0 || m[1] != 0x7D || m[2] != 0x46 || m[3] != 0x4C || m[4] != 123u || m[n - 1u] != 0xF7) {
        h_bad++;
        return;
    }
    x = m[5] | m[6] << 7; y = m[7] | m[8] << 7; w = m[9] | m[10] << 7;
    h = m[11] | m[12] << 7; row = m[13] | m[14] << 7; flags = m[15];
    for (k = 16u; k < n - 1u;) {
        uint32_t b = m[k++], g = n - 1u - k > 7u ? 7u : n - 1u - k, t;
        for (t = 0; t < g && rn < sizeof rle; t++) rle[rn++] = (uint8_t)(m[k++] | ((b >> t) & 1u) << 7);
    }
    h_mode = (flags & 4u) != 0;
    h_mode_msgs[h_mode]++;
    side = h_mode ? 120u : 240u;
    if (rn > 256u || w > 120u || x + w > side || y + h > side) { h_bad++; return; }
    q = row * w;
    for (k = 0; k < rn;) {
        uint32_t c = rle[k++], cnt = c < 64u ? c + 1u : c - 63u, t;
        for (t = 0; t < cnt; t++, q++) {
            uint32_t o = c < 64u ? k + 2u * t : k;
            if (w && q < w * h)
                dimg[(y + q / w) * 240u + x + q % w] = (uint16_t)(rle[o] << 8 | rle[o + 1u]);
            else
                h_bad++;
        }
        k += c < 64u ? 2u * cnt : 2u;
    }
    if (flags & 2u)
        h_passes++;
}
static void h_bytes_in(const uint8_t *b, uint32_t n)   /* USB-MIDI events of the ring */
{
    uint32_t i;
    for (i = 0; i < n; i += 4u) {
        uint32_t cin = b[i] & 15u, k = cin == 4u || cin == 7u ? 3u : cin == 6u ? 2u : cin == 5u ? 1u : 0u, j;
        for (j = 0; j < k; j++) {
            uint8_t v = b[i + 1u + j];
            if (v == 0xF0) mw_n = 0;
            if (mw_n < sizeof mw) mw[mw_n++] = v;
            if (v == 0xF7) { h_message(mw, mw_n); mw_n = 0; }
        }
    }
}
static void link_run(uint32_t us)       /* usb.c ep1_tx, a packet at a time while the link has room */
{
    link_credit += (double)us * link_bps / 1e6;
    for (;;) {
        uint8_t b[64];
        uint32_t n;
        if (link_credit < 48.0)
            return;
        n = mir_drain(b, 0, 1);
        if (!n) {
            if (link_credit > 48.0) link_credit = 48.0;
            return;
        }
        h_bytes_in(b, n);
        link_credit -= 48.0;
    }
}

/* a FORGE request, answered as editor.c ed_handle does: the reply's arguments in ed_out[0 .. ed_n); 0: no reply */
static int req(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    ed_n = 0;
    ed_w.last_ms = fm1_ms;
    return fg_handle(cmd, a, n);
}
static int screen_op(uint32_t op, uint32_t mode, uint32_t host)
{
    uint8_t a[3] = {(uint8_t)op, (uint8_t)mode, (uint8_t)host};
    return req(FG_SCREEN, a, host ? 3u : 2u);
}

/* ---- the device's main loop (main.c), a frame ---- */
static void dev_frame(void)
{
    uint64_t t0 = t_us;
    uint32_t q;
    static int32_t o[CTL * 2];
    for (q = 0; q < 22u; q++) {                 /* the audio ISR's blocks (as tests/ui_pages_test.c frame) */
        uint32_t i;
        mix_block(o, CTL);
        for (i = 1; i < CTL; i += 2u) {
            scope_bufr[scope_w & (SCOPE_N - 1u)] = vis_tap[2u * i + 1u];
            scope_buf[scope_w++ & (SCOPE_N - 1u)] = vis_tap[2u * i];
        }
    }
    ed_w.last_ms = fm1_ms;
    ui_input();
    ui_leds();
    mir_pass_begin();
    ui_draw();
    mir_pass_end();
    ld_sync();
    adv(1000);                                  /* (the UI's own work, roughly) */
    while (t_us - t0 < 16000u)
        adv(250);
}
static void dev_frames(uint32_t n) { while (n--) dev_frame(); }

static uint8_t wmask[240 * 240], wref[240 * 240];
static uint32_t wrong_pixels(uint8_t *mk)       /* the host's picture vs the panel (mode 1: vs the mean of each 2 x 2) */
{
    uint32_t k, n = 0;
    memset(mk, 0, 240u * 240u);
    if (!h_mode) {
        for (k = 0; k < 240u * 240u; k++) {
            uint16_t v = (uint16_t)(screen[k] >> 8 | screen[k] << 8);
            n += mk[k] = dimg[k] != v;
        }
    } else {
        uint32_t x, y;
        for (y = 0; y < 120u; y++)
            for (x = 0; x < 120u; x++) {
                uint32_t sr = 0, sg = 0, sb = 0, dx, dy;
                for (dy = 0; dy < 2u; dy++)
                    for (dx = 0; dx < 2u; dx++) {
                        uint16_t v = screen[(2u * y + dy) * 240u + 2u * x + dx];
                        v = (uint16_t)(v >> 8 | v << 8);
                        sr += v >> 11; sg += (v >> 5) & 63u; sb += v & 31u;
                    }
                k = y * 240u + x;
                n += mk[k] = dimg[k] != (uint16_t)(((sr + 2u) / 4u) << 11 | ((sg + 2u) / 4u) << 5 | ((sb + 2u) / 4u));
            }
    }
    return n;
}
/* the pixels that differ from what a whole redraw over an idle link gives (mode 1: the part means at odd edges are
 * inherent); *inherent: how many those are */
static uint32_t wrong_vs_whole(uint32_t *inherent)
{
    uint32_t k, n = 0, keep = link_bps;
    wrong_pixels(wmask);
    link_bps = 10000000u;
    screen_op(2, 0, 1);
    dev_frames(40);
    *inherent = wrong_pixels(wref);
    for (k = 0; k < 240u * 240u; k++)
        n += wmask[k] != wref[k];
    link_bps = keep;
    return n;
}
static void dump_ppm(const char *path, const uint16_t *img, uint32_t side, int swap)
{
    FILE *f = fopen(path, "wb");
    uint32_t x, y;
    if (!f) return;
    fprintf(f, "P6\n%u %u\n255\n", side, side);
    for (y = 0; y < side; y++)
        for (x = 0; x < side; x++) {
            uint16_t v = img[y * 240u + x];
            if (swap) v = (uint16_t)(v >> 8 | v << 8);
            fputc(((v >> 11) & 31u) * 255u / 31u, f);
            fputc(((v >> 5) & 63u) * 255u / 63u, f);
            fputc((v & 31u) * 255u / 31u, f);
        }
    fclose(f);
}

/* the mirror as an app starts it (FP 0.5.9 session: op 1 with its host id, FG_WATCH 8, 8, op 2) */
static void session(uint32_t mode, uint32_t bps)
{
    uint8_t a[2] = {8, 8};
    memset(&mir, 0, sizeof mir);
    mir_qr = mir_qw = 0; mir_open = 0;
    mir_stale_clear();
    usb.config = 1; usb.suspended = 0;
    t_us = (uint64_t)fm1_ms * 1000u; dma_end = 0;
    link_bps = bps; link_credit = 0;
    memset(dimg, 0, sizeof dimg);
    h_passes = h_msgs = h_bytes = h_bad = 0; mw_n = 0; h_mode = mode;
    h_mode_msgs[0] = h_mode_msgs[1] = 0;
    screen_op(1, mode, 1);
    req(FG_WATCH, a, 2);
    screen_op(2, 0, 1);
}

static void home_quiet(void)
{
    fm1_in.notes = 0;
    vis_on = 0;
    ui.menu = 0;
    go_home();
}

/* ---- the load runs ---- */
typedef struct { double kbps, fps, bpp; uint32_t drops, wrong, inherent; } run_t;
static int app_new;
static uint32_t app_ticks, app_drops;
static uint64_t app_next;
static void app_look(void)              /* the app's 1.5 s renewal and its redraw rule */
{
    if (t_us < app_next)
        return;
    app_next = t_us + 1500000u;
    screen_op(3, 0, 1);
    if (app_drops != 0xFFFFFFFFu && ((!app_new && mir.drops > app_drops) || ++app_ticks >= 7u)) {
        app_ticks = 0;
        screen_op(2, 0, 1);
    }
    app_drops = mir.drops;
}
static void page_load(uint32_t page)    /* 0: HOME, the arp over a held chord; 1: the visualiser */
{
    track_t *t;
    home_quiet();
    dev_frames(4);
    t = TSEL;
    t->p[P_AMODE] = 1; t->p[P_ARATE] = 2; t->p[P_AOCT] = 2; t->p[P_AGATE] = 64;
    fm1_in.notes |= 1u << 0 | 1u << 4 | 1u << 7;
    if (page == 1u)
        vis_open();
    ui.force = 1;
}
static void page_unload(void)           /* the keys let go; the sound dies away */
{
    fm1_in.notes = 0;
}
static run_t load_run(uint32_t page, uint32_t mode, uint32_t bps)
{
    run_t r;
    uint32_t i, b0, p0, d0;
    uint64_t t0;
    memset(&r, 0, sizeof r);
    page_load(page);
    session(mode, bps);
    app_ticks = 0; app_drops = 0xFFFFFFFFu; app_next = 0;
    for (i = 0; i < 60u; i++) { dev_frame(); app_look(); }          /* 1 s to settle */
    b0 = h_bytes; p0 = h_passes; d0 = mir.drops; t0 = t_us;
    while (t_us - t0 < 5000000u) { dev_frame(); app_look(); }       /* 5 s measured */
    r.kbps = (h_bytes - b0) / 5000.0;
    r.fps = (h_passes - p0) / 5.0;
    r.bpp = h_passes > p0 ? (double)(h_bytes - b0) / (h_passes - p0) : 0;
    r.drops = mir.drops - d0;
    page_unload();
    t0 = t_us;
    while (t_us - t0 < 4000000u) { dev_frame(); app_look(); }       /* the load stops: 4 s more as the app runs */
    if (getenv("FF_SCREEN_PPM")) {
        char p[160];
        snprintf(p, sizeof p, "build/host/ff_screen_%s_m%u_%u_host.ppm", page ? "VIS" : "HOME", mode, bps / 1000u);
        dump_ppm(p, dimg, h_mode ? 120u : 240u, 0);
        snprintf(p, sizeof p, "build/host/ff_screen_%s_m%u_%u_panel.ppm", page ? "VIS" : "HOME", mode, bps / 1000u);
        dump_ppm(p, screen, 240u, 1);
    }
    r.wrong = wrong_vs_whole(&r.inherent);
    vis_on = 0;
    return r;
}

static uint32_t rd5(const uint8_t *p) { uint32_t v = 0, i; for (i = 0; i < 5u; i++) v |= (uint32_t)p[i] << (7u * i); return v; }

int main(int argc, char **argv)
{
    uint32_t i, m, inh;
    outdir = argc > 1 ? argv[1] : "build/host";
    bank_resolve();
    panel = PANEL_DEFAULT;
    layers_init();
    host_tracks_init();
    for (i = 0; i < NPART; i++) { set_engine_of(&trk[i], TRK_DEF[i][0]); apply_preset_to(&trk[i], TRK_DEF[i][1]); trk[i].engine = trk[i].eng_req; }
    song.g[G_BPM] = 120;
    fm1_ms = 1000;
    t_us = 1000000u;
    usb.config = 1;
    go_home(); ui.force = 1; dev_frames(3);

    /* ---- the commands ---- */
    {
        uint8_t a[3] = {0, 0, 0};
        check(req(FG_ID, a, 0) && ed_n >= 6u && ed_out[0] == 3u && ed_out[1] == 1u && ed_out[2] == 1u &&
              (ed_out[3] | ed_out[4] << 7) == 98u + 8192u && !strcmp((const char *)ed_out + 5, FELUCCA_VERSION),
              "FG_ID: firmware 3 (Forge Five), lineage 1 (Clipper), version 1, caps 98 (screen, leases, heals), the name");
        check(!req(113, a, 0) && !req(116, a, 0) && !req(121, a, 0) && !req(127, a, 0),
              "FG_GEN, FG_ACT, FG_PLAYHEAD and the rest of 112..127: not this firmware's (no reply)");
        a[0] = 15; check(req(FG_WATCH, a, 1) && ed_out[0] == 8u, "FG_WATCH 15: only the screen's bit 3 is taken");
        a[0] = 0; a[1] = 7; check(req(FG_WATCH, a, 2) && ed_out[0] == 8u, "FG_WATCH 0, 7: bit 3 stays (another host's bits only)");
        a[0] = 0; a[1] = 8; check(req(FG_WATCH, a, 2) && ed_out[0] == 0u, "FG_WATCH 0, 8: the screen's bit cleared");
        check(screen_op(3, 0, 1) && ed_out[0] == 3u && ed_out[1] == 2u, "FG_SCREEN op 3 while off: rc 2");
        check(screen_op(2, 0, 1) && ed_out[1] == 2u, "FG_SCREEN op 2 while off: rc 2");
        check(screen_op(4, 0, 0) && ed_out[1] == 1u && screen_op(1, 2, 1) && ed_out[1] == 1u &&
              screen_op(1, 0, 7) && ed_out[1] == 1u && !mir.on, "FG_SCREEN: op 4, mode 2, host 7: rc 1, nothing on");
        check(screen_op(1, 1, 1) && ed_out[1] == 0u && mir.on && mir.mode == 1u && mir.lease == 2u, "op 1, mode 1, host 1: on, its lease");
        check(screen_op(1, 1, 2) && mir.lease == 6u, "op 1, host 2: a second lease");
        check(screen_op(0, 0, 1) && mir.on && mir.lease == 4u, "op 0, host 1: its lease given back, host 2's keeps it on");
        check(screen_op(3, 0, 2) && ed_n == 2u + 4u * 5u + 2u && ed_out[22] == 1u && ed_out[23] == 4u,
              "op 3: counters, then the mode (1) and the leases (bit 2)");
        fm1_ms += 6000u;
        mir_check();
        check(!mir.on && !mir.lease, "host 2 silent for 6 s: its lease ended, the mirror off");
        check(screen_op(1, 0, 0) && mir.on && mir.lease == 1u, "a host-less op 1 (an FP 0.5.7 app): lease 0");
        check(screen_op(0, 0, 0) && !mir.on && !mir.lease, "a host-less op 0: off");
        usb.config = 1;
    }

    /* ---- every page of a walk, both modes: the host's picture is the panel's ---- */
    for (m = 0; m < 2u; m++) {
        static const char *NAME[] = {"HOME", "ENV", "EDIT", "FX", "GLOBAL", "SCALE", "SEQ", "MENU", "ABOUT", "VISUALISER"};
        uint32_t pg, bad = 0, worst = 0;
        home_quiet(); dev_frames(3);
        session(m, 96000u);
        for (pg = 0; pg < sizeof NAME / sizeof NAME[0]; pg++) {
            uint32_t w;
            home_quiet();
            switch (pg) {
            case 1: open_family(FAM_ENV); break;
            case 2: open_family(FAM_EDIT); break;
            case 3: open_family(FAM_FX); break;
            case 4: open_family(FAM_GLO); break;
            case 5: open_family(FAM_SCL); break;
            case 6: open_family(FAM_SEQ); break;
            case 7: ui.menu = 1; break;
            case 8: ui.menu = 2; break;
            case 9: vis_open(); break;
            }
            ui.force = 1;
            dev_frames(30);
            w = wrong_vs_whole(&inh);
            if (w) { printf("ff screen: mode %u, %s: %u pixels not the panel's\n", m, NAME[pg], w); bad++; }
            if (w > worst) worst = w;
            dev_frames(2);
        }
        {
            char what[120];
            snprintf(what, sizeof what, "mode %u: a walk over %u pages, the host's picture the panel's after each",
                     m, (unsigned)(sizeof NAME / sizeof NAME[0]));
            check(!bad, what);
        }
        check(h_bad == 0u && h_mode_msgs[!m] == 0u, m ? "mode 1: every message well formed, flags bit 2 on every one"
                                                      : "mode 0: every message well formed, flags bit 2 on none");
        h_mode_msgs[0] = h_mode_msgs[1] = 0;
        vis_on = 0; ui.menu = 0;
    }

    /* ---- a mode change by the other host: the whole screen again, in the new mode ---- */
    {
        uint32_t m0;
        home_quiet(); dev_frames(3);
        session(0, 96000u);
        dev_frames(20);
        screen_op(1, 0, 2);                         /* (another app: host 2, same mode) */
        h_mode_msgs[0] = h_mode_msgs[1] = 0;
        screen_op(1, 1, 2);                         /* .. now mode 1 */
        dev_frames(30);
        m0 = h_mode_msgs[1];
        check(mir.mode == 1u && m0 > 0u && h_mode_msgs[0] == 0u && wrong_vs_whole(&inh) == 0u,
              "the other host's mode 1: every message after it has bit 2, the host's picture whole in mode 1");
    }

    /* ---- under load ---- */
    {
        static const uint32_t BPS[2] = {96000u, 54000u};
        uint32_t p, b, ap;
        for (ap = 0; ap < 2u; ap++) {
            app_new = (int)ap;
            printf("ff screen load, %s:\n", ap ? "an app that knows caps bit 6 (op 2 every 10 s only)"
                                              : "the older apps (op 2 after drops and every 10 s)");
            printf("ff screen load: page  mode link   KB/s   fps  B/pass  drops  wrong (part means)\n");
            for (p = 0; p < 2u; p++)
                for (m = 0; m < 2u; m++)
                    for (b = 0; b < 2u; b++) {
                        char what[160];
                        run_t r = load_run(p, m, BPS[b]);
                        printf("ff screen load: %-5s %u    %3u   %5.1f %5.1f %7.0f %5u  %5u (%u)\n", p ? "VIS" : "HOME",
                               m, BPS[b] / 1000u, r.kbps, r.fps, r.bpp, r.drops, r.wrong, r.inherent);
                        snprintf(what, sizeof what, "%s, mode %u, %u KB/s, app rule %u: after the load the host's picture is the panel's",
                                 p ? "VIS" : "HOME", m, BPS[b] / 1000u, ap);
                        check(r.wrong == 0u && (m || r.inherent == 0u), what);
                    }
        }
    }

    /* ---- the counters, and off ---- */
    {
        check(screen_op(3, 0, 1) && rd5(ed_out + 2) == mir.msgs && rd5(ed_out + 12) == mir.drops &&
              rd5(ed_out + 17) == mir.passes && mir.msgs > 0u, "op 3: messages, bytes, drops, passes as counted");
        usb.resets++;
        mir_check();
        check(!mir.on, "a USB reset: the mirror off");
    }
    printf("ff screen: %s\n", fails ? "FAILED" : "all ok");
    return fails != 0;
}
