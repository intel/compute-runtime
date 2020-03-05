# Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver

## Introduction

The Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver
is an open source project providing compute API support (Level Zero, OpenCL)
for Intel graphics hardware architectures (HD Graphics, Xe).

## What is NEO?

NEO is the shorthand name for Compute Runtime contained within this repository.
It is also a development mindset that we adopted when we first started the
implementation effort for OpenCL.

The project evolved beyond a single API and NEO no longer implies a specific API.
When talking about a specific API, we will mention it by name (e.g. Level Zero, OpenCL).

## License

The Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver
is distributed under the MIT License.

You may obtain a copy of the License at: https://opensource.org/licenses/MIT

## Supported Platforms

|Platform|OpenCL|Level Zero|
|--------|:----:|:--------:|
|Intel Core Processors with Gen8 graphics devices (formerly Broadwell)| 2.1 | - |
|Intel Core Processors with Gen9 graphics devices (formerly Skylake, Kaby Lake, Coffee Lake)| 2.1 | Y |
|Intel Atom Processors with Gen9 graphics devices (formerly Apollo Lake, Gemini Lake)| 1.2 | - |
|Intel Core Processors with Gen11 graphics devices (formerly Ice Lake)| 2.1 | Y |
|Intel Core Processors with Gen12 graphics devices (formerly Tiger Lake)| 2.1 | Y |

## Release cadence

* Once a week, we run extended validation cycle on a selected driver.
* When the extended validation cycle tests pass, the corresponding commit on github is tagged using
the format yy.ww.bbbb (yy - year, ww - work week, bbbb - incremental build number).
* Typically for weekly tags we will post a binary release (e.g. deb).
* Quality level of the driver (per platform) will be provided in the Release Notes.

## Installation Options

To allow NEO access to GPU device make sure user has permissions to files /dev/dri/renderD*.

### Via system package manager

NEO is available for installation on a variety of Linux distributions
and can be installed via the distro's package manager.

For example on Ubuntu* 19.04, 19.10:

```
apt-get install intel-opencl-icd
```

Procedures for other
[distributions](https://github.com/intel/compute-runtime/blob/master/DISTRIBUTIONS.md).

### Manual download

.deb packages for Ubuntu are provided along with installation instructions and
Release Notes on the [release page](https://github.com/intel/compute-runtime/releases)

## Linking applications

Directly linking to the runtime library is not supported:
* Level Zero applications should link with [Level Zero loader](https://github.com/oneapi-src/level-zero)
* OpenCL applications should link with [ICD loader library (ocl-icd)](https://github.com/OCL-dev/ocl-icd)

## Dependencies

* GmmLib - https://github.com/intel/gmmlib
* Intel Graphics Compiler - https://github.com/intel/intel-graphics-compiler

## How to provide feedback

By default, please submit an issue using native github.com [interface](https://github.com/intel/compute-runtime/issues).

## How to contribute

Create a pull request on github.com with your patch. Make sure your change is cleanly building
and passing ULTs. A maintainer will contact you if there are questions or concerns.
See
[contribution guidelines](https://github.com/intel/compute-runtime/blob/master/CONTRIBUTING.md)
for more details.

## See also

* [Contribution guidelines](https://github.com/intel/compute-runtime/blob/master/CONTRIBUTING.md)
* [Frequently Asked Questions](https://github.com/intel/compute-runtime/blob/master/FAQ.md)

### Level Zero specific
* [oneAPI Level Zero specification](https://spec.oneapi.com/versions/latest/elements/l0/source/index.html)
* [Intel(R) OneApi Level Zero Specification API C/C++ header files](https://github.com/oneapi-src/level-zero/)
* [oneAPI Level Zero tests](https://github.com/oneapi-src/level-zero-tests/)

### OpenCL specific

* [OpenCL on Linux guide](https://github.com/bashbaug/OpenCLPapers/blob/markdown/OpenCLOnLinux.md)
* [Intel(R) GPU Compute Samples](https://github.com/intel/compute-samples)
* [Frequently Asked Questions](https://github.com/intel/compute-runtime/blob/master/opencl/doc/FAQ.md)
* [Interoperability with VTune](https://github.com/intel/compute-runtime/blob/master/opencl/doc/VTUNE.md)
* [OpenCL Conformance Tests](https://github.com/KhronosGroup/OpenCL-CTS/)

___(*) Other names and brands may be claimed as property of others.___