#!/bin/bash
# Build libsweetgl.dylib: HakiOS's EAGLContext replacement (src/eagl.c + src/gl_osmesa.c)
# behind iOS's OpenGL ES, linked against our Mesa OSMesa build in $GPU_EAGL_WORK/out.
#   GPU_EAGL_WORK=... bash gpu/eagl/build-eagl.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="${GPU_EAGL_WORK:?set GPU_EAGL_WORK}"
OUT="$WORK/out"
SDK="$(xcrun --sdk iphoneos --show-sdk-path)"
NM="$(xcrun -f llvm-nm)"
cd "$HERE"

if [ ! -f gl.xml ]; then
  curl -sSL --fail -o gl.xml https://raw.githubusercontent.com/KhronosGroup/OpenGL-Registry/main/xml/gl.xml
fi
"$NM" -gU "$OUT/libOSMesa.8.dylib" | awk '$3 ~ /^_gl/ {print substr($3,2)}' | sort -u > exported-gl.txt
wc -l exported-gl.txt
python3 mkeagl.py eagl_slots.txt gl.xml exported-gl.txt eagl.c > eagl_gen.h
wc -l eagl_gen.h

xcrun --sdk iphoneos clang --target=arm64-apple-ios16.0 -isysroot "$SDK" -O2 -dynamiclib \
    -I. -I"$WORK/mesa/include" \
    -install_name /usr/local/lib/libsweetgl.dylib \
    -framework CoreFoundation -framework IOKit -framework IOSurface -lobjc \
    "$OUT/libOSMesa.8.dylib" \
    -Wl,-rpath,/usr/local/lib -Wl,-adhoc_codesign \
    -o "$OUT/libsweetgl.dylib" eagl.c gl_osmesa.c
ls -l "$OUT/libsweetgl.dylib"
file "$OUT/libsweetgl.dylib"
