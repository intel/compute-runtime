# Intel(R) Graphics Compute Runtime for OpenCL(TM)

## Known Issues and Limitations

OpenCL compliance of a driver built from open-source components should not be
assumed by default. Intel will clearly designate / tag specific builds to
indicate production quality including formal compliance. Other builds should be
considered experimental.

### Build limitations

While NEO can be compiled with various clang/gcc compilers, to compile the whole stack the following are required:
* clang-4.0
* llvm-4.0
* gcc < 7.0
* Python 2.7

### ICD Loader

The driver requires Khronos ICD loader to operate correctly:
https://github.com/KhronosGroup/OpenCL-ICD-Loader

When building NEO, you will get a warning (build will continue), if source code for this repository is missing in your workspace.

Workaround:
If ocl-icd package (version >= 2.2.11) is installed (or included in distribution), set environment variable OCL_ICD_ASSUME_ICD_EXTENSION=1

### Functional delta

The driver has the following functional delta compared to previously released drivers:
* Intel's closed source SRB5.0 driver (aka Classic)
  https://software.intel.com/en-us/articles/opencl-drivers#latest_linux_driver
* Intel's former open-source Beignet driver
  https://01.org/beignet

#### Generic extensions
* cl_khr_mipmap
* cl_khr_mipmap_writes

_Currently under development_

#### Preview extensions
* cl_intelx_video_enhancement
* cl_intelx_video_enhancement_camera_pipeline
* cl_intelx_video_enhancement_color_pipeline
* cl_intelx_hevc_pak

_Currently no plan to implement. If interested in these features, please use SRB5 to evaluate and provide feedback._

#### Other capabilities
* OpenGL sharing with MESA driver - _will implement in the future (no specific timeline)_
* CL_MEM_SVM_FINE_GRAIN_BUFFER (if using unpatched i915) - _patch is WIP_

___(*) Other names and brands my be claimed as property of others.___

