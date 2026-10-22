#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Khantipol Kasemsant; built on Felucca, (C) 2026 Leo Kuroshita, Hügelton Instruments
# Forge Five: the distribution folder of a build (CI's artifact, a release's zip):
#   tools/ff_dist.sh DIR [commit] [ref] [run] [tests]
# DIR gets forge-five.fwsc, forge-five.bin, loader/ota.bin, SHA256SUMS, MANIFEST.txt (what was built, from what, how),
# LICENSE, LICENSING.md, LICENSES/, ATTRIBUTION.txt (the CC0 samples), README.md, BUILDING.md, docs/FORGE-FIVE.md and
# the test log. Run it after ./build.sh and the tests.
set -e
cd "$(dirname "$0")/.."
D=${1:?usage: ff_dist.sh DIR [commit] [ref] [run] [tests]}
COMMIT=${2:-$(git rev-parse HEAD 2>/dev/null || echo unknown)}
REF=${3:-$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo unknown)}
RUN=${4:-local}
TESTS=${5:-$([ -f build/test.log ] && echo "see test.log" || echo "not run")}   # (CI passes "passed": it only gets here then)
PKG=$(ls build/forge-five-*.fwsc build/forge-five.fwsc 2>/dev/null | head -1)   # (a release build: forge-five-X.Y.fwsc)
[ -n "$PKG" ] || { echo "ff_dist: build/forge-five.fwsc is missing (run ./build.sh first)" >&2; exit 1; }
rm -rf "$D"
mkdir -p "$D/loader" "$D/LICENSES" "$D/docs"
cp "$PKG" "$D/forge-five.fwsc"
cp build/forge-five.bin "$D/forge-five.bin"
cp build/loader/ota.bin "$D/loader/ota.bin"
cp LICENSE LICENSING.md README.md BUILDING.md CREDITS.md "$D/"
cp docs/FORGE-FIVE.md "$D/docs/"
cp LICENSES/*.txt "$D/LICENSES/"
[ -f assets/samples-cc0/ATTRIBUTION.txt ] && cp assets/samples-cc0/ATTRIBUTION.txt "$D/ATTRIBUTION.txt"
[ -f build/test.log ] && cp build/test.log "$D/test.log"
UV=$(sed -n 's/^#define FELUCCA_VERSION *"\([^"]*\)".*/\1/p' firmware/src/felucca.c)
{
    echo "Forge Five $UV (on Felucca 1.6.1)"
    echo "package     forge-five.fwsc  $(wc -c < "$PKG") B"
    echo "app         forge-five.bin   $(wc -c < build/forge-five.bin) B"
    echo "loader      loader/ota.bin   $(wc -c < build/loader/ota.bin) B"
    echo "commit      $COMMIT"
    echo "ref         $REF"
    echo "run         $RUN"
    echo "built       $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "engines     $(git -C firmware/engines rev-parse HEAD 2>/dev/null || echo unknown) (Felucca-Engines, MIT)"
    echo "build flags $(env | grep -o '^FELUCCA_[A-Z0-9_]*=[0-9]' | sort | tr '\n' ' ')(unset: the defaults in firmware/src/felucca.c)"
    echo "host tests  $TESTS"
    echo "source      https://github.com/kkasemsa/Forge-Five (branch main); upstream https://github.com/hugelton/Felucca"
    echo "licence     GPL-3.0-only (LICENSE, LICENSING.md, LICENSES/)"
} > "$D/MANIFEST.txt"
(cd "$D" && sha256sum forge-five.fwsc forge-five.bin loader/ota.bin MANIFEST.txt > SHA256SUMS)
echo "dist: $D ($TESTS)"; cat "$D/MANIFEST.txt"
