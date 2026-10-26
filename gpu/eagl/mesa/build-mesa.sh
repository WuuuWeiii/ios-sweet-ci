#!/bin/bash
# Cross-build Mesa 25.0.7 (freedreno + softpipe, OSMesa frontend) for iOS arm64e.
# Adapted from Bergamota gpu/eagl/mesa/build-mesa.sh (MIT); iOS 16 target, no venv assumptions.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="${GPU_EAGL_WORK:?set GPU_EAGL_WORK}"
TAG="${MESA_TAG:-mesa-25.0.7}"
ARCH="${ARCH:-arm64}"
mkdir -p "$WORK"; cd "$WORK"
python3 -m venv venv 2>/dev/null || true
./venv/bin/pip -q install --disable-pip-version-check mako pyyaml packaging setuptools meson ninja
export PATH="$WORK/venv/bin:$PATH"
if [ ! -d mesa ]; then
  curl -sL --fail -o "$TAG.tar.xz" "https://archive.mesa3d.org/$TAG.tar.xz"
  tar xJf "$TAG.tar.xz"
  mv "$TAG" mesa
  ( cd mesa && patch -p1 --batch --forward < "$HERE/bergamota-mesa-25.0.7.patch" )
  python3 - "$WORK/mesa/meson.build" <<'PY'
# The msm winsys (libfreedreno_drm) lives under `if system_has_kms_drm`; the msm
# requests go through our libdrm stand-in, whose headers are real libdrm ones.
import sys
p = sys.argv[1]
s = open(p).read()
old = "system_has_kms_drm = ['openbsd', 'netbsd', 'freebsd', 'gnu/kfreebsd', 'dragonfly', 'linux', 'sunos', 'android', 'managarm'].contains(host_machine.system())"
new = "system_has_kms_drm = ['openbsd', 'netbsd', 'freebsd', 'gnu/kfreebsd', 'dragonfly', 'linux', 'sunos', 'android', 'managarm', 'darwin'].contains(host_machine.system())"
assert old in s, 'kms_drm line not found'
open(p, 'w').write(s.replace(old, new, 1))
print('mesh.build: darwin treated as KMS/DRM so libfreedreno_drm is built')
PY
fi
SDK="$(xcrun --sdk iphoneos --show-sdk-path)"
if [ ! -d libdrm ]; then
  rm -rf libdrm-x && mkdir -p libdrm-x
  got=0
  for u in "https://dri.freedesktop.org/libdrm/libdrm-2.4.124.tar.xz" \
           "https://gitlab.freedesktop.org/mesa/drm/-/archive/libdrm-2.4.124/drm-libdrm-2.4.124.tar.gz" \
           "https://dri.freedesktop.org/libdrm/libdrm-2.4.124.tar.bz2"; do
    f="$(basename "$u")"
    echo "libdrm: trying $u"
    if curl -sL --fail -o "$f" "$u"; then
      if tar xf "$f" -C libdrm-x 2>/dev/null; then
        d="$(find libdrm-x -maxdepth 1 -mindepth 1 -type d | head -1)"
        if [ -n "$d" ]; then mv "$d" libdrm; got=1; break; fi
      fi
      echo "libdrm: $f did not extract"
    fi
  done
  [ "$got" = 1 ] || { echo "cannot obtain libdrm headers" >&2; exit 1; }
fi
# the libdrm stand-in (this repo's kernel ABI: SweetAdreno630)
bash "$HERE/../../freedreno-sweet/build.sh" "$WORK/drm-standin-out" >/dev/null
mkdir -p drm-standin pc
cp "$WORK/drm-standin-out/libdrm.2.arm64.dylib" drm-standin/libdrm.2.dylib
ln -sf libdrm.2.dylib drm-standin/libdrm.dylib
cat > pc/libdrm.pc <<EOP
prefix=$WORK/libdrm
Name: libdrm
Description: libdrm stand-in for SweetAdreno630
Version: 2.4.124
Cflags: -I$WORK/libdrm/include/drm -I$WORK/libdrm
Libs: -L$WORK/drm-standin -ldrm
EOP
CROSS="$WORK/ios-$ARCH.cross"
BISON="$(brew --prefix bison 2>/dev/null || echo /usr/local)/bin/bison"
[ -x "$BISON" ] || BISON="$(command -v bison)"
cat > "$CROSS" <<EOT
[binaries]
c = ['xcrun','--sdk','iphoneos','clang','-arch','$ARCH']
cpp = ['xcrun','--sdk','iphoneos','clang++','-arch','$ARCH']
objc = ['xcrun','--sdk','iphoneos','clang','-arch','$ARCH']
ar = ['xcrun','--sdk','iphoneos','ar']
strip = ['xcrun','--sdk','iphoneos','strip']
ranlib = ['xcrun','--sdk','iphoneos','ranlib']
bison = '$BISON'
pkg-config = '$(command -v pkg-config)'

[built-in options]
pkg_config_path = ['$WORK/pc']
c_args = ['-miphoneos-version-min=16.0','-isysroot','$SDK']
cpp_args = ['-miphoneos-version-min=16.0','-isysroot','$SDK']
c_link_args = ['-miphoneos-version-min=16.0','-isysroot','$SDK']
cpp_link_args = ['-miphoneos-version-min=16.0','-isysroot','$SDK']

[host_machine]
system = 'darwin'
subsystem = 'ios'
cpu_family = 'aarch64'
cpu = '$ARCH'
endian = 'little'

[properties]
needs_exe_wrapper = true
EOT
BLD="$WORK/build-hw"
if [ ! -d "$BLD" ]; then
  ( cd mesa && PKG_CONFIG_PATH="$WORK/pc" PKG_CONFIG_LIBDIR="$WORK/pc" meson setup "$BLD" --cross-file "$CROSS" --buildtype=release \
    -Dgallium-drivers=softpipe,freedreno -Dfreedreno-kmds=msm -Dgallium-xa=disabled -Dtools= \
    -Dvulkan-drivers= -Dllvm=disabled -Dglx=disabled -Degl=disabled -Dgbm=disabled -Dplatforms= \
    -Dosmesa=true -Dopengl=true -Dgles1=enabled -Dgles2=enabled -Dshared-glapi=enabled \
    -Dvalgrind=disabled -Dlibunwind=disabled -Dzstd=disabled -Dxmlconfig=disabled -Dbuild-tests=false \
    -Dglvnd=disabled -Dlmsensors=disabled -Dgallium-opencl=disabled -Dgallium-rusticl=false )
fi
PKG_CONFIG_PATH="$WORK/pc" PKG_CONFIG_LIBDIR="$WORK/pc" ninja -C "$BLD" src/gallium/targets/osmesa/libOSMesa.8.dylib
mkdir -p "$WORK/out"
cp -f "$BLD/src/gallium/targets/osmesa/libOSMesa.8.dylib" "$WORK/out/"
cp -f "$WORK/drm-standin/libdrm.2.dylib" "$WORK/out/"
ls -l "$WORK/out"
