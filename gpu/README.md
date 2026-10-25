# GPU userland (M6/M7)

Sources adapted from [Bergamota](https://github.com/ry-diffusion/Bergamota) (MIT):

- `freedreno-sweet/sweet_drm.c` — libdrm stand-in for Mesa's freedreno "msm" backend; forwards the
  msm DRM requests to the IOKit user client `SweetAdreno630` of this kernel (selectors are the
  Linux msm uapi ones; see `iokit/Kernel/arm/SweetAdrenoABI.h` in the 0183 patch).
- `eagl/mesa/bergamota-mesa-25.0.7.patch` — Mesa 25.0.7 changes: freedreno/msm over the stand-in,
  OSMesa GLES profile, and the hardware OSMesa target that renders into IOSurfaces.

Built by `.github/workflows/gpu-userland.yml` on a macOS runner:

    bash gpu/eagl/mesa/build-mesa.sh      # GPU_EAGL_WORK=... ; outputs libOSMesa.8.dylib + libdrm.2.dylib

See Bergamota's `docs/GPU_EAGL.md` and `docs/GPU_KERNEL.md` for the design; our kernel implements
the same ABI (patches 0180-0183).
