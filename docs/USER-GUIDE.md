# Forge Five — User Guide

Forge Five `FF 0.1 DEV` · for the M-VAVE FM-1 · based on [Clipper](https://github.com/isod89/clipper-fm1)

Forge Five turns the FM-1 into a synthesizer with engines built for particular kinds of music, each with a front panel of
its own, on **two parts** that can layer or split the keyboard. It is made to be played from Ableton Move or any MIDI
keyboard or sequencer. This guide covers what the current build does; installing is in [INSTALL.md](INSTALL.md).

> **Development build.** It has run on one FM-1 so far (23 Oct 2026): the CPU measures below come from it. Expect rough
> edges, keep a way back ready (INSTALL.md, step 6), and report anything odd in the
> [Issues](https://github.com/kkasemsa/Forge-Five/issues).

**The engines and the instruments they follow**

| Engine | In the style of | For |
| --- | --- | --- |
| **LADDER** | Moog Minimoog Model D-style (1970): three oscillators into the four-pole transistor ladder | Berlin School basses and leads |
| **CHORD-D** | Roland Juno-60 / Juno-106-style: one DCO, sub, four-position high-pass, the BBD chorus I / II / I+II | dub techno chords |
| **BRASS** | Oberheim SEM-style voice (its filter sweeping low-pass to notch to high-pass) with Sequential Prophet-5-style poly-mod and sync | pads and brass |
| **RIBBON** | Yamaha CS-80-style: two layers a voice, the ribbon, brilliance and touch | film-score brass, leads and pads |
| **WAVE-P** (to come) | PPG Wave-style: wavetables stepped without smoothing | digital pads and bells |
| DCO (base) | Clipper's engine in the Roland Juno-6 / 60 / 106-style | — |

Each engine is written from the published behaviour of those instruments: Forge Five contains none of their code, ROMs,
wavetables or samples, and is not affiliated with or endorsed by their makers. The names are trademarks of their
owners, used here only to describe a style; the firmware, its screens and its preset names use neutral names.

**Contents** —
[1. Getting around](#1-getting-around) ·
[2. Finding sounds](#2-finding-sounds) ·
[3. Playing it from Move or a keyboard](#3-playing-it-from-move-or-a-keyboard) ·
[4. Two parts: LAYER, SPLIT, MULTI](#4-two-parts-layer-split-multi) ·
[5. Voices and the CPU](#5-voices-and-the-cpu) ·
[6. Saving](#6-saving) ·
[7. LADDER](#7-ladder) ·
[8. CHORD-D](#8-chord-d) ·
[9. BRASS](#9-brass) ·
[10. RIBBON](#10-ribbon) ·
[11. The base engines](#11-the-base-engines) ·
[12. Known limits](#12-known-limits)

---

## 1. Getting around

| Control | What it does |
| --- | --- |
| **HOME** | The sound's name, family and engine, and a scope. The four knobs play the engine's four main controls (each engine's are listed below). |
| **PRESETS** (knob) | Steps through the sounds. |
| **ALGORITHM** | Jumps to the next family (PAD, BASS, LEAD, PLCK, KEYS, STAB, FX…). |
| **EDIT** | The sound's pages. **Tap EDIT again for the next page**; SELECT goes back and forth. In order: **EDIT 1**, **EDIT 2**, then the engine's own pages (named for each engine, e.g. OSC A, F ENV), then VOICE, VOICE 2, VOICE 3, MACROS, LAYER, LAYER 2, LAYER 3, MORPH, RANDOM, UNDO. The envelope (ENV) and LFO pages have buttons of their own. |
| **FX** | Sends, the sound's filter, INSERT, delay, reverb, chorus, EQ and compressor. |
| **GLO** | Tempo, master, tuning, **SYSTEM** (CPU / LATE / PEAK), MIDI settings. |
| **SAVE** | **PRESETS** (the list, filtered by engine), **USER** (your presets), tools. |
| **HOME held** | The menu (SYS > ABOUT shows the version and credits). |

**UNDO / COMPARE** (EDIT > UNDO): each change you make becomes a step once you stop turning; KNOB 1 steps back and
forward through up to 16, KNOB 2 compares the loaded sound with your edits. HOME shows **EDITED** when the sound differs
from what you loaded.

## 2. Finding sounds

1. Tap **SAVE**, go to the **PRESETS** page.
2. Turn **KNOB 2** to pick an engine: ALL, ANALOG, HYBRID, WAVE, DCO, **LADDER**, **CHORD-D**, **BRASS**, **RIBBON**, or
   USER.
3. Turn **KNOB 1** (or the PRESETS knob) through that engine's sounds.

Or turn PRESETS from HOME through the whole list, and ALGORITHM to jump by family. KNOB 4 on the PRESETS page adds a sound
to the favourites at the top of the list.

## 3. Playing it from Move or a keyboard

- **Channel**: every MIDI channel plays the instrument (GLO > MIDI > IN CH: OMNI) until you use MULTI (section 4).
- **Pitch bend**, **mod wheel** (CC 1), **expression** (CC 11), **sustain** (CC 64) and aftertouch work on every engine.
- **CC 16–19** play the four **MACROS** (EDIT > MACROS):

  | Macro | CC | What it does |
  | --- | --- | --- |
  | **BRITE** | 16 | The cutoff (or RIBBON's brilliance), about ±2.5 octaves |
  | **TIMBR** | 17 | The engine's "shape": LADDER the pulse widths (SQR, WIDE, NARW waves); CHORD-D the pulse width; BRASS the widths through its PWM amount (nothing while PWM is 0); RIBBON nothing yet |
  | **MOTN** | 18 | Motion: a vibrato and an LFO on the cutoff |
  | **SPACE** | 19 | More reverb, delay and stereo spread |

  Per-engine meanings for the macros are planned for a later version (README roadmap, Phase 7).
- **Program changes** (with bank select, CC 0) pick sounds in the list's order (GLO > MIDI 2 > PROG). The full CC list:
  [midi.md](midi.md).

## 4. Two parts: LAYER, SPLIT, MULTI

Part **A** is the sound you are editing; part **B** is the layer. Set them up on **EDIT > LAYER** (tap EDIT until LAYER):

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| **LAYER** | **MODE**: OFF, LAYER, SPLIT, MULTI | **SOUND**: part B's sound | **SPLIT**: the split note (MULTI: part B's MIDI channel) | **LEVEL** of part B |
| **LAYER 2** | **OCT**: part B's octave (−2..+2) | **PAN** of part B | **FILT**: part B's filter | **SWAP**: exchange A and B |
| **LAYER 3** | **VOICES**: part B's voice cap (ALL, 1–8) | | | |

- **LAYER**: every note plays both parts.
- **SPLIT**: **part A below the split note, part B from the split note up**. The default is middle C: the FM-1 shows it
  as **C4** (MIDI note 60), Ableton and Move call the same note **C3**. Typical: a bass on A, chords on B.
- **MULTI**: part B on its own MIDI channel (its own bend, mod wheel, pedal); every other channel plays part A.

Part A's own voice cap is on **EDIT > VOICE 3** (VOICES).

## 5. Voices and the CPU

The two parts share **8 voices' worth of CPU**. A voice of a heavier engine costs more of it, so you get fewer of them:

| Engine | A voice costs | Most voices (both parts together) | Measured on the FM-1 |
| --- | --- | --- | --- |
| ANALOG, HYBRID, WAVE, DCO | 1 | 8 | — |
| LADDER | 1.25 | 6 (4 a part) | 4 notes: 44 % |
| CHORD-D | 1 | 8 | 8 voices: 64 % |
| BRASS | 1 | 8 | 8 notes: 56 % |
| RIBBON | 2 (provisional) | 4 | not measured yet |

(CPU with nothing playing: 14 %. GLO > SYSTEM shows CPU, LATE and PEAK in turn.) When the budget is full, a new note
takes the place of an older one: released notes first, then held ones, but never a part's lowest held note (the bass)
or its MONO / LEGATO note. Both of the FM-1's cores render the voices.

## 6. Saving

- What you play is kept across power cycles (the autosave).
- **SAVE > USER** stores the sound as a user preset (32 slots): every value of the sound, **including the engine's own
  pages** (everything on its OSC, F ENV, CHORD, RIBBON… pages). The layer set-up (EDIT > LAYER) belongs to the
  instrument, not to a sound, and is kept by the autosave.
- The web editor does not know the Forge Five engines yet (section 12): use the FM-1 for them.

---

## 7. LADDER

*In the style of the Moog Minimoog Model D-style monosynth (its oscillator bank, mixer overload and ladder filter).*

**Berlin School basses and leads.** Three oscillators into a four-pole transistor-ladder low-pass, in the manner of the
classic 1970s three-oscillator portable monosynth. The mixer drives the filter's input: one oscillator stays clean,
three at full saturate it — the "fat" sound — and OVERLOAD pushes it further. Up to 4 voices a part (VOICE: POLY);
its presets play MONO / LEGATO, several with glide.

**HOME knobs**: CUTOFF · EMPH · CONTOR · GLIDE

| Page | Control | Range | What it does |
| --- | --- | --- | --- |
| EDIT 1 (FILTER) | **CUTOFF** | 0–127 | The ladder's cutoff |
| | **EMPH** | 0–127 | Emphasis (resonance); sings on its own near the top |
| | **CONTOR** | 0–127 | How far the filter contour opens the cutoff |
| | **KBD** | OFF, 1/3, 2/3, FULL | How much the cutoff follows the keyboard |
| EDIT 2 (CONTOUR) | **F ATK / F DEC / F SUS** | 0–127 | The filter contour's attack, decay, sustain |
| | **DECAY** | OFF / ON | ON: the release follows the decay times; OFF: a short release |
| OSC 1-2 | **O1 RNG / O2 RNG** | LO, 32′, 16′, 8′, 4′, 2′ | Each oscillator's octave (LO: sub-audio) |
| | **O1 WAV / O2 WAV** | TRI, SHARK, SAW, SQR, WIDE, NARW | Waveform (SHARK: a ramp-triangle; WIDE / NARW: pulses) |
| OSC 2-3 | **O2 FRQ / O3 FRQ** | ±7 semitones | Tuning against OSC 1, finer near the centre (slow beating) |
| | **O3 RNG**, **O3 WAV** | as above; RSAW = reverse saw | OSC 3's octave and wave |
| MIX | **OSC 1 / OSC 2 / OSC 3 / NOISE** | 0–127 | The mixer |
| MIX 2 | **N COL** | WHITE / PINK | Noise colour |
| | **OVLD** | 0–127 | Overload: drives the filter's input further |
| | **O3 KBD** | OFF / ON | OFF: OSC 3 leaves the keyboard — at LO it becomes the modulation oscillator (~0.25–16 Hz) |
| | **DRIFT** | 0–127 | Slow random wandering of each oscillator's tuning (0: stable) |
| CTRL | **MIX** | 0–127 | The modulation source: OSC 3 ↔ NOISE |
| | **WHEEL** | 0–127 | Modulation amount (the mod wheel adds to it) |
| | **OSC / FILT** | OFF / ON | Send the modulation to the oscillators' pitch / the cutoff |

The loudness envelope is the ENV page (A D S R); glide is on VOICE.

**Presets**

| Preset | Family | Description |
| --- | --- | --- |
| BERLIN BASS | BASS | Three saws, OSC 2 a hair sharp, OSC 3 an octave down: the sequencer bass |
| FAT LEAD | LEAD | Detuned saws and a square at 4′, with glide |
| PEDAL SUB | BASS | The bass-pedal sound: two 32′ saws and a 16′ triangle, dark |
| SEQ PLUCK | PLUCK | A resonant zap for 16th-note sequences, no sustain |
| BRASS | LEAD | Two detuned saws, the filter opening slowly |
| WHISTLE | LEAD | No oscillators: the filter self-oscillating as a sine, tracking the keys in full |
| WOBBLE | BASS | OSC 3 as a slow triangle on the cutoff |
| VIBRATO LEAD | LEAD | OSC 3 as a ~5 Hz triangle on the pitch, light |
| FM GROWL | BASS | OSC 3 at audio rate a fifth up, silent, frequency-modulating OSC 1 and 2 |
| NOISE RISER | FX | Pink noise through a resonant filter that opens over ~6 s |
| OVERDRIVE | LEAD | Everything up plus OVERLOAD: the mixer-feedback growl |
| DRONE | PAD | 32′ saws a fifth apart, a slow contour, lots of drift |
| FLUTE | LEAD | A triangle and a breath of noise, slow attack, a touch of vibrato |
| KICK BASS | BASS | A short, deep thump with a click of contour |

## 8. CHORD-D

*In the style of the Roland Juno-60 / Juno-106-style DCO polysynths and their bucket-brigade chorus; the chord memory
follows the dub techno way of playing one-finger chords.*

**Dub techno chords.** One DCO per voice (saw, a pulse whose width moves, or both), a square sub an octave down, noise, a
four-position high-pass, a 24 dB resonant low-pass, one envelope for filter and amplifier (or the amplifier gated), and a
stereo **bucket-brigade chorus** (I, II, I+II). Its signature is the **chord memory**: one key plays a whole chord.
8 voices.

**HOME knobs**: CUTOFF · RESO · ENV · CHORUS

| Page | Control | Range | What it does |
| --- | --- | --- | --- |
| EDIT 1 (VCF) | **CUTOFF**, **RESO** | 0–127 | The 24 dB low-pass |
| | **PWM** | 0–127 | Pulse width, 50 % to ~5 % (the LFO and TIMBR move it too) |
| | **CHORUS** | OFF, I, II, I+II | I: slow and wide; II: faster, deeper; I+II: fast and shallow |
| EDIT 2 (SUB ENV) | **SUB** | 0–127 | The square an octave down |
| | **HPF** | 0, 1, 2, 3 | 0: a little bass boost; 1: flat; 2 and 3: high-pass at ~240 / ~720 Hz |
| | **ENV** | 0–127 | How far the envelope opens the cutoff |
| | **RELEASE** | 0–127 | The envelope's release (the same value as R on the ENV page) |
| DCO | **WAVE** | SAW, PLS, S+P | The DCO's wave |
| | **NOISE** | 0–127 | Noise level |
| | **KBD** | 0–127 | Filter keyboard tracking (64: half) |
| | **POL** | + / − | The envelope opens (+) or closes (−) the filter |
| VCA BBD | **VCA** | ENV / GATE | GATE: the amplifier is fully open while a key is held; the envelope only shapes the filter |
| | **HISS / TONE / WIDTH** | 0–127 | The chorus's noise, its darkness, its stereo width |
| CHORD | **CHORD** | OFF, MEM, LEARN | Chord memory (below) |
| | **N2 … N4** | –, m2 … 2OCT | The chord's notes above the key |
| CHORD 2 | **N5 … N7** | as above | More chord notes (up to 7 notes in all) |
| | **VEL>F** | 0–127 | Velocity to the cutoff |

**Chord memory**

- **MEM**: every key plays the stored chord built on it. Releasing the key releases the notes it started, even if
  you changed the chord meanwhile; two chords that share a note keep it until both keys are up. Works with SPLIT,
  LAYER, the arpeggiator and the sequencer.
- **LEARN**: set CHORD to LEARN, **hold a chord, let go** — it is stored (intervals from the lowest note, up to 7
  notes) and CHORD switches to MEM. A single key alone is not learnt.
- Or set N2–N7 by hand: m3 + 5 + m7 is a minor seventh, add 9 for a minor ninth.

**Presets**

| Preset | Family | Description |
| --- | --- | --- |
| DUB STAB | STAB | A minor seventh from one key, short and dark, into the echo: the dub techno chord |
| ECHO CHORD | STAB | A minor ninth on the pulse, chorus II, a longer tail for the delay to carry |
| DEEP PAD | PAD | An open minor ninth (fifth, seventh, ninth, tenth), slow, the pulse width breathing |
| SUB PULSE | BASS | A mono bass of pulse and sub; amplifier gated, so the filter envelope alone shapes it |
| CHOR STRINGS | PAD | Saw strings, high-passed, chorus II wide; chord memory off — play the chords by hand |

## 9. BRASS

*In the style of the Oberheim SEM-style state-variable voice (the module the Two-, Four- and Eight-Voice polysynths
were built from: SEM-style) and the Sequential Prophet-5-style poly-mod and sync.*

**Pads and brass.** Two oscillators per voice in the manner of the classic late-1970s American two-oscillator polysynths:
hard sync, **poly-mod**, a 2-pole state-variable filter that sweeps from low-pass through notch to high-pass, a 4-pole
mode and band-pass, its own filter envelope, and **AGE** — each voice a little different, like the separate voice
boards of the originals. 8 voices.

**HOME knobs**: CUTOFF · RESO · ENV · PMOD

| Page | Control | Range | What it does |
| --- | --- | --- | --- |
| EDIT 1 (VCF) | **CUTOFF**, **RESO** | 0–127 | The filter (resonance stops short of self-oscillation) |
| | **ENV** | −64…+63 | Filter envelope depth, either direction |
| | **DETUNE** | 0–127 cents | OSC B against OSC A |
| EDIT 2 (PMOD ENV) | **PMOD** | 0–127 | Poly-mod: OSC B's amount into the destinations on POLYMOD |
| | **TYPE** | 2P, 4P, BP | 2P: 12 dB with MODE; 4P: 24 dB low-pass; BP: band-pass |
| | **ATTACK**, **RELEASE** | 0–127 | The loudness envelope's A and R (the same values as the ENV page) |
| OSC A / OSC B | **WAVE** | SAW, PLS, BOTH, TRI | Each oscillator's wave |
| | **PW** | 0–127 | Pulse width |
| | **RANGE** | 16′, 8′, 4′ | Octave |
| | **LEVEL** | 0–127 | Mixer level |
| MIX | **B FREQ** | ±7 semitones | OSC B's tuning |
| | **SYNC** | OFF / ON | OSC A hard-synced to OSC B (sweep A's pitch with PM ENV for the classic sync lead) |
| | **NOISE** | 0–127 | Noise into the filter |
| | **PWM** | 0–127 | The LFO (and TIMBR) on both pulse widths |
| F ENV | **F ATK / F DEC / F SUS / F REL** | 0–127 | The filter envelope |
| FILTER | **MODE** | 0–127 | TYPE 2P: 0 low-pass … 64 notch … 127 high-pass |
| | **KBD** | OFF, HALF, FULL | Filter keyboard tracking |
| | **VEL>F** | 0–127 | Velocity to the cutoff |
| | **AGE** | 0–127 | Per-voice slop: each voice drifts slowly (up to ±10 cents), sits a little off in tune, cutoff and envelope times. Each voice keeps its own character |
| POLYMOD | **PM ENV** | −64…+63 | The filter envelope as a second poly-mod source |
| | **PM FREQ / PM PW / PM FILT** | OFF / ON | Poly-mod destinations: OSC A's pitch (audio-rate FM), OSC A's pulse width, the cutoff |

**Presets**

| Preset | Family | Description |
| --- | --- | --- |
| JUMP BRASS | STAB | Two saws a few cents apart, the filter opening on each chord: the 80s brass stab |
| SYNC LEAD | LEAD | A synced to B, the filter envelope sweeping A's pitch through poly-mod: the scream |
| WARM PAD | PAD | A saw and a pulse, the LFO on the widths, the 12 dB filter low |
| POLYMOD BELL | KEYS | B a fifth and an octave up into A's pitch: an inharmonic, decaying ring |
| STACK BRASS | STAB | The 24 dB filter, two saws, more AGE |
| SOFT STRINGS | PAD | Saws at 8′ and 4′, slow |
| NOTCH SWEEP | PAD | MODE at the notch, the LFO on the cutoff, a little noise |
| PWM PAD | PAD | Two pulses, deep PWM |
| BP PLUCK | PLUCK | The band-pass, resonant |
| POLY BASS | BASS | A saw and a square an octave down, the 24 dB filter |
| GROWL | LEAD | OSC B into the cutoff and A's pitch: a snarling bass-lead |
| AGED PAD | PAD | AGE near the top: each voice its own tune, cutoff and drift |
| ARP PLUCK | PLUCK | Short and bright, for the arpeggiator |

## 10. RIBBON

*In the style of the Yamaha CS-80-style two-layer polysynth, its ribbon controller and its touch-sensitive brilliance.*

**The big film-score sound.** Two layers per voice in the manner of the classic late-1970s two-layer polysynth with a
ribbon above its keys. Each layer has its own oscillator, a high-pass into a low-pass, and its own filter envelope;
layer II has its own loudness envelope too, so it can swell in after layer I. **BRILL** opens both filters, **TOUCH**
brightens a note the harder you strike it and the longer you hold it, and **K4 is the ribbon**: a pitch bend that springs
back. 4 voices while its weight is provisional (section 5).

**HOME knobs**: BRILL · RESO · BAL · **RIBBON**

| Page | Control | Range | What it does |
| --- | --- | --- | --- |
| EDIT 1 (BRILL BAL) | **BRILL** | 0–127 | Brilliance: both layers' cutoff |
| | **RESO** | 0–127 | Both filters' resonance |
| | **BAL** | −64…+63 | −64 layer I alone, 0 both, +63 layer II alone |
| | **RIBBON** | −64…+63 | The ribbon (below) |
| EDIT 2 (TOUCH) | **DETUNE** | 0–127 cents | Layer II against layer I |
| | **ATTACK**, **RELEASE** | 0–127 | The loudness envelope's A and R (the same values as the ENV page) |
| | **TOUCH** | 0–127 | How much the touch brightens the sound (up to +40 cutoff steps) |
| LAYER I / LAYER II | **WAVE** | SAW, PLS, SINE | The layer's oscillator |
| | **PW** | 0–127 | Pulse width |
| | **HPF** | 0–127 | The layer's high-pass (0: off) |
| | **FEET** | 16′, 8′, 4′ | The layer's octave |
| I FILTER / II FILTER | **F ATK / F DEC / F SUS** | 0–127 | The layer's filter envelope |
| | **F AMT** | −64…+63 | Its depth, either direction |
| II AMP | **II ATK / II DEC / II SUS / II REL** | 0–127 | Layer II's own loudness envelope (inside the voice's) |
| SUB RING | **SUB** | 0–127 | A square an octave under layer I |
| | **RING** | 0–127 | Ring modulation of layer I by layer II (bells) |
| | **NOISE** | 0–127 | A breath of noise into both layers |
| | **KBD** | OFF, HALF, FULL | Filter keyboard tracking |
| RIBBON | **RANGE** | 1–24 semitones | How far the ribbon bends at either end |
| | **TARGET** | LAST / ALL | LAST: only the newest note held bends; ALL: every voice |
| | **SPRING** | OFF / ON | ON (default): the ribbon returns to the middle once K4 rests |
| | **HOLD** | time | How long a held note takes to reach full TOUCH (default ~1.5 s) |

**Playing the ribbon.** On HOME (or EDIT 1), turn **K4**: the newest note bends up or down from wherever K4 starts.
Let go: after **300 ms** at rest it glides back to the middle (the whole range in about 175 ms). Turn SPRING OFF to make
the bend stay. The ribbon is a performance control: turning it does not count as an edit (no UNDO step, no EDITED).

**Playing TOUCH.** Half of TOUCH's brightening comes from velocity, half from how long the key has been held, reaching
its full amount at HOLD. Strike hard for an instant bright attack; hold a chord and it opens over a second or two.

**Presets**

| Preset | Family | Description |
| --- | --- | --- |
| NOIR BRASS | STAB | Two saws, layer II an octave up and swelling in later, the filters opening slowly; hold to brighten |
| VICTORY LEAD | LEAD | A saw and a pulse with glide; the ribbon bends the newest note an octave each way |
| MEMORY PAD | PAD | A pulse and a saw an octave up, slow in and slow out |
| RIBBON BEND | LEAD | A lead made for the ribbon: an octave each way, springing back |
| RAIN BELL | KEYS | Two sines an octave apart rung against each other (RING), a long fall |
| FILM STRINGS | PAD | Two saws, high-passed a little, a slow swell |
| BREATH FLUTE | LEAD | A sine and its breath, layer I alone |
| TOUCH BRASS | STAB | Dark until touched: TOUCH near the top, a short HOLD |

---

## 11. The base engines

Four of Clipper's engines stay in Forge Five as the ground the five are built on. Their sounds are Clipper's; their
pages and controls are described in [CLIPPER-README.md](CLIPPER-README.md) and Clipper's QUICKSTART.

| Engine | What it is |
| --- | --- |
| **ANALOG** | Two band-limited oscillators, noise, drive, and eight filter models (EDIT > FILTER > MODL) |
| **HYBRID** | Three oscillators, two filters and a 16-slot matrix; its four macros M1–M4 replace BRITE…SPACE |
| **WAVE** | Wavetables (WAVE's user tables come from the web editor) |
| **DCO** | Clipper's one-DCO poly with the bucket-brigade chorus; CHORD-D is built on it |

**Sounds by family**

| Engine | BASS | KEYS / ORGAN | PAD | LEAD | PLUCK | STAB | FX |
| --- | --- | --- | --- | --- | --- | --- | --- |
| ANALOG | 808 BOOM, 808 DIRTY, 808 SLIDE, SUB BASS, PLUGG BASS, REESE, WOBBLE, ACID 303, FUNK BASS, MONO BASS, SAW BASS, PEDAL BASS, DYN BASS, MINI BASS | | WARM PAD, DARK STR, ATMOS PAD, SINE PAD, CHORUS PAD | SUPERSAW, G-FUNK LD, FAT LEAD, PAN FLUTE, PWM LEAD, POLY LEAD, WAH LEAD, MINI LEAD | TRAP PLUCK, SQR PLUCK | SYN BRASS, BIG BRASS, UNI STAB | |
| HYBRID | ACID, WOBBLE, REESE, FOLD BASS, 808 SUB, ACID HARD, LADR BASS | FM KEYS | CLIPPER, POLY PAD, STRINGS, VOX, GLASS PAD, DRONE, DARK PAD, LADDER PAD | SYNC LEAD, HOOVER, CHIP LEAD, TRANCE, CLIP WIDE, SOFT SYNC, LADR LEAD | PLUCK, SAW PLUCK, FM BELL, SPACE BEL, METAL, BRIGHT BEL | BRASS | RISER, INIT |
| WAVE | SYNC BASS, FM GROWL, WT BASS, CHIP BASS | PD WAVE, PD KEYS, ORGAN WT, WT ORGAN2 | WAVE PAD, WT SWEEP, VOCAL WT, HARMONICS, COMB PAD, PWM STR, VOCAL PAD, DIGI PAD, HARM PAD | WT BASIC, RESO SWP, FOLD LEAD, CHIP WT, ODD LEAD, SYNC SCREAM, WT SUPER | DIGI BELL, FM BELLS, COMB BELL | | USER WT |
| DCO | DCO BASS, DCO SUB | POLY KEYS, DCO PIANO, DCO ORGAN | DCO PAD, DCO STRINGS, DREAM PAD | SYNTHWAVE LD, DCO LEAD, HOOVER DCO | ARP PLUCK, TURING DCO | DCO BRASS, 80S HITS, SYNTH BRASS 2 | NOISE SWEEP |

(USER WT plays WAVE's user wavetable slot: empty until you send a table from the web editor.)

## 12. Known limits

- **The web editor** does not know LADDER, CHORD-D, BRASS or RIBBON yet: edit them on the FM-1. Their sounds are kept
  in user presets and backups all the same.
- **WAVE-P**, the fifth engine, is the next to come.
- **Later (Phase 7)**: MORPH between two versions of a sound for these engines, per-engine macro meanings, the encoder
  turned while a key is held as a per-note expression, an 8-slot modulation matrix, a full preset library, a Thai manual.
- RIBBON's voice weight (2) is provisional until it is measured on the FM-1; it may well drop to ~1 (8 voices).

---

Forge Five is free software, GPL-3.0-only. Based on Clipper by isod89 / 3dSam, on SLOOP, and on Felucca by Leo Kuroshita
(Hügelton Instruments). Not affiliated with or endorsed by M-VAVE.
