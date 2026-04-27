<!---

Copyright (C) 2020-2025 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Building NEO driver

Instructions have been tested on Ubuntu* and CentOS*. They assume a clean installation of a stable version.

1. Download & install required packages

Example (Ubuntu):

```shell
sudo apt-get install cmake g++ git pkg-config
```

Example (CentOS):

```shell
sudo dnf install gcc-c++ cmake git make
```

2. Install required dependencies

Neo requires:
- [Intel(R) Graphics Compiler for OpenCL(TM)](https://github.com/intel/intel-graphics-compiler)
- [Intel(R) Graphics Memory Management](https://github.com/intel/gmmlib)

Please visit their repositories for building and installation instructions.

Use versions compatible with selected [Neo release](https://github.com/intel/compute-runtime/releases).
The exact required versions are listed in `manifests/manifest.yml` for each release.

3. Create workspace folder and download sources:

Example:

```shell
mkdir workspace
cd workspace
git clone https://github.com/intel/compute-runtime neo
```

4. Create folder for build: 

Example:

```shell
mkdir build
```

5. (Optional) Enabling additional extensions

* [cl_intel_va_api_media_sharing](https://github.com/intel/compute-runtime/blob/master/opencl/doc/cl_intel_va_api_media_sharing.md)

6. Build and install

Example:

```shell
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DNEO_SKIP_UNIT_TESTS=1 ../neo
make -j`nproc`
sudo make install
```

## Checking dependency versions

Each compute-runtime release pins its required dependency versions in
`manifests/manifest.yml`. Before building, verify the system-installed versions
are compatible:

```shell
# Check installed versions
pkg-config --modversion igc-opencl   # IGC (e.g. 2.32.1)
pkg-config --modversion igdgmm       # GmmLib (e.g. 12.9.0)

# Check required versions
grep -A3 'gmmlib:' manifests/manifest.yml  # "revision" field (e.g. intel-gmmlib-22.9.0)
grep -A3 'igc:' manifests/manifest.yml     # "branch" field (e.g. releases/2.32.x)
```

If the installed versions are too old or missing, follow the instructions below
to install compatible versions to a local prefix, or see
[BUILD_LOCAL.md](BUILD_LOCAL.md) for detailed instructions.

## Building without root access

If you cannot install dependencies system-wide (no sudo), you can build GmmLib
from source and use prebuilt IGC packages, both installed to a local prefix.

### Build and install GmmLib to a local prefix

The required GmmLib version is listed as the `gmmlib` `revision` in `manifests/manifest.yml`.
Replace the tag below with the one from your manifest.

```shell
git clone --depth 1 -b <GMMLIB_TAG> https://github.com/intel/gmmlib.git
cd gmmlib && mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$HOME/local ..
make -j`nproc`
make install
```

### Install IGC from prebuilt packages

The required IGC version can be determined from the `igc` `branch` field in
`manifests/manifest.yml` (e.g. `releases/2.32.x`). Find the matching release
at https://github.com/intel/intel-graphics-compiler/releases and download
the four `_amd64.deb` packages (core, core-devel, opencl, opencl-devel).

```shell
mkdir -p $HOME/local/igc_extract && cd $HOME/local/igc_extract
for f in /path/to/intel-igc-*.deb; do dpkg-deb -x "$f" .; done
cp -r usr/local/include/* $HOME/local/include/
cp -r usr/local/lib/* $HOME/local/lib/
```

Then fix the prefix in `$HOME/local/lib/pkgconfig/igc-opencl.pc`:

```
prefix=/home/<user>/local
```

### Build compute-runtime with local dependencies

```shell
cd build
PKG_CONFIG_PATH=$HOME/local/lib/pkgconfig cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DNEO_SKIP_UNIT_TESTS=1 \
  -DCMAKE_PREFIX_PATH=$HOME/local \
  -DCOMPILE_BUILT_INS=OFF \
  ../neo
make -j`nproc`
```

Note: `-DCOMPILE_BUILT_INS=OFF` is required because the offline compiler (ocloc)
loads IGC at runtime via `dlopen` and will find the old system-installed IGC
instead of the local one. To enable built-in kernel compilation, either replace
the system IGC libraries or run `make` with `LD_LIBRARY_PATH=$HOME/local/lib`.

## Optional - Building NEO with support for XeKMD EU Debugging

NEO Driver has build options to enable support for EU Debugging with XeKMD. Kernel support for this feature is currently only available via a topic branch hosted at https://gitlab.freedesktop.org/miku/kernel/-/tree/eudebug-dev 

To build NEO with support for this feature follow above steps with these additional cmake options added to step 6.

` -DNEO_ENABLE_XE_EU_DEBUG_SUPPORT=1  -DNEO_USE_XE_EU_DEBUG_EXP_UPSTREAM=1`

## Optional - Building NEO with support for Gen8, Gen9 and Gen11 devices

Starting from release [24.35.30872.22](https://github.com/intel/compute-runtime/releases/tag/24.35.30872.22) regular packages support Gen12 and later devices.

Gen8, Gen9 and Gen11 devices related code is available on [releases/24.35](https://github.com/intel/compute-runtime/tree/releases/24.35) branch. It is no longer available on master branch.

To build NEO with support for Gen8, Gen9 and Gen11 devices follow above steps with these additional cmake options added to step 6.

` -DNEO_LEGACY_PLATFORMS_SUPPORT=1  -DNEO_CURRENT_PLATFORMS_SUPPORT=0`


___(*) Other names and brands may be claimed as property of others.___
