# Intel(R) Graphics Compute Runtime for OpenCL(TM)

## Known Issues and Limitations

OpenCL compliance of a driver built from open-source components should not be
assumed by default. Intel will clearly designate / tag specific builds to
indicate production quality including formal compliance. Other builds should be
considered experimental. 

The driver requires Khronos ICD loader to operate correctly:
https://github.com/KhronosGroup/OpenCL-ICD-Loader

The driver has the following functional delta compared to previously released drivers:
* Intel's closed source SRB5.0 driver (aka Classic)  
  https://software.intel.com/en-us/articles/opencl-drivers#latest_linux_driver
* Intel's former open-source Beignet driver  
  https://01.org/beignet

### Generic extensions
* cl_khr_mipmap
* cl_khr_mipmap_writes
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

