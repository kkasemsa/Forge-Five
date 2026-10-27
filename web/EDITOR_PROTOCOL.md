# SLOOP editor protocol (SysEx over USB-MIDI)

The firmware side is `firmware/src/editor.c` (SLOOP is based on Felucca: the frames keep its "FL"
header). Commands 16-26 (user presets and live sync) form protocol v2; commands 27-30 (tracks) form
protocol v3; commands 31-32 (any track's parameters) form protocol v4; command 33 and the extra step,
`INFO` and `TRACK` bytes form protocol v5 (SLOOP 2.0); commands 34-36 (backup / restore) form protocol v6
(SLOOP 2.3); commands 37-40 (the steps' nudges and parameter locks) form protocol v7, commands 41-42 (the
steps' fill conditions) protocol v8 and commands 68-71 (the FM6 engine's patches) protocol v9 (all SLOOP 2.4).

**v3 (four tracks):** the device has four tracks: 1..3 are synth parts, 4 is the drum track. One
of them is *selected* (the TRACKS page on the device, or `TRACK`). Every v1 / v2 command acts on the
selected track (its parameters, engine, preset, steps, the user presets it stores or loads); `TRACK`,
`TRACK_MIX`, `TRACK_DUMP` and `TRACK_STEP` reach any track. Command numbers 1-26 are unchanged.

**v4 (any track's parameters):** `TRACK_PARAM` gets or sets a parameter of any track without changing
the selection, and the `TRACK_CHANGED` push follows level, pan and mute of the tracks that are not
selected. The v1-v3 commands are byte for byte as before; v4 is asked for with bit 1 of `WATCH`.

**v5 (SLOOP 2.0):** the drum track has 16 lanes (one sound per white key) with a level and a ratchet per
hit; `DRUM_STEP` reads and writes them. Synth steps carry a level and a ratchet per note. `INFO` ends with
the protocol version (5) and `TRACK` with the solo mask. Every addition is a byte appended at the end of
a reply or a request, so v1-v4 editors keep working (they see the drum lanes as GM notes, below).

## Framing

A request is `F0 7D 46 4C <cmd> <args...> F7`:

- `7D` is the non-commercial SysEx ID.
- `46 4C` is "FL".

Every request gets exactly one reply, with the same header and the same `<cmd>`. Requests
the device does not understand get no reply. Every data byte is 7 bit. While the editor
watches (v2, `WATCH`), the device also sends push frames (cmds 23, 24, 26) at any time.

| Item | Encoding |
| --- | --- |
| value (v14) | 2 bytes, LSB first, holding value + 8192, so the range is -8192..8191. `[lo, hi]`: value = (lo \| hi << 7) − 8192 |
| string | ASCII bytes, ended by a 0 byte |
| scope | 0 = parameter of the selected track (`P_*`, 0..P_COUNT−1); 1 = global parameter (`G_*`, 0..G_COUNT−1) |
| track | 0..3: tracks 1..3 (synth parts), 3 = the drum track |
| engine byte | 0..NENGINES−1; NENGINES = the drum track (it has no engine and no presets) |

The engine parameters are `P_E0..P_E7`: P_COUNT−8 .. P_COUNT−1, and `INFO` gives `P_E0`.
Their meaning, range and names depend on the current engine, so re-read `DESC` for them
after an engine change.

## Commands

| cmd | Request args | Reply args |
| --- | --- | --- |
| 1 INFO | — | version string, NENGINES, P_COUNT, G_COUNT, NSTEP, P_E0, then NENGINES engine-name strings, then (v3) NTRK (4), then (v5) the protocol version (5); older firmware ends after the names / NTRK |
| 2 GET | scope, id | scope, id, v14 |
| 3 SET | scope, id, v14 | scope, id, v14 (the value after clamping). Setting global `G_ENGSEL` (id from DESC label "ENG") changes the engine with its defaults |
| 4 DUMP | — | engine, preset, then P_COUNT × v14 (the selected track), then G_COUNT × v14 (globals) |
| 5 DESC | scope, id | scope, id, fmt, min v14, max v14, def v14, label string, unit string, then for an enum (fmt 8) one name string per value (at most 64; firmware before SLOOP sent at most 16) |
| 6 STEP_GET | index 0..NSTEP−1 | index, n (0..4 notes), note0..note3, time (0 NOTE, 1 TIE, 2 REST), flags (1 accent, 2 slide), vel, then (v5) lvl, hi, rat |
| 7 STEP_SET | index, n, note0..3, time, flags, vel [, lvl, hi, rat (v5)] | same as STEP_GET (after the write). Without the v5 bytes the step's levels and ratchets become 0 |
| 8 PRESET | engine, preset | engine, preset (applies the preset: sound, sends, arp; never the pattern, the mix or the key: `LEVEL PAN MUTE`, `LEN DIV SWG GATE`, `ROOT SCL QNT CHORD` stay) |
| 9 PROJECT | op (0 load, 1 save, 2 query), slot 0..3 | op, slot, used (1/0). Save writes flash: allow ~2 s |

The local Studio build may append status `1` to a PROJECT reply when playback
prevents a load/save. No operation occurred; stop playback and try again.
An absent status byte retains the original reply format.
| 10 NAMES | engine | engine, count, count preset-name strings, then the two edit-page titles |
| 11 SMP_BEGIN | slot 0..2 | slot, rc (0 ok). Erases the slot's header sector: the slot is empty from now on |
| 12 SMP_WRITE | slot, offset (3 × 7 bit, LSB first), pack7 data (≤ 256 bytes) | slot, offset, rc: 0 ok, 1 arguments, 2 erase, 3 write, 4 slot in use (send SMP_BEGIN first). Offset ≥ 512 and a multiple of 256; writes go in increasing order (a write at a 4 KiB boundary erases that sector) |
| 13 SMP_END | slot, pack7 header (480 bytes) | slot, rc: 0 ok, 1 size, 2 header, 3 data CRC, 4 flash, 5 zones |
| 14 SMP_ERASE | slot | slot, rc (erases the whole slot, ~1 s) |
| 15 SMP_INFO | — | slots, slot KiB, then per slot: zone count (0 = empty), name string, data KiB |
| 16 UP_LIST | start, count (1..16) | start, count, total slots, then per slot: used (0/1), engine, name string ("" if unused) |
| 17 UP_GET | slot | slot, used, engine, name, P_COUNT × v14, 16 × (note, flags) |
| 18 UP_PUT | slot, engine, name, P_COUNT × v14, 16 × (note, flags) | slot, rc (0 ok, 1 args, 2 flash, 3 stop the song first). Writes flash: allow 1 s |
| 19 UP_STORE | slot, name | slot, rc (3: stop the song first). Stores the current sound: engine, parameters, the first 16 sequencer steps as the pattern (TIE steps → flag 4) |
| 20 UP_LOAD | slot | slot, rc (0 ok, 1 empty/invalid). Applies it |
| 21 UP_ERASE | slot | slot, rc (3: stop the song first) |
| 22 WATCH | on (0/1; v4: 3 = also `TRACK_CHANGED`) | on (0/1; v4 firmware: 3 when 3 was asked for). While on, the device pushes cmds 23, 24, 26 (and 32 with bit 1) |
| 23 CHANGED (push) | — | scope, id, v14 |
| 24 RELOAD (push) | — | engine, preset, then (v3) the selected track |
| 25 PING | — | 0 |
| 26 STEP_CHANGED (push) | — | index, then (v3) the selected track |

| cmd (v3) | Request args | Reply args |
| --- | --- | --- |
| 27 TRACK | — (query), or track (select it) | selected track, NTRK, then per track: engine byte, preset, level v14, mute (0/1), armed (0/1, live recording) |
| 28 TRACK_MIX | track (get), or track, level v14 (0..127), mute (set) | track, level v14, mute. The drum track's level is global `G_DRLVL` (GLO > DRUMS LEVEL); mute is the track's `P_MUTE` |
| 29 TRACK_DUMP | track | track, engine byte, preset, P_COUNT × v14 (that track's parameters; no globals) |
| 30 TRACK_STEP | track, index (get), or track, index, n, note0..3, time, flags, vel [, lvl, hi, rat] (set) | track, index, n, note0..3, time, flags, vel, then (v5) lvl, hi, rat |

| cmd (v4) | Request args | Reply args |
| --- | --- | --- |
| 31 TRACK_PARAM | track, id (get), or track, id, v14 (set); id = `P_*` (0..P_COUNT−1) | track, id, v14 (the value after clamping, as `SET`). The selection does not change; no push about the editor's own write |
| 32 TRACK_CHANGED (push) | — | track, id, v14: `P_LEVEL`, `P_PAN` or `P_MUTE` of a track that is not selected changed on the device (only while `WATCH` was sent with bit 1) |

| cmd (v5) | Request args | Reply args |
| --- | --- | --- |
| 33 DRUM_STEP | index (get), or index, on (3 bytes), lvl (5 bytes), rat (5 bytes) (set) | index, on (3 bytes), lvl (5 bytes), rat (5 bytes): the drum track's step, whichever track is selected |

| cmd (v7) | Request args | Reply args |
| --- | --- | --- |
| 37 LOCK_GET | track | track, n, then n × (step, param, v14 value): the track's parameter locks (n ≤ 24, unsorted) |
| 38 LOCK_SET | track, step, param [, v14 value] (no value: delete the lock of that step and parameter) | track, step, param, rc, has (0/1), v14 (the lock's value after clamping; 0 when there is none). rc: 0 ok, 1 step ≥ NSTEP or param ≥ P_COUNT, 2 the parameter cannot be locked, 3 no free slot (24 a track) |
| 39 MICRO_GET | track | track, then NSTEP bytes: each step's nudge + 64 (so 32..95; 64 = on the grid) |
| 40 MICRO_SET | track, step, nudge + 64 | track, step, nudge + 64 (after clamping to −32..31) |

| cmd (v8) | Request args | Reply args |
| --- | --- | --- |
| 41 FILL_GET | track | track, then the track's 16 condition bytes as stored (2 bits per step, step i in byte i / 4 at bits 2 (i mod 4)..+1), **pack7** (19 bytes on the wire) |
| 42 FILL_SET | track, step, cond (0 normal, 1 fill only, 2 no fill; 3 = 0) | track, step, cond (as stored) |

| cmd (v9) | Request args | Reply args |
| --- | --- | --- |
| 68 FM6_GET | target, index | target, index, rc, then (rc 0) the 128-byte packed patch |
| 69 FM6_PUT | target, index, the 128-byte packed patch | target, index, rc |
| 70 FM6_LIST | — | factory count, bank count, then per slot (factory first): used (0/1), name string ("" if empty) |
| 71 FM6_ERASE | bank index | index, rc |

**pack7:** groups of up to 7 bytes, each preceded by one byte holding their top bits
(bit j = bit 7 of byte j).

**User sample slot** (80 KiB each: USR1..USR3 at flash 0xA0000, 0xB4000, 0xC8000 and, from SLOOP 2.4, USR4 at
0xE7000 — SMP_INFO says how many; SAMPLE / GRAIN sets USR1..USR4, the drum track's KIT USR1..USR4 and USR3+4; reference uploader
`tools/fm1_sample_upload.py`, slot builder `sampleio.user_slot`; the editor's port of it is
checked byte for byte by `web/test_web.mjs`): header at 0, ADPCM data at 512.

| Offset | Field |
| --- | --- |
| 0 | magic `"FSMP"` (u32 0x504D5346), u16 version 1, u8 zone count 1..16, u8 0 |
| 8 | name, 8 ASCII bytes (0-padded) |
| 16 | u32 data length (bytes), u32 CRC-32 (zlib) of the data, 8 bytes 0 |
| 32 | 16 zones × 28 bytes: u32 off (in the data), n (samples), loop start, loop end, rate (Hz / 44100 × 65536); i16 root × 16 (MIDI note), ADPCM predictor at the loop start; u8 step index at the loop start, lo note, hi note, looped (0/1) |

Data is IMA ADPCM, 4 bit, low nibble first, starting from predictor 0 and step index 0.
All little endian.

`fmt` values (`firmware/src/core.h`):

| Value | Name | Value | Name | Value | Name |
| --- | --- | --- | --- | --- | --- |
| 0 | INT | 5 | CUTOFF | 10 | NOTE |
| 1 | PCT | 6 | DB | 11 | ONOFF |
| 2 | BIPCT | 7 | SEMI | 12 | OCT |
| 3 | TIME | 8 | ENUM | 13 | STEPS |
| 4 | LFOHZ | 9 | BPM | 14 | SWING |
| | | | | 15 | FILT |

v5 formats: **SWING** 0..100 shown as the MPC swing, 50 % (straight) + value / 4 (so 75 % at 100);
**FILT** −64..63: 0 OFF, below 0 a low-pass closing (LP 1..100 %), above 0 a high-pass (HP 1..100 %).
**PCT** is the share of the range: value × 100 / max (rounded).

The editor should show the value with the unit; formatting it exactly like the device does
is not required.

## v2: user presets

A user preset = engine (0..NENGINES−1), name (1..12 chars, ASCII 32..126; the device shows it upper
case), all P_COUNT instrument parameters (v14 each, the same order as `DUMP`), and a 16-step pattern:
16 × (note 0..127 (0 = rest), flags: 1 accent, 2 slide, 4 tie). Loading one applies the engine and
the parameters of the sound; the pattern stored with it is never loaded (changing a sound never changes
the sequence), and the mix, pattern and key parameters stay (as `PRESET`). The slots are numbered 0..31
(the device shows U01..U32).

- `UP_LIST`: count is cut at 16 and at the last slot (start ≥ 32: count 0, no entries).
- `UP_GET` of an empty slot has the same shape with used 0, engine 0, name "" and all values 0.
  Values come back in the current parameter order, inside their ranges.
- `UP_PUT`: rc 1 for a slot ≥ 32, an engine ≥ NENGINES, a name that is empty, longer than 12 or has
  bytes outside 32..126, or a frame that is too short. Values are clamped to their ranges for that
  engine. A note with flag 4 is stored as a tie (note 0); flags on a rest are dropped.
- `UP_STORE`: name "" stores with the automatic name the device uses (engine name + slot number,
  "ANALOG 07"). rc 1 for a bad slot or name.
- rc 2 = the flash write failed or there is no flash; the slot is still changed in RAM until power-off.
- Frames stay below 640 bytes (`UP_PUT` is 5 + 1 + 1 + 13 + 2 × P_COUNT + 32 + 1).

**On the device:** SAVE > USER page: KNOB 1 picks the slot, KNOB 2 LOAD, KNOB 3 ERASE, KNOB 4 SAVE
(one detent arms, a second one within ~1.5 s acts, as PROJECT LOAD / SAVE). SAVE uses the automatic
name. SELECT and the SAVE > PRESETS browser continue past the factory presets into the used user
presets.

**Flash** (`firmware/src/upreset.c`): two storage objects (`OBJ_UPRESET0/1`, A/B sector pairs at
0xDC000..0xDFFFF), 16 records of 192 bytes each, behind a bank header (magic "UPB1", record size,
slot count; a mismatch reads as an empty bank). A record keeps its layout version (mismatch: empty)
and the P_COUNT it was stored with; another count is mapped by count (last 8 values = P_E0..P_E7, the
first ones = P_LEVEL.. in order, missing ones = defaults). P_COUNT was 53 (P_E0 45) until the SLICER
parameters (SLCR, PAT, RATE, DEPTH: ids 45..48) went in just before P_E0: P_COUNT 57, P_E0 49; SLOOP 2.0
added CHORD (id 49): P_COUNT 58, P_E0 50 (and G_COUNT 32: DUST, DUCK, FILT, ROLL, NEW at 27..31). An
editor takes them from `INFO`; older records load with the SLICER off and CHORD off. SLOOP 2.5 adds `G_DRDLY` (id 32,
GLO > DRUMS DLY, the drums' delay send): G_COUNT 33 (a 2.4 device says 32; the editor takes both). A project keeps it in
the byte after `sel` (format 5 unchanged; 0 in older projects).

## v2: live sync

- `WATCH 1` starts the pushes. Watching ends by itself 3 s after the last request of any kind (send
  `PING` about every 1 s), on a USB reset, and when the host goes away; `WATCH 0` ends it at once.
- **CHANGED** (scope, id, v14): a parameter changed on the device (knob, menu, sequencer edit of a
  `P_*`), not by the editor's own `SET`. Coalesced: each (scope, id) at most every 20 ms, with the
  latest value.
- **RELOAD** (engine, preset): the engine, a preset, a user preset or a project was loaded; re-read
  `DESC` of the engine parameters, `DUMP` and the steps. It is also sent after loads the editor asked
  for (`SET` of G_ENGSEL, `PRESET`, `PROJECT` load, `UP_LOAD`).
- **STEP_CHANGED** (index): a sequencer step changed on the device (record, clear, step edit,
  pattern load); not after the editor's own `STEP_SET`.
- Push frames have the normal header. Accept them at any time, also while waiting for a reply:
  match replies by cmd (23, 24 and 26 are never replies). The device sends at most a few per
  ~5 ms pass, and only when its USB send queue has room, so a push never delays a reply.

## v3: tracks

- The drum track: `DUMP` / `RELOAD` / `TRACK` give the engine byte NENGINES. Its `P_*` values exist
  (the pattern parameters `LEN DIV SWG GATE`, `PAN`, `MUTE` and `P_E0`, the kit, are used; the rest is
  ignored). `PRESET`, `SET` of `G_ENGSEL` and `UP_LOAD` do nothing there (`UP_LOAD` and `UP_STORE` answer
  rc 1). `DESC` of `P_E0` is the enum `KIT`; `P_E1..P_E7` describe engine 0. Its steps hold the 16 drum
  lanes (v5, `DRUM_STEP`); the v1-v4 step commands see them as GM notes (up to 4 per step).
- Selecting a track with `TRACK` does not push `RELOAD` (the editor re-reads `DUMP`, the steps and the
  engine `DESC` itself); selecting one on the device does (`RELOAD` with the new track).
- Pushes are about the selected track only: `CHANGED` (scope 0) and `STEP_CHANGED` refer to it, and
  changes to other tracks (live recording from MIDI into another track, `TRACK_*` writes) push nothing.
- Level and mute are also `P_LEVEL` / `P_MUTE` of the selected track (`SET`); `TRACK_MIX` reaches the
  others. Presets and user presets change a part's sound but keep its mix (`P_LEVEL`, `P_PAN`,
  `P_MUTE`), its pattern parameters (`LEN DIV SWG GATE`) and its key (`ROOT SCL QNT`, `CHORD`).
- Projects (`PROJECT`) save and load all four tracks and the selection (SLOOP 2.4: project format 5,
  "FUN5", format 4 + nudges and locks; formats 4 ("FUN4", SLOOP 2.0: the drum lanes, levels and ratchets),
  3, 2 and 1 from older firmware are converted when loaded, a format 1 project into track 1).
- Older firmware (no NTRK in `INFO`): one instrument; skip the track UI.

## v4: any track's parameters

- **Finding out:** send `WATCH 3`. v4 firmware answers 3; v3 (0.8) firmware answers 1, does not know
  cmds 31 / 32 (no reply) and never pushes `TRACK_CHANGED`. `WATCH 1` behaves exactly as in v2 / v3
  (reply 1, no `TRACK_CHANGED`). Match the `WATCH` reply by bit 0.
- `TRACK_PARAM` clamps like `SET` scope 0: to the range of that parameter; the engine parameters
  `P_E0..P_E7` to the ranges of that track's engine (the drum track: `P_E0` the kit, the others engine 0,
  as `DESC`). A parameter
  with a fixed range (min = max) keeps its value. A track ≥ NTRK or an id ≥ P_COUNT gets no reply.
  For the selected track it is the same as `SET` scope 0. The drum track's level is still `G_DRLVL`
  (`SET` scope 1 or `TRACK_MIX`); its `P_LEVEL` is not used.
- `TRACK_CHANGED` is never about the selected track (its changes stay `CHANGED` scope 0). Coalesced like
  `CHANGED` (each track and id at most every 20 ms, latest value), and not sent for the editor's own
  `TRACK_PARAM` / `TRACK_MIX` writes. After a selection change (`RELOAD`, or the editor's `TRACK`) the
  device takes the current values as known.

## v5: drum lanes, levels, ratchets (SLOOP 2.0)

- **Finding out:** `INFO` ends with 5. Older firmware ends after NTRK (or the engine names): use the
  v1-v4 commands only.
- **Drum lanes** (`firmware/src/drums.c` `LANE_NOTE`), one per white key from F3: 0 kick (36), 1 kick 2
  (35), 2 snare (38), 3 clap (39), 4 hat (42), 5 open hat (46), 6 pedal (44), 7 rim (37), 8 snare 2 (40),
  9 low tom (43), 10 hi tom (48), 11 crash (49), 12 ride (51), 13 shaker (70), 14 conga (63), 15 cowbell
  (56). A black key plays the lane of the white key left of it.
- **Levels** (2 bits): 0 NORM (as played), 1 GHOST, 2 SOFT, 3 HARD. **Ratchets** (2 bits): 0..3 = x1..x4
  hits in the step.
- **`DRUM_STEP`:** `on` is 16 bits (bit l = lane l), sent as 3 × 7 bits LSB first; `lvl` and `rat` are
  32 bits each (lane l in bits 2l..2l+1), sent as 5 × 7 bits LSB first. A set replaces the whole step;
  the lanes that are off read back with level and ratchet 0. An index ≥ NSTEP gets no reply. Not a push:
  `STEP_CHANGED` with the drum track selected means "re-read `DRUM_STEP` of that index".
- **The drum track through the old commands:** `STEP_GET` / `TRACK_STEP` give its first 4 lanes that are
  on as GM notes (lane order), time NOTE (REST if none), flag 1 (accent) if one of them is HARD, vel 100,
  and lvl / hi / rat 0. A `STEP_SET` / `TRACK_STEP` write puts each note on its nearest lane (GM 35..81,
  `LANE_OF_GM`), level HARD if the accent flag is set, else NORM, ratchet x1; time TIE / REST clears the
  step.
- **Synth steps:** `lvl` holds 2 bits per note (note k in bits 2k..2k+1, the same levels), `rat` 2 bits per
  note (x1..x4); both 8 bits, sent as 7-bit bytes with their top bits in `hi` (bit 0: lvl bit 7, bit 1:
  rat bit 7).
- **`TRACK`** ends with the solo mask (bit per track; GLO + key on the device). A soloed track plays,
  the others are faded out unless soloed too; mute and solo do not change `P_MUTE` of other tracks.
- **The kit** is the drum track's `P_E0`: `DESC` of `P_E0` with the drum track selected is the enum
  `KIT` (34 kits: ORIGINAL..DUST, the GM sample kit and its treatments, then the synthesised kits from
  808). `DESC` of `P_E1..P_E7` there still describes engine 0 (unused).

## v6: backup / restore (SLOOP 2.3)

`INFO` ends with 6. Objects: **0** the working project (a `project_t`, as the autosave), **1** the settings
(`persist_t`: colours, low cut, zoom, the panel calibration, the song order, the lights word: lights, SYNC,
the REC screen's mode and start, USB AUDIO and, since 2.4, MIDI OUT = SEQ (bit 14) and IN = CLOCK (bit 15)),
**2..5** the projects 1..4 (song sections A..D; length 0 = empty), **6..7** the user preset banks (`up_bank_t`,
16 records each; 0 = empty), **32..35** the user sample slots USR1..4 (35: SLOOP 2.4) (header + ADPCM data, as in flash; 0 =
empty). Numbers are 5 × 7 bit (u35, LSB first); data is pack7.

| cmd | Request args | Reply args |
| --- | --- | --- |
| 34 BK_LIST | — | rc (0 ok, 4 no flash), count, then per object: id, length u35, CRC-32 u35 (zlib). Takes a snapshot of the working project and the settings for GET |
| 35 BK_GET | id, offset u35, count (2 × 7 bit, 1..256) | id, rc (0 ok, 1 arguments, 5 the snapshot is gone: LIST again), offset u35, count, pack7 data |
| 36 BK_PUT | op 0 begin: id 0..9 (8: v9, 9: v10), length u35, CRC-32 u35 · op 1 data: id, offset u35, pack7 (≤ 256 bytes, in order) · op 2 commit: id · op 3 abort: id | op, id, rc: 0 ok, 1 arguments, 2 not a valid object (CRC, magic, sizes, ranges), 3 stop the song first (projects and preset banks; since 2.5 the settings too when their song order differs), 4 flash, 5 no begin for this object (or more than 15 s ago) |

A restore stages one object in RAM (the project load buffer), checks it at the commit as a load checks it
(projects: magic, size and sum, older formats converted; banks: magic, record size, slot count; settings:
magic, palette, a permutation of the buttons and knobs, a valid song order) and writes it through the usual
A/B commit; the working project is loaded at once (the song must be stopped). Samples are restored with
`SMP_BEGIN` / `SMP_WRITE` / `SMP_END` (the header is the first 480 bytes of the object, the data from byte
512), an empty slot with `SMP_ERASE`. The editor's file is JSON: `{format: "sloop-backup", version: 1,
firmware, date, objects: [{id, len, crc, data (base64)}]}`; it is checked (lengths, CRCs) before anything is
written.

**The song order (SLOOP 2.5, the editor's Song page)** is bytes 48..83 of the settings object: `arr_config_t`
(`firmware/src/arranger.h`): count (1..16), loop (0/1), 2 reserved, then 16 × {section 0..3 = A..D, bars
1..64}; bytes 0..3 are the magic "PER3" (0x50455233, little-endian). The editor reads object 1 (BK_LIST, BK_GET),
changes those 36 bytes only and writes it back (BK_PUT). The commit answers rc 2 for an invalid order and,
since 2.5, rc 3 while the song plays if the order changed (as the SONG screen saves only when stopped).

## v7: step nudge and parameter locks (SLOOP 2.4)

`INFO` ends with 7. Each step of a track has a **nudge** (micro timing, `track_t.micro`): −32..31 in 1/64 of the
step's length, negative = early (the step fires inside the previous grid step), positive = late, 0 = on the grid.
On the drum track it moves the whole step (every lane). Sent as one 7-bit byte offset by 64 (−32 → 32, 0 → 64,
31 → 95). Each track also holds up to 24 **parameter locks** (`plock_t`: step, param, value): on that step the
track's `P_*` parameter takes the lock's value, and goes back to what it was at the next step that does not lock
it (Elektron style; notes still ringing follow, the engines read the parameters every block). A knob turned on the
device while a lock is in force wins: the value found is kept as the new base. Several locks may share a step
(different parameters); one lock per (step, parameter).

- **Lockable parameters** (firmware `seq.c p_lockable`): `P_LEVEL` .. `P_LD_AMP` (ids 0..16: level, the envelope,
  its destinations, the LFO and its destinations), `P_SGATE` (32), the sends `P_DIST` .. `P_REV` (33..36),
  `P_GLIDE` (38), `P_PAN` (39), `P_DETUNE` (44), the SLICER `P_SLCR` .. `P_SLDEPTH` (45..48) and the engine's
  `P_E0` .. `P_E7` (50..57). Not lockable: the pattern (`LEN DIV SWG`), the arp, the key (`ROOT SCL QNT TRN CHORD`),
  the voice mode and its options (`VCE GLMOD PRIO ALLOC`), `MUTE`. `LOCK_SET` on one answers rc 2 and writes nothing.
- **Values** are clamped to the parameter's range on that track (the engine parameters to the engine's; the drum
  track's `P_E0` to the kits), as `TRACK_PARAM`. A load clamps them again and frees locks on a parameter that is
  not lockable or a step past 63.
- **Pushes:** a nudge or a lock changed on the device (SEQ + a step held + KNOB 4 / PRESETS, OCT− to clear) is a
  change of that step: `STEP_CHANGED` (index) for the selected track; re-read `MICRO_GET` / `LOCK_GET` (two
  requests) with the step. The editor's own `LOCK_SET` / `MICRO_SET` push nothing. While a lock is in force the
  parameter's live value is what `GET` / `DUMP` return and `CHANGED` reports (coalesced, as any knob).
- **Projects** (`PROJECT`, the backup object 0 and 2..5) are format 5 ("FUN5", 3816 bytes): format 4 plus, per
  track, 64 nudge bytes, 24 × 4-byte locks and 16 bytes of fill conditions (v8, below). Format 4 projects (SLOOP
  2.0 .. 2.3) load with no nudge, no lock and no condition.
- A device that does not know these commands (v6 and older) sends no reply: use `INFO`'s version byte.
- **On the device:** SEQ held + a step key held: KNOB 1 sound / note, KNOB 2 LEVEL, KNOB 3 RATCHET, KNOB 4 the
  step's nudge; PRESETS a lock of the lock parameter (the sound parameter last touched on a page, ENV > FLT at
  power-on; ALGORITHM steps through the lockable ones), made at the track's value, then moved; the title shows it
  ("lock dst 14"); OCT− clears the held steps' nudges and locks. A tile with a nudge or a lock carries a dot in its corner.

## v8: fill conditions and quick chain (SLOOP 2.4)

`INFO` ends with 8. Each step of a track has a **fill condition** (`track_t.fill`, 2 bits per step, `core.h FC_*`):
0 **normal** (plays always), 1 **FILL ONLY** (plays only during a fill), 2 **NO FILL** (silent during a fill); 3 is read
as 0. A fill is on while the player holds GLO + white key 9 (*fill*), or for one whole bar after GLO + key 10 (*bar*:
armed, the next bar plays as a fill, then off; pressed again before the bar: cancelled). A step whose condition fails is
skipped whole: no note, no MIDI OUT, no ratchet, no parameter lock of its own (the bases come back, as at a step without
locks); it still ends the previous step's notes as a REST would, and the pattern position moves on. On the drum track the
condition is the whole step (every lane). The arp and the rolls are not conditions' business. STOP ends a held and an
armed fill.

- `FILL_GET` returns the 16 bytes as stored, pack7 (16 bytes do not fit 7-bit SysEx bytes: three groups of a top-bits
  byte and up to 7 bytes, 19 bytes); `FILL_SET` writes one step's condition and answers it as stored. Neither pushes.
- **Pushes:** a condition changed on the device (SEQ + a step held + OCT+ cycles normal → fill only → no fill; OCT− resets
  it with the nudge and locks) is a change of that step: `STEP_CHANGED` (index) for the selected track; re-read
  `FILL_GET` with `MICRO_GET` / `LOCK_GET`.
- **Projects:** the 16 bytes sit after the locks in each track of format 5 (3816 bytes in all; the format kept its
  magic, 2.4 being unreleased: a FUN5 image of a 2.4 development build without the field, 3752 bytes, reads as empty).
- **On the screen:** a FILL ONLY step's tile carries a small **F** in its top left corner, a NO FILL step's an **×**; the
  nudge / lock dot stays in the top right. The GLO layer's row 3 reads *fill* (lit while a fill plays) and *bar* (framed
  while armed, lit during its bar).
- **Quick chain** (no protocol: on the device only): SAVE held while playing, two or more section keys (1–4) tapped in any
  order, up to 8 (repeats allowed), then SAVE let go: the first section starts on the next bar as before, then each next
  one after the previous one has played its **pattern length** (the longest track: ceil(LEN × step / bar) bars, at least
  1, from the section's project in RAM), round and round until a single section tap, STOP or PLAY in song mode. The SAVE
  layer's sub line reads *chain A B B C*; the section playing is lit, the next one framed. SONG REC records what the chain
  plays as it records any section change. A device that does not know these commands (v7 and older) sends no reply:
  use `INFO`'s version byte.

## v9: FM6 patches (SLOOP 2.4)

`INFO` ends with 9; the engine list gains **FM6** (engine 9, after GRAIN; SLICE, when built, is 10). The commands keep
Felucca 1.0's numbers (68–71) so the two editors stay close; firmware `editor_fm6.c`.

- **The patch** is the DX7 single-voice layout of 155 bytes (six operators of 21 bytes, the sixth first, then the voice:
  pitch envelope, algorithm 0–31, feedback, oscillator key sync, the LFO, transpose, the 10-character name). On the wire it
  is the **128-byte packed record** of a DX7 32-voice bank (every byte already 7-bit: no pack7). The editor's `FM6` object
  (`unpack`, `pack`, `sanitize`) is the reference; `web/test_web.mjs` checks it against the firmware's factory patches
  (`build/gen/felucca_fm6.h`).
- **Targets:** **0** a synth track's own patch (index 0–2; the drum track has none), **1** a bank slot (0–26, in flash),
  **2** a factory patch (0–7, read only).
- **rc:** 0 ok, 1 bad arguments or index, 2 an empty bank slot (GET) or a flash error (PUT / ERASE), 3 stop the song first
  (bank writes only: a flash erase silences the audio for ~50 ms).
- A **PUT to a track** plays at once and is that track's patch until the track loads another (a project, a preset, a user
  preset, PTCH turned). A project keeps PTCH (P_E7: F1–F8 = 0–7, B1–B27 = 8–34), not the patch: to keep an edited patch,
  PUT it to a bank slot and set PTCH to it. User presets work the same way.
- **The bank:** 27 packed records, one flash object (A/B sectors at 0xE5000 / 0xE6000, as every SLOOP object); in the
  backup it is object **8** (27 × 128 bytes; 0 = empty), restored before the projects. A backup from 2.3 has no object 8:
  the bank is left as it is.
- **SysEx files** are the editor's business, not the device's: a single voice (`F0 43 0n 00 01 1B`, 155 bytes, checksum,
  `F7`: 163 bytes) or a bank (`F0 43 0n 09 20 00`, 32 × 128, checksum, `F7`: 4104 bytes). Import unpacks, sanitizes every
  value into its range and PUTs the record; export packs. A wrong checksum is read but reported.
- **Store cartridge** (editor 2.5, no new command): the ticked voices of a bank go to B1..Bn in one write. The editor
  reads object 8 (`BK_LIST`, `BK_GET`), replaces records 0..n-1 (sets their `used` bits), keeps the others, and writes the
  whole `fm6_bank_t` back with `BK_PUT` (begin / data / commit; rc 3 while the song plays). One sector erase instead of n.
  A bank it cannot read (another layout) is never overwritten. If asked, it then writes a user preset per voice (`UP_PUT`,
  one each): engine FM6, the voice's name, every value at its default and PTCH (P_E7) = 8 + the bank slot.
- No pushes: after a PTCH change on the device (a `CHANGED` of P_E7) the editor re-reads the track's patch.
- A device that does not know these commands (v8 and older) sends no reply: use `INFO`'s version byte (the editor hides
  its FM6 panel).

## v10: the SYN drum kits (SLOOP 2.5)

`INFO` ends with 10. The drum track's kits end with **SYN1..SYN4** (drums.c `DRUM_SYN`): synthesised kits of the user's,
kept in RAM (the drum voices play them from there) and in flash after the settings (`persist_t`, then `dsu_bank_t`:
one A/B object; firmware before 2.5 reads its `persist_t` and leaves the rest). A sound is the firmware's `dsnd_t`
(drum_synth.c), 22 bytes in this order: wave (0 OFF, 1 SINE, 2 TRI, 3 SQUARE, 4 FM, 5 BELL), src (noise 0 OFF, 1 WHITE,
2 METAL, 3 CYM, 4 CHIP; | 16 CLAP), pitch (MIDI note), fine (1/16 semitones), bend (semitones, 0..96), btime (DECAY_K
index), hold (2 ms units), decay (DECAY_K), tlev, t2 (the second partial's ratio x 32, 0 none), t2lev, click, nlev, nhold
(2 ms), ndec (DECAY_K), flt (mode 0..3 OFF LP BP HP | 4 the tone too | resonance 0..31 << 3), fcut (CUTOFF_HZ index),
fenv (signed: CUTOFF_HZ steps), hpf (CUTOFF_HZ index, 0 off), chip (MIDI note of the chip noise's clock / 8), drive,
level (128 + 4 x dB). DECAY_K: 5 ms .. 4 s over 0..127 (exponential), CUTOFF_HZ: 30 Hz .. 16 kHz. Every value received
is put into range before a voice sees it.

| cmd | Request args | Reply args |
| --- | --- | --- |
| 72 DSYN_LIST | — | factory count (32), user count (4), stored (0: an edit not stored yet), the factory names, then per user kit: its name, the factory kit it started from |
| 73 DSYN_GET | which: a factory kit 0..31, or 64 + k (SYN k+1) | which, rc (1: no such kit), name, pack7 (crush, src, 16 x 22 bytes) |
| 74 DSYN_PUT | k 0..3, part, pack7 data | k, part, rc (0 ok, 1 arguments). part 0..15: a sound (22 bytes, in the synthesised lanes' order KICK SNARE CLAP HAT OPEN-HAT TOM-LO TOM-HI CRASH RIDE SHAKER CONGA RIM COWBELL CLAVE KICK-2 SNARE-2); 16: name (8 ASCII) + crush + src; 17: the whole kit from a factory kit (1 byte) |
| 75 DSYN_STORE | — | rc: 0 ok, 3 stop the song first, 4 flash |
| 76 DSYN_PLAY | k, lane 0..15, velocity 1..127 | k, lane, rc: the sound on the drum voices (at the next audio block) |

Backup object **9** is the four kits (`dsu_bank_t`, 1464 bytes: "DSU1", version 1, count 4, then per kit name 8,
crush, src, 2 reserved, 16 x 22). Its commit answers rc 3 while the song plays. A device before v10 answers the
begin of object 9 with rc 5: the editor skips it (as object 8 on a v8 device).

## v11: HYBRID patches (Clipper)

The HYBRID engine (`firmware/src/eng_hybrid.c`, engine 12) plays a 128-byte patch on each synth part
(`hy_patch_t`; the editor's `HY` module packs and unpacks it field by field). INFO ends with protocol 11.

| cmd | name | request | reply |
| --- | --- | --- | --- |
| 77 | HY_GET | target, index | target, index, rc, [pack7 of the 128 bytes] |
| 78 | HY_PUT | 0, part, pack7 of the 128 bytes | 0, part, rc |

- HY_GET target 0: the patch part `index` (0..2) plays, or the one sent to it and not played yet; target 1:
  factory patch `index` (the PTCH names come with the DESC of P_E3).
- HY_PUT: the part's own patch from its next audio block, every value put into range (`hy_sanitize`); it stays
  until PTCH turns or another sound loads. A project keeps PTCH only.
- rc: 0 ok, 1 a bad request (another target, part 3 = the drums, a short patch, a factory index past the end).

Layout (byte offsets; signed fields two's complement): oscillators 0..17 (6 each: type, coarse -48..48, fine -64..63,
level, shape, flags bit 0 reset / bit 1 fixed pitch), ring 18, drift 19, drive 20, route 21 (SER, PAR, SPLIT),
balance 22, filters 23..34 (6 each: type, cutoff, resonance, ENV2 amount -64..63, key tracking -64..63, velocity),
ENV2 35..38 and ENV3 39..42 (A D S R), LFO 2 and 3 43..50 (4 each: wave, rate, flags bit 0 key / bit 1 tempo sync,
fade), the matrix 51..114 (16 slots of source, via, destination, amount -64..63), bend range 115 (0..24), level 116,
0 at 117, the name 118..127 (ASCII, NUL-padded).

## v12: FMX patches (Clipper 0.6)

The FMX engine (`firmware/src/eng_fmx.c`, engine 19) plays a 128-byte patch on each synth part (`fmx_patch_t`, every
byte 0..127; the editor's `FX` module packs and unpacks it). INFO ends with protocol 12.

| cmd | name | request | reply |
| --- | --- | --- | --- |
| 90 | FMX_GET | target, index | target, index, rc, [pack7 of the 128 bytes] |
| 91 | FMX_PUT | 0, part, pack7 of the 128 bytes | 0, part, rc |
| 92 | FMX_STORE | slot, pack7 of the 128 bytes | slot, rc |

- FMX_GET target 0: the patch part `index` (0..2) plays; 1: factory patch `index` (0..15); 2: bank slot `index`
  (0..26): an FMX record as it is, a DX7 voice converted (`fmx_from_dx7`), an empty slot rc 2.
- FMX_PUT: the part's patch from its next block, every value put into range (`fmx_sanitize`); it stays until a sound
  loads or PTCH turns.
- FMX_STORE: into the FM6 bank's slot (the same 27 slots, `fm6_bank.c`), marked FMX's in the bank's spare word (FM6
  sees that slot empty); rc 3 while the song plays (the flash write silences the audio a moment), 2 a flash error.
- rc: 0 ok, 1 a bad request.

Layout: six operators 0..71 (12 each: wave 0..9 SINE TRI SAW SQR HALF ABS QUART PULSE SIN8 NOISE, mode bits 0-1 PM / FM /
RING and bit 2 FIX, coarse 0..31 (0 = x0.5; FIX: 0..3 = 1, 10, 100, 1000 Hz), fine 0..99, level, A D S R, velocity, key
scaling, detune 0..14 with 7 in tune), the routing 72..107 (`mod[to][from]`, 0..127; an operator into itself is its
feedback), the outputs 108..113, the template it came from 114 (1..40, 0 none), the pitch envelope 115 (64 none, +-24
semitones) and its time 116, the level 117 (100 unity), the name 118..127.

## v13: the MOD SEQ (Clipper 0.8)

INFO ends with 13. Each synth part (0 the instrument, 1 the layer) has a MOD SEQ (`seq.c mseq_tick`): four lanes of
16 steps, each -8..7, read by the matrix as the sources SEQ1..SEQ4 (`MS_SEQ1..MS_SEQ4`, 11..14). Its RATE, LEN, SMTH,
START (ids 83..86) and SPICE, DICE (87, 88: the arp's and the sequencer's notes vary) are parameters as any other
(GET / SET / DESC); P_E0 is 89, P_COUNT 97. The steps:

| cmd | name | request | reply |
|---|---|---|---|
| 93 | MSEQ_GET | part | part, rc, 64 bytes |
| 94 | MSEQ_PUT | part, 64 bytes | part, rc |

- the 64 bytes: lane 1's 16 steps, then lane 2's, 3's, 4's; each the value + 8 (0..15). MSEQ_PUT clamps each.
- rc 0 ok, 1 bad request (a part other than 0, 1; MSEQ_PUT without its 64 bytes).
- They go with the sound: a factory preset brings four shapes to start from; user presets keep them (the record's
  note / flags, `upreset.c up_ms_put`; their RATE .. DICE in the bank's side record) and projects (track 4's free lock
  slots; RATE .. DICE after 0.6's INSERT in track 3's). UP_GET sends note / flags as they are.
- 0.8 also has the engines JUNO (21) and VECTOR (22), and the INSERT's types JUNO1, JUNO2, JUN12 (9..11: the JUNO
  chorus on any sound; A WIDTH, B TONE, C NOISE).

## v14: the TUNING (Clipper 0.9)

INFO ends with 14. 0.9 adds two parameters before P_E0: VINTAGE (89, VOICE 2: each voice's own tuning, drift and
cutoff, 0..127) and LOOP (90, the SPICE page: 1..16, how many notes SPICE / DICE and the TURNG arp repeat); P_E0 is 91,
P_COUNT 99. The filter's MODL (ANALOG, WAVE, VECTOR) goes SVF, MOOG, MS20, DIODE, SEM, NOTCH, BP, HP (1..8); the arp's
MODE gains TURNG (15: a Turing machine over the held notes; SPICE its MUTATion, DICE its SEED, LOOP its length).

The FM-1's tuning (GLO > TUNING: the system and its ROOT, every sound) is kept with the settings; the USER system plays
a scale sent here (a Scala .scl file: the editor reads it):

| cmd | name | request | reply |
|---|---|---|---|
| 95 | TUNE_GET | - | rc, system, root, n, period (3 bytes), n - 1 degrees (3 bytes each), (Clipper 1.0) A4 - 400 Hz |
| 96 | TUNE_PUT | system, root, n, period (3 bytes), n - 1 degrees (3 bytes each) [, A4 - 400 Hz] | rc |

- system 0..13: EQUAL, JUST, PYTHA, MEANT, WERCK (Werckmeister III), KIRNB (Kirnberger III), RAST, BAYAT, SLEND,
  PELOG, 19EDO, 24EDO, 31EDO, USER. root 0..11 (C..B): the note that is 1/1 (the tables) or C4 + root (USER).
- The USER scale: n 1..48 degrees (the first, 0, not sent), the period and the degrees in 1/16 cent, each 21 bits as
  3 bytes of 7 bits (low first). The degrees rise and stay below the period (16..65535). Each key plays the next degree;
  a period other than 1200 cents (the tritave of Bohlen-Pierce, ...) is kept as it is.
- (Clipper 1.0) the reference A4, 400..480 Hz, as one byte (0..80) after the degrees: GET always sends it, PUT takes it
  optionally (without it, A4 stays as it is). 0.9 sends none and refuses a PUT with it: send it only when GET gave one.
- rc 0 ok, 1 bad request (a system / root out of range, n 0 or above 48, the bytes not as n says, degrees not rising).
  TUNE_PUT keeps the scale even when another system plays (USER plays it); the settings are written a few seconds later.

## Clipper 1.0: the same protocol (14), a larger layout

No new command: INFO still ends with 14 and reports the layout. 1.0 adds 27 parameters before P_E0 and one global:

| ids | what |
|---|---|
| 91 | SPREAD (VOICE 3, 0..127: each voice's place in the stereo field) |
| 92..94 | LFO 3: RATE, WAVE (SIN TRI SAW SQR S&H), MODE (FREE, KEY) |
| 95 | ENV 2 LOOP (ONE, LOOP) |
| 96..99 | the MACROS: BRITE, TIMBR (-64..63), MOTN, SPACE (0..127) |
| 100..117 | the matrix's slots 7..12 (SRC, DST, AMT each, as slots 1..6 at 60..77) |
| global 33 | G_DMODE, the delay's MODE (DLY 2: MONO, PING, TAPE) |

P_E0 is 118, P_COUNT 126, G_COUNT 34. The matrix's sources gain LFO3 (15) and MAC1..MAC4 (16..19), in every slot. The
reverb's TYPE (global 28) goes ROOM, PLATE, HALL, SHIMMER (0..3). A sound of an older layout maps by count (the editor's
patchByCount: the common values by place up to the older P_E0, the engine's 8 by theirs, the rest left as the device has
them). UP_PUT carries P_COUNT values as before (126).

## Forge Five 0.1.1: the FORGE extension, the screen mirror

The device's screen to a host app over the same port: every canvas the UI sends to the panel and every solid fill also
goes out as FG_RECT pushes, only what the host does not have yet (`firmware/src/fg_mirror.c`). The commands are those of
the FORGE extension, a range shared by the Forge firmwares (FORGE ONE, Forge Phoenix, Forge Five): 112..127, append-only;
a command there that a firmware lacks gets no reply, as any other. Forge Five answers FG_ID, FG_WATCH and FG_SCREEN and
sends FG_RECT; the rest of the range (Forge Phoenix's generator, actions and playhead) is not this firmware's. Framing as
above: `F0 7D 46 4C <cmd> ... F7`; "2 bytes" = 7 bits each, low first; **v14** = value + 8192 in 2 bytes.

| Cmd | Name | Request | Reply |
|---|---|---|---|
| 112 | FG_ID | (none) | firmware (**3: Forge Five**; 1 FORGE ONE, 2 Forge Phoenix), lineage (**1: Clipper**; 0 Felucca), extension version (1), caps (v14), name (ASCII, 0-terminated: the version string) |
| 118 | FG_WATCH | mask [, which] | the mask now set. Bit 3: the screen's pushes, the only bit this firmware takes. With `which`, only the bits in it change (two apps on one device, each its own) |
| 122 | FG_SCREEN | op [, mode [, host]] | op, rc; op 3: then messages, bytes, drops, passes (5 bytes each, 7 bits, low first), the mode (1 byte), the leases (1 byte, bit h) |
| 123 | FG_RECT (push) | | x, y, w, h, row (2 bytes each), flags, then pack7 of the RLE |

- **caps** (what this firmware supports): bit 1 the screen mirror, bit 5 its leases and FG_RECT's mode bit, bit 6 it
  heals its own losses. Forge Five: **98** (bits 1, 5, 6). Bits 0, 2, 3 (generator, input, playhead) stay clear.
- **FG_SCREEN**: op 0 off, 1 on (mode 0: 240 x 240 pixels, 1: 120 x 120 half pixels, each the mean of a 2 x 2 block),
  2 the whole screen at the next pass, 3 the counters. rc 0 ok, 1 bad arguments (op above 3, mode above 1, host 7 and
  up, op 1 without a mode), 2 not now (op 2 or 3 while off).
- **Leases** (two apps on one device, e.g. a companion app's screen page beside a separate mirror app): **host 1..6**
  names an app. op 1 with a host takes that host's lease and sets the mode (the most recent op 1 wins; a mode change
  sends the whole screen again, in the new mode); op 0 with a host gives back that host's lease only; any FG_SCREEN
  with the host renews it; a lease ends **5 s after that host's last FG_SCREEN**. The mirror is on while any lease is
  live. Without a host byte: lease 0, kept by any request within 5 s; op 0 turns the mirror off for every app. A USB
  reset or unplug turns it off.
- **FG_RECT**: the rect in the mode's pixels; `row` = the rect's row this message starts at, its pixels fill the
  rect's rows from there, left to right. flags: bit 0 the rect's last message, bit 1 the end of a draw pass (show the
  picture now; such a message may carry no rect: w = h = 0), bit 2 the stream is in mode 1 (on every message of it,
  whoever chose the mode). A rect is at most 120 wide (wider draws go as strips).
- **RLE** (at most 256 bytes a message, before pack7): a byte 0..63 = n + 1 pixels follow; 64..127 = the next pixel
  n - 63 times. A pixel: 2 bytes, RGB565, high byte first. **pack7**: groups of up to 7 bytes, each led by a byte
  holding their top bits (bit k = byte k's bit 7).
- **What goes**: a draw pass at most every 33 ms, and only what the host does not have (a table of hashes of 15-pixel
  blocks of each row); a pass with nothing new sends nothing. A draw that cannot go (the ring full) leaves its area
  owed; the next draw of it sends it, and an area still owed once the drawing settles gets a whole redraw of the page
  (at most twice a second), which also sends only what the host lacks. So an app should **not** send op 2 after the
  drop counter rises (caps bit 6); op 2 every 10 s or more as a safety net is fine.
- **Transport**: whole messages go into an 8 KB ring; the USB poll sends them after the editor's replies and pushes
  and the MIDI out, never in the middle of either, and finishes a started message before anything else. The main loop
  never waits for room beyond the panel's own transfer; nothing of it runs in the audio interrupt.
- **Session**: FG_ID (caps bit 1), `FG_SCREEN 1, mode, host`, `FG_WATCH 8, 8`, `FG_SCREEN 2, 0, host`; apply every
  FG_RECT, show the picture at flags bit 1 (size it from bit 2); `FG_SCREEN 3, 0, host` every second or two while
  showing; `FG_SCREEN 0, 0, host` when done.

Measured on the host (`tests/ff_screen_test.c`: the real UI, the real ring and pace, a link drained at 96 or 54 KB/s,
5 s with the arpeggiator playing a held chord; an app renewing every 1.5 s, op 2 every 10 s):

| page, mode | 96 KB/s: KB/s, pictures/s, B a picture, drops | 54 KB/s |
|---|---|---|
| HOME, 0 (240 x 240) | 75.4, 17.2, 4,385, 0 | 47.2, 11.0, 4,287, 1 |
| HOME, 1 (120 x 120) | 32.5, 17.6, 1,844, 0 | 31.7, 17.6, 1,803, 0 |
| the visualiser, 0 | 76.2, 16.4, 4,649, 0 | 48.6, 11.0, 4,414, 3 |
| the visualiser, 1 | 32.0, 17.6, 1,817, 0 | 31.7, 17.6, 1,803, 0 |

Once the load stops, the host's picture is the panel's (mode 0 every pixel, mode 1 every half pixel), on every page of
a walk too, at either rate.

## Notes for the editor

- **One request at a time.** Wait for the reply, about 10–50 ms, before sending the next.
  The device holds only one incoming SysEx frame.
- **Following the device.** With v2 firmware, `WATCH` and `PING` (above). Older firmware pushes
  nothing (no reply to `PING`): poll `DUMP` about every 300–500 ms while the page is visible.
- **Port.** The device's MIDI port is named "Felucca" (USB 1209:0001; SLOOP keeps the name so editors
  and installers find it). Updates use the same
  port with other SysEx (the `F0 22 24 35 …` keys, `00 59 …` frames); never send those
  from the editor. Since SLOOP 2.3 (after Felucca 1.0) the same USB device also has an audio input
  ("Felucca", 44.1 kHz stereo; bcdDevice 3.11): the MIDI port and this protocol are unchanged, and both
  work while the computer records.
- **Safety.** Only `PROJECT` save, the sample-slot commands and `UP_PUT` / `UP_STORE` / `UP_ERASE` write flash, and only in
  Felucca's own storage; never the app or the update area. Since SLOOP 2.4 every one of them, and
  `BK_PUT` of a preset bank, answers rc 3 while the song plays (as the panel refuses to save then): a
  flash erase silences the audio for about 50 ms.
