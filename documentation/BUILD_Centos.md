# Intel(R) Graphics Compute Runtime for OpenCL(TM)

## Building

*Instructions assume clean Centos 7.4.1708 installation.**

1. Download & install required dependencies

Example:

```shell
sudo yum groups install  "Development Tools"
sudo yum install centos-release-scl epel-release
sudo yum install devtoolset-4-gcc-c++ llvm-toolset-7-clang cmake3 ninja-build p7zip rpm-build dpkg xorg-x11-util-macros libpciaccess-devel which zlib-devel
sudo /usr/sbin/alternatives --install /usr/bin/cmake cmake /usr/bin/cmake3 50
```

2. Create workspace folder and download sources:
```
	workspace
	  |- clang_source       https://github.com/llvm-mirror/clang
	  |- common_clang       https://github.com/intel/opencl-clang
	  |- llvm_patches       https://github.com/intel/llvm-patches
	  |- llvm_source        https://github.com/llvm-mirror/llvm
	  |- gmmlib             https://github.com/intel/gmmlib
	  |- igc                https://github.com/intel/intel-graphics-compiler
	  |- opencl_headers     https://github.com/KhronosGroup/OpenCL-Headers
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
git clone https://github.com/KhronosGroup/OpenCL-Headers opencl_headers
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
scl enable devtoolset-4 llvm-toolset-7 "cmake -DBUILD_TYPE=Release -DCMAKE_BUILD_TYPE=Release ../neo"
scl enable devtoolset-4 llvm-toolset-7 "make -j `nproc` package"
```

## Installing

To install OpenCL driver please use rpm package generated during build

Example:

```shell
sudo rpm -i intel-opencl-1.0-0.x86_64-igdrcl.rpm
```

___(*) Other names and brands my be claimed as property of others.___
