# Intel(R) Graphics Compute Runtime for OpenCL(TM)

## Introduction

The Intel(R) Graphics Compute Runtime for OpenCL(TM) is a open source project to
converge Intel's development efforts on OpenCL(TM) compute stacks supporting the
GEN graphics hardware architecture.

Please refer to http://01.org/compute-runtime for additional details regarding Intel's
motivation and intentions wrt OpenCL support in the open source.

## License

The Intel(R) Graphics Compute Runtime for OpenCL(TM) is distributed under the MIT.

You may obtain a copy of the License at:

https://opensource.org/licenses/MIT

## Dependencies

GmmLib - https://github.com/intel/gmmlib  
Intel Graphics Compiler - https://github.com/intel/intel-graphics-compiler  
Google Test v1.7.0 - https://github.com/google/googletest  
Google Mock v1.7.0 - https://github.com/google/googlemock  
Khronos OpenCL Headers - https://github.com/KhronosGroup/OpenCL-Headers  
LibDRM - https://anongit.freedesktop.org/git/mesa/drm.git  

## Building

*Instructions assume clean Ubuntu 16.04.3 LTS installation.*

1. Download & install required dependencies 

Example:

```shell
sudo apt-get install ccache flex bison clang-4.0 cmake g++ git patch zlib1g-dev 
```

2. Create workspace folder and download sources:
```
	workspace
	  |- clang_source       https://github.com/llvm-mirror/clang
	  |- common_clang       https://github.com/intel/opencl-clang
	  |- llvm_patches       https://github.com/intel/llvm-patches
	  |- llvm_source        https://github.com/llvm-mirror/llvm
	  |- gmmlib             https://github.com/intel/gmmlib
	  |- gmock              https://github.com/google/googlemock
	  |- gtest              https://github.com/google/googletest
	  |- igc                https://github.com/intel/intel-graphics-compiler
	  |- khronos            https://github.com/KhronosGroup/OpenCL-Headers
	  |- libdrm             https://anongit.freedesktop.org/git/mesa/drm.git
	  |- neo                https://github.com/intel/compute-runtime
```

Example:

```shell
git clone -b release_40 https://github.com/llvm-mirror/clang clang_source
git clone https://github.com/intel/opencl-clang common_clang
git clone https://github.com/intel/llvm-patches llvm_patches
git clone -b release_40 https://github.com/llvm-mirror/llvm llvm_source
git clone https://github.com/intel/gmmlib gmmlib
git clone -b release-1.7.0 https://github.com/google/googlemock gmock
git clone -b release-1.7.0 https://github.com/google/googletest gtest
git clone https://github.com/intel/intel-graphics-compiler igc
git clone https://github.com/KhronosGroup/OpenCL-Headers khronos
git clone https://anongit.freedesktop.org/git/mesa/drm.git libdrm
git clone https://github.com/intel/compute-runtime neo
```

Note: Instructions for compiling *Intel Graphics Compiler* copied from https://github.com/intel/intel-graphics-compiler/blob/master/README.md 

3. Create folder for build: 

Example:

```shell
mkdir build
```

4. Build complete driver:

```shell
cd build
cmake -DBUILD_TYPE=Release -DCMAKE_BUILD_TYPE=Release ../neo
make -j`nproc` package
```

### Install

To install OpenCL driver please use deb package generated during build

Example:

```shell
sudo dpkg -i intel-opencl-1.0-0.x86_64-igdrcl.deb
```

## Supported Platforms

Intel Core Processors with Gen8 graphics devices (formerly Broadwell) - OpenCL 2.0  
Intel Core Processors with Gen9 graphics devices (formerly Skylake, Kaby Lake, Coffee Lake) - OpenCL 2.1  
Intel Atom Processors with Gen9 graphics devices (formerly Apollo Lake, Gemini Lake) - OpenCL 1.2  

## How to provide feedback

By default, please submit an issue using native github.com interface: https://github.com/intel/compute-runtime/issues.  


## How to contribute

Create a pull request on github.com with your patch. Make sure your change is cleanly building and passing ULTs.
A maintainer will contact you if there are questions or concerns.


## Known Issues and Limitations

OpenCL compliance of a driver built from open-source components should not be
assumed by default. Intel will clearly designate / tag specific builds to
indicate production quality including formal compliance. Other builds should be
considered experimental. 

The driver requires Khronos ICD loader to operate correctly:
https://github.com/KhronosGroup/OpenCL-ICD-Loader

The driver has the following functional delta compared to previously released drivers:
* Intel's closed source SRB5.0 driver (aka Classic)  
  https://software.intel.com/en-us/articles/opencl-drivers#latest_linux_driver
* Intel's former open-source Beignet driver  
  https://01.org/beignet

### Generic extensions
* cl_khr_mipmap
* cl_khr_mipmap_writes
* cl_khr_fp64
### Preview extensions
* cl_intelx_video_enhancement
* cl_intelx_video_enhancement_camera_pipeline
* cl_intelx_video_enhancement_color_pipeline
* cl_intelx_hevc_pak
### Other capabilities
* OpenGL sharing with MESA driver
* CL_MEM_SVM_FINE_GRAIN_BUFFER (if using unpatched i915)


___(*) Other names and brands my be claimed as property of others.___

