<!---

Copyright (C) 2026 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Get Handle For Address Range

* [Overview](#Overview)
* [Definitions](#Definitions)
* [Usage](#Usage)
* [Synchronization and Lifetime](#Synchronization-and-Lifetime)
* [Known Issues and Limitations](#Known-Issues-and-Limitations)

# Overview

`zeIntelMemGetHandleForAddressRange` returns a DMA-BUF file descriptor for memory of an existing device allocation, including allocations created without `ze_external_memory_export_desc_t` and allocations owned by another framework. The descriptor can be passed to a kernel driver of another subsystem, for example to `ibv_reg_dmabuf_mr` to register the memory with an RDMA NIC.

The descriptor names the whole object that backs the pointer. For a pooled allocation that object is the whole pool, so the pointer usually lies at a non-zero offset in it. Chain `ze_intel_mem_range_location_exp_properties_t` to get that offset together with the base address and size of the object.

Support is reported per device by chaining `ze_intel_device_mem_range_handle_exp_properties_t` to `zeDeviceGetExternalMemoryProperties`.

# Definitions

```cpp
#define ZE_INTEL_MEM_GET_HANDLE_FOR_ADDRESS_RANGE_EXP_NAME "ZE_intel_experimental_mem_get_handle_for_address_range"

typedef enum _ze_intel_mem_range_handle_type_exp_t {
    ZE_INTEL_MEM_RANGE_HANDLE_TYPE_EXP_DMA_BUF_FD = 0, ///< Linux DMA-BUF file descriptor, returned as an int
    ZE_INTEL_MEM_RANGE_HANDLE_TYPE_EXP_FORCE_UINT32 = 0x7fffffff
} ze_intel_mem_range_handle_type_exp_t;

typedef struct _ze_intel_mem_range_location_exp_properties_t {
    ze_structure_type_ext_t stype; ///< ZE_INTEL_STRUCTURE_TYPE_MEM_RANGE_LOCATION_EXP_PROPERTIES
    void *pNext;
    uint64_t offset;               ///< [out] byte offset of ptr within the handle
    void *base;                    ///< [out] address that maps to offset 0 of the handle
    size_t size;                   ///< [out] size in bytes of the object named by the handle
} ze_intel_mem_range_location_exp_properties_t;

typedef struct _ze_intel_device_mem_range_handle_exp_properties_t {
    ze_structure_type_ext_t stype;                    ///< ZE_INTEL_STRUCTURE_TYPE_DEVICE_MEM_RANGE_HANDLE_EXP_PROPERTIES
    void *pNext;
    ze_external_memory_type_flags_t rangeExportTypes; ///< [out] ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF when supported, 0 otherwise
} ze_intel_device_mem_range_handle_exp_properties_t;
```

## Interfaces

```cpp
ze_result_t ZE_APICALL
zeIntelMemGetHandleForAddressRange(
    ze_context_handle_t hContext,                    ///< [in] handle of the context
    void *pHandle,                                   ///< [out] returned handle, an int for ZE_INTEL_MEM_RANGE_HANDLE_TYPE_EXP_DMA_BUF_FD
    const void *ptr,                                 ///< [in] start of the address range
    size_t size,                                     ///< [in] size in bytes of the address range
    ze_intel_mem_range_handle_type_exp_t handleType, ///< [in] type of the handle to return
    void *pNext                                      ///< [in,out][optional] ze_intel_mem_range_location_exp_properties_t
);
```

The full description of parameters and return codes is in `level_zero/driver_experimental/zex_memory.h`.

# Usage

```cpp
int fd = -1;
ze_intel_mem_range_location_exp_properties_t location{ZE_INTEL_STRUCTURE_TYPE_MEM_RANGE_LOCATION_EXP_PROPERTIES};
zeIntelMemGetHandleForAddressRange(hContext, &fd, ptr, len, ZE_INTEL_MEM_RANGE_HANDLE_TYPE_EXP_DMA_BUF_FD, &location);

// The object ends location.size - location.offset bytes after ptr.
// If len goes past that, call again with location.base + location.size for the rest.

// Register only the requested range...
ibv_reg_dmabuf_mr(pd, location.offset, len, reinterpret_cast<uint64_t>(ptr), fd, access);
// ...or the whole object once, and reach every allocation inside it at its own device address.
ibv_reg_dmabuf_mr(pd, 0, location.size, reinterpret_cast<uint64_t>(location.base), fd, access);

close(fd); // the registration keeps its own reference
```

Neither `ptr` nor `len` needs to be page aligned: every object starts page aligned, so `location.offset` and `ptr` always have the same offset within a page.

The descriptor names the whole object, so it is not an isolation boundary. Registering only the requested range keeps the registration inside the allocation; the descriptor itself does not.

# Synchronization and Lifetime

* GPU writes are visible through the descriptor once the host has synchronized with the GPU work that made them, for example with `zeCommandQueueSynchronize`, `zeEventHostSynchronize` or `zeCommandListHostSynchronize`.
* Writes made through the descriptor, for example by a NIC, are visible to GPU work submitted after those writes have completed.
* If the memory is compressed, the driver decompresses the whole object in place, so GPU work that accesses any allocation in that object, including other allocations of the same pool, must be complete before the call.
* The application must not free, unmap or remap the memory, or destroy its physical memory, while the descriptor or a registration made from it is in use. After that the driver may hand the memory to another allocation, including the space of a pooled allocation.

# Known Issues and Limitations

* Supported only with the Linux DRM kernel drivers, i915 and xe. Not supported on Windows or WSL.
* Only device memory is supported: device allocations, pooled device allocations and virtual memory ranges mapped to a whole physical memory object from its start. Other memory returns `ZE_RESULT_ERROR_INCOMPATIBLE_RESOURCE`, as do allocations split across several objects and imported allocations that map only part of the exported object.
* Compressed memory is decompressed before the descriptor is returned when the kernel driver can do it. With xe this needs a discrete Xe2 or later GPU with flat CCS and a kernel with `DRM_XE_VM_BIND_FLAG_DECOMPRESS`. Otherwise compressed memory returns `ZE_RESULT_ERROR_INCOMPATIBLE_RESOURCE`; allocating it with `ZE_MEMORY_COMPRESSION_HINTS_EXT_FLAG_UNCOMPRESSED` avoids that.
