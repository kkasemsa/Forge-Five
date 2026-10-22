#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Forge Five CI: the errors of a failed ./build.sh (build.log) as GitHub annotations, one per compiler error or
check failure, with the lines that follow (the context of a clang error), so they show on the run's summary page
and through the API.    build_failures.py build.log"""
import re
import sys

BAD = re.compile(r"error:|\bFAIL\b|fatal|undefined (reference|symbol)|Traceback|SystemExit|build: |is empty|not found")


def esc(s):
    return s.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")


def main(path):
    lines = open(path, errors="replace").read().splitlines()
    n = 0
    for i, line in enumerate(lines):
        if BAD.search(line) and n < 30:
            ctx = "\n".join(l[:300] for l in lines[i:i + 4])
            m = re.match(r"([^:\s]+):(\d+):", line)
            where = f"file={m[1]},line={m[2]}," if m else ""
            print(f"::error {where}title=build::{esc(ctx)}")
            n += 1
    tail = "\n".join(l[:300] for l in lines[-40:])
    print(f"::error title=build.log tail::{esc(tail)}")


if __name__ == "__main__":
    main(sys.argv[1])
