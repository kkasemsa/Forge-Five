/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Khantipol Kasemsant; built on Clipper (isod89), SLOOP, Felucca (C) 2026 Leo Kuroshita, Hügelton Instruments */
/* Forge Five: what the rest of Clipper references of the engines the build leaves out (FORGE_FIVE, core.h FF_ENG),
 * as data without behaviour: the FM6 patch bank (eng_fm6.c, fm6_bank.c: never scanned, never written, no slot used),
 * the FMX patch type and its per-part state (eng_fmx.c: the UNDO history and the editor carry one; byte for byte the
 * record the stores keep). The engines themselves are ENG_OFF (eng_off.c). Included by eng_off.c. */

/* ---- FM6 (eng_fm6.c, fm6_core.c, fm6_bank.c) ---- */
#define FM6_BANK_N 27u           /* patch bank slots (fm6_bank.c) */
#define FM6_PACKED 128u
#define FM6_ERASE 0xFFu
static uint8_t fm6_pend;                            /* bit per part: a sound's patch waiting for the fade (never set) */
static volatile uint8_t fmx_reload;                 /* bit p, part p's FMX patch from its PTCH again (never set) */
typedef struct {
    uint32_t magic;
    uint16_t ver, nslot;
    uint32_t used;
    uint32_t rsv;
    uint8_t v[FM6_BANK_N][FM6_PACKED];
} fm6_bank_t;
_Static_assert(sizeof(fm6_bank_t) == 3472u, "FM6 bank layout");
static int8_t fm6_bank_cur = -1;                    /* no valid bank copy */
static void fm6_track_loaded(const track_t *t) { (void)t; }
static void fm6_init(void) {}
static void fm6_poll(void) {}
static void fm6_bank_scan(void) { fm6_bank_cur = -1; }
static void fm6_bank_changed(void) {}
static int fm6_bank_has(uint32_t k) { (void)k; return 0; }

/* ---- FMX (eng_fmx.c) ---- */
#define FMX_NOP 6u
#define FMX_NFACTORY 16u
#define FMX_NSLOT (FMX_NFACTORY + FM6_BANK_N)
enum { FXM_PM, FXM_FM, FXM_RING, FXM_N };
#define FXM_FIX 4u
enum { FXW_SINE, FXW_TRI, FXW_SAW, FXW_SQR, FXW_HALF, FXW_ABS, FXW_QUART, FXW_PULSE, FXW_SIN8, FXW_NOISE,
       FXW_SAW8, FXW_SQR8, FXW_PULS8 };
typedef struct {
    uint8_t wave, mode, coarse, fine, level;
    uint8_t adsr[4];
    uint8_t vel, ksc, dtn;
} fmx_op_t;
typedef struct {
    fmx_op_t op[FMX_NOP];
    uint8_t mod[FMX_NOP][FMX_NOP];
    uint8_t out[FMX_NOP];
    uint8_t alg;
    uint8_t peg, ptime;
    uint8_t level;
    char name[10];
} fmx_patch_t;
_Static_assert(sizeof(fmx_patch_t) == 128u, "an FMX patch fills an FM6 bank record");
static const char *const N_FMX_WAVE[] = {"SINE", "TRI", "SAW", "SQR", "HALF", "ABS", "QUART", "PULSE", "SIN8", "NOISE",
                                         "SAW8", "SQR8", "PULS8"};
static const char *const N_FMX_MODE[] = {"PM", "FM", "RING"};
static const char *const N_FMX_FIX[] = {"RATIO", "FIX"};
static fmx_patch_t fmx_cur[NPART];
static volatile uint8_t fmx_dirty[NPART];
static int16_t fmx_slot_seen[NPART];
static int (*const fmx_bank_store)(uint32_t slot, const uint8_t *pk) = 0;   /* (ui_synth.c X_FSTORE: "NO FLASH") */

/* ---- names other code points at (icons.c) ---- */
static const char *const N_TRIO_MODE[] = {"LP", "BP", "HP"};
