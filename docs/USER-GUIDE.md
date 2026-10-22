# Forge Five — User Guide (FF 0.2)

Forge Five turns the M-VAVE FM-1 into four classic synths on four MIDI channels, meant to be played from Ableton Move
(or any sequencer). It is Felucca 1.6.1 underneath: everything Felucca does — its engines, effects, sequencer, pages,
menus, web editor — still works and is documented in the
[Felucca wiki](https://github.com/hugelton/Felucca/wiki/Home). This guide covers what Forge Five adds and how to use
the unit as a multitimbral synth module. Installing: [INSTALL.md](INSTALL.md). What changed, technically:
[FORGE-FIVE.md](FORGE-FIVE.md).

## 1. The idea

| | Felucca | Forge Five |
| --- | --- | --- |
| Parts | 4 tracks, one engine each | the same 4 parts, seen as one **Multi** on GLO |
| Engines | 13 | 15: **LADDER** (Model D-style monosynth) and **SEM** (Oberheim-style) added, with full front panels |
| Panels | 8 EDIT values per engine | LADDER 28 values, SEM 24, on their own pages (XP 1..5) |
| Voices | 8, any engine | a budget of 8 units: light engines 1.0, SEM 1.5, LADDER 2.5; VOICES cap per part |
| MIDI | CC map, MIDI LEARN | plus CC16–19 = the engine's four HOME knobs on every part (macros) |

The sequencer, arpeggiator, scales, chords, FX, projects and user presets are Felucca's, untouched.

## 2. Connecting to Move

1. FM-1 to Move by USB (the FM-1 is a class-compliant USB-MIDI device; it also takes TRS MIDI IN).
2. On Move, make up to four MIDI tracks whose output is the FM-1, **channels 1 to 4**: channel 1 plays part 1,
   2 plays part 2, and so on. That is Forge Five's power-on routing (MENU > MIDI > MIDI IN = CH1-4).
3. Want other channels (say 10–13 to keep 1–4 for other gear)? GLO > MULTI, KNOB 2 on each part: the first turn
   switches MIDI IN to TRACK and from then on each part has its own channel (1–16, OMNI, OFF). Projects keep this.
4. Notes, velocity, pitch bend, sustain (CC64) and mod wheel (CC1) work per part. To keep the FM-1's synced LFOs and
   delay in time with Move, set CLK to USB on the CLOCK page (HOME > MIXER > CLOCK) and let Move send clock.

The FM-1's audio goes out of its headphone / line jack; USB audio (the stereo master) is also there if you record on a
computer. Move does not take audio in over USB, so for a jam the FM-1 goes to the mixer beside Move.

## 3. The Multi (GLO)

Press **GLO**: the first page is **MULTI**, the four parts as four cards under the four knobs, one part selected
(the card with the bar). The ALGORITHM knob steps the selected part, or press a track pad.

| Knob | Does |
| --- | --- |
| 1 ENG | the part's engine, in PRESETS order (ANALOG, FM6, LADDER, SEM, PHASE, LOFI, SAMPLE, VOICE, TRIO, WHEEL, GRAIN, PHYS, NOISE, SLICE, DRUM) |
| 2 CH | the part's MIDI channel: 1..16, OMNI, OFF (the card shows CH* until MIDI IN is on TRACK; the first turn puts it there) |
| 3 VOICES | ALL or 1..8: the most voices this part may hold |
| 4 LEVEL | the part's level (the MIXER's) |

GLO again: SONG, as in Felucca. OCT- closes. Everything on MULTI is saved with the project (SAVE > PROJECT), so
a project is a whole setup: four engines, four sounds with their panels, channels, voice caps, levels, FX.

### Voices and the budget

The FM-1 has eight voices' worth of CPU. In Forge Five they are **eight units**, and a voice costs what its engine
costs: ANALOG, FM6, TRIO and the other Felucca engines 1.0; SEM 1.5; LADDER 2.5. So a LADDER bass (MONO, 2.5) beside
an FM6 pad can leave five pad voices; three LADDER voices and two of anything else fit exactly. When a new note does
not fit, the allocator takes the lightest voices it needs from the longest-sounding notes; a MONO / LEGATO / UNISON
part's held note is never taken.

**VOICES** (MULTI KNOB 3, also VOICE 3) caps a part so the others always have room: a 2-voice SEM lead, a 3-voice LADDER
chord, ALL for the pad. It belongs to the part, not the sound — loading a preset or changing the engine keeps it.

## 4. Picking engines and sounds

As in Felucca: **PRESETS** lists the selected part's sounds (KNOB 2 changes the engine, KNOB 3 marks a favourite,
KNOB 4 lists). Or hold **EDIT**: the white keys from F3 are the fifteen engines in the order above, one key each; the
last white key **G5 is INIT** (the engine's init sound) and the black key beside it, **F#5, is CLEAN INIT** (no FX, no
modulation). A sound load keeps the part's steps, scale, arp and VOICES, and SAVE held undoes it.

LADDER and SEM have 14 and 15 presets each (in [FORGE-FIVE.md](FORGE-FIVE.md)); their favourites share one row, so
star freely.

## 5. LADDER — the Model D-style monosynth

Three oscillators, a mixer with noise, the four-pole transistor ladder with EMPHASIS that sings, the two contours.
Mono by default; POLY and UNISON are capped at four voices.

**Pages** (EDIT steps through them; HOME shows CUTOFF, EMPH, CONTOR, GLIDE):

| Page | Knobs |
| --- | --- |
| FILTER | CUTOFF · EMPH (resonance; self-oscillates from ~106) · CONTOR (how much the filter contour opens it) · KBD (keyboard tracking OFF 1/3 2/3 FULL) |
| CONTOUR | F ATK · F DEC · F SUS · DECAY switch (ON: the release equals the decay, as the original's switch) |
| OSC 1-2 | O1 RNG (LO 32' 16' 8' 4' 2') · O1 WAV (TRI SHARK SAW SQR WIDE NARW) · O2 RNG · O2 WAV |
| OSC 2-3 | O2 FRQ (±7 semitones, fine near centre) · O3 RNG · O3 FRQ · O3 WAV (RSAW in place of SAW) |
| MIX | OSC 1 · OSC 2 · OSC 3 · NOISE |
| MIX 2 | N COL (WHITE / PINK) · OVLD (overload: push the mixer into the filter) · O3 KBD (OFF = OSC 3 is free-running, the LFO) · DRIFT |
| CTRL | MIX (OSC 3 ↔ NOISE as the modulation source) · WHEEL (amount) · OSC (to pitch) · FILT (to cutoff) |

The loudness contour is the part's ENV page; GLIDE is on VOICE. The mod wheel on Move (CC1) is the WHEEL knob: set
OSC for vibrato / trills, FILT for wah; O3 KBD OFF with O3 RNG LO makes OSC 3 the LFO, as on the original.

**Recipes**
- *Bass that cuts*: SAW + SQR at 16'/8', O2 FRQ a few cents, CUTOFF 40, EMPH 50, CONTOR 70, F DEC 40, DECAY switch
  on, KBD 1/3.
- *Lead*: three saws at 8' 8' 4', O2 FRQ +7, GLIDE 20, EMPH 80, FILT via WHEEL.
- *Whistle*: all oscillators off in MIX, EMPH 115, KBD FULL — the filter plays the keyboard.

## 6. SEM — the Oberheim-style synth

Two oscillators, the two-pole state-variable filter whose MODE knob sweeps from low-pass through notch to high-pass
(the SEM's own sound), plus the four-pole and band-pass of its later siblings. Poly by default; a voice costs 1.5.

**Pages** (HOME shows CUTOFF, RESO, MODE, GLIDE):

| Page | Knobs |
| --- | --- |
| FILTER | CUTOFF · RESO · MODE (LP ← notch → HP; works with TYPE 2P) · TYPE (2P / 4P / BP) |
| F ENV | F ATK · F DEC · F SUS · F REL (the filter envelope; F AMT on MOD sets how much, ± ) |
| OSC 1 | O1 WAV (SAW PLS BOTH TRI) · O1 PW · O1 RNG (16' 8' 4') · OSC 1 level |
| OSC 2 | O2 WAV · O2 PW · O2 RNG · OSC 2 level |
| MIX | O2 FRQ (±7 st) · DETUNE (cents) · SYNC (OSC 2 to OSC 1) · NOISE |
| MOD | F AMT (envelope → cutoff, negative allowed) · KBD (OFF HALF FULL) · X-MOD (OSC 2 into the cutoff) · PWM (LFO to the pulse widths) |

**Recipes**
- *Brass*: both SAW, DETUNE 10, TYPE 2P, MODE at LP, RESO 20, F AMT +40, F ATK 15, F DEC 60.
- *The notch sweep*: BOTH on both oscillators, TYPE 2P, RESO 60, MODE in the middle, then move CUTOFF with CC74 or
  macro 1 from Move: the hollow phaser-like sweep is the notch travelling.
- *Sync lead*: SYNC ON, O2 FRQ +9 swept by F AMT; TYPE 4P for a rounder top.
- *Pluck*: TYPE BP, F SUS 0, F DEC 30, F AMT +60.

## 7. Playing from Move: controllers

Always on, per part, on the part's channel:

| CC | Does |
| --- | --- |
| 1 | mod wheel (LADDER: the WHEEL amount; others: the MOD matrix source) |
| 5 | GLIDE |
| 7 / 10 | part LEVEL / PAN |
| 64 | sustain |
| 71 / 74 | the engine's resonance / brightness: LADDER EMPH / CUTOFF, SEM RESO / CUTOFF, ANALOG RES / CUT, FM6 MLVL … |
| 72 / 73 / 75 | amp release / attack / decay |
| 91 / 93 / 94 | reverb / chorus / delay send |
| **16 17 18 19** | **the engine's HOME knobs 1..4** (LADDER: CUTOFF EMPH CONTOR GLIDE; SEM: CUTOFF RESO MODE GLIDE; ANALOG: CUT RES …) |

CC16–19 are the macros: map Move's four knobs (or a MIDI track's four controller lanes) to them once and every part
answers with the four knobs of whatever engine it runs, no re-mapping per sound. **MIDI LEARN** (MENU > MIDI) still
gives any CC to any parameter of any part and wins over the map.

A CC from Move moves the parameter exactly as the knob would, and while the selected part is armed and playing, the
AUTOMATION page records it like a knob move, so a sweep can be captured into a Felucca pattern too.

## 8. Saving

- **SAVE > PROJECT**: the whole Multi (engines, sounds with their full panels, channels, VOICES, levels, FX, patterns).
- **SAVE > USER**: one part's sound as a user preset, panel included (32 slots); it comes back on any part.
- Felucca's web editor / backup (`web/`) backs up and restores everything, but it does not yet show the LADDER and
  SEM panel values — it keeps them, it just cannot edit them.
- Settings, user presets and projects live in flash outside the firmware image and survive an update within the
  Felucca family; a backup from the web editor before the first Forge Five install is still wise.

## 9. Tips and limits

- **CPU**: MENU > ABOUT > CPU shows the load. Four busy parts with SEM chords and a LADDER bass can reach the limit;
  VOICES caps are the tool. MENU > SYSTEM > 2ND CORE (beta in Felucca 1.6.1) moves part of the effects to the second
  core.
- The LADDER filter's modulation inputs (WHEEL → FILT, OSC 3) update once per 32 samples: fine for LFO rates, not for
  audio-rate filter FM. Same for SEM's X-MOD.
- Felucca's sequencer still runs under the Multi; press STOP if a project's pattern starts playing on load and you
  only want Move to play.
- Two parts on the same channel (TRACK mode): only the first of them plays it — for a layer, send the notes on two
  channels from Move. A part on OMNI plays every channel no other part claims; OFF makes a part keys-only.
- Undo: SAVE held, one step, for sound loads and panel edits made by a load; knob moves are live.

## 10. Where to look

- [FORGE-FIVE.md](FORGE-FIVE.md): every parameter, the storage format, the voice weights, building.
- [INSTALL.md](INSTALL.md): getting a build on the FM-1 and back off it.
- [Felucca wiki](https://github.com/hugelton/Felucca/wiki/Home): the sequencer, FX, scales, arp, DRUM, SLICE, the menu,
  the web editor — all unchanged.
- Issues and ideas: https://github.com/kkasemsa/Forge-Five/issues
