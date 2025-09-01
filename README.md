<!---

Copyright (C) 2018-2025 Intel Corporation

SPDX-License-Identifier: MIT

-->

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

|Platform|OpenCL|Level Zero|WSL
|--------|:----:|:-----:|:-:|
|DG1| 3.0 | 1.13 | Y |
|Alchemist| 3.0 | 1.13 | Y |
|Battlemage| 3.0 | 1.13 | Y |
|Tiger Lake| 3.0 | 1.13 | Y |
|Rocket Lake| 3.0 | 1.13 | Y |
|Alder Lake| 3.0 | 1.13 | Y |
|Raptor Lake| 3.0 | 1.13 | Y |
|Meteor Lake| 3.0 | 1.13 | Y |
|Arrow Lake| 3.0 | 1.13 | Y |
|Lunar Lake| 3.0 | 1.13 | Y |
|Panther Lake| 3.0 | 1.13 | Y |

_No code changes may be introduced that would regress support for any currently supported hardware. All contributions must ensure continued compatibility and functionality across all supported hardware platforms. Failure to maintain hardware compatibility may result in the rejection or reversion of the contribution. Any deliberate modifications or removal of hardware support will be transparently communicated in the release notes._

_Debug parameters, environmental variables, and internal data structures are considered as internal implementation detail and may be changed or removed at any time._

## Support for legacy platforms

Support for Gen8, Gen9 and Gen11 devices is delivered via packages with legacy1 suffix, more details [here](LEGACY_PLATFORMS.md)

## Legacy Platforms

|Platform|OpenCL|Level Zero|WSL|
|--------|:----:|:--------:|:-:|
|Intel Core Processors with Gen8 graphics devices (formerly Broadwell)| 3.0 | - | - |
|Intel Core Processors with Gen9 graphics devices (formerly Skylake, Kaby Lake)| 3.0 | 1.5 | - |
|Intel Core Processors with Gen9 graphics devices (formerly Coffee Lake)| 3.0 | 1.5 | Y |
|Intel Atom Processors with Gen9 graphics devices (formerly Apollo Lake, Gemini Lake)| 3.0 | - | - |
|Intel Core Processors with Gen11 graphics devices (formerly Ice Lake)| 3.0 | 1.5 | Y |
|Intel Atom Processors with Gen11 graphics devices (formerly Elkhart Lake)| 3.0 | - | Y |

## Release cadence

_Release cadence changed from weekly to monthly late 2022_

* At the beginning of each calendar month, we identify a well-tested driver version from the previous month as a release candidate for our monthly release.
* We create a release branch and apply selected fixes for significant issues. 
* The branch naming convention is releases/yy.ww (yy - year, ww - work week of release candidate)
* The builds are tagged using the following format: yy.ww.bbbbb.hh (yy - year, ww - work week, bbbbb - incremental build number from the master branch, hh - incremental commit number on release branch).
* We publish and document a monthly release from the tip of that branch. 
* During subsequent weeks of a given month, we continue to cherry-pick fixes to that branch and may publish a hotfix release. 
* Quality level of the driver (per platform) will be provided in the Release Notes.
* Once a monthly release is posted on compute-runtime GitHub, it may propagate to secondary release channels and/or be repackaged / rebuilt for convenience (e.g., [intel-graphics](https://launchpad.net/~kobuk-team/+archive/ubuntu/intel-graphics)). Users should choose package origin (GitHub/PPA) that is most convenient for them.
* A secondary channel must not release a version that was not released on GitHub prior.
 

## Installation Options

To allow NEO access to GPU device make sure user has permissions to files /dev/dri/renderD*.

### Via system package manager

NEO is available for installation on a variety of Linux distributions
and can be installed via the distro's package manager.

For example on Ubuntu* 22.04:

```
apt-get install intel-opencl-icd
```

### Manual download

.deb packages for Ubuntu are provided along with installation instructions and
Release Notes on the [release page](https://github.com/intel/compute-runtime/releases)

## Linking applications

Directly linking to the runtime library is not supported:
* Level Zero applications should link with [Level Zero loader](https://github.com/oneapi-src/level-zero)
* OpenCL applications should link with [ICD loader library](https://github.com/KhronosGroup/OpenCL-ICD-Loader)

## Dependencies

* GmmLib - https://github.com/intel/gmmlib
* Intel Graphics Compiler - https://github.com/intel/intel-graphics-compiler

In addition, to enable performance counters support, the following packages are needed:
* Intel(R) Metrics Discovery (MDAPI) - https://github.com/intel/metrics-discovery
* Intel(R) Metrics Library for MDAPI - https://github.com/intel/metrics-library

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
* [Programmers Guide](https://github.com/intel/compute-runtime/blob/master/programmers-guide/PROGRAMMERS_GUIDE.md)
* [Issue Submission Guide](https://github.com/intel/compute-runtime/blob/master/ISSUE_SUBMISSION_GUIDE.md)
* [Frequently Asked Questions](https://github.com/intel/compute-runtime/blob/master/FAQ.md)

### Level Zero specific
* [oneAPI Level Zero specification](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/index.html)
* [Intel(R) OneApi Level Zero Specification API C/C++ header files](https://github.com/oneapi-src/level-zero/)
* [oneAPI Level Zero tests](https://github.com/oneapi-src/level-zero-tests/)

### OpenCL specific

* [OpenCL on Linux guide](https://github.com/bashbaug/OpenCLPapers/blob/markdown/OpenCLOnLinux.md)
* [Intel(R) GPU Compute Samples](https://github.com/intel/compute-samples)
* [Frequently Asked Questions](https://github.com/intel/compute-runtime/blob/master/opencl/doc/FAQ.md)
* [Interoperability with VTune](https://github.com/intel/compute-runtime/blob/master/opencl/doc/VTUNE.md)
* [OpenCL Conformance Tests](https://github.com/KhronosGroup/OpenCL-CTS/)

___(*) Other names and brands may be claimed as property of others.___
