# Intel(R) Graphics Compute Runtime for OpenCL(TM)

## Introduction

The Intel(R) Graphics Compute Runtime for OpenCL(TM) is a open source project to
converge Intel's development efforts on OpenCL(TM) compute stacks supporting the
GEN graphics hardware architecture.

Please refer to http://01.org/compute-runtime for additional details regarding Intel's
motivation and intentions wrt OpenCL support in the open source.

## License

The Intel(R) Graphics Compute Runtime for OpenCL(TM) is distributed under the MIT License.

You may obtain a copy of the License at: https://opensource.org/licenses/MIT

## Dependencies

* GmmLib - https://github.com/intel/gmmlib  
* Intel Graphics Compiler - https://github.com/intel/intel-graphics-compiler  
* Khronos OpenCL Headers - https://github.com/KhronosGroup/OpenCL-Headers  

## Optional dependencies

Below packages are needed to enable [cl_intel_va_api_media_sharing](https://www.khronos.org/registry/OpenCL/extensions/intel/cl_intel_va_api_media_sharing.txt) extension

* libdrm - https://anongit.freedesktop.org/git/mesa/drm.git  
* libva - https://github.com/intel/libva.git  

## Supported Platforms

* Intel Core Processors with Gen8 graphics devices (formerly Broadwell) - OpenCL 2.1  
* Intel Core Processors with Gen9 graphics devices (formerly Skylake, Kaby Lake, Coffee Lake) - OpenCL 2.1  
* Intel Atom Processors with Gen9 graphics devices (formerly Apollo Lake, Gemini Lake) - OpenCL 1.2  

## Linking applications

When building applications, they should link with ICD loader library (ocl-icd).
Directly linking to the runtime library (igdrcl) is not supported.

## How to provide feedback

By default, please submit an issue using native github.com interface: https://github.com/intel/compute-runtime/issues.  

## How to contribute

Create a pull request on github.com with your patch. Make sure your change is cleanly building and passing ULTs.
A maintainer will contact you if there are questions or concerns.

## See also
* Building and installation:
  * [Ubuntu 16.04](https://github.com/intel/compute-runtime/blob/master/documentation/BUILD_Ubuntu.md)
  * [Centos 7](https://github.com/intel/compute-runtime/blob/master/documentation/BUILD_Centos.md)
* Contribution guidelines: [CONTRIB.md](https://github.com/intel/compute-runtime/blob/master/documentation/CONTRIB.md)
* Known issues and limitations: [LIMITATIONS.md](https://github.com/intel/compute-runtime/blob/master/documentation/LIMITATIONS.md)
* Frequently asked questions: [FAQ.md](https://github.com/intel/compute-runtime/blob/master/documentation/FAQ.md)
* Quality expectations: [RELEASES.md](https://github.com/intel/compute-runtime/blob/master/documentation/RELEASES.md)

___(*) Other names and brands my be claimed as property of others.___
