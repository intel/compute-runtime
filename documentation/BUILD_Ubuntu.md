# Intel(R) Graphics Compute Runtime for OpenCL(TM)

## Building

*Instructions assume clean Ubuntu 16.04.3 LTS installation.**

1. Download & install required packages

Example:

```shell
sudo apt-get install ccache cmake g++ git pkg-config
```

See [LIMITATIONS.md](https://github.com/intel/compute-runtime/blob/master/documentation/LIMITATIONS.md) for other requirements and dependencies, when building and installing NEO.

2. Instal required dependencies

Neo requires [Intel(R) Graphics Compiler for OpenCL(TM)](https://github.com/intel/intel-graphics-compiler) and [Intel(R) Graphics Memory Management](https://github.com/intel/gmmlib) to be installed on your system.

Please visit theirs repositories and install intel-igc-opencl-devel and intel-gmmlib-devel packages including all required dependencies

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
cmake -DBUILD_TYPE=Release -DCMAKE_BUILD_TYPE=Release ../neo
make -j`nproc` package
```

## Installing

To install OpenCL driver please use deb package generated during build

Example:

```shell
sudo dpkg -i intel-opencl-*.x86_64-igdrcl.deb
```

___(*) Other names and brands my be claimed as property of others.___
