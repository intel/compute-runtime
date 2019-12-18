# Intel(R) Graphics Compute Runtime for OpenCL(TM)

## Introduction

The Intel(R) Graphics Compute Runtime for OpenCL(TM) is an open source project to
converge Intel's development efforts on OpenCL(TM) compute stacks supporting the
GEN graphics hardware architecture.

Please refer to http://01.org/compute-runtime for additional details regarding Intel's
motivation and intentions wrt OpenCL support in the open source.

## License

The Intel(R) Graphics Compute Runtime for OpenCL(TM) is distributed under the MIT License.

You may obtain a copy of the License at: https://opensource.org/licenses/MIT

## Installation Options

To allow Neo accessing GPU device make sure user has permissions to files in /dev/dri directory. In first step /dev/dri/renderD* files are opened, if it fails, /dev/dri/card* files are used.

Under Ubuntu* or Centos* user must be in video group. In Fedora* all users by default have access to /dev/dri/renderD* files, but have to be in video group to access /dev/dri/card* files.

### Via system package manager

NEO is available for installation on a variety of Linux distributions and can be installed via the distro's package manager. 

For example on Ubuntu* 19.04, 19.10:

```
apt-get install intel-opencl-icd
```

Procedures for other [distributions](https://github.com/intel/compute-runtime/blob/master/documentation/Neo_in_distributions.md).

### Manual download

.deb packages for Ubuntu are provided along with installation instructions and Release Notes on the [release page](https://github.com/intel/compute-runtime/releases)


## Dependencies

* GmmLib - https://github.com/intel/gmmlib
* Intel Graphics Compiler - https://github.com/intel/intel-graphics-compiler

## Optional dependencies

Below packages are needed to enable [cl_intel_va_api_media_sharing](https://www.khronos.org/registry/OpenCL/extensions/intel/cl_intel_va_api_media_sharing.txt) extension

* libdrm - https://anongit.freedesktop.org/git/mesa/drm.git
* libva - https://github.com/intel/libva.git

## Supported Platforms

* Intel Core Processors with Gen8 graphics devices (formerly Broadwell) - OpenCL 2.1
* Intel Core Processors with Gen9 graphics devices (formerly Skylake, Kaby Lake, Coffee Lake) - OpenCL 2.1
* Intel Atom Processors with Gen9 graphics devices (formerly Apollo Lake, Gemini Lake) - OpenCL 1.2
* Intel Core Processors with Gen11 graphics devices (formerly Ice Lake) - OpenCL 2.1
* Intel Core Processors with Gen12 graphics devices (formerly Tiger Lake) - OpenCL 2.1

## Linking applications

When building applications, they should link with ICD loader library (ocl-icd).
Directly linking to the runtime library (igdrcl) is not supported.

## Tutorial applications

The [Intel(R) GPU Compute Samples repository](https://github.com/intel/compute-samples/blob/master/compute_samples/applications/usm_hello_world/README.md) 
has sample source code to demonstrate features of Intel(R) Graphics Compute Runtime for OpenCL(TM) Driver.

## How to provide feedback

By default, please submit an issue using native github.com interface: https://github.com/intel/compute-runtime/issues.

## How to contribute

Create a pull request on github.com with your patch. Make sure your change is cleanly building and passing ULTs.
A maintainer will contact you if there are questions or concerns.
See [contribution guidelines](https://github.com/intel/compute-runtime/blob/master/documentation/CONTRIB.md) for more details.

## See also

* [OpenCL on Linux guide](https://github.com/bashbaug/OpenCLPapers/blob/markdown/OpenCLOnLinux.md)
* Interoperability with Intel Tools: [TOOLS.md](https://github.com/intel/compute-runtime/blob/master/documentation/TOOLS.md)
* Contribution guidelines: [CONTRIB.md](https://github.com/intel/compute-runtime/blob/master/documentation/CONTRIB.md)
* Known issues and limitations: [LIMITATIONS.md](https://github.com/intel/compute-runtime/blob/master/documentation/LIMITATIONS.md)
* Frequently asked questions: [FAQ.md](https://github.com/intel/compute-runtime/blob/master/documentation/FAQ.md)
* Quality expectations: [RELEASES.md](https://github.com/intel/compute-runtime/blob/master/documentation/RELEASES.md)

___(*) Other names and brands may be claimed as property of others.___
