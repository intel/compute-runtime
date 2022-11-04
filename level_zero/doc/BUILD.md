<!---

Copyright (C) 2020-2021 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Building Level Zero

These instructions have been tested on Ubuntu* and complement those existing for NEO in the top-level BUILD.md file.

1. Install/build Level Zero dependencies

To use Fabric related APIs, please build and/or install [libnl-3.7.0](https://www.linuxfromscratch.org/blfs/view/svn/basicnet/libnl.html).
If installing to a local folder, `-DLIBGENL_INCLUDE_DIR=<local install folder path>/libnl/include/libnl3/` could be passed to the cmake line of NEO build(Please refer top-level BUILD.md).

To use Sysman events API, please build and/or install libudev-dev 

2. Install/build Level Zero loader and Level Zero headers

Install Level Zero loader and headers from [https://github.com/oneapi-src/level-zero/releases](https://github.com/oneapi-src/level-zero/releases).

For execution, only the level-zero package is needed; for compilation, level-zero-devel package is also required.

Alternatively, build Level Zero loader from source, as indicated in [https://github.com/oneapi-src/level-zero](https://github.com/oneapi-src/level-zero).

Build will generate ze_loader library and symlinks, as well as those for ze_validation_layer.

3. Install/build Level Zero driver

Install Level Zero package from [https://github.com/intel/compute-runtime/releases](https://github.com/intel/compute-runtime/releases).

Alternatively, follow instructions in top-level BUILD.md file to build NEO. Level Zero is built by default.

When built, ze_intel_gpu library and symlinks are generated.

4. Build your application

Compilation needs to include the Level Zero headers and to link against the loader library:

```shell
g++ zello_world_gpu.cpp -o zello_world_gpu -lze_loader
```

If libraries not installed in system paths, include Level Zero headers and path to Level Zero loader:

```shell
g++ -I<path_to_Level_Zero_headers> zello_world_gpu.cpp -o zello_world_gpu -L<path_to_libze_loader.so> -lze_loader
```

5. Execute your application

If Level Zero loader packages have been built and installed in the system, then they will be present in system paths:

```shell
./zello_world_gpu
```

Sample output:

```shell
Device :
 * name : Intel(R) Graphics Gen9 [0x5912]
 * type : GPU
 * vendorId : 8086

Zello World Results validation PASSED
```

If libraries not installed in system paths, add paths to ze_loader and ze_intel_gpu libraries:

```shell
LD_LIBRARY_PATH=<path_to_libze_loader.so>:<path_to_libze_intel_gpu.so> ./zello_world_gpu
```

___(*) Other names and brands may be claimed as property of others.___