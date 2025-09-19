<!---

Copyright (C) 2021-2025 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Release Notes v1.13

Level Zero Core API.

September 2025

Changes in this release:

The update involves changes introduced across Level Zero spec from v1.6 upto and including v1.13. Some of the important changes are as follows:

| Feature	| Spec link	|
| ------------------ | -------------------|
| Support for RTAS Builder API | https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html#rtasbuilder |
| Support for Counter Based Event Pool API | https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html#counterbasedeventpool |
| Support for Bindless Images API | https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html#bindlessimages |
| Support for Immediate Command List Append API | https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html#immediatecommandlistappend |
| Support for Mutable Command List API | https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html#mutablecommandlist |

# Release Notes v1.6

Level Zero Core API.

September 2024

Changes in this release:

The update involves changes introduced in Level Zero spec v1.6. Some of the important changes are as follows:

| Feature	| Spec link	| Notes |
| ------------------ | -------------------| ----------------------------- |
| Support for zeCommandListHostSynchronize API |	https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html#zecommandlisthostsynchronize | |
| Support for zeDriverGetLastErrorDescription API |	https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html#zedrivergetlasterrordescription | |
| Support for zeMemPutIpcHandle API | https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html#zememputipchandle | |
| Support for EventPool Get and put IPC handle APIs | https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html#zeeventpoolgetipchandle , https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html#zeeventpoolputipchandle | |
| Support for host mapped and synchronized event timestamp extension API | https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html#eventquerykerneltimestamps | |

# Release Notes v1.5

Level Zero Core API.

June 2024

Changes in this release:

The update involves changes introduced across Level Zero spec from v1.3 upto and including v1.5. Some of the important changes are as follows:

| Feature	| Spec link	| Notes |
| ------------------ | -------------------| ----------------------------- |
| Support for Device IP Version extension |	https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html#deviceipversion-structures | |
| Support for Image view and Image view planar extension |	https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html#imageview | |
| Support for sub allocation properties extension |	https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html#suballocationsproperties	| |
| Allow IPC events with timestamp events		| | Previously spec had limitation disallowing usage of IPC for event pools created with timestamp flag. This limitation is now removed for both spec and implementation |
| Support for kernel max group size properties extension |	https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html#kernelmaxgroupsizeproperties	| |


# Release Notes v1.3

Level Zero Core API.

January 2022

## Changes in this release:

### Implicit Scaling

Implicit scaling has been enabled by default on Level Zero on Xe HPC (PVC) B and later steppings. The `EnableImplicitScaling` debug key may be used to enable (`EnableImplicitScaling=1`) or disable (`EnableImplicitScaling=0`) implicit scaling on on Xe HPC and other multi-tile architectures.

### [Blocking Free](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html#zememfreeext)

The blocking free memory policy has been implemented for `zeMemFreeExt` extension. Defer free policy will be added in upcoming releases.

### [PCI Properties Extension](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/EXT_PCIProperties.html#pci-properties-extension)

Support for PCI properties extension has been added via `zeDevicePciGetPropertiesExt` interface. This currently provides access to device's BDF address only. Device bandwidth property will be exposed in future based on support from underlying components

### [Memory Compression Hints](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/EXT_MemoryCompressionHints.html#memory-compression-hints-extension)

Memory compression hints for shared and device memory allocations and images have been added.

### Sampler Address Modes Fix

Level Zero driver had a bug in the implementation of the ZE_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER and ZE_SAMPLER_ADDRESS_MODE_CLAMP address modes, where this were being implemented invertedly. This is now fixed and users can use driver's version to determine which address mode to use. Details on how DPC++ is handling this can be found in:

[https://github.com/intel/llvm/blob/756c2e8fb45e44b51b32bd8a22b3c325f17bb5c9/sycl/plugins/level_zero/pi_level_zero.cpp#L5264?]


# Release Notes v1.2

Level Zero Core API.

August 2021

## Changes in this release:

### [Extension to create image views for planar formats](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/EXT_ImageViewPlanar.html#ze-extension-image-view-planar)

This extension allows accessing each plane for planar formats and have different interpretations of created images.

Sample code:

[https://github.com/intel/compute-runtime/blob/master/level_zero/core/test/black_box_tests/zello_image_view.cpp](https://github.com/intel/compute-runtime/blob/master/level_zero/core/test/black_box_tests/zello_image_view.cpp)

### [Extension for querying image properties](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html?highlight=ze_image_memory_properties_exp_t#_CPPv432ze_image_memory_properties_exp_t)

This extension allows querying the different properties of an image, such as size, row pitch, and slice pitch.

### [Definition of ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2 properties](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html#_CPPv4N19ze_structure_type_t39ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2E)

`ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2` properties allows users to request driver to return timer resolution in cycles per seconds,
as defined v1.2 specification:

```cpp
ze_api_version_t version;
zeDriverGetApiVersion(hDriver, &version);
...
ze_device_properties_t devProperties = {};
devProperties->stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2;
zeDeviceGetProperties(device, &devProperties);

uint64_t timerResolutionInCyclesPerSecond = devProperties.timerResolution;
```

If `ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2` is not set, then timer resolution is returned in nanoseconds, as defined in v1.1.

```cpp
ze_api_version_t version;
zeDriverGetApiVersion(hDriver, &version);
...
ze_device_properties_t devProperties = {};
zeDeviceGetProperties(device, &devProperties);

uint64_t timerResolutionInNanoSeconds = devProperties.timerResolution;
```
### Extension to set preferred allocation for USM shared allocations
[`ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_INITIAL_PLACEMENT`](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html#_CPPv4N26ze_device_mem_alloc_flag_t47ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_INITIAL_PLACEMENTE) and [`ZE_HOST_MEM_ALLOC_FLAG_BIAS_INITIAL_PLACEMENT`](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html#_CPPv4N24ze_host_mem_alloc_flag_t45ZE_HOST_MEM_ALLOC_FLAG_BIAS_INITIAL_PLACEMENTE) can now be set in
`ze_device_mem_alloc_flags_t` and `ze_host_mem_alloc_flags_t`, respectively, when creating a shared-alloaction, to indicate
the driver where a shared-allocation should be initially placed.

### [IPC Memory Cache Bias Flags](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html?highlight=ze_ipc_memory_flag_bias_cached#ze-ipc-memory-flags-t)

`ZE_IPC_MEMORY_FLAG_BIAS_CACHED` and `ZE_IPC_MEMORY_FLAG_BIAS_UNCACHED ` can be passed when opening an IPC
memory handle with `zeMemOpenIpcHandle` to set the cache settings of the imported allocation.

### [Support for preferred group size](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html?highlight=ze_kernel_preferred_group_size_properties_t#ze-kernel-preferred-group-size-properties-t)

`ze_kernel_preferred_group_size_properties_t` can be used through `zeKernelGetProperties` to query for the preferred
multiple group size of a kernel for submission. Submitting a kernel with the preferred group size returned by the driver
may improve performance in certain platforms.

### [Module compilation options](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/PROG.html#module-build-options)

Optimization levels can now be passed to `zeModuleCreate` using the `-ze-opt-level` option, which are then communicated
to the underlying graphics compiler as hint to indicate the level of optimization desired.

### [Extension to read the timestamps of each subdevice](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html?highlight=zeeventquerytimestampsexp#zeeventquerytimestampsexp)

This extension defines the `zeEventQueryTimestampsExp` interface to query for timestamps of the parent device or
all of the available subdevices.

### [Extension to set thread arbitration policy](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html?highlight=ze_structure_type_device_properties_1_2#kernelschedulinghints)

The `zeKernelSchedulingHintExp` interface allows applications to set the thread arbitration policy desired for the
target kernel. Available policies can be queried by application through `zeDeviceGetModuleProperties` with the
[`ze_scheduling_hint_exp_properties_t`](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html?highlight=ze_scheduling_hint_exp_properties_t#_CPPv435ze_scheduling_hint_exp_properties_t) structure.

Policies include:

* `ZE_SCHEDULING_HINT_EXP_FLAG_OLDEST_FIRST`
* `ZE_SCHEDULING_HINT_EXP_FLAG_ROUND_ROBIN`
* `ZE_SCHEDULING_HINT_EXP_FLAG_STALL_BASED_ROUND_ROBIN`

### [Extension for cache reservation](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/EXT_CacheReservation.html#cache-reservation-extension)

With `zeDeviceReserveCacheExt`, applications can reserve sections of the GPU cache for exclusive use. Cache level
support varies between platforms.

Likewise, `zeDeviceSetCacheAdviceExt`, can be used to set a region of the cached as reserved or non-reserved region. If default behavior selected, then non-reserved is used, where region is accessible to all clients or applications.


# Release Notes v1.1

Level Zero Core API.

April 2021

## Changes in this release:

### Device allocations larger than 4GB size.
https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html?highlight=relaxed#relaxedalloclimits-enums

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
https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html?highlight=zedevicegetglobaltimestamps#_CPPv427zeDeviceGetGlobalTimestamps18ze_device_handle_tP8uint64_tP8uint64_t

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
https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html?highlight=globaloffset#_CPPv426zeKernelSetGlobalOffsetExp18ze_kernel_handle_t8uint32_t8uint32_t8uint32_t

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
https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html?highlight=ze_structure_type_float_atomic_ext_properties#_CPPv432ze_float_atomic_ext_properties_t

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
https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html?highlight=zecontextcreate#_CPPv417zeContextCreateEx18ze_driver_handle_tPK17ze_context_desc_t8uint32_tP18ze_device_handle_tP19ze_context_handle_t

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
https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html?highlight=timerresolution#_CPPv4N22ze_device_properties_t15timerResolutionE

Time resolution returned by device properties has been changed to cycles/second (v1.0 has a resolution of nano-seconds).
To help libraries with the transition to the new resolution, the `UseCyclesPerSecondTimer` variable has been defined.
When set to 1, the driver will return the resolution defined for v1.1 (cycles/second), otherwise, it will still
return the resolution for v1.0 (nanoseconds). The use of this environment variable is only temporal while applications
and libraries complete their transition to v1.1 and will be eventually eliminated, leaving the resolution for v1.1 as default.

When reading querying for the timer resolution, applications then need to keep in mind:

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
