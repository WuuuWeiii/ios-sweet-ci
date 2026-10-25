#!/bin/bash
# Build the libdrm stand-in (sweet_drm.c) for iOS arm64e and ad-hoc sign it.
# From Bergamota gpu/freedreno-sweet (MIT); retargeted to iOS 16.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:-$HERE/out}"
SDK="$(xcrun --sdk iphoneos --show-sdk-path)"
mkdir -p "$OUT"
xcrun --sdk iphoneos clang -arch arm64e -miphoneos-version-min=16.0 -isysroot "$SDK" \
    -O2 -g -Wall -Wextra -fvisibility=hidden \
    -dynamiclib -install_name @rpath/libdrm.2.dylib -current_version 2.4.0 -compatibility_version 2.0.0 \
    "$HERE/sweet_drm.c" -o "$OUT/libdrm.2.arm64e.dylib" \
    -framework IOKit -framework CoreFoundation
cp -f "$OUT/libdrm.2.arm64e.dylib" "$OUT/libdrm.2.dylib"
codesign -s - -f "$OUT/libdrm.2.dylib" 2>/dev/null || true
ls -l "$OUT"
