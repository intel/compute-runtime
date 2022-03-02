<!---

Copyright (C) 2020-2021 Intel Corporation

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

___(*) Other names and brands may be claimed as property of others.___
