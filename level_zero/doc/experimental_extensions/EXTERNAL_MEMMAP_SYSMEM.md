<!---

Copyright (C) 2026 Intel Corporation

SPDX-License-Identifier: MIT

-->


# External Memory Mapping for System Memory

* [Overview](#Overview)
* [Definitions](#Definitions)
* [Usage](#Usage)
* [Known Issues and Limitations](#Known-Issues-and-Limitations)

# Overview

`ze_external_memmap_sysmem` lets an application take memory it has **already
allocated itself** - a plain host allocation such as `malloc`, `aligned_alloc`,
`mmap`, or a framework buffer - and map it into a Level Zero USM host allocation
**without a copy**. The pointer returned by `zeMemAllocHost` refers to the same
physical pages as the application buffer and is directly accessible by both the
CPU and the GPU.

This is useful whenever the data does not originate inside a Level Zero
allocation and you want to avoid the extra allocation-plus-`memcpy` step that
would otherwise be required to bring it onto the GPU. On unified-memory devices
(where the CPU and GPU share physical DRAM) this gives true zero-copy access to
an application-owned buffer. It is the Level Zero equivalent of registering an
existing host pointer for device access (for example `cudaHostRegister`).

The extension is exposed as an extended descriptor passed through the `pNext`
member of `ze_host_mem_alloc_desc_t`; it does not add any new entry points.

# Definitions

```cpp
///////////////////////////////////////////////////////////////////////////////
#ifndef ZE_EXTERNAL_MEMORY_MAPPING_EXT_NAME
/// @brief External Memory Mapping Extension Name
#define ZE_EXTERNAL_MEMORY_MAPPING_EXT_NAME  "ZE_extension_external_memmap_sysmem"
#endif // ZE_EXTERNAL_MEMORY_MAPPING_EXT_NAME

///////////////////////////////////////////////////////////////////////////////
/// @brief External Memory Mapping Extension Version(s)
typedef enum _ze_external_memmap_sysmem_ext_version_t
{
    ZE_EXTERNAL_MEMMAP_SYSMEM_EXT_VERSION_1_0 = ZE_MAKE_VERSION( 1, 0 ),     ///< version 1.0
    ZE_EXTERNAL_MEMMAP_SYSMEM_EXT_VERSION_CURRENT = ZE_MAKE_VERSION( 1, 0 ), ///< latest known version
    ZE_EXTERNAL_MEMMAP_SYSMEM_EXT_VERSION_FORCE_UINT32 = 0x7fffffff
} ze_external_memmap_sysmem_ext_version_t;
```

## Structures

```cpp
///////////////////////////////////////////////////////////////////////////////
/// @brief Maps external system memory for an allocation
///
/// @details
///     - This structure may be passed to ::zeMemAllocHost, via the `pNext`
///       member of ::ze_host_mem_alloc_desc_t to map system memory for a host
///       allocation.
///     - The system memory pointer and size being mapped must be page aligned
///       based on the supported page sizes on the device.
typedef struct _ze_external_memmap_sysmem_ext_desc_t
{
    ze_structure_type_t stype;      ///< [in] ::ZE_STRUCTURE_TYPE_EXTERNAL_MEMMAP_SYSMEM_EXT_DESC
    const void* pNext;              ///< [in][optional] pointer to extension-specific structure
    void* pSystemMemory;           ///< [in] system memory pointer to map; must be page-aligned.
    uint64_t size;                 ///< [in] size of the system memory to map; must be page-aligned.
} ze_external_memmap_sysmem_ext_desc_t;
```

# Usage

Check that the driver reports the extension before using it:

```cpp
uint32_t count = 0;
zeDriverGetExtensionProperties(hDriver, &count, nullptr);
std::vector<ze_driver_extension_properties_t> extensions(count);
zeDriverGetExtensionProperties(hDriver, &count, extensions.data());

bool supported = false;
for (const auto &ext : extensions) {
    if (std::strcmp(ext.name, ZE_EXTERNAL_MEMORY_MAPPING_EXT_NAME) == 0) {
        supported = true;
        break;
    }
}
```

Map an application-owned buffer and use it on the GPU with no copy. For brevity
the return code of every call should be checked (omitted here):

```cpp
#include <level_zero/ze_api.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>

// Rounds "value" up to a multiple of "alignment" (a power of two).
static size_t alignUp(size_t value, size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

void useApplicationBufferOnGpu(ze_context_handle_t context,
                               ze_device_handle_t device,
                               ze_kernel_handle_t kernel) {
    // Both pSystemMemory and size must be aligned to a page size supported by
    // the device. Query it via ze_device_memory_access_properties_t / the
    // device page-size properties; 4 KB is used here for illustration.
    constexpr size_t pageSize = 4096;
    const uint32_t elementCount = 1920 * 1080 * 3;
    const size_t size = alignUp(elementCount, pageSize);

    // The application already owns and fills its own host buffer.
    void *appBuffer = aligned_alloc(pageSize, size);
    memset(appBuffer, 0, size);
    // ... application produces data into appBuffer (decode, capture, compute) ...

    // Map that existing system memory into a USM host allocation - no copy.
    ze_external_memmap_sysmem_ext_desc_t memmapDesc = {
        ZE_STRUCTURE_TYPE_EXTERNAL_MEMMAP_SYSMEM_EXT_DESC};
    memmapDesc.pSystemMemory = appBuffer;
    memmapDesc.size = size;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    hostDesc.pNext = &memmapDesc;

    // usmPtr refers to the same physical pages as appBuffer and is accessible
    // by both the CPU and the GPU.
    void *usmPtr = nullptr;
    zeMemAllocHost(context, &hostDesc, size, pageSize, &usmPtr);

    // Create an immediate command list - appended work is submitted for
    // execution right away, no separate command queue. "computeOrdinal" is a
    // compute-capable command queue group ordinal obtained from
    // zeDeviceGetCommandQueueGroupProperties().
    const uint32_t computeOrdinal = 0u;
    ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    queueDesc.ordinal = computeOrdinal;
    queueDesc.index = 0;

    ze_command_list_handle_t cmdList = nullptr;
    zeCommandListCreateImmediate(context, device, &queueDesc, &cmdList);

    // Launch a kernel that takes the mapped pointer as its argument using the
    // simplified single-call append: it sets the group size and all kernel
    // arguments and enqueues the launch in one step (equivalent to
    // zeKernelSetGroupSize + zeKernelSetArgumentValue + AppendLaunchKernel).
    // The kernel reads/writes the application's own pages directly, no copy.
    // (module load + zeKernelCreate omitted for brevity.)
    uint32_t groupSizeX = 0u, groupSizeY = 0u, groupSizeZ = 0u;
    zeKernelSuggestGroupSize(kernel, elementCount, 1u, 1u,
                             &groupSizeX, &groupSizeY, &groupSizeZ);

    ze_group_size_t groupSizes = {groupSizeX, groupSizeY, groupSizeZ};
    ze_group_count_t groupCounts = {elementCount / groupSizeX, 1u, 1u};

    // pArguments is an array of pointers, one per kernel argument, each pointing
    // to that argument's value - here a single pointer argument (the mapped buffer).
    void *kernelArgs[] = {&usmPtr};
    zeCommandListAppendLaunchKernelWithArguments(
        cmdList, kernel, groupCounts, groupSizes, kernelArgs,
        nullptr, nullptr, 0, nullptr);

    // Wait for the immediate command list to complete the submitted work; the
    // kernel's results are then visible in appBuffer directly on the CPU.
    zeCommandListHostSynchronize(cmdList, UINT64_MAX);

    // Freeing the USM allocation releases the mapping. The application still
    // owns appBuffer and must free it itself, after the mapping is gone.
    zeCommandListDestroy(cmdList);
    zeMemFree(context, usmPtr);
    free(appBuffer);
}
```

# Known Issues and Limitations

* `pSystemMemory` and `size` must both be aligned to a page size supported by
  the device; otherwise `zeMemAllocHost` returns `ZE_RESULT_ERROR_UNSUPPORTED_SIZE`
  / `ZE_RESULT_ERROR_INVALID_ARGUMENT`.
* The application retains ownership of the underlying system memory. It must keep
  that memory valid for the entire lifetime of the USM allocation, and free it
  only after the allocation has been released with `zeMemFree`.
* The descriptor is only valid on `zeMemAllocHost` (host USM). It is not accepted
  by `zeMemAllocDevice` or `zeMemAllocShared`.
