#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Forge Five CI: turn the failures in a tests/run_tests.sh log into GitHub annotations, one per failing
section (the "== name" line before them), so they show on the run's summary page and through the API.
    test_failures.py build/test.log"""
import re
import sys

BAD = re.compile(r"\bFAIL|\bfailed\b|\berror:|Assertion|Segmentation fault|Aborted|undefined reference|over budget")


def esc(s):
    return s.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")


def main(path):
    sec, found = "(before the first section)", {}
    for line in open(path, errors="replace"):
        line = line.rstrip("\n")
        if line.startswith("== "):
            sec = line[3:]
            continue
        if BAD.search(line) and not line.rstrip().endswith(" ok") and "ALL HOST TESTS PASSED" not in line:
            found.setdefault(sec, []).append(line.strip()[:300])
    for sec, lines in list(found.items())[:9]:
        body = "\n".join(lines[:15]) + ("\n..." if len(lines) > 15 else "")
        print(f"::error title={esc(sec[:120])}::{esc(body)}")
    if len(found) > 9:
        print(f"::error title=more failures::{len(found) - 9} more failing sections (see build/test.log)")
    tail = open(path, errors="replace").read().splitlines()[-25:]
    print(f"::warning title=log tail::{esc(chr(10).join(t[:200] for t in tail))}")


main(sys.argv[1])
