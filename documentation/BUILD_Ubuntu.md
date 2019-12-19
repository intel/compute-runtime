# Intel(R) Graphics Compute Runtime for OpenCL(TM)

1. Download & install required packages

Example:

```shell
sudo apt-get install cmake g++ git pkg-config
```

See [LIMITATIONS.md](https://github.com/intel/compute-runtime/blob/master/documentation/LIMITATIONS.md)
for other requirements and dependencies, when building and installing NEO.

2. Instal required dependencies

Neo requires:
- [Intel(R) Graphics Compiler for OpenCL(TM)](https://github.com/intel/intel-graphics-compiler)
- [Intel(R) Graphics Memory Management](https://github.com/intel/gmmlib)

Please visit theirs repositories for building and instalation instructions or use packages from 
[launchpad repository](https://launchpad.net/~intel-opencl/+archive/ubuntu/intel-opencl)
Use versions compatible with selected [Neo release](https://github.com/intel/compute-runtime/releases)

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

5. Enabling additional extension

* [cl_intel_va_api_media_sharing](https://github.com/intel/compute-runtime/blob/master/documentation/cl_intel_va_api_media_sharing.md)

6. Build and install

Example:

```shell
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DSKIP_UNIT_TESTS=1 ../neo
make -j`nproc`
sudo make install
```

___(*) Other names and brands may be claimed as property of others.___
