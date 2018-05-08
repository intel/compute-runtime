# Intel(R) Graphics Compute Runtime for OpenCL(TM)

## Building

*Instructions assume clean Ubuntu 16.04.3 LTS installation.**

1. Download & install required dependencies

Example:

```shell
sudo apt-get install ccache flex bison clang-4.0 cmake g++ git patch zlib1g-dev autoconf xutils-dev libtool pkg-config libpciaccess-dev
```

See [LIMITATIONS.md](https://github.com/intel/compute-runtime/blob/master/documentation/LIMITATIONS.md) for other requirements and dependencies, when building and installing NEO.

2. Create workspace folder and download sources:
```
	workspace
	  |- clang_source       https://github.com/llvm-mirror/clang
	  |- common_clang       https://github.com/intel/opencl-clang
	  |- llvm_patches       https://github.com/intel/llvm-patches
	  |- llvm_source        https://github.com/llvm-mirror/llvm
	  |- gmmlib             https://github.com/intel/gmmlib
	  |- igc                https://github.com/intel/intel-graphics-compiler
	  |- khronos            https://github.com/KhronosGroup/OpenCL-Headers
	  |- neo                https://github.com/intel/compute-runtime
```

Example:

```shell
git clone -b release_40 https://github.com/llvm-mirror/clang clang_source
git clone https://github.com/intel/opencl-clang common_clang
git clone https://github.com/intel/llvm-patches llvm_patches
git clone -b release_40 https://github.com/llvm-mirror/llvm llvm_source
git clone https://github.com/intel/gmmlib gmmlib
git clone https://github.com/intel/intel-graphics-compiler igc
git clone https://github.com/KhronosGroup/OpenCL-Headers khronos
git clone https://github.com/intel/compute-runtime neo
```

Note: Instructions for compiling *Intel Graphics Compiler* copied from https://github.com/intel/intel-graphics-compiler/blob/master/README.md 

3. Create folder for build: 

Example:

```shell
mkdir build
```

4. Enabling additional extension

* [cl_intel_va_api_media_sharing](https://github.com/intel/compute-runtime/blob/master/documentation/cl_intel_va_api_media_sharing.md)

5. Build complete driver:

```shell
cd build
cmake -DBUILD_TYPE=Release -DCMAKE_BUILD_TYPE=Release ../neo
make -j`nproc` package
```

## Installing

To install OpenCL driver please use deb package generated during build

Example:

```shell
sudo dpkg -i intel-opencl-1.0-0.x86_64-igdrcl.deb
```

___(*) Other names and brands my be claimed as property of others.___
