# LADDER engine (fork of Felucca)

A three-oscillator monosynth engine in the manner of the classic 1970s American portable synth, added to Felucca as engine 14.
Source: `firmware/src/eng_ladder.c`. Test: `tests/ladder_test.c`. Demos: `build/ladder_demo/` after the tests run.

## Pages (EDIT button, in this order; the five after CONTOUR are the engine patch's XP 1..5)

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

## MIDI (from Move)

- Notes on the part's channel (MENU > MIDI > MIDI IN, CH1-4 by default).
- CC74 = CUTOFF, CC71 = EMPH, CC5 = GLIDE, CC72/73/75 = loudness release/attack/decay.
- Mod wheel: CC1 on the part's channel drives WHEEL directly (the larger of the knob and the wheel counts).

## Voices and cost

Mono/legato by default (presets). POLY and UNISON are capped at 4 voices: a voice costs ~2.7× ANALOG's (heaviest settings ~3.3×), about 9 % of the CPU on the device by Felucca's own ratio.

## Presets

BERLIN BASS, FAT LEAD, PEDAL SUB, SEQ PLUCK, BRASS, WHISTLE, WOBBLE, VIBRATO LEAD, FM GROWL, NOISE RISER, OVERDRIVE, DRONE, FLUTE, KICK BASS.

## Storage

LADDER's 20 extra values are its **engine patch** (`firmware/src/engine_xp.c`): a part's engine keeps up to 64 values of
its own in `track_t.xp[]`, edited on pages of its own (XP 1..8, titled by the engine) and stored as a 128-byte 7-bit blob
in the slot every project and user preset already has for a part's FM6 patch. An FM6 part stores its FM6 patch there
bit for bit, as before; any other engine with extra values stores its blob. No project or preset format changed.
LADDER's favourites use DIGITAL's retired favourites row.

Any engine gets a full panel the same way: a `param_desc_t` table, page titles, a preset-load hook, one line in
`engines.c eng_xp()`. The web editor does not know the blob yet.

## Known limits (v0.1)

- The bundled web editor knows LADDER's eight EDIT values, not the 20 of its engine patch (no motion, locks or MIDI LEARN on those either). Edit them on the device.
- FILT MOD reads its source once a block (32 samples): fine for LFO rates, not audio-rate filter FM.
- Not yet run on hardware: build, flash and check on a unit with a recovery path ready (see BUILDING-LADDER below).

## BUILDING-LADDER (macOS)

Follow `BUILDING.md` (Docker Desktop, the JieLi toolchain via `tools/get_toolchain.sh`, the AC79 SDK, Python 3 + Pillow), then:

```
./build.sh --release 0.1-ladder      # build/felucca-0.1-ladder.fwsc
tests/run_tests.sh                   # host tests, including LADDER's
python3 tools/fm1_install.py --info  # identify the FM-1 (writes nothing)
python3 tools/fm1_install.py build/felucca-0.1-ladder.fwsc
```

Before the first flash: keep M-VAVE's V15 file, take the installer's full backup, and have a recovery route
(FM-1 Transporter with a Seeed XIAO RP2040) in case the unit lands in WL80UBOOT mode.
