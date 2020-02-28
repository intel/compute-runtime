# Intel(R) Graphics Compute Runtime for OpenCL(TM)

## Introduction

The Intel(R) Graphics Compute Runtime for OpenCL(TM) is an open source project to
converge Intel's development efforts on OpenCL(TM) compute stacks supporting the
GEN graphics hardware architecture.

Please refer to http://01.org/compute-runtime for additional details regarding Intel's
motivation and intentions wrt OpenCL support in open source.

## License

The Intel(R) Graphics Compute Runtime for OpenCL(TM) is distributed under the MIT License.

You may obtain a copy of the License at: https://opensource.org/licenses/MIT

## Supported Platforms

* Intel Core Processors with Gen8 graphics devices (formerly Broadwell) - OpenCL 2.1
* Intel Core Processors with Gen9 graphics devices (formerly Skylake, Kaby Lake, Coffee Lake) - OpenCL 2.1
* Intel Atom Processors with Gen9 graphics devices (formerly Apollo Lake, Gemini Lake) - OpenCL 1.2
* Intel Core Processors with Gen11 graphics devices (formerly Ice Lake) - OpenCL 2.1
* Intel Core Processors with Gen12 graphics devices (formerly Tiger Lake) - OpenCL 2.1

## Release cadence

* Once a week, we run extended validation cycle on a selected driver.
* When the extended validation cycle tests pass, the corresponding commit on github is tagged using
the format yy.ww.bbbb (yy - year, ww - work week, bbbb - incremental build number).
* Typically for weekly tags we will post a binary release (e.g. deb).
* Quality level of the driver (per platform) will be provided in the Release Notes.

## Installation Options

To allow NEO accessing GPU device make sure user has permissions to files /dev/dri/renderD*.

Under Ubuntu* or Centos* user must be in video group.
In Fedora* all users by default have access to /dev/dri/renderD* files.

### Via system package manager

NEO is available for installation on a variety of Linux distributions
and can be installed via the distro's package manager.

For example on Ubuntu* 19.04, 19.10:

```
apt-get install intel-opencl-icd
```

Procedures for other
[distributions](https://github.com/intel/compute-runtime/blob/master/documentation/DISTRIBUTIONS.md).

## Linking applications

When building applications, they should link with ICD loader library (ocl-icd).
Directly linking to the runtime library (igdrcl) is not supported.

### Manual download

.deb packages for Ubuntu are provided along with installation instructions and
Release Notes on the [release page](https://github.com/intel/compute-runtime/releases)

## Dependencies

* GmmLib - https://github.com/intel/gmmlib
* Intel Graphics Compiler - https://github.com/intel/intel-graphics-compiler

## Optional dependencies

To enable
[cl_intel_va_api_media_sharing](https://www.khronos.org/registry/OpenCL/extensions/intel/cl_intel_va_api_media_sharing.txt)
extension, the following packages are required:

* libdrm - https://anongit.freedesktop.org/git/mesa/drm.git
* libva - https://github.com/intel/libva.git

## How to provide feedback

By default, please submit an issue using native github.com [interface](https://github.com/intel/compute-runtime/issues).

## How to contribute

Create a pull request on github.com with your patch. Make sure your change is cleanly building and passing ULTs.
A maintainer will contact you if there are questions or concerns.
See
[contribution guidelines](https://github.com/intel/compute-runtime/blob/master/documentation/CONTRIBUTING.md)
for more details.

## See also

* [Contribution guidelines](https://github.com/intel/compute-runtime/blob/master/documentation/CONTRIBUTING.md)
* [Frequently Asked Questions](https://github.com/intel/compute-runtime/blob/master/FAQ.md)

### OpenCL specific

* [OpenCL on Linux guide](https://github.com/bashbaug/OpenCLPapers/blob/markdown/OpenCLOnLinux.md)
* [Intel(R) GPU Compute Samples](https://github.com/intel/compute-samples)
* [Frequently Asked Questions](https://github.com/intel/compute-runtime/blob/master/opencl/doc/FAQ.md)
* [Known issues and limitations](https://github.com/intel/compute-runtime/blob/master/opencl/doc/LIMITATIONS.md)
* [Interoperability with VTune](https://github.com/intel/compute-runtime/blob/master/opencl/doc/VTUNE.md)
* [OpenCL Conformance Tests](https://github.com/KhronosGroup/OpenCL-CTS/)

___(*) Other names and brands may be claimed as property of others.___