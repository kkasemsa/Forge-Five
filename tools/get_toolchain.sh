#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Download JieLi's Linux toolchain (clang 4.0.1 for pi32v2) and link it as DEST/toolchain.
#   tools/get_toolchain.sh [DEST]      (default: ~/.jieli)
# Forge Five (from FORGE ONE 0.9.1): the archive is saved first and its SHA-256 checked against JIELI_TOOLCHAIN_SHA256 (the value
# below, or the environment's) before anything is extracted; the digest is printed either way, so a new
# archive's can be pinned. JIELI_TOOLCHAIN_SHA256=skip extracts an unknown one (prints a warning).
set -e
DEST="${1:-$HOME/.jieli}"
URL=https://pkgman.jieliapp.com/s/linux-toolchain
PIN_DEFAULT=a1545cddbc451a06bac43eb5cd96d831470c42a97c5c0b1f98879a20fc7647bd   # jieli-linux-toolchains-20260730.1, 26027508 B (CI, 2026-10-22)
PIN="${JIELI_TOOLCHAIN_SHA256:-$PIN_DEFAULT}"
mkdir -p "$DEST"
ARC="$DEST/linux-toolchain.tar.xz"
curl -fL --retry 3 -o "$ARC" "$URL"
SUM="$( (sha256sum "$ARC" 2>/dev/null || shasum -a 256 "$ARC") | cut -d' ' -f1)"
echo "toolchain archive sha256 $SUM ($(wc -c < "$ARC") B)"
if [ -z "$PIN" ]; then
    echo "get_toolchain: no JIELI_TOOLCHAIN_SHA256 to check it against (set it to this digest, or to skip)" >&2
    exit 1
elif [ "$PIN" = skip ]; then
    echo "get_toolchain: WARNING: the archive's digest is not checked (JIELI_TOOLCHAIN_SHA256=skip)" >&2
elif [ "$PIN" != "$SUM" ]; then
    echo "get_toolchain: the archive's sha256 is not the pinned one ($PIN): not extracted" >&2
    rm -f "$ARC"
    exit 1
fi
tar -xJ -C "$DEST" -f "$ARC"
rm -f "$ARC"
TC="$(ls -d "$DEST"/jieli-linux-toolchains-* | sort | tail -1)"
ln -sfn "$(basename "$TC")" "$DEST/toolchain"
echo "JIELI_TOOLCHAIN=$DEST/toolchain ($(basename "$TC"))"
