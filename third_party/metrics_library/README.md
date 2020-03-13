# Intel(R) Metrics Library for MDAPI

## Introduction

This software is a user mode driver helper library that provides access to GPU performance counters.

## License

Intel(R) Metrics Library for MDAPI is distributed under the MIT License.

You may obtain a copy of the License at:
https://opensource.org/licenses/MIT

## Supported Platforms

- Intel(R) Processors with Gen12 graphics devices
- Intel(R) Processors with Gen11 graphics devices
- Intel(R) Processors with Gen9 graphics devices

## Supported Operating Systems

Intel(R) Metrics Library for MDAPI is supported on Linux family operating systems with minimum kernel version 4.14.

## Build and Install
Not a stand alone software component. Serves as a helper library for particular Intel drivers.
There is no need to build the library as long as it is an integrated part of those Intel drivers.

1\. Download sources.

2\. Run CMake generation:

```shell
cmake .
```

3\. Build:

```shell
make -j$(nproc)
```

4\. Built library will be here (for 64-bit Linux):

```shell
(project_root)/dump/linux64/metrics_library/libigdml64.so
```

5\. To prepare an installation package:

```shell
make package
```

6\. Install:

```shell
sudo dpkg -i intel-metrics-library*.deb
```

*Note: To clear CMake parameters remove CMakeCache.txt, then regenerate.*

##
___(*) Other names and brands my be claimed as property of others.___
