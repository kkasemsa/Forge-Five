#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Khantipol Kasemsant
"""Forge Five (spec v0.3 §7): no brand or model names in what the user sees or in the record. Checks
  - every string literal in firmware/src/*.c and *.h (the UI, engine and preset names, messages),
  - the Markdown of README.md and docs/,
  - the web editor's text (web/*.html, web/*.js, web/*.mjs string literals),
  - and, given --commits A..B, the messages of those commits (Forge Five's own: not Clipper's history, UPSTREAM_BASES),
for Moog (Minimoog ...), Oberheim, Roland, Sequential, Yamaha, PPG, Korg, "Model D", "SEM", Juno, Prophet, Jupiter
and "CS-80" (as words, any case). Comments in the C
sources are not checked: Clipper's history stays as it is. tools/brand_allow.txt lists lines that may pass
(one regular expression a line; a '#' line is a note), such as Clipper's engine names that a later phase renames.
    tools/brand_check.py [--commits RANGE]       exit 1 on a hit"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BRAND = re.compile(r"\b(\w*moog|oberheim|roland|sequential|yamaha|ppg|model[ -]?d|sem|juno\w*|jun12|prophet|proph|jupiter|cs[ -]?80|korg|ob[ -]?x\w*|ms[ -]?20|taurus|cz[ -]?\d*)\b",
                   re.I)                                 # (\w*moog: Minimoog, Micromoog too; the nicknames
                                                         # and model numbers of presets too: OBX, PROPH, MS20, CZ)
STR = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
UPSTREAM_DOCS = {"CLIPPER-README.md", "release-1.0.md", "release-1.1.md", "test-sequencer.md"}
# Clipper's (and SLOOP's, Felucca's) history up to the commit Forge Five is built on: their words, not ours. The rule
# is for Forge Five's own commits, so a range that reaches back into it (a push over an unrelated branch) stops there.
# A later merge of Clipper adds its new commit here.
UPSTREAM_BASES = ["e69067211c4690f34e494b5b896e4585163dcc2b"]       # isod89/clipper-fm1, Clipper 1.1


def allowed():
    p = ROOT / "tools" / "brand_allow.txt"
    if not p.exists():
        return []
    return [re.compile(l.strip()) for l in p.read_text().splitlines() if l.strip() and not l.startswith("#")]


def main(argv):
    allow = allowed()
    hits = []

    def check(where, text):
        if BRAND.search(text) and not any(a.search(text) for a in allow):
            hits.append(f"{where}: {text.strip()[:120]}")

    for f in sorted(list((ROOT / "firmware" / "src").glob("*.c")) + list((ROOT / "firmware" / "src").glob("*.h"))):
        src = f.read_text(errors="replace")
        src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)          # (comments: not the user's)
        src = re.sub(r"//[^\n]*", "", src)
        for i, line in enumerate(src.splitlines(), 1):
            for m in STR.finditer(line):
                check(f"{f.relative_to(ROOT)}:{i}", m.group(1))
    for f in [ROOT / "README.md"] + sorted((ROOT / "docs").glob("*.md")):
        if f.name in UPSTREAM_DOCS or not f.exists():          # (Clipper's and SLOOP's own words, kept as they are)
            continue
        for i, line in enumerate(f.read_text(errors="replace").splitlines(), 1):
            check(f"{f.relative_to(ROOT)}:{i}", line)
    for f in sorted((ROOT / "web").glob("*.html")) + sorted((ROOT / "web").glob("*.js")) + sorted((ROOT / "web").glob("*.mjs")):
        for i, line in enumerate(f.read_text(errors="replace").splitlines(), 1):
            for m in STR.finditer(line):
                check(f"{f.relative_to(ROOT)}:{i}", m.group(1))
    if len(argv) == 3 and argv[1] == "--commits":
        rng = argv[2]
        if rng.startswith("0" * 40):                           # (a new branch: no "before"; the last 20 commits then)
            rng = "-20"
        bases = [b for b in UPSTREAM_BASES
                 if subprocess.run(["git", "cat-file", "-e", b + "^{commit}"], cwd=ROOT, capture_output=True).returncode == 0]
        if not bases:                                      # (a shallow checkout: the range could not be bounded)
            print("brand check: the Clipper base commit is not in this checkout (fetch the whole history: "
                  "actions/checkout fetch-depth: 0); the commits cannot be checked", file=sys.stderr)
            return 2
        notup = ["--not"] + bases
        r = subprocess.run(["git", "log", "--format=%H%n%B%x00", rng] + notup, cwd=ROOT, capture_output=True, text=True)
        if r.returncode:                                   # (the commit before a force-push may be gone: the last 20 of ours)
            print(f"brand check: {rng} not in this checkout; the last 20 commits of Forge Five instead", file=sys.stderr)
            r = subprocess.run(["git", "log", "--format=%H%n%B%x00", "-20"] + notup, cwd=ROOT, capture_output=True, text=True)
            if r.returncode:
                print("brand check: git log failed: " + r.stderr.strip(), file=sys.stderr)
                return 2
        log = r.stdout
        for rec in log.split("\0"):
            if rec.strip():
                sha, _, msg = rec.strip().partition("\n")
                for line in msg.splitlines():
                    check(f"commit {sha[:9]}", line)
    for h in hits:
        print("brand:", h)
    print(f"brand check: {len(hits)} hit(s)" if hits else "brand check: clean")
    return 1 if hits else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
