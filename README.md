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

## Building

<TODO:insert instructions here>


### Install

<TODO:insert instructions here>

## Supported Platforms

Intel Core Processors supporting Gen8 graphics devices - OpenCL 2.0  
Intel Core Processors supporting Gen9 graphics devices - OpenCL 2.1  
Intel Atom Processors supporting Gen9 graphics devices - OpenCL 1.2  

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

The driver has the following functional delta compared to previously released drivers:
* Intel's closed source SRB5.0 driver (aka Classic)  
  https://software.intel.com/en-us/articles/opencl-drivers#latest_linux_driver
* Intel's former open-source Beignet driver  
  https://01.org/beignet

### Generic extensions
* cl_khr_mipmap
* cl_khr_mipmap_writes
* cl_khr_priority_hints
* cl_khr_throttle_hints
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

