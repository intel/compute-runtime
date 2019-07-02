# Intel(R) Graphics Compute Runtime for OpenCL(TM)

## Building

*Instructions assume clean Centos 7.4.1708 installation.**

1. Download & install required dependencies

Example:

```shell
sudo yum groups install  "Development Tools"
sudo yum install centos-release-scl epel-release
sudo yum install devtoolset-4-gcc-c++ cmake3 ninja-build
sudo /usr/sbin/alternatives --install /usr/bin/cmake cmake /usr/bin/cmake3 50
```

2. Install required dependencies

Neo requires [Intel(R) Graphics Compiler for OpenCL(TM)](https://github.com/intel/intel-graphics-compiler) and [Intel(R) Graphics Memory Management](https://github.com/intel/gmmlib) to be installed on your system.

Please visit IGC and GmmLib repositories to build and install intel-igc-opencl-devel and intel-gmmlib-devel packages including all required dependencies

3. Create workspace folder and download sources:

Example:
```
	workspace
	  |- neo                https://github.com/intel/compute-runtime
```

Example:

```shell
git clone https://github.com/intel/compute-runtime neo
```

Note: Instructions for compiling *Intel Graphics Compiler* copied from https://github.com/intel/intel-graphics-compiler/blob/master/README.md 

4. Create folder for build: 

Example:

```shell
mkdir build
```

5. Enabling additional extension

* [cl_intel_va_api_media_sharing](https://github.com/intel/compute-runtime/blob/master/documentation/cl_intel_va_api_media_sharing.md)

6. Build complete driver:

```shell
cd build
scl enable devtoolset-4 "cmake -DBUILD_TYPE=Release -DCMAKE_BUILD_TYPE=Release ../neo"
scl enable devtoolset-4 "make -j `nproc` package"
```

## Installing

To install OpenCL driver please use rpm package generated during build

Example:

```shell
sudo rpm -i intel-opencl-1.0-0.x86_64-igdrcl.rpm
```

___(*) Other names and brands my be claimed as property of others.___
