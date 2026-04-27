# Building compute-runtime with local dependencies

This document describes how to check whether the system-installed IGC and GmmLib
are compatible with this codebase, and how to install matching versions to a
local prefix (`~/local`) without sudo if they are not.

## 0. Check installed dependency versions

Each compute-runtime release pins its required dependency versions in
`manifests/manifest.yml`. Compare the installed versions against the manifest:

```shell
# Check installed versions
pkg-config --modversion igc-opencl   # IGC version (e.g. 2.30.1)
pkg-config --modversion igdgmm       # GmmLib version (e.g. 12.9.0)

# Check required versions in the manifest
grep -A3 'gmmlib:' manifests/manifest.yml  # look at "revision" field
grep -A3 'igc:' manifests/manifest.yml     # look at "branch" field
```

The GmmLib `revision` field is a git tag (e.g. `intel-gmmlib-22.9.0`).
The IGC `branch` field specifies a release branch (e.g. `releases/2.32.x`);
the system IGC major.minor version must match (e.g. `2.32.x`).

If the system versions are compatible, you can skip the steps below and build
directly using the standard instructions in `BUILD.md`. If they are too old or
missing, follow the steps below to install them locally.

## 1. Build and install GmmLib

The required GmmLib version is listed as the `gmmlib` `revision` in `manifests/manifest.yml`
(e.g. `intel-gmmlib-22.9.0`). Replace the tag below with the one from your manifest.

```bash
cd /tmp
git clone --depth 1 -b intel-gmmlib-22.9.0 https://github.com/intel/gmmlib.git
cd gmmlib && mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$HOME/local ..
make -j$(nproc)
make install
```

## 2. Install IGC from prebuilt packages

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

mkdir -p ~/local/igc_extract && cd ~/local/igc_extract
for f in /tmp/intel-igc-*.deb; do dpkg-deb -x "$f" .; done
cp -r usr/local/include/* ~/local/include/
cp -r usr/local/lib/* ~/local/lib/
```

Then fix the prefix in `~/local/lib/pkgconfig/igc-opencl.pc` to point to your
local directory:

```
prefix=/home/<user>/local
```

## 3. Build compute-runtime

```bash
cd ~/work/compute-runtime
mkdir -p build && cd build
PKG_CONFIG_PATH=~/local/lib/pkgconfig cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DNEO_SKIP_UNIT_TESTS=1 \
  -DCMAKE_PREFIX_PATH=$HOME/local \
  -DCOMPILE_BUILT_INS=OFF \
  ..
make -j$(nproc)
```

## Notes

- **`-DCOMPILE_BUILT_INS=OFF`** is required when using a local IGC install because `ocloc` loads IGC via `dlopen` at runtime and may find an incompatible system-installed version instead of the local one. To enable built-in kernel compilation, either replace the system IGC or set `LD_LIBRARY_PATH=~/local/lib`.
- **Built artifacts** are placed in `build/bin/`:
  - `libze_intel_gpu.so` — Level Zero driver
  - `libigdrcl.so` — OpenCL driver
  - `ocloc` — Offline compiler
- Check `manifests/manifest.yml` for the exact dependency versions expected by each release.
- Find the latest release tag via: https://github.com/intel/compute-runtime/releases/latest
