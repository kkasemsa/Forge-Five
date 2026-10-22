# Clipper 1.1: MIDI implementation

Clipper is meant to be played from a MIDI keyboard, a controller, a sequencer or a DAW as much as from its own keys. This is everything it answers to and everything it sends, over USB and the TRS jack alike. *(En français : [midi-fr.md](midi-fr.md).)*

## The settings (GLO > MIDI, GLO > MIDI 2)

They are settings of the FM-1: they stay after a power-off, whatever sound is loaded.

| Page | Knob | Values | What it does |
|---|---|---|---|
| MIDI | **IN CH** | OMNI, 1..16 | The channel Clipper listens on. OMNI (the default): every channel. With MPE on, every channel; in MULTI, the layer keeps its own channel (EDIT > LAYER > CH). |
| MIDI | **OUT** | 1..16 | The channel the keys, the arpeggiator, the sequencer and the knobs (CC OUT) send on. |
| MIDI | **BEND** | AUTO, 1..24 | The pitch bend range in semitones. AUTO: the HYBRID patch's own range, ±2 on the other engines. RPN 0 sets it too. |
| MIDI | **LOCAL** | ON, OFF | OFF: the keys only go out on MIDI, the sound plays what comes in (a DAW sending the track back to Clipper: no doubled notes). CC 122 sets it too. |
| MIDI 2 | **PROG** | ON, OFF | Program changes load sounds (ON, the default) or are ignored. |
| MIDI 2 | **SEND** | OFF, CC | CC: turning a knob sends the CC that sets it (the CCs below), so a DAW records the moves and plays them back. OFF (the default): nothing is sent. |
| MIDI 2 | **EWI** | OFF, VOL, V+AT | For a breath controller (an EWI, a TEC, a breath controller with an adapter) sending CC 2. VOL: the breath is the volume (0 silent, 127 as set). V+AT: the aftertouch too, so the MOD page's AT destination (CUT: the filter opens as you blow; VIB, WAH, TREM) and the matrix's AT source follow it. OFF (the default): CC 2 does nothing. Set the sound's VOICE to MONO or LEGATO, with a little GLIDE. |

GLO > SYSTEM keeps the rest: **MIDI** (what goes out: the keys only, or the sequencer and arp too), **SYNC** (the clock: internal, USB, TRS), **IN** (NOTES, or CLOCK: the notes ignored). MENU > MIDI has **CLOCK OUT**. MENU > SYSTEM has **MPE**.

## Received

| Message | What it does |
|---|---|
| Note on / off | Plays the sound, with velocity. On the NOTES page, writes the step under edit. |
| Pitch bend | Over the BEND range (above). |
| Channel pressure, polyphonic pressure | Aftertouch: the MOD page's AT destination, and the matrix's AT source. |
| **Program change** | Loads the sound at that place of the PRESETS list: program 0 is the first factory sound. The favourites are not counted; the factory sounds come in the list's order (as SAVE > PRESETS shows them with the filter on ALL), then your user presets. With a bank (CC 0), program *p* of bank *b* is place *b* × 128 + *p*. |
| CC 0 | Bank select (MSB) for the next program change. CC 32 (LSB) is ignored. |
| CC 1 | Mod wheel: the MOD page's MW destination, and the matrix's MW source. |
| CC 2 | Breath: with MIDI 2 > EWI, the volume (VOL) and the aftertouch (V+AT). |
| CC 5 | GLIDE. |
| CC 6 | Data entry: with RPN 0 selected, the pitch bend range (1..24 semitones). |
| CC 7 | LEVEL. |
| CC 10 | PAN (64 the centre). |
| **CC 11** | Expression: the sound's volume (127 as set, 0 silent), on top of LEVEL; also the matrix's EXPR source. |
| CC 16..19 | The four MACROS (BRITE, TIMBR, MOTN, SPACE); HYBRID: M1..M4. |
| CC 20 | MORPH. |
| CC 64 | Sustain pedal. |
| CC 71 | Resonance (the engine's RES or Q). |
| CC 72, 73, 75 | Release, attack, decay. |
| CC 74 | The engine's cutoff or brightness, else the FILTER. |
| CC 76, 77, 78 | The LFO's rate, its pitch depth (vibrato: 64 none), its FADE (the delay before it comes in). |
| CC 91, 93, 94 | Reverb, chorus, delay sends. |
| CC 98, 99 | NRPN: not used (a data entry after them changes nothing). |
| CC 100, 101 | RPN select (RPN 0: the bend range). |
| **CC 120** | All sound off. |
| **CC 121** | Reset all controllers: bend centred, mod wheel and pressure 0, expression full, pedal up. |
| **CC 122** | Local control (0 off, 127 on): as LOCAL. |
| **CC 123** | All notes off (124..127, the mode messages, too). |
| Clock, start, continue, stop | With GLO > SYSTEM > SYNC = USB or TRS: the sequencer, the arpeggiator and the delay follow the master's tempo, start and stop. |
| MPE | MENU > SYSTEM > MPE: each note's own bend, pressure and CC 74 on channels 2..16. |
| **Identity request** | `F0 7E 7F 06 01 F7` (any device ID): Clipper answers `F0 7E 7F 06 02 7D 43 4C 46 31 01 01 00 00 F7` (7D, non-commercial; family "CL", model "F1", version 1.1). |
| SysEx | The web editor's and the updater's own commands. |

A CC sets its value as the knob would: 0..127 over the whole range, 64 the middle of a bipolar one.

## Sent

| Message | When |
|---|---|
| Note on / off | The keys (always, on OUT), and the sequencer and the arpeggiator with GLO > SYSTEM > MIDI = SEQ. |
| Control change | With MIDI 2 > SEND = CC: a knob turned on a value the CCs above set (EDIT, ENV, FX..., and the HOME knobs), when the value it stands for changes. A sound loaded sends nothing, and a CC received is never sent back. |
| Clock, start, stop | MENU > MIDI > CLOCK OUT (USB). |
| Identity reply | To an identity request. |

## Not there (yet)

Song position pointer, CC 65 (portamento on / off), CC 66 (sostenuto), NRPNs, MIDI 2.0.

## Checking it on an FM-1

1. GLO pressed until **MIDI**: IN CH, OUT, BEND, LOCAL. GLO again: **MIDI 2**: PROG, SEND, EWI.
2. From a DAW or a controller, send program changes 0, 1, 2: the sounds of the list, in order; CC 0 = 1 then program 0: the 129th sound. PROG OFF: nothing changes.
3. Hold a chord and move CC 11 down: the sound fades out; back up: as before.
4. Hold a chord and send CC 123 (or press the DAW's panic button, often CC 120 / 123): the notes stop.
5. RPN 0 = 12 (CC 101 0, CC 100 0, CC 6 12): the bend goes an octave. GLO > MIDI > BEND shows 12; set it back to AUTO.
6. LOCAL OFF with the DAW's track sending back to Clipper: each key plays once, through the DAW.
7. SEND = CC, record a DAW track's CCs, turn ENV's RELEASE and FX's sends: the DAW gets CC 72, 91, 94; play it back: the knobs' moves come back.
8. With an EWI or a breath controller: MIDI 2 > EWI = VOL, VOICE LEGATO: blow softly, then hard: the note swells; EWI = V+AT and the MOD page's AT = CUT: the sound brightens as you blow.
9. A DAW or a tool that asks for the identity (MIDI-OX, Bome SendSX, `amidi`) gets the reply above.
