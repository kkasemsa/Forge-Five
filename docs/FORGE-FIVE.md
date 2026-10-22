# Forge Five

Classic synths with their real front panels, on four MIDI channels. A fork of [Felucca](https://github.com/hugelton/Felucca)
(GPL-3.0) for the M-VAVE FM-1, made to be played from Ableton Move. Version FF 0.2: the LADDER and SEM engines, the
engine patch, the MULTI page, per-part VOICES and the cost-unit voice allocator, CC16–19 macros. Not yet run on hardware.

## The Multi

GLO opens **MULTI** first (then SONG): the four parts as cards, one knob each: ENG (the engine, in PRESETS order),
MIDI CH (1..16, OMNI, OFF; a turn puts MENU > MIDI > MIDI IN on TRACK), VOICES (ALL or 1..8), LEVEL. ALGORITHM steps
the part. Projects keep all of it. At power-on the parts listen on channels 1..4 (MIDI IN CH1-4).

**VOICES** (also on VOICE 3) is the part's, not the sound's: a preset or engine load keeps it, and it is never
automated. ALL = the engine's own cap (LADDER 4, the others 8).

**The voice budget** is 8 units, not 8 voices: a Felucca engine's voice costs 1.0, SEM's 1.5, LADDER's 2.5. A new
note that does not fit steals the lightest voices it needs (a MONO / LEGATO / UNISON part's held note is protected,
as in Felucca). Six ANALOG notes and a LADDER bass fit (6.0 + 2.5 > 8: one ANALOG voice gives way); three LADDER
notes and two ANALOG ones fit exactly.

## LADDER engine

A three-oscillator monosynth engine in the manner of the classic 1970s American portable synth, engine 14.
Source: `firmware/src/eng_ladder.c`. Test: `tests/ladder_test.c`. Demos: `build/ladder_demo/` after the tests run.

### Pages (EDIT button, in this order; the five after CONTOUR are the engine patch's XP 1..5)

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| FILTER (EDIT 1) | CUTOFF | EMPH (sings from ~106) | CONTOR (contour amount) | KBD (OFF 1/3 2/3 FULL) |
| CONTOUR (EDIT 2) | F ATK | F DEC | F SUS | DECAY (switch: release = decay) |
| OSC 1-2 | O1 RNG (LO 32' 16' 8' 4' 2') | O1 WAV (TRI SHARK SAW SQR WIDE NARW) | O2 RNG | O2 WAV |
| OSC 2-3 | O2 FRQ (±7 st, fine near centre) | O3 RNG | O3 FRQ (at LO: ±3 octaves) | O3 WAV (RSAW instead of SAW) |
| MIX | OSC 1 | OSC 2 | OSC 3 | NOISE |
| MIX 2 | N COL (WHITE PINK) | OVLD (overload) | O3 KBD (OFF = OSC 3 free, the LFO) | DRIFT |
| CTRL | MIX (OSC 3 ↔ NOISE) | WHEEL (the amount) | OSC (to the oscillators) | FILT (to the cutoff) |

Loudness contour = the track's ENV page. GLIDE = VOICE page. HOME knobs: CUTOFF, EMPH, CONTOR, GLIDE.
Mono/legato by default (presets); POLY and UNISON are capped at 4 voices.

Presets: BERLIN BASS, FAT LEAD, PEDAL SUB, SEQ PLUCK, BRASS, WHISTLE, WOBBLE, VIBRATO LEAD, FM GROWL, NOISE RISER,
OVERDRIVE, DRONE, FLUTE, KICK BASS.

## SEM engine

A two-oscillator engine in the manner of the classic 1970s Oberheim module, engine 15: two VCOs (saw, pulse, both,
triangle; 16' 8' 4'), hard sync, X-MOD (OSC 2 into the cutoff), a state-variable filter whose **MODE** knob sweeps
low-pass through notch to high-pass (the SEM's signature), or the 4-pole low-pass and band-pass of its later
siblings. Source: `firmware/src/eng_sem.c`. Test: `tests/sem_test.c`. Demos: `build/sem_demo/`.

### Pages (the four after F ENV are the engine patch's XP 1..4)

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| FILTER (EDIT 1) | CUTOFF | RESO | MODE (LP ← notch → HP; 2P only) | TYPE (2P 4P BP) |
| F ENV (EDIT 2) | F ATK | F DEC | F SUS | F REL |
| OSC 1 | O1 WAV (SAW PLS BOTH TRI) | O1 PW | O1 RNG (16' 8' 4') | OSC 1 (level) |
| OSC 2 | O2 WAV | O2 PW | O2 RNG | OSC 2 (level) |
| MIX | O2 FRQ (±7 st) | DETUNE (cents) | SYNC (OFF ON) | NOISE |
| MOD | F AMT (envelope → cutoff, ±) | KBD (OFF HALF FULL) | X-MOD | PWM (the LFO's) |

Poly by default; a voice costs 1.5 units. HOME knobs: CUTOFF, RESO, MODE, GLIDE.

Presets: OB BRASS, JUMP STAB, OBX PAD, SEM BASS, NOTCH SWEEP, SYNC LEAD, PWM PAD, UNISON LEAD, BP PLUCK,
FILM PAD, POLY BASS, SYNC BELL, TRI ORGAN, DRONE, ARP PLUCK.

## MIDI (from Move)

- Notes on the part's channel (MULTI's MIDI CH; MENU > MIDI > MIDI IN CH1-4 by default).
- CC74 = CUTOFF, CC71 = EMPH / RESO, CC5 = GLIDE, CC72/73/75 = loudness release/attack/decay (as Felucca).
- **CC16–19 = the engine's four HOME knobs** on every engine (LADDER: CUTOFF EMPH CONTOR GLIDE; SEM: CUTOFF RESO MODE
  GLIDE; ANALOG: its own), unless MIDI LEARN has given the CC to something else. Map Move's four macro knobs to
  CC16–19 once and every part answers with its own panel.
- Mod wheel: CC1 on a LADDER part drives WHEEL directly (the larger of the knob and the wheel counts).

## The EDIT layer with 15 engines

EDIT held, the white keys F3..F5 pick the fifteen engines in PRESETS order (ANALOG FM6 LADDER SEM PHASE LOFI SAMPLE
VOICE TRIO WHEEL GRAIN PHYS NOISE SLICE DRUM), **G5 is INIT and F#5 (the black key beside it) is CLEAN INIT**;
the layer draws them as five rows, the CLEAN cell naming its key. Felucca's own layout (INIT E5, CLEAN INIT F5)
is kept for builds with fewer engines.

## Storage

LADDER's 20 and SEM's 16 extra values are their **engine patch** (`firmware/src/engine_xp.c`): a part's engine keeps up
to 64 values of its own in `track_t.xp[]`, edited on pages of its own (XP 1..8, titled by the engine) and stored as a
128-byte 7-bit blob in the slot every project and user preset already has for a part's FM6 patch. An FM6 part stores
its FM6 patch there bit for bit, as before; any other engine with extra values stores its blob. No project or preset
format changed. LADDER's favourites use DIGITAL's retired favourites row; SEM's the upper half of it.

Any engine gets a full panel the same way: a `param_desc_t` table, page titles, a preset-load hook, one line in
`engines.c eng_xp()` and its weight in `eng_weight()`.

## Known limits (FF 0.2)

- The bundled web editor knows each engine's eight EDIT values and presets, not the engine patch (no motion, locks or
  MIDI LEARN on those either). Edit them on the device.
- LADDER's FILT MOD and SEM's X-MOD read their source once a block (32 samples): fine for LFO rates, not audio-rate
  filter FM.
- Voice weights are host-measured ratios; confirm on the device (MENU > ABOUT > CPU) before trusting the 8.0 budget.
- Not yet run on hardware: build, flash and check on a unit with a recovery path ready (see below).

## Building (macOS)

Follow `BUILDING.md` (Docker Desktop, the JieLi toolchain via `tools/get_toolchain.sh`, the AC79 SDK, Python 3 + Pillow), then:

```
./build.sh --release 0.2-ff          # build/felucca-0.2-ff.fwsc (identity FM-1_902)
tests/run_tests.sh                   # host tests, including LADDER's, SEM's, the engine patch's and the Multi's
python3 tools/fm1_install.py --info  # identify the FM-1 (writes nothing)
python3 tools/fm1_install.py build/felucca-0.2-ff.fwsc
```

Before the first flash: keep M-VAVE's V15 file, take the installer's full backup, and have a recovery route
(FM-1 Transporter with a Seeed XIAO RP2040) in case the unit lands in WL80UBOOT mode.
