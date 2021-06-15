<!---

Copyright (C) 2021 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Release Notes v1.1

Level Zero Core API.

April 2021

## Changes in this release:

### Device allocations larger than 4GB size.
https://spec.oneapi.com/level-zero/latest/core/api.html?highlight=relaxed#relaxedalloclimits-enums

L0 driver now allows the allocation of buffers larger than 4GB. To use, the `ze_relaxed_allocation_limits_exp_desc_t`
structure needs to be passed to `zeMemAllocHost` or `zeMemAllocShared` as a linked descriptor.

Sample code:

```cpp
ze_relaxed_allocation_limits_exp_desc_t relaxedDesc = {};
relaxedDesc.stype = ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXP_DESC;
relaxedDesc.flags = ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE;

ze_device_mem_alloc_desc_t deviceDesc = {};
deviceDesc.pNext = &relaxedDesc;
zeMemAllocDevice(context, &deviceDesc, size, 0, device, &ptr);
```

In addition to this, kernels need to be compiled with `ze-opt-greater-than-4GB-buffer-required`. This needs to be
passed in `pBuildFlags` field in `ze_module_desc_t` descriptor while calling `zeModuleCreate`.

### zeDeviceGetGlobalTimestamps for CPU/GPU synchronized time.
https://spec.oneapi.com/level-zero/latest/core/api.html?highlight=zedevicegetglobaltimestamps#_CPPv427zeDeviceGetGlobalTimestamps18ze_device_handle_tP8uint64_tP8uint64_t

Returns synchronized Host and device global timestamps.

Sample code:

```cpp
ze_relaxed_allocation_limits_exp_desc_t relaxedDesc = {};
relaxedDesc.stype = ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXP_DESC;
relaxedDesc.flags = ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE;

ze_device_mem_alloc_desc_t deviceDesc = {};
deviceDesc.pNext = &relaxedDesc;
zeMemAllocDevice(context, &deviceDesc, size, 0, device, &ptr);
```

### Global work offset
https://spec.oneapi.com/level-zero/latest/core/api.html?highlight=globaloffset#_CPPv426zeKernelSetGlobalOffsetExp18ze_kernel_handle_t8uint32_t8uint32_t8uint32_t

Applications now can set a global work offset to kernels.

Sample code:

```cpp
...
uint32_t groupSizeX = sizeX;
uint32_t groupSizeY = 1u;
uint32_t groupSizeZ = 1u;
zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ);

uint32_t offsetx = offset;
uint32_t offsety = 0;
uint32_t offsetz = 0;
zeKernelSetGlobalOffsetExp(kernel, offsetx, offsety, offsetz);
...
```

### Atomic floating point properties
https://spec.oneapi.com/level-zero/latest/core/api.html?highlight=ze_structure_type_float_atomic_ext_properties#_CPPv432ze_float_atomic_ext_properties_t

Applications now can query for floating atomic properties supported by the device in a kernel.
This is done by passing `ze_float_atomic_ext_properties_t` to zeDeviceGetModuleProperties as a linked property structure.

Sample code:

```cpp
ze_device_module_properties_t kernelProperties = {};
ze_float_atomic_ext_properties_t extendedProperties = {};
extendedProperties.stype = ZE_STRUCTURE_TYPE_FLOAT_ATOMIC_EXT_PROPERTIES;
kernelProperties.pNext = &extendedProperties;
zeDeviceGetModuleProperties(hDevice, &kernelProperties);

if (extendedProperties.fp16Flags & ZE_DEVICE_FP_ATOMIC_EXT_FLAG_GLOBAL_ADD) {
    // kernel supports floating atomic add and subtract
}
```

### Context Creation for specific devices
https://spec.oneapi.com/level-zero/latest/core/api.html?highlight=zecontextcreate#_CPPv417zeContextCreateEx18ze_driver_handle_tPK17ze_context_desc_t8uint32_tP18ze_device_handle_tP19ze_context_handle_t

Added `zeContextCreateEX` to create a context with a set of devices. Resources allocated against that context
are visible only to the devices for which the context was created.

Sample code:

```cpp
std::vector<ze_device_handle_t> devices;
devices.push_back(device0);
devices.push_back(device1);
...
zeContextCreateEx(hDriver, &desc, devices.size(), devices.data(), &phContext);
```

### Change on timer resolution
https://spec.oneapi.com/level-zero/latest/core/api.html?highlight=timerresolution#_CPPv4N22ze_device_properties_t15timerResolutionE

Time resolution returned by device properties has been changed to cycles/second (v1.0 has a resolution of nano-seconds).
To help libraries with the transtition to the new resolution, the `UseCyclesPerSecondTimer` variable has been defined.
When set to 1, the driver will return the resolution defined for v1.1 (cycles/second), otherwise, it will still
return the resolution for v1.0 (nanoseconds). The use of this environment variable is only temporal while applications
and libraries complete their transition to v1.1 and will be eventually eliminated, leaving the resolution for v1.1 as default.

When reading querying for the timere resolution, applications then need to keep in mind:

* If `ZE_API_VERSION_1_0` returned by `zeDriverGetApiVersion`: Timer resolution is nanoseconds.
* If `ZE_API_VERSION_1_1` returned by `zeDriverGetApiVersion`: Timer resolution is nanoseconds, as in v1.0.
* If `ZE_API_VERSION_1_1` returned by `zeDriverGetApiVersion` and `UseCyclesPerSecondTimer=1`: Timer resolution is cycles per seconds, as in v1.1.

Note: In Release builds, `NEOReadDebugKeys=1` may be needed to read environment variables. To confirm the L0 driver is
reading the environment variables, please use `PrintDebugSettings=1`, which will print them at the beginning of the
application. See below:

```sh
$ PrintDebugSettings=1 UseCyclesPerSecondTimer=1 ./zello_world_gpu
Non-default value of debug variable: PrintDebugSettings = 1
Non-default value of debug variable: UseCyclesPerSecondTimer = 1
...
```

Sample code:

if `UseCyclesPerSecondTimer=1` set

```cpp
ze_api_version_t version;
zeDriverGetApiVersion(hDriver, &version);
...
ze_device_properties_t devProperties = {};
zeDeviceGetProperties(device, &devProperties);

if (version == ZE_API_VERSION_1_1) {
    uint64_t timerResolutionInCyclesPerSecond = devProperties.timerResolution;
} else {
    uint64_t timerResolutionInNanoSeconds = devProperties.timerResolution;
}

...
```

if `UseCyclesPerSecondTimer` not set

```cpp
ze_api_version_t version;
zeDriverGetApiVersion(hDriver, &version);
...
ze_device_properties_t devProperties = {};
zeDeviceGetProperties(device, &devProperties);

uint64_t timerResolutionInNanoSeconds = devProperties.timerResolution;
...
```