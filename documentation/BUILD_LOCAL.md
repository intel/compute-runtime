<!---

Copyright (C) 2026 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Building compute-runtime with local dependencies

This document describes how to check whether the system-installed IGC and GmmLib
are compatible with this codebase, and how to install matching versions to a
local prefix without sudo if they are not.

The easiest approach is to use the provided script which automates all steps:

```shell
./scripts/build_local.sh <tag> [install_prefix]
# Example:
./scripts/build_local.sh 26.09.37435.1 /opt/my_prefix
```

The steps below document the same process manually. `PREFIX` is the directory
where GmmLib and IGC are installed. It is passed as `CMAKE_PREFIX_PATH` and
`PKG_CONFIG_PATH` during the build, and must be on `LD_LIBRARY_PATH` at
driver load time. It defaults to `$HOME/local` when using the script, but
any writable path that does not require sudo works:

```shell
export PREFIX=/tmp/my_prefix   # any writable path that does not require sudo
```

## 0. Check installed dependency versions

Each compute-runtime release pins its required dependency versions in
`manifests/manifest.yml`. Compare the installed versions against the manifest:

```shell
# Check installed versions
pkg-config --modversion igc-opencl   # IGC version (e.g. 2.30.1)
pkg-config --modversion igdgmm       # GmmLib version (e.g. 12.9.0)

# Check required versions in the manifest
grep -A3 'gmmlib:' manifests/manifest.yml    # look at "revision" field
grep -A3 'igc:' manifests/manifest.yml       # look at "branch" field
grep -A3 'level_zero:' manifests/manifest.yml # look at "revision" field
```

The GmmLib `revision` field is a git tag (e.g. `intel-gmmlib-22.9.0`).
The IGC `branch` field specifies a release branch (e.g. `releases/2.32.x`);
the system IGC major.minor version must match (e.g. `2.32.x`).
The level_zero `revision` field is a git tag (e.g. `v1.28.2`); the system
level-zero headers must be at least that version.

If the system versions are compatible, you can skip the steps below and build
directly using the standard instructions in `BUILD.md`. If they are too old or
missing, follow the steps below to install them locally.

## 1. Clone level-zero headers

The compute-runtime CMake build looks for level-zero headers in a directory
named `level_zero` that is a **sibling** of the compute-runtime source tree.
Always clone the exact revision pinned in the manifest because newer releases
may require headers not present in the system-installed version (e.g. `zer_ddi.h`
was added in v1.28).

```bash
L0_REV=$(grep -A5 'level_zero:' manifests/manifest.yml | grep revision | awk '{print $2}')
# Clone next to compute-runtime - the directory name "level_zero" is required.
git clone --depth 1 -b "${L0_REV}" https://github.com/oneapi-src/level-zero.git \
    $(dirname $(pwd))/level_zero
```

## 2. Build and install GmmLib

The required GmmLib version is listed as the `gmmlib` `revision` in `manifests/manifest.yml`
(e.g. `intel-gmmlib-22.9.0`). Replace the tag below with the one from your manifest.

```bash
GMMLIB_REV=$(grep -A5 'gmmlib:' manifests/manifest.yml | grep revision | awk '{print $2}')
cd /tmp
git clone --depth 1 -b "${GMMLIB_REV}" https://github.com/intel/gmmlib.git
cd gmmlib && mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="${PREFIX}" ..
make -j$(nproc)
make install
```

## 3. Install IGC from prebuilt packages

The required IGC version can be determined from the `igc` `branch` field in
`manifests/manifest.yml` (e.g. `releases/2.32.x`). Find the matching release
at https://github.com/intel/intel-graphics-compiler/releases and download
the four `_amd64.deb` packages (core, core-devel, opencl, opencl-devel).
Replace the version numbers below with the ones matching your manifest.

```bash
cd /tmp
wget https://github.com/intel/intel-graphics-compiler/releases/download/<IGC_TAG>/intel-igc-core-2_<VERSION>_amd64.deb
wget https://github.com/intel/intel-graphics-compiler/releases/download/<IGC_TAG>/intel-igc-opencl-2_<VERSION>_amd64.deb
wget https://github.com/intel/intel-graphics-compiler/releases/download/<IGC_TAG>/intel-igc-core-devel_<VERSION>_amd64.deb
wget https://github.com/intel/intel-graphics-compiler/releases/download/<IGC_TAG>/intel-igc-opencl-devel_<VERSION>_amd64.deb

mkdir -p /tmp/igc_extract && cd /tmp/igc_extract
for f in /tmp/intel-igc-*.deb; do dpkg-deb -x "$f" .; done
cp -r usr/local/include/* "${PREFIX}/include/"
cp -r usr/local/lib/* "${PREFIX}/lib/"
```

Then fix the prefix in `${PREFIX}/lib/pkgconfig/igc-opencl.pc` to point to
your local directory:

```
prefix=${PREFIX}
```

## 4. Build compute-runtime

```bash
cd /path/to/compute-runtime
mkdir -p build && cd build
PKG_CONFIG_PATH="${PREFIX}/lib/pkgconfig" cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DNEO_SKIP_UNIT_TESTS=1 \
  -DCMAKE_PREFIX_PATH="${PREFIX}" \
  -DCOMPILE_BUILT_INS=OFF \
  ..
make -j$(nproc)
```

> **Note:** The level-zero clone from Step 1 must be placed as a sibling of
> the compute-runtime source directory (e.g. `/tmp/level_zero` next to
> `/tmp/compute-runtime`). CMake detects this layout automatically and copies
> the headers into the required `include/level_zero/` sub-directory. Do **not**
> pass `-DLEVEL_ZERO_ROOT` - that flag bypasses the header copy step.

## Notes

- **level-zero headers** must be cloned at the revision pinned in `manifests/manifest.yml` as a sibling directory named `level_zero` next to the compute-runtime source. The system-installed headers may be too old and lack recently-added headers (e.g. `zer_ddi.h` added in v1.28).
- **`-DCOMPILE_BUILT_INS=OFF`** is required when using a local IGC install because `ocloc` loads IGC via `dlopen` at runtime and may find an incompatible system-installed version instead of the local one. To enable built-in kernel compilation, either replace the system IGC or set `LD_LIBRARY_PATH=${PREFIX}/lib`.
- **Built artifacts** are placed in `build/bin/`:
  - `libze_intel_gpu.so.1` -- Level Zero driver
  - `libigdrcl.so` -- OpenCL driver
  - `ocloc` -- Offline compiler
- Check `manifests/manifest.yml` for the exact dependency versions expected by each release.
- Find the latest release tag via: https://github.com/intel/compute-runtime/releases/latest
