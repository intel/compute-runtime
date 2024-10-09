<!---

Copyright (C) 2023 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Allocations greater than 4GB

* [Introduction](#Introduction)
* [Creating allocations greater than 4GB](#creating-allocations-greater-than-4GB)
* [Intel Graphics Compiler build flags](#intel-graphics-compiler-build-flags)

# Introduction

OpenCL and Level Zero APIs allow to allocate memory with size restrictions. Maximum allocation size for those APIs can be queried by

* `clGetDeviceInfo` with param name `CL_DEVICE_MAX_MEM_ALLOC_SIZE` in OpenCL
* `zeDeviceGetProperties` in Level Zero

According to HW architecture, "stateful addressing model" limits maximum allocation size to 4GB. Because of this limitation, default maximum size supported by NEO is 4GB. 


It's possible to relax this limitation for both APIs under certain conditions:

* kernel must be compiled in stateless mode [Intel Graphics Compiler Build Flags](#intel-graphics-compiler-build-flags)
* memory must be allocated with flag allowing bigger allocation size [Creating Allocations Greater Than 4GB](#creating-allocations-greater-than-4GB)

# Creating allocations greater than 4GB

## Level Zero

To allocate memory greater than 4GB in Level Zero, it is necessary to pass `ze_relaxed_allocation_limits_exp_desc_t` struct to API call that allocates memory. 

This structure must be passed by `pNext` member of: 
* `ze_device_mem_alloc_desc_t` when allocating with `zeMemAllocShared` and `zeMemAllocDevice`

    ```cpp
    ze_relaxed_allocation_limits_exp_desc_t relaxedAllocationLimitsExpDesc = {ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXP_DESC};
    relaxedAllocationLimitsExpDesc.flags |= ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE;

    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.pNext = &relaxedAllocationLimitsExpDesc;

    zeMemAllocDevice(hContext, &deviceDesc, size, alignment, hDevice, pptr);
    ```

* `ze_host_mem_alloc_desc_t` when allocating with `zeMemAllocHost`

    ```cpp
    ze_relaxed_allocation_limits_exp_desc_t relaxedAllocationLimitsExpDesc = {ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXP_DESC};
    relaxedAllocationLimitsExpDesc.flags |= ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    hostDesc.pNext = &relaxedAllocationLimitsExpDesc;

    zeMemAllocHost(hContext, &hostDesc, size, alignment, pptr);
    ```

Structure `ze_relaxed_allocation_limits_exp_desc_t` must have `ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE` flag set. 


## OpenCL

To allocate memory greater than 4GB in OpenCL you need to use `CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL` flag. 

* For api calls:
    * `clCreateBuffer`
    * `clCreateBufferWithProperties` 	
    * `clCreateBufferWithPropertiesINTEL`

    `CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL` flag must be set in passed `cl_mem_flags flags` param.

    ```cpp
    cl_mem_flags flags = 0;
    flags |= CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;

    cl_mem buffer = clCreateBuffer(context, flags, size, host_ptr, errcode_ret);
    ```
* For api call:
    * `clSVMAlloc` 

    `CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL` flag must be set in passed `cl_svm_mem_flags flags` param.

    ```cpp
    cl_svm_mem_flags flags = 0;
    flags |= CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;

    void *alloc = clSVMAlloc(context, flags, size, alignment);
    ```

* For api calls:
    * `clSharedMemAllocINTEL`
    * `clDeviceMemAllocINTEL`
    * `clHostMemAllocINTEL`

    `CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL` flag must be set in `cl_mem_flags` or `cl_mem_flags_intel` property, in `cl_mem_properties_intel *properties` param.

    ```cpp
    cl_mem_flags_intel flags = 0;
    flags |= CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;
    cl_mem_properties_intel properties[] = {CL_MEM_FLAGS_INTEL, flags, 0};

    void *alloc = clSharedMemAllocINTEL(context, device, properties, size, alignment, errcode_ret);
    ```

## Debug Keys

NEO allows to relax buffer size limitation with Debug Key named `AllowUnrestrictedSize` (Works with both APIs) 

When set to 1 - maximum allocation size is ignored during buffer creation, despite `ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE`/`CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL` is not passed.

When set to 0 - size restrictions are enforced.

You need to keep in mind that it's only a debug key which is used for driver development and debug process. It's not a part of specification so there is no guarantee that it will work correctly in every case and can be deprecated in any time. 


# Intel Graphics Compiler build flags

To compile a kernel in stateless addressing model required to allow use of buffers that are bigger than 4GB, following compilation flag must be used:

## Level Zero 

`-ze-opt-greater-than-4GB-buffer-required` This flag must be set in `pBuildFlags` member of `ze_module_desc_t` that is passed to `zeModuleCreate`

```cpp
ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
moduleDesc.pInputModule = moduleData;
moduleDesc.inputSize = moduleSize;
moduleDesc.pBuildFlags = "-ze-opt-greater-than-4GB-buffer-required";

zeModuleCreate(hContext, hDevice, &moduleDesc, phModule, phBuildLog);
```

## OpenCL 

`-cl-intel-greater-than-4GB-buffer-required` This flag must be set in `options` param that is passed to `clBuildProgram`

```cpp
const char options[] = "-cl-intel-greater-than-4GB-buffer-required";

clBuildProgram(program, num_devices, device_list, options, callback, user_data);
```


When above flags are passed, compiler compiles kernels in a stateless addressing model allowing usage of allocations of any size.

# References

https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html#relaxedalloclimits

https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html#ze-module-desc-t
