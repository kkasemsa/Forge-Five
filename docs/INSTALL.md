# Installing Forge Five on the FM-1

Nothing to build on the Mac: GitHub builds the firmware (`.github/workflows/build.yml`), you download the package and
send it to the FM-1 over USB-MIDI with one Python script. Installing is at your own risk; the way back to M-VAVE's
firmware is at the end.

## 1. Get the package

**A green build of `main`** (any push): open
[Actions](https://github.com/kkasemsa/Forge-Five/actions), pick the latest run with a green tick, scroll to
**Artifacts** and download `forge-five-<sha>`. Unzip it: `forge-five.fwsc` is the package, `MANIFEST.txt` says what
it is (version, commit, build flags, "host tests passed"), `SHA256SUMS` lets you check the download:

```
cd ~/Downloads/forge-five-<sha>
shasum -a 256 -c SHA256SUMS
```

**A release** (a tag `ff-X.Y`): [Releases](https://github.com/kkasemsa/Forge-Five/releases) carries
`forge-five-X.Y.fwsc`, the same files zipped, and a source tarball.

A red run publishes `forge-five-FAILED-<sha>` with the logs instead; there is nothing to install in it.

## 2. Get the installer

`tools/fm1_install.py` is one file with no dependencies beyond two packages. From a clone of the repository, or on its
own:

```
pip3 install mido python-rtmidi
curl -fLO https://raw.githubusercontent.com/kkasemsa/Forge-Five/main/tools/fm1_install.py
```

(Felucca's web installer at hugelton.github.io installs Felucca's own releases only; for your own package use the
script, or a local copy of the site as `BUILDING.md` describes.)

## 3. Before the first install

- Keep M-VAVE's **`FM-1.fwsc` (V15)** somewhere safe: it is the way back to the stock firmware (step 6).
- Connect the FM-1 by USB. Quit anything holding its MIDI port (Ableton Live, Move's connection, a DAW, MIDI
  monitors): the installer needs the port to itself.
- See that the FM-1 answers (reads only, writes nothing):

  ```
  python3 fm1_install.py --info
  ```

  It prints the identity of the firmware on it (`FM-1_015` stock V15, `FM-1_900` a Clipper or Forge Five build without `--release`,
  `FM-1_9XY` a release X.Y). "FM-1 not found": another program holds the port, or the cable is charge-only.

- Know the recovery route. Forge Five has not run on hardware yet and an install that fails half-way can leave the FM-1 in its boot
  loader (**WL80UBOOT** on the USB bus, a dark screen). The installer itself resumes a cut-off install when run again;
  if the unit no longer starts at all, [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter) (a Seeed
  XIAO RP2040 wired to the board) brings it back. Install on a unit you can afford to have down for a while.

## 4. Install

```
python3 fm1_install.py forge-five.fwsc
```

It shows the package's identity and the FM-1's, asks once (`--yes` skips the question), then: step 1, the running
firmware reads the loader out of the package and restarts into it; step 2, the loader writes the whole package;
the FM-1 restarts and the script checks the installed identity. Two to three minutes; the screen goes dark in between,
that is normal. Keep the cable in until the script says it is done.

If it stops ("connection lost", a timeout), run the same command again: a loader that is already on the unit finishes
the write. `--port NAME` picks the MIDI port when more than one FM-1-like port is present.

Exit codes: 0 done · 1 cancelled · 2 bad package · 3 FM-1 not found · 4 connection lost · 5 timeout · 6 wrong model or
another identity after the install.

## 5. Check the unit

- The boot screen shows the Forge Five mark, the version (`0.1 DEV` or the release's) and "based on clipper + felucca";
  HOME held > SYS > ABOUT shows the credits.
- The sounds: SAVE > PRESETS, KNOB 2 picks an engine (ALL, ANALOG, HYBRID, WAVE, DCO, LADDER, CHORD-D, BRASS, USER),
  KNOB 1 or the PRESETS knob goes through its sounds; play from Move on channel 1 (every channel plays the instrument
  until a layer or split is on).
- GLO > SYSTEM shows the CPU. The measures the voice budget was set from (23 Oct 2026, both cores): nothing playing
  14 %; LADDER FAT LEAD on POLY, 3 notes 41 %, 4 notes 44 %; BRASS STACK BRASS, 5 notes 45 %, 8 notes 56 %; CHORD-D
  DEEP PAD, 2 keys (8 voices) 64 %; RIBBON NOIR BRASS (25 Oct), 4 notes 38 %, 8 notes 53 %. Above ~80 % is
  worth reporting, with the sound and the number of notes.
- Settings, user presets and projects live in flash outside the firmware image. A Forge Five install over Clipper keeps
  Clipper's: sounds of the engines Forge Five leaves out load silent (ENG_OFF). Take a backup from the web editor first.

## 6. Back to Clipper, Felucca or stock

Any Felucca-family package (Clipper, SLOOP, Felucca) installs the same way (`python3 fm1_install.py clipper-1.1.fwsc`). The unmodified
official V15 is accepted without `--force`:

```
python3 fm1_install.py FM-1.fwsc
```

Anything else without Felucca's loader mark is refused unless `--force` is given, on purpose.

## Troubleshooting

| Symptom | Likely cause, what to do |
| --- | --- |
| `FM-1 not found` | The port is taken (quit Live / Move / MIDI apps) or the cable carries no data. `--info` again. |
| `no Felucca update loader in this package` | Not a Forge Five / Felucca package, or a corrupted download: check `SHA256SUMS`. |
| Timeout waiting for the loader or the restart (exit 5) | Unplug, plug, run again; the loader resumes. |
| Dark screen, `WL80UBOOT` on the USB bus, the installer cannot reach it | The boot loader: FM-1-transporter (above). |
| Another identity after the install | The write did not complete: run the install again. |
| `ModuleNotFoundError: mido` | `pip3 install mido python-rtmidi` for the same `python3` you run the script with. |
