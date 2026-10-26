#!/bin/bash
# Build the userspace GPU tests against the Mesa build in $GPU_EAGL_WORK/out.
#   GPU_EAGL_WORK=... bash gpu/test/build-tests.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="${GPU_EAGL_WORK:?set GPU_EAGL_WORK}"
OUT="$WORK/out"
TESTS="$OUT/tests"
SDK="$(xcrun --sdk iphoneos --show-sdk-path)"
mkdir -p "$TESTS"

echo "== sweet-iokit-test (IOKit user client)"
xcrun --sdk iphoneos clang --target=arm64-apple-ios16.0 -isysroot "$SDK" -O2 \
    -o "$TESTS/sweet-iokit-test" "$HERE/sweet-iokit-test.c" \
    -framework IOKit -framework CoreFoundation -Wl,-adhoc_codesign

echo "== static_gputest (no dyld, raw syscalls)"
xcrun --sdk iphoneos clang --target=arm64-apple-ios16.0 -isysroot "$SDK" -nostdlib -static \
    -Wl,-e,_start -o "$TESTS/static_gputest" "$HERE/static_gputest.S" || true

echo "== osmesa_gputest (Mesa freedreno offscreen)"
xcrun --sdk iphoneos clang --target=arm64-apple-ios16.0 -isysroot "$SDK" -O2 \
    -I"$WORK/mesa/include" -I"$WORK/mesa/include/GLES2" -I"$WORK/mesa/include/KHR" \
    -o "$TESTS/osmesa_gputest" "$HERE/osmesa_gputest.c" \
    "$OUT/libOSMesa.8.dylib" "$OUT/libdrm.2.dylib" \
    -Wl,-rpath,/usr/local/lib -Wl,-adhoc_codesign

cp -f "$OUT/libOSMesa.8.dylib" "$OUT/libdrm.2.dylib" "$TESTS/"
cp -f "$HERE"/org.sweet.iokittest.plist "$HERE"/org.sweet.osmesatest.plist "$TESTS/"
chmod 755 "$TESTS/sweet-iokit-test" "$TESTS/osmesa_gputest"
ls -l "$TESTS"
