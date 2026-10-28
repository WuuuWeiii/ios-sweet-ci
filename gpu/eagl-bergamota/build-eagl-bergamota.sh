#!/bin/bash
# Build Bergamota's OpenGLES.framework injection shim + bgl-launch (arm64, iOS 16) against
# the Mesa OSMesa/freedreno build in $GPU_EAGL_WORK/out (see gpu/eagl/mesa/build-mesa.sh).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="${GPU_EAGL_WORK:?set GPU_EAGL_WORK}"
OUT="$WORK/out"
BOUT="$WORK/out-eagl"
SDK="$(xcrun --sdk iphoneos --show-sdk-path)"
CC="xcrun --sdk iphoneos clang --target=arm64-apple-ios16.0 -isysroot $SDK"
mkdir -p "$BOUT/OpenGLES.framework" "$BOUT/obj"
python3 "$HERE/tools/gen_gl_stubs.py" > "$HERE/src/gl_stubs.c"
CF="-O2 -g -fvisibility=hidden -Wall -Wno-deprecated-declarations -I$HERE/src -I$WORK/mesa/include"
for f in gl_stubs gl_resolve gl_unimpl osm macro_ctx macro_slots; do
  $CC $CF -c "$HERE/src/$f.c" -o "$BOUT/obj/$f.o"
done
$CC $CF -fno-objc-arc -fobjc-exceptions -DEAGL_INJECT -c "$HERE/src/eagl.m" -o "$BOUT/obj/eagl.o"
$CC $CF -fno-objc-arc -DEAGL_INJECT -c "$HERE/src/ca_server_hook.m" -o "$BOUT/obj/ca_server_hook.o"
cp -f "$OUT/libOSMesa.8.dylib" "$BOUT/"
cp -f "$OUT/libdrm.2.dylib" "$BOUT/" 2>/dev/null || true
$CC -dynamiclib -o "$BOUT/OpenGLES.framework/OpenGLES" "$BOUT"/obj/*.o \
    -install_name @rpath/OpenGLES.framework/OpenGLES \
    -Wl,-not_for_dyld_shared_cache -compatibility_version 1.0.0 -current_version 1.0.0 \
    -framework Foundation -framework CoreFoundation -framework CoreGraphics -framework IOSurface \
    "$BOUT/libOSMesa.8.dylib" -Wl,-rpath,@loader_path/.. -Wl,-undefined,error
$CC -O2 -Wall "$HERE/launch/bgl-launch.c" -o "$BOUT/bgl-launch" \
    -framework Foundation -framework CoreFoundation 2>/dev/null || \
  $CC -O2 -Wall "$HERE/launch/bgl-launch.c" -o "$BOUT/bgl-launch"
for f in "$BOUT/OpenGLES.framework/OpenGLES" "$BOUT/bgl-launch"; do
  codesign -f -s - "$f" 2>/dev/null || true
done
ls -l "$BOUT" "$BOUT/OpenGLES.framework"
