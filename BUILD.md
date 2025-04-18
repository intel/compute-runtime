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

Please visit their repositories for building and instalation instructions.

Use versions compatible with selected [Neo release](https://github.com/intel/compute-runtime/releases).

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
