# Forge Five

[![License: GPL-3.0-only](https://img.shields.io/badge/license-GPL--3.0--only-blue.svg)](LICENSE)

A synthesizer firmware for the M-VAVE FM-1, **based on [Clipper](https://github.com/isod89/clipper-fm1)** (isod89 / 3dSam),
which grew out of [SLOOP](https://github.com/isod89/sloop-fm1) and [Felucca](https://github.com/hugelton/Felucca)
(Leo Kuroshita / Hügelton Instruments). Where Clipper offers 23 engines, Forge Five will offer five, each tuned to one
kind of music and given a real front panel, on two parts that **layer or split** the keyboard, played from Ableton Move
or any MIDI sequencer:

| Engine | Style | For |
| --- | --- | --- |
| **LADDER** | three oscillators, a four-pole ladder, overdrive, glide (a classic 1970s American monosynth) | Berlin School bass and leads |
| **CHORD-D** | one DCO, a sub, a 24 dB low-pass, a stereo bucket-brigade chorus, chord memory | dub techno chords |
| **BRASS** | two oscillators, hard sync, poly-mod, a 2-pole state-variable filter that sweeps low-pass → notch → high-pass, or 4-pole; per-voice age | pads and brass |
| **RIBBON** | two layers a voice, a pitch ribbon on a knob, brilliance and resonance | the big film synth sound |
| **WAVE-P** | wavetables with a stepped (uninterpolated) mode, two oscillators, a 24 dB filter | digital wavetable pads and bells |

Voices share a budget of eight units (an engine's voice costs what it costs); a part can be capped. Four macros per
engine on CC 16–19, a simple arpeggiator, Clipper's effects, modulation matrix, web editor and installer.

**Status: in development, first measures on the FM-1 done (23 Oct 2026).** The current build (`FF 0.1 DEV`) is Clipper
trimmed to the four engines the five are built on (ANALOG for its filter models, HYBRID, WAVE, DCO: Clipper's
one-DCO poly) plus four Forge Five engines: **LADDER** (three oscillators, the ladder filter, 14 presets), **CHORD-D**
(one DCO, the stereo bucket-brigade chorus, chord memory with LEARN, 5 presets), **BRASS** (two oscillators, sync,
poly-mod, a 2-pole filter sweeping low-pass to notch to high-pass, 4-pole and band-pass, per-voice AGE, 13 presets) and
**RIBBON** (two layers a voice, a pitch ribbon on K4 that springs back, brilliance from the touch, ring and sub, 8
presets); WAVE-P follows (the [roadmap](#roadmap)). Their voices run on both cores; the voice budget's weights come
from the unit (docs/INSTALL.md). Install at your own risk, with a way back ready.

- **Install:** [docs/INSTALL.md](docs/INSTALL.md) — GitHub builds the package, one script sends it to the FM-1.
- **Build it yourself:** [BUILDING.md](BUILDING.md) (Linux x86-64 natively, macOS through Docker, Windows through WSL).
- **Clipper's own documentation** (everything Forge Five has not changed): [docs/CLIPPER-README.md](docs/CLIPPER-README.md),
  [docs/midi.md](docs/midi.md), the [Felucca wiki](https://github.com/hugelton/Felucca/wiki/Home).
- Issues and ideas: https://github.com/kkasemsa/Forge-Five/issues

## Roadmap

| Phase | Release | What |
| --- | --- | --- |
| 0 | — | the fork builds and passes its tests on GitHub ✅; CPU measured on the unit ✅ (23 Oct 2026) |
| 1 | `0.1 DEV` | trim, rebrand, two parts with a split point, the voice budget, the macros, the per-part engine patch |
| 2 | 0.1 alpha | LADDER |
| 3 | 0.2 | CHORD-D |
| 4 | 0.3 | BRASS, AGE |
| 5 | 0.4 | RIBBON (the ribbon on K4, TOUCH from velocity and the time a key is held) |
| 6 | 0.5 | WAVE-P |
| 7 | 1.0 | MORPH A/B, the presets, the editor, a Thai manual; and, moved here from Phase 5: the encoder turned while a key is held as a per-note expression, the 8-slot modulation matrix (spec v0.2 section 4) |

The earlier Forge Five, built on Felucca 1.6.1 with four parts (FF 0.2: the LADDER engine, the engine patch, a MULTI page),
is kept on the branch `archive/felucca-base` and the tag `ff-0.2-felucca`; its LADDER, voice budget, macros and engine
patch are being ported here.

## Credits and licence

Free software, **GPL-3.0-only** ([LICENSE](LICENSE)): the whole source, the web editor included. Forge Five is built on
Clipper by isod89 / 3dSam, on SLOOP, and on Felucca by Leo Kuroshita (@kurogedelic), Hügelton Instruments; the bundled
fonts and ported DSP keep their own licences ([LICENSES/](LICENSES/), [LICENSING.md](LICENSING.md)). Algorithms, not
code or data, are borrowed from the classic instruments: no ROMs, wavetables or samples of any real instrument are used,
and no manufacturer's or model's name is.

M-VAVE and FM-1 are trademarks of their respective owners. Forge Five is not affiliated with or endorsed by M-VAVE, nor
by the Clipper, SLOOP or Felucca projects.

Copyright (C) 2026 Khantipol Kasemsant.
